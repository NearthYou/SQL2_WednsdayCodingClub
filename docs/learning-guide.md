# B+tree Mini SQL DB 학습 가이드

이 문서는 코드를 처음 읽는 사람이 전체 흐름을 먼저 잡고, 점점 낮은 수준으로 내려가며 각 모듈과 함수의 의미를 이해할 수 있도록 작성했다.

읽는 추천 순서:

1. 전체 실행 흐름
2. 데이터가 메모리에서 어떻게 생겼는지
3. SQL batch가 어떻게 query 구조체가 되는지
4. executor가 B+ 트리와 선형 탐색 중 무엇을 선택하는지
5. INSERT 실패 시 rollback이 어떻게 되는지
6. B+ 트리 split과 range scan
7. 함수 사전

## 1. 전체 그림

이 프로젝트는 상용 DB를 만드는 프로젝트가 아니다. 학습용으로 아래 흐름을 직접 볼 수 있게 만든 작은 DB이다.

```text
사용자 입력
-> DB 파일 로드
-> rows를 메모리에 올림
-> rows 기준으로 B+ 트리 index 재구성
-> SQL 문자열 입력
-> SQL batch parsing
-> query별 실행
-> SELECT 결과 출력
-> INSERT는 cache와 B+ 트리에 반영
-> batch 전체 성공 시 binary DB 저장
-> 하나라도 실패 시 rollback
```

핵심 아이디어는 단순하다.

- 실제 record는 `Table.rows` 배열에 있다.
- B+ 트리는 record 전체가 아니라 `id -> row_index`만 저장한다.
- `WHERE id = N`은 B+ 트리 단건 조회를 쓴다.
- `WHERE id BETWEEN A AND B`는 B+ 트리 leaf chain을 따라 range scan한다.
- `WHERE value = N`은 일부러 선형 탐색을 쓴다.
- 이렇게 해서 index lookup과 linear scan의 차이를 관찰한다.

## 2. 프로그램 실행 흐름

`src/main.c`의 `main` 함수가 전체 오케스트레이션을 담당한다.

```text
main
-> parse_order
-> DB path 입력
-> storage_load
   -> database_init
   -> table_append_loaded
   -> database_rebuild_indexes
-> read_sql_from_user
   -> input_unquote_cli_sql 또는 input_read_text_file
-> parse_batch
-> execute_batch
   -> transaction_begin
   -> execute_select 또는 execute_insert
   -> 실패 시 transaction_rollback
-> storage_save
-> printer_print_transaction_committed 또는 printer_print_transaction_rolled_back
-> cleanup
```

중요한 점:

- DB 파일은 실행 초기에 한 번 읽는다.
- B+ 트리는 파일에서 직접 읽지 않고, row를 읽은 뒤 메모리에서 다시 만든다.
- SQL batch 전체가 하나의 transaction이다.
- 저장은 batch 전체가 성공했을 때만 한다.

## 3. 데이터 모델

V1은 하나의 table만 다룬다.

```text
records(
  id INT64 implicit,
  name STRING,
  value INT64
)
```

코드에서는 `include/db.h`에 정의되어 있다.

```c
typedef struct {
    int64_t id;
    char *name;
    int64_t value;
} Record;
```

`Record`는 한 행이다.

```c
typedef struct {
    Record *rows;
    size_t row_count;
    size_t row_capacity;
    int64_t next_id;
    BPTree *index;
    size_t index_order;
} Table;
```

`Table`은 rows 배열과 ID index를 함께 들고 있다.

- `rows`: 실제 record 배열
- `row_count`: 현재 record 수
- `row_capacity`: malloc/realloc으로 확보한 배열 용량
- `next_id`: 다음 INSERT에 부여할 ID
- `index`: `id -> row_index` B+ 트리
- `index_order`: B+ 트리 차수

```c
typedef struct {
    Table records;
} Database;
```

`Database`는 지금은 `records` table 하나만 가진다. 여러 table을 지원하지 않는 이유는 B+ 트리, parser, transaction을 학습하는 데 집중하기 위해서다.

## 4. SQL에서 Query 구조체까지

V1 parser는 범용 SQL parser가 아니다. 지원하는 문장만 명시적으로 알아본다.

지원 SQL:

```sql
SELECT * FROM records;
SELECT * FROM records WHERE id = 10;
SELECT * FROM records WHERE id BETWEEN 10 AND 20;
SELECT * FROM records WHERE value = 100;
INSERT INTO records VALUES ('alice', 100);
```

파싱 결과는 `Query` 구조체이다.

```c
typedef enum {
    QUERY_SELECT_ALL = 0,
    QUERY_SELECT_ID_EQ,
    QUERY_SELECT_ID_RANGE,
    QUERY_SELECT_VALUE_EQ,
    QUERY_INSERT
} QueryType;
```

이 enum 덕분에 executor는 복잡한 AST를 순회하지 않는다. 그냥 `query->type`을 보고 어떤 실행 경로를 탈지 결정한다.

```c
typedef struct {
    QueryType type;
    int64_t id;
    int64_t min_id;
    int64_t max_id;
    int64_t value;
    char *insert_name;
    int64_t insert_value;
} Query;
```

각 필드는 query type에 따라 의미가 달라진다.

- `QUERY_SELECT_ID_EQ`: `id` 사용
- `QUERY_SELECT_ID_RANGE`: `min_id`, `max_id` 사용
- `QUERY_SELECT_VALUE_EQ`: `value` 사용
- `QUERY_INSERT`: `insert_name`, `insert_value` 사용

여러 SQL 문장은 `QueryBatch`에 담긴다.

```c
typedef struct {
    Query *items;
    size_t count;
    size_t capacity;
} QueryBatch;
```

## 5. Executor의 선택

`execute_batch`는 query를 순서대로 실행한다.

선택 규칙:

```text
QUERY_SELECT_ALL
-> 모든 row index를 결과 목록에 넣음

QUERY_SELECT_ID_EQ
-> bptree_find

QUERY_SELECT_ID_RANGE
-> bptree_find_range

QUERY_SELECT_VALUE_EQ
-> rows 배열을 처음부터 끝까지 선형 탐색

QUERY_INSERT
-> table_insert
```

결과 출력은 `printer_print_rows`가 맡는다.

executor가 직접 표를 그리지 않는 이유:

- executor는 "무엇을 실행할지"에 집중한다.
- printer는 "어떻게 보여줄지"에 집중한다.
- 그래서 실행 로직과 출력 로직을 따로 리뷰할 수 있다.

## 6. Transaction과 Rollback

V1의 변경 연산은 `INSERT`뿐이다. 그래서 rollback을 단순하게 설계했다.

transaction 시작 시 저장하는 값:

```c
typedef struct {
    size_t start_row_count;
    int64_t start_next_id;
} Transaction;
```

실패 시:

```text
rows를 start_row_count까지 truncate
next_id를 start_next_id로 복구
B+ 트리를 rows 기준으로 재구성
```

이 방식은 B+ 트리 delete를 구현하지 않아도 된다. 학습용 V1에서는 `INSERT`만 rollback하면 되므로 충분히 명확하다.

## 7. Binary DB 파일

파일에는 B+ 트리 노드를 저장하지 않는다.

저장하는 것:

```text
magic: MSQLDB1
version: 1
next_id
row_count
row 반복:
  id
  name length
  name bytes
  value
```

load 흐름:

```text
storage_load
-> magic/version 검사
-> next_id 읽기
-> row_count 읽기
-> rows 읽기
-> next_id가 row id보다 작은지 검사
-> database_rebuild_indexes
```

save 흐름:

```text
storage_save
-> path.tmp에 전체 DB 쓰기
-> 성공하면 rename(path.tmp, path)
```

temp file을 쓰는 이유는 저장 도중 실패했을 때 기존 DB 파일을 망가뜨리지 않기 위해서다.

## 8. B+ 트리 이해

이 프로젝트의 B+ 트리는 `id -> row_index` mapping만 저장한다.

내부 구조:

```c
typedef struct BPTreeNode {
    bool is_leaf;
    size_t num_keys;
    int64_t *keys;
    struct BPTreeNode **children;
    size_t *record_ptrs;
    struct BPTreeNode *next;
} BPTreeNode;
```

노드 타입별 사용 필드:

```text
internal node:
  is_leaf = false
  keys[]
  children[]

leaf node:
  is_leaf = true
  keys[]
  record_ptrs[]
  next
```

`order`의 의미:

- order `m`은 internal node가 가질 수 있는 최대 child 수이다.
- internal node의 최대 key 수는 `m - 1`이다.
- leaf도 최대 `m - 1`개 key를 가진다.

### INSERT 흐름

```text
bptree_insert
-> insert_recursive
-> leaf까지 내려감
-> leaf_insert_sorted
-> overflow면 split_leaf
-> 부모에 promoted key 삽입
-> internal overflow면 split_internal
-> root split이면 새 root 생성
```

### leaf split

leaf가 꽉 차면 정렬된 key를 좌우 leaf로 나눈다.

```text
left leaf: 작은 key들
right leaf: 큰 key들
parent에 올라가는 key: right leaf의 첫 번째 key
left->next = right
```

이 `next` 포인터가 range scan에 쓰인다.

### internal split

internal node가 overflow되면 중앙 key를 parent로 올린다.

```text
left internal: 중앙 key보다 작은 key들
promoted key: parent로 올라감
right internal: 중앙 key보다 큰 key들
```

promoted key는 left/right child에 남지 않는다.

### range search

`WHERE id BETWEEN A AND B`는 다음처럼 동작한다.

```text
1. A 이상이 들어갈 leaf를 찾음
2. 해당 leaf에서 A 이상 key부터 읽음
3. leaf->next를 따라 오른쪽 leaf로 이동
4. key가 B보다 커지면 중단
5. 각 key의 record_ptrs를 결과에 추가
```

## 9. 주요 읽기 경로

처음 코드를 읽는다면 이 순서가 좋다.

1. `include/db.h`
2. `src/db.c`
3. `include/bptree.h`
4. `src/bptree.c`
5. `include/parser.h`
6. `src/parser.c`
7. `src/executor.c`
8. `src/storage.c`
9. `src/main.c`
10. `tests/test_bptree.c`
11. `tests/test_executor.c`

## 10. 함수 사전

아래는 production 코드 중심의 함수 사전이다. `static` 함수는 해당 `.c` 파일 내부에서만 쓰이는 helper이다.

## 10.1 error 모듈

파일:

- `include/error.h`
- `src/error.c`

### `void error_clear(SqlError *error)`

언제 쓰나:

- 새 작업을 시작하기 전 `SqlError`를 빈 상태로 만들 때 사용한다.

어디서 쓰나:

- `main`
- tests
- executor 내부 rollback error 처리

인자:

- `error`: 초기화할 오류 구조체

반환:

- 없음

의미:

- `phase`를 `ERROR_NONE`으로 만들고, statement index와 message를 비운다.

### `void error_set(SqlError *error, ErrorPhase phase, size_t statement_index, const char *fmt, ...)`

언제 쓰나:

- 어떤 단계에서 실패했는지 사용자에게 전달해야 할 때 사용한다.

어디서 쓰나:

- input, parser, storage, executor, transaction, save 경로 전체

인자:

- `error`: 기록 대상
- `phase`: 실패 단계
- `statement_index`: 몇 번째 SQL statement에서 실패했는지
- `fmt`, `...`: `printf` 스타일 메시지

반환:

- 없음

의미:

- 실패 원인을 구조화해서 저장한다.

### `const char *error_phase_name(ErrorPhase phase)`

언제 쓰나:

- 오류를 출력할 때 enum을 사람이 읽을 문자열로 바꾼다.

어디서 쓰나:

- `printer_print_error`

인자:

- `phase`: 오류 단계 enum

반환:

- `"input"`, `"parse"`, `"storage"` 같은 문자열

의미:

- error output의 phase 이름을 통일한다.

## 10.2 input 모듈

파일:

- `include/input.h`
- `src/input.c`

### `bool input_read_line(FILE *in, char *buffer, size_t buffer_size)`

언제 쓰나:

- 사용자 입력 한 줄을 읽을 때 사용한다.

어디서 쓰나:

- `main`
- `read_sql_from_user`

인자:

- `in`: 입력 stream, 보통 `stdin`
- `buffer`: 입력을 저장할 배열
- `buffer_size`: 배열 크기

반환:

- 성공하면 `true`
- EOF 또는 읽기 실패면 `false`

의미:

- `fgets`로 한 줄을 읽고 마지막 newline을 제거한다.

### `static const char *skip_left_spaces(const char *p)`

언제 쓰나:

- CLI SQL에서 맨 앞 공백을 무시할 때 사용한다.

어디서 쓰나:

- `input_unquote_cli_sql`

인자:

- `p`: 문자열 위치

반환:

- 앞쪽 공백을 지나친 위치

의미:

- input 모듈 내부용 공백 처리 helper이다.

### `bool input_unquote_cli_sql(const char *line, char **out_sql, SqlError *error)`

언제 쓰나:

- CLI mode에서 사용자가 입력한 SQL이 쌍따옴표로 감싸져 있는지 확인할 때 사용한다.

어디서 쓰나:

- `read_sql_from_user`

인자:

- `line`: 사용자가 입력한 전체 줄
- `out_sql`: 따옴표를 제거한 SQL 문자열을 받을 포인터
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`, `*out_sql`에 malloc된 SQL 문자열 저장
- 실패하면 `false`

의미:

- `"SELECT ...;"`에서 바깥 `"`를 제거한다.
- 호출자가 `*out_sql`을 `free`해야 한다.

### `bool input_read_text_file(const char *path, char **out_sql, SqlError *error)`

언제 쓰나:

- SQL batch text file을 읽을 때 사용한다.

어디서 쓰나:

- `read_sql_from_user`

인자:

- `path`: SQL 파일 경로
- `out_sql`: 파일 전체 내용을 받을 포인터
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`, `*out_sql`에 malloc된 파일 내용 저장
- 실패하면 `false`

의미:

- SQL batch file 전체를 메모리 문자열로 읽는다.

## 10.3 main 모듈

파일:

- `src/main.c`

### `static size_t parse_order(int argc, char **argv)`

언제 쓰나:

- 프로그램 시작 시 B+ 트리 차수를 정할 때 사용한다.

어디서 쓰나:

- `main`

인자:

- `argc`, `argv`: command line argument

반환:

- `--order N`이 있고 `N >= 3`이면 `N`
- 아니면 기본값 `128`

의미:

- B+ 트리 order를 CLI에서 조절할 수 있게 한다.

### `static bool read_sql_from_user(char **out_sql, SqlError *error)`

언제 쓰나:

- DB를 로드한 뒤 SQL 입력 방식을 선택하고 SQL 문자열을 얻을 때 사용한다.

어디서 쓰나:

- `main`

인자:

- `out_sql`: 최종 SQL batch 문자열을 받을 포인터
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- 실패하면 `false`

의미:

- mode `1`이면 quoted CLI SQL을 읽는다.
- mode `2`이면 SQL batch text file을 읽는다.

### `int main(int argc, char **argv)`

언제 쓰나:

- 프로그램 진입점이다.

어디서 쓰나:

- OS가 실행한다.

인자:

- `argc`, `argv`: command line argument

반환:

- 성공하면 `0`
- 실패하면 `1`

의미:

- DB load, SQL input, parse, execute, save, cleanup 전체 흐름을 연결한다.

## 10.4 DB 모듈

파일:

- `include/db.h`
- `src/db.c`

### `static char *string_duplicate(const char *value)`

언제 쓰나:

- record name 문자열을 table 내부에 소유시키기 위해 복사할 때 사용한다.

어디서 쓰나:

- `table_append_loaded`

인자:

- `value`: 복사할 문자열

반환:

- 성공하면 malloc된 문자열
- 실패하면 `NULL`

의미:

- C 표준 `strdup` 대신 직접 구현한 문자열 복사 helper이다.

### `bool table_init(Table *table, size_t index_order, SqlError *error)`

언제 쓰나:

- records table을 처음 만들 때 사용한다.

어디서 쓰나:

- `database_init`
- storage load 초기화

인자:

- `table`: 초기화할 table
- `index_order`: B+ 트리 order
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- B+ 트리 생성 실패 시 `false`

의미:

- row 배열을 비우고, `next_id = 1`로 설정하고, 빈 B+ 트리를 만든다.

### `void table_free(Table *table)`

언제 쓰나:

- table이 더 이상 필요 없을 때 사용한다.

어디서 쓰나:

- `database_free`
- storage load 실패 cleanup

인자:

- `table`: 해제할 table

반환:

- 없음

의미:

- 모든 record name, rows 배열, B+ 트리를 해제한다.

### `static bool table_reserve(Table *table, size_t needed, SqlError *error)`

언제 쓰나:

- row를 append하기 전에 rows 배열 용량을 확보할 때 사용한다.

어디서 쓰나:

- `table_append_loaded`

인자:

- `table`: 대상 table
- `needed`: 필요한 최소 row 수
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- realloc 실패 시 `false`

의미:

- dynamic array 용량을 늘린다.

### `bool table_append_loaded(Table *table, int64_t id, const char *name, int64_t value, SqlError *error)`

언제 쓰나:

- 파일에서 읽은 row를 table에 넣을 때 사용한다.
- 내부적으로 `table_insert`도 ID가 정해진 row 추가에 이 함수를 재사용한다.

어디서 쓰나:

- `storage_load`
- `table_insert`

인자:

- `table`: 대상 table
- `id`: row ID
- `name`: row name
- `value`: row value
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- 잘못된 ID, 메모리 부족이면 `false`

의미:

- rows 배열에 record를 append한다.
- 이 함수 자체는 B+ 트리에 insert하지 않는다.

### `bool table_insert(Table *table, const char *name, int64_t value, SqlError *error)`

언제 쓰나:

- SQL `INSERT`를 실행할 때 사용한다.

어디서 쓰나:

- `execute_insert`
- fixture/perf 도구

인자:

- `table`: 대상 table
- `name`: 새 record name
- `value`: 새 record value
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- row append 또는 B+ 트리 insert 실패 시 `false`

의미:

- `id = next_id`를 부여한다.
- row를 append한다.
- `next_id`를 증가시킨다.
- B+ 트리에 `id -> row_index`를 등록한다.

### `void table_truncate(Table *table, size_t row_count)`

언제 쓰나:

- rollback 또는 insert 실패 보정 시 rows를 이전 개수로 되돌릴 때 사용한다.

어디서 쓰나:

- `transaction_rollback`
- `table_insert` 실패 보정

인자:

- `table`: 대상 table
- `row_count`: 남길 row 수

반환:

- 없음

의미:

- `row_count` 이후 record의 name 메모리를 해제하고 row count를 줄인다.

### `bool table_rebuild_index(Table *table, SqlError *error)`

언제 쓰나:

- DB load 후, rollback 후 B+ 트리를 rows 기준으로 다시 만들 때 사용한다.

어디서 쓰나:

- `database_rebuild_indexes`
- `transaction_rollback`

인자:

- `table`: 대상 table
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- B+ 트리 생성, 중복 ID, invariant 실패 시 `false`

의미:

- 기존 B+ 트리를 버리고 rows 배열을 처음부터 스캔해 index를 다시 만든다.

### `bool database_init(Database *db, size_t index_order, SqlError *error)`

언제 쓰나:

- 새 Database 구조체를 준비할 때 사용한다.

어디서 쓰나:

- `storage_load`
- tests
- tools

인자:

- `db`: 초기화할 database
- `index_order`: B+ 트리 order
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- 실패하면 `false`

의미:

- 현재는 `records` table 하나를 초기화한다.

### `void database_free(Database *db)`

언제 쓰나:

- Database 사용이 끝났을 때 사용한다.

어디서 쓰나:

- `main`
- tests
- tools

인자:

- `db`: 해제할 database

반환:

- 없음

의미:

- 내부 table 자원을 해제한다.

### `bool database_rebuild_indexes(Database *db, SqlError *error)`

언제 쓰나:

- 모든 table의 index를 다시 만들 때 사용한다.

어디서 쓰나:

- `storage_load`
- `transaction_rollback`

인자:

- `db`: 대상 database
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- 실패하면 `false`

의미:

- 현재는 `records` table의 B+ 트리만 재구성한다.

## 10.5 B+ 트리 모듈

파일:

- `include/bptree.h`
- `src/bptree.c`

### `static size_t max_keys(const BPTree *tree)`

언제 쓰나:

- 노드가 overflow인지 판단할 때 사용한다.

어디서 쓰나:

- insert, validate 경로

인자:

- `tree`: 대상 B+ 트리

반환:

- `tree->order - 1`

의미:

- order `m`에서 한 노드가 가질 수 있는 최대 key 수를 계산한다.

### `void row_index_list_init(RowIndexList *list)`

언제 쓰나:

- SELECT 결과 row index 목록을 사용하기 전에 호출한다.

어디서 쓰나:

- executor
- B+ 트리 tests
- perf tool

인자:

- `list`: 초기화할 목록

반환:

- 없음

의미:

- dynamic array 상태를 빈 값으로 만든다.

### `bool row_index_list_append(RowIndexList *list, size_t row_index)`

언제 쓰나:

- SELECT 결과에 row index를 추가할 때 사용한다.

어디서 쓰나:

- `bptree_find_range`
- executor helper

인자:

- `list`: 대상 목록
- `row_index`: 추가할 row index

반환:

- 성공하면 `true`
- 메모리 부족이면 `false`

의미:

- 결과 목록을 필요하면 확장하고 값을 append한다.

### `void row_index_list_free(RowIndexList *list)`

언제 쓰나:

- SELECT 결과 목록 사용이 끝났을 때 호출한다.

어디서 쓰나:

- executor
- tests
- perf tool

인자:

- `list`: 해제할 목록

반환:

- 없음

의미:

- 내부 배열을 free하고 상태를 비운다.

### `static BPTreeNode *node_create(size_t order, bool is_leaf)`

언제 쓰나:

- 새 B+ 트리 노드를 만들 때 사용한다.

어디서 쓰나:

- `bptree_create`
- `split_leaf`
- `split_internal`
- root split

인자:

- `order`: 트리 차수
- `is_leaf`: leaf 여부

반환:

- 성공하면 새 노드 포인터
- 실패하면 `NULL`

의미:

- leaf면 `record_ptrs`를 만들고, internal이면 `children`을 만든다.

### `BPTree *bptree_create(size_t order)`

언제 쓰나:

- 빈 ID index를 만들 때 사용한다.

어디서 쓰나:

- `table_init`
- `table_rebuild_index`
- tests

인자:

- `order`: B+ 트리 차수. 3보다 작으면 3으로 보정한다.

반환:

- 성공하면 B+ 트리 포인터
- 실패하면 `NULL`

의미:

- 빈 leaf root를 가진 B+ 트리를 생성한다.

### `static void node_free(BPTreeNode *node)`

언제 쓰나:

- B+ 트리 노드를 재귀적으로 해제할 때 사용한다.

어디서 쓰나:

- `bptree_free`

인자:

- `node`: 해제할 노드

반환:

- 없음

의미:

- internal node면 children을 먼저 해제하고, 배열과 노드를 해제한다.

### `void bptree_free(BPTree *tree)`

언제 쓰나:

- B+ 트리 사용이 끝났을 때 호출한다.

어디서 쓰나:

- `table_free`
- `table_rebuild_index`
- tests

인자:

- `tree`: 해제할 B+ 트리

반환:

- 없음

의미:

- 전체 노드와 tree 구조체를 해제한다.

### `static size_t lower_bound(const int64_t *keys, size_t count, int64_t key)`

언제 쓰나:

- 정렬된 key 배열에서 key가 들어갈 위치를 찾을 때 사용한다.

어디서 쓰나:

- leaf insert
- find
- range find

인자:

- `keys`: 정렬된 key 배열
- `count`: key 개수
- `key`: 찾을 key

반환:

- `keys[pos] >= key`인 첫 위치
- 없으면 `count`

의미:

- B+ 트리 내부의 작은 검색 helper이다.

### `static size_t child_index_for(const BPTreeNode *node, int64_t key)`

언제 쓰나:

- internal node에서 어느 child로 내려갈지 결정할 때 사용한다.

어디서 쓰나:

- insert
- find leaf

인자:

- `node`: internal node
- `key`: 찾거나 삽입할 key

반환:

- 내려갈 child index

의미:

- `key >= separator`이면 오른쪽 child로 이동한다.

### `static bool leaf_insert_sorted(BPTreeNode *leaf, int64_t key, size_t row_index)`

언제 쓰나:

- leaf node에 새 key를 넣을 때 사용한다.

어디서 쓰나:

- `insert_recursive`

인자:

- `leaf`: 삽입 대상 leaf
- `key`: ID
- `row_index`: record 위치

반환:

- 성공하면 `true`
- 중복 key면 `false`

의미:

- leaf의 key 정렬을 유지하며 `key -> row_index`를 삽입한다.

### `static SplitResult split_leaf(BPTree *tree, BPTreeNode *leaf)`

언제 쓰나:

- leaf가 최대 key 수를 넘었을 때 사용한다.

어디서 쓰나:

- `insert_recursive`

인자:

- `tree`: order를 알기 위한 B+ 트리
- `leaf`: overflow된 leaf

반환:

- split 성공 정보, promoted key, 오른쪽 leaf 포인터

의미:

- leaf를 좌우로 나누고 `next` chain을 연결한다.
- 오른쪽 leaf의 첫 key를 부모에 올릴 key로 반환한다.

### `static bool internal_insert_child(BPTreeNode *node, size_t pos, int64_t key, BPTreeNode *right_child)`

언제 쓰나:

- child split 결과를 parent internal node에 반영할 때 사용한다.

어디서 쓰나:

- `insert_recursive`

인자:

- `node`: parent internal node
- `pos`: key를 삽입할 위치
- `key`: promoted key
- `right_child`: split으로 생긴 오른쪽 child

반환:

- 항상 `true`

의미:

- parent의 key/child 배열을 한 칸씩 밀고 promoted key와 right child를 넣는다.

### `static SplitResult split_internal(BPTree *tree, BPTreeNode *node)`

언제 쓰나:

- internal node가 overflow되었을 때 사용한다.

어디서 쓰나:

- `insert_recursive`

인자:

- `tree`: order를 알기 위한 B+ 트리
- `node`: overflow된 internal node

반환:

- split 성공 정보, promoted key, 오른쪽 internal node

의미:

- 중앙 key를 부모로 올리고, 좌우 internal node로 나눈다.

### `static SplitResult insert_recursive(BPTree *tree, BPTreeNode *node, int64_t key, size_t row_index, bool *inserted)`

언제 쓰나:

- B+ 트리 insert의 실제 재귀 작업을 수행할 때 사용한다.

어디서 쓰나:

- `bptree_insert`

인자:

- `tree`: 대상 B+ 트리
- `node`: 현재 방문 중인 node
- `key`: ID
- `row_index`: record 위치
- `inserted`: 실제 삽입 여부를 받을 포인터

반환:

- split이 발생했는지와 promoted 정보를 담은 `SplitResult`

의미:

- leaf까지 내려가 삽입하고, split 결과를 부모 방향으로 전파한다.

### `bool bptree_insert(BPTree *tree, int64_t id, size_t row_index)`

언제 쓰나:

- 새 record ID를 index에 등록할 때 사용한다.

어디서 쓰나:

- `table_insert`
- `table_rebuild_index`
- tests
- perf

인자:

- `tree`: 대상 B+ 트리
- `id`: unique key
- `row_index`: rows 배열 위치

반환:

- 성공하면 `true`
- 중복 key 또는 메모리 실패면 `false`

의미:

- `id -> row_index` mapping을 B+ 트리에 넣는다.
- root split이 생기면 새 root를 만든다.

### `static const BPTreeNode *find_leaf(const BPTree *tree, int64_t key)`

언제 쓰나:

- 특정 key가 있을 leaf를 찾을 때 사용한다.

어디서 쓰나:

- `bptree_find`
- `bptree_find_range`

인자:

- `tree`: 대상 B+ 트리
- `key`: 찾을 key

반환:

- 도착한 leaf node 포인터

의미:

- root에서 internal separator key를 따라 leaf까지 내려간다.

### `bool bptree_find(const BPTree *tree, int64_t id, size_t *out_row_index)`

언제 쓰나:

- `WHERE id = N` 단건 조회를 실행할 때 사용한다.

어디서 쓰나:

- executor
- tests

인자:

- `tree`: 대상 B+ 트리
- `id`: 찾을 ID
- `out_row_index`: 찾은 row index를 받을 포인터

반환:

- 찾으면 `true`
- 없으면 `false`

의미:

- B+ 트리에서 ID를 찾아 rows 배열 위치를 반환한다.

### `bool bptree_find_range(const BPTree *tree, int64_t min_id, int64_t max_id, RowIndexList *out_rows)`

언제 쓰나:

- `WHERE id BETWEEN A AND B` range 조회를 실행할 때 사용한다.

어디서 쓰나:

- executor
- tests
- perf

인자:

- `tree`: 대상 B+ 트리
- `min_id`: inclusive 시작 ID
- `max_id`: inclusive 끝 ID
- `out_rows`: 결과 row index 목록

반환:

- 성공하면 `true`
- `min_id > max_id` 또는 메모리 부족이면 `false`

의미:

- `min_id`가 들어갈 leaf를 찾은 뒤 leaf `next`를 따라가며 range 안의 row index를 모은다.

### `size_t bptree_height(const BPTree *tree)`

언제 쓰나:

- 테스트나 디버깅에서 트리 높이를 확인할 때 사용한다.

어디서 쓰나:

- tests

인자:

- `tree`: 대상 B+ 트리

반환:

- tree height

의미:

- root split이 실제로 일어났는지 관찰할 수 있다.

### `size_t bptree_key_count(const BPTree *tree)`

언제 쓰나:

- 테스트나 디버깅에서 저장된 key 수를 확인할 때 사용한다.

어디서 쓰나:

- tests

인자:

- `tree`: 대상 B+ 트리

반환:

- 전체 key 수

의미:

- 삽입된 unique ID 수를 반환한다.

### `static bool report_fail(BPTreeValidationReport *report, const char *message)`

언제 쓰나:

- validate 중 실패 이유를 기록할 때 사용한다.

어디서 쓰나:

- validate helpers

인자:

- `report`: 검증 결과 구조체
- `message`: 실패 메시지

반환:

- 항상 `false`

의미:

- `return report_fail(...)` 형태로 실패 코드를 짧게 만든다.

### `static bool validate_node(...)`

언제 쓰나:

- B+ 트리 node invariant를 재귀적으로 검사할 때 사용한다.

어디서 쓰나:

- `bptree_validate`

인자:

- `tree`: 대상 트리
- `node`: 현재 node
- `depth`: 현재 깊이
- `leaf_depth`: 모든 leaf가 같은 깊이인지 비교할 값
- `report`: 결과 저장

반환:

- 유효하면 `true`
- 깨진 invariant가 있으면 `false`

의미:

- key 정렬, key 수, null child, leaf depth를 확인한다.

### `static const BPTreeNode *leftmost_leaf(const BPTreeNode *node)`

언제 쓰나:

- leaf chain 검증 시작점을 찾을 때 사용한다.

어디서 쓰나:

- `validate_leaf_chain`

인자:

- `node`: 보통 root

반환:

- 가장 왼쪽 leaf

의미:

- B+ 트리 leaf linked list의 시작점을 찾는다.

### `static bool validate_leaf_chain(const BPTree *tree, BPTreeValidationReport *report)`

언제 쓰나:

- leaf `next` chain이 전체 key를 정렬된 순서로 연결하는지 검사할 때 사용한다.

어디서 쓰나:

- `bptree_validate`

인자:

- `tree`: 대상 B+ 트리
- `report`: 결과 저장

반환:

- 유효하면 `true`
- 깨졌으면 `false`

의미:

- range search가 의존하는 leaf chain의 정렬성과 개수를 검증한다.

### `bool bptree_validate(const BPTree *tree, BPTreeValidationReport *out_report)`

언제 쓰나:

- insert 후, rebuild 후, 테스트에서 B+ 트리 invariant를 확인할 때 사용한다.

어디서 쓰나:

- `execute_insert`
- `table_rebuild_index`
- tests

인자:

- `tree`: 검증할 B+ 트리
- `out_report`: 검증 결과를 받을 구조체. `NULL`이어도 된다.

반환:

- 유효하면 `true`
- 깨졌으면 `false`

의미:

- 학습용 프로젝트에서 가장 중요한 안전장치이다.

## 10.6 storage 모듈

파일:

- `include/storage.h`
- `src/storage.c`

### `static bool read_exact(FILE *file, void *buffer, size_t size)`

언제 쓰나:

- binary file에서 정확한 byte 수를 읽어야 할 때 사용한다.

어디서 쓰나:

- storage read helpers
- `storage_load`

인자:

- `file`: 파일 stream
- `buffer`: 읽은 내용을 저장할 위치
- `size`: 읽을 byte 수

반환:

- 정확히 읽으면 `true`
- 부족하면 `false`

의미:

- binary format 파싱의 기본 단위이다.

### `static bool write_exact(FILE *file, const void *buffer, size_t size)`

언제 쓰나:

- binary file에 정확한 byte 수를 써야 할 때 사용한다.

어디서 쓰나:

- storage write helpers
- `storage_save`

인자:

- `file`: 파일 stream
- `buffer`: 쓸 데이터
- `size`: 쓸 byte 수

반환:

- 정확히 쓰면 `true`
- 실패하면 `false`

의미:

- binary format 저장의 기본 단위이다.

### `read_u32`, `read_u64`, `read_i64`

언제 쓰나:

- binary DB에서 숫자 필드를 읽을 때 사용한다.

어디서 쓰나:

- `storage_load`

인자:

- `file`: 파일 stream
- `value`: 읽은 값을 받을 포인터

반환:

- 성공하면 `true`
- 실패하면 `false`

의미:

- 타입별 read helper이다.

### `write_u32`, `write_u64`, `write_i64`

언제 쓰나:

- binary DB에 숫자 필드를 저장할 때 사용한다.

어디서 쓰나:

- `storage_save`

인자:

- `file`: 파일 stream
- `value`: 저장할 값

반환:

- 성공하면 `true`
- 실패하면 `false`

의미:

- 타입별 write helper이다.

### `bool storage_load(const char *path, Database *db, size_t index_order, SqlError *error)`

언제 쓰나:

- 프로그램 시작 시 binary DB 파일을 읽을 때 사용한다.

어디서 쓰나:

- `main`
- tests

인자:

- `path`: DB 파일 경로
- `db`: 로드 결과를 받을 Database
- `index_order`: 재구성할 B+ 트리 order
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- 파일 열기, magic mismatch, format 손상, index rebuild 실패 시 `false`

의미:

- rows와 `next_id`를 파일에서 읽고, rows 기준으로 B+ 트리를 다시 만든다.

### `bool storage_save(const char *path, const Database *db, SqlError *error)`

언제 쓰나:

- transaction이 성공한 뒤 DB를 파일에 저장할 때 사용한다.

어디서 쓰나:

- `main`
- tests
- fixture tool

인자:

- `path`: 저장 대상 DB 파일 경로
- `db`: 저장할 Database
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- temp file 쓰기 또는 rename 실패 시 `false`

의미:

- DB 전체를 `path.tmp`에 쓴 뒤 rename해서 저장 안정성을 높인다.

## 10.7 parser 모듈

파일:

- `include/parser.h`
- `src/parser.c`

### `static char *string_duplicate_range(const char *start, size_t len)`

언제 쓰나:

- SQL 문자열 일부를 잘라 새 문자열로 만들 때 사용한다.

어디서 쓰나:

- statement 복사
- identifier/string literal 복사

인자:

- `start`: 시작 위치
- `len`: 복사할 길이

반환:

- malloc된 문자열
- 실패하면 `NULL`

의미:

- parser 내부에서 부분 문자열을 소유 가능한 문자열로 만든다.

### `static bool is_identifier_char(char c)`

언제 쓰나:

- identifier에 허용되는 문자인지 확인할 때 사용한다.

어디서 쓰나:

- keyword boundary 검사
- identifier parsing

인자:

- `c`: 검사할 문자

반환:

- 영문자/숫자/underscore이면 `true`

의미:

- `records`, `value` 같은 이름의 문자 규칙을 정의한다.

### `static void skip_spaces(const char **p)`

언제 쓰나:

- SQL token 사이 공백을 무시할 때 사용한다.

어디서 쓰나:

- parser helper 대부분

인자:

- `p`: 현재 문자열 위치 포인터의 주소

반환:

- 없음

의미:

- `*p`를 다음 non-space 위치로 이동시킨다.

### `static bool keyword_matches(const char *p, const char *keyword)`

언제 쓰나:

- 현재 위치가 특정 keyword인지 확인할 때 사용한다.

어디서 쓰나:

- `consume_keyword`
- `parse_statement`

인자:

- `p`: 현재 문자열 위치
- `keyword`: 기대 keyword

반환:

- 대소문자 무시하고 keyword가 맞으면 `true`

의미:

- `SELECTx`를 `SELECT`로 잘못 인식하지 않도록 identifier boundary도 확인한다.

### `static bool consume_keyword(const char **p, const char *keyword)`

언제 쓰나:

- keyword가 있으면 소비하고 다음 위치로 이동할 때 사용한다.

어디서 쓰나:

- select/insert parser

인자:

- `p`: 현재 위치 포인터의 주소
- `keyword`: 기대 keyword

반환:

- 있으면 `true`
- 없으면 `false`

의미:

- parser의 "기대한 token을 읽기" helper이다.

### `static bool consume_char(const char **p, char value)`

언제 쓰나:

- `*`, `=`, `,`, `(`, `)` 같은 단일 문자를 소비할 때 사용한다.

어디서 쓰나:

- select/insert parser

인자:

- `p`: 현재 위치 포인터의 주소
- `value`: 기대 문자

반환:

- 있으면 `true`
- 없으면 `false`

의미:

- 단일 punctuation token을 읽는다.

### `static char *parse_identifier(const char **p)`

언제 쓰나:

- table 이름이나 column 이름을 읽을 때 사용한다.

어디서 쓰나:

- `parse_records_table`
- `parse_select`

인자:

- `p`: 현재 위치 포인터의 주소

반환:

- malloc된 identifier 문자열
- 실패하면 `NULL`

의미:

- identifier 규칙에 맞는 문자열을 읽는다.

### `static bool equals_ignore_case(const char *left, const char *right)`

언제 쓰나:

- table/column 이름을 대소문자 무시하고 비교할 때 사용한다.

어디서 쓰나:

- `parse_records_table`
- `parse_select`

인자:

- `left`, `right`: 비교할 문자열

반환:

- 같으면 `true`

의미:

- SQL keyword와 identifier 비교를 case-insensitive로 만든다.

### `static bool parse_int64(const char **p, int64_t *out)`

언제 쓰나:

- SQL 숫자 literal을 읽을 때 사용한다.

어디서 쓰나:

- select/insert parser

인자:

- `p`: 현재 위치 포인터의 주소
- `out`: 읽은 정수를 받을 포인터

반환:

- 성공하면 `true`
- 숫자가 아니거나 범위를 넘으면 `false`

의미:

- `strtoll` 기반 INT64 parser이다.

### `static bool parse_single_quoted_string(const char **p, char **out)`

언제 쓰나:

- INSERT의 `'name'` 문자열 literal을 읽을 때 사용한다.

어디서 쓰나:

- `parse_insert`

인자:

- `p`: 현재 위치 포인터의 주소
- `out`: malloc된 문자열을 받을 포인터

반환:

- 성공하면 `true`
- 따옴표가 맞지 않거나 메모리 부족이면 `false`

의미:

- 단순 single-quoted string을 읽는다. escape 처리는 V1 범위 밖이다.

### `static bool at_end(const char *p)`

언제 쓰나:

- statement 끝에 예상치 못한 토큰이 남았는지 확인할 때 사용한다.

어디서 쓰나:

- select/insert parser

인자:

- `p`: 현재 문자열 위치

반환:

- 공백 외 문자가 없으면 `true`

의미:

- parser가 일부만 읽고 나머지를 무시하는 실수를 막는다.

### `void query_batch_init(QueryBatch *batch)`

언제 쓰나:

- `QueryBatch`를 사용하기 전에 호출한다.

어디서 쓰나:

- `parse_batch`

인자:

- `batch`: 초기화할 batch

반환:

- 없음

의미:

- dynamic array 상태를 빈 값으로 만든다.

### `static void query_free(Query *query)`

언제 쓰나:

- query 내부의 동적 메모리를 해제할 때 사용한다.

어디서 쓰나:

- `query_batch_free`

인자:

- `query`: 해제할 query

반환:

- 없음

의미:

- 현재는 `insert_name`만 해제한다.

### `void query_batch_free(QueryBatch *batch)`

언제 쓰나:

- parse 결과 사용이 끝났을 때 호출한다.

어디서 쓰나:

- `main`
- tests

인자:

- `batch`: 해제할 batch

반환:

- 없음

의미:

- batch 내부 query와 배열을 해제한다.

### `static bool query_batch_append(QueryBatch *batch, Query query, SqlError *error)`

언제 쓰나:

- 새 query를 batch에 추가할 때 사용한다.

어디서 쓰나:

- `parse_batch`

인자:

- `batch`: 대상 batch
- `query`: 추가할 query
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- 메모리 부족이면 `false`

의미:

- query dynamic array를 관리한다.

### `static bool parse_records_table(const char **p, size_t statement_index, SqlError *error)`

언제 쓰나:

- SQL에서 table 이름이 `records`인지 확인할 때 사용한다.

어디서 쓰나:

- `parse_select`
- `parse_insert`

인자:

- `p`: 현재 위치 포인터의 주소
- `statement_index`: 오류 표시용 statement 번호
- `error`: 실패 시 오류 기록

반환:

- `records`이면 `true`
- 아니면 `false`

의미:

- V1이 single-table임을 parser에서 명확히 제한한다.

### `static bool parse_select(const char *statement, Query *query, size_t statement_index, SqlError *error)`

언제 쓰나:

- 하나의 SELECT statement를 Query로 바꿀 때 사용한다.

어디서 쓰나:

- `parse_statement`

인자:

- `statement`: semicolon이 제거된 SQL statement
- `query`: 결과를 받을 Query
- `statement_index`: 오류 표시용 번호
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- 지원하지 않는 SELECT면 `false`

의미:

- `SELECT *`, `WHERE id =`, `WHERE id BETWEEN`, `WHERE value =`를 구분한다.

### `static bool parse_insert(const char *statement, Query *query, size_t statement_index, SqlError *error)`

언제 쓰나:

- 하나의 INSERT statement를 Query로 바꿀 때 사용한다.

어디서 쓰나:

- `parse_statement`

인자:

- `statement`: semicolon이 제거된 SQL statement
- `query`: 결과를 받을 Query
- `statement_index`: 오류 표시용 번호
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- 문법이 맞지 않으면 `false`

의미:

- `INSERT INTO records VALUES ('name', value)`만 허용한다.

### `static bool parse_statement(const char *statement, Query *query, size_t statement_index, SqlError *error)`

언제 쓰나:

- statement가 SELECT인지 INSERT인지 먼저 판단할 때 사용한다.

어디서 쓰나:

- `parse_batch`

인자:

- `statement`: semicolon이 제거된 SQL statement
- `query`: 결과 Query
- `statement_index`: 오류 표시용 번호
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- 지원하지 않는 keyword면 `false`

의미:

- 최상위 statement dispatcher이다.

### `bool parse_batch(const char *sql, QueryBatch *batch, SqlError *error)`

언제 쓰나:

- 사용자가 입력한 전체 SQL batch를 parsing할 때 사용한다.

어디서 쓰나:

- `main`
- tests

인자:

- `sql`: 전체 SQL 문자열
- `batch`: 결과 QueryBatch
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- 빈 입력, semicolon 누락, 빈 statement, 문법 오류면 `false`

의미:

- semicolon 기준으로 statement를 나누고 각각 Query로 변환한다.

## 10.8 transaction 모듈

파일:

- `include/transaction.h`
- `src/transaction.c`

### `void transaction_begin(Transaction *tx, const Database *db)`

언제 쓰나:

- batch 실행 직전에 rollback 기준점을 저장할 때 사용한다.

어디서 쓰나:

- `execute_batch`

인자:

- `tx`: transaction 상태를 받을 구조체
- `db`: 현재 database 상태

반환:

- 없음

의미:

- `row_count`와 `next_id`를 저장한다.

### `bool transaction_rollback(const Transaction *tx, Database *db, SqlError *error)`

언제 쓰나:

- batch 실행 중 실패했을 때 사용한다.

어디서 쓰나:

- `execute_batch`

인자:

- `tx`: 시작 시점 정보
- `db`: 되돌릴 database
- `error`: 실패 시 오류 기록

반환:

- rollback 성공하면 `true`
- index rebuild 실패 시 `false`

의미:

- rows를 시작 row count로 줄이고, `next_id`를 복구하고, B+ 트리를 재구성한다.

## 10.9 executor 모듈

파일:

- `include/executor.h`
- `src/executor.c`

### `static bool append_all_rows(const Table *table, RowIndexList *rows)`

언제 쓰나:

- `SELECT * FROM records` 실행 시 모든 row를 결과에 넣을 때 사용한다.

어디서 쓰나:

- `execute_select`

인자:

- `table`: 대상 table
- `rows`: 결과 row index 목록

반환:

- 성공하면 `true`
- 메모리 부족이면 `false`

의미:

- `0`부터 `row_count - 1`까지 row index를 append한다.

### `static bool append_value_matches(const Table *table, int64_t value, RowIndexList *rows)`

언제 쓰나:

- `WHERE value = N` 선형 탐색 시 사용한다.

어디서 쓰나:

- `execute_select`

인자:

- `table`: 대상 table
- `value`: 찾을 value
- `rows`: 결과 row index 목록

반환:

- 성공하면 `true`
- 메모리 부족이면 `false`

의미:

- rows 배열 전체를 scan해서 value가 같은 row index를 추가한다.

### `static bool execute_select(Database *db, const Query *query, size_t statement_index, FILE *out, SqlError *error)`

언제 쓰나:

- SELECT query 하나를 실행할 때 사용한다.

어디서 쓰나:

- `execute_batch`

인자:

- `db`: 조회 대상 database
- `query`: 실행할 SELECT query
- `statement_index`: 오류 표시용 번호
- `out`: 출력 stream
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- range 오류 또는 결과 목록 메모리 부족이면 `false`

의미:

- QueryType에 따라 전체 scan, B+ 트리 단건, B+ 트리 range, value 선형 탐색 중 하나를 선택하고 결과를 출력한다.

### `static bool execute_insert(Database *db, const Query *query, size_t statement_index, SqlError *error)`

언제 쓰나:

- INSERT query 하나를 실행할 때 사용한다.

어디서 쓰나:

- `execute_batch`

인자:

- `db`: 변경 대상 database
- `query`: INSERT query
- `statement_index`: 오류 표시용 번호
- `error`: 실패 시 오류 기록

반환:

- 성공하면 `true`
- insert 또는 B+ 트리 validate 실패 시 `false`

의미:

- 새 row를 넣고, B+ 트리 invariant가 유지되는지 확인한다.

### `bool execute_batch(Database *db, const QueryBatch *batch, FILE *out, SqlError *error)`

언제 쓰나:

- 전체 SQL batch를 실행할 때 사용한다.

어디서 쓰나:

- `main`
- tests

인자:

- `db`: 실행 대상 database
- `batch`: parse된 query 목록
- `out`: SELECT 출력 stream
- `error`: 실패 시 오류 기록

반환:

- 모든 query가 성공하면 `true`
- 하나라도 실패하면 rollback 후 `false`

의미:

- transaction 경계를 잡고 query를 순서대로 실행한다.

## 10.10 printer 모듈

파일:

- `include/printer.h`
- `src/printer.c`

### `static size_t digits_width_int64(int64_t value)`

언제 쓰나:

- 숫자 column의 출력 폭을 계산할 때 사용한다.

어디서 쓰나:

- `printer_print_rows`

인자:

- `value`: 출력할 정수

반환:

- 문자열로 표현했을 때 길이

의미:

- pretty print 정렬을 위한 helper이다.

### `static void print_border(FILE *out, size_t id_w, size_t name_w, size_t value_w)`

언제 쓰나:

- 표의 구분선을 출력할 때 사용한다.

어디서 쓰나:

- `printer_print_rows`

인자:

- `out`: 출력 stream
- `id_w`, `name_w`, `value_w`: 각 column 폭

반환:

- 없음

의미:

- `+----+------+-------+` 형태의 border를 출력한다.

### `void printer_print_rows(FILE *out, const Table *table, const RowIndexList *rows)`

언제 쓰나:

- SELECT 결과를 사용자에게 보여줄 때 사용한다.

어디서 쓰나:

- `execute_select`

인자:

- `out`: 출력 stream
- `table`: row를 읽을 table
- `rows`: 출력할 row index 목록

반환:

- 없음

의미:

- row index 목록을 실제 record로 바꾸고 표 형태로 출력한다.

### `void printer_print_error(FILE *out, const SqlError *error)`

언제 쓰나:

- 실패 메시지를 출력할 때 사용한다.

어디서 쓰나:

- `main`

인자:

- `out`: 출력 stream, 보통 `stderr`
- `error`: 출력할 오류

반환:

- 없음

의미:

- `ERROR [phase] statement N: message` 형식으로 오류를 출력한다.

### `void printer_print_transaction_rolled_back(FILE *out)`

언제 쓰나:

- batch 실패 후 rollback되었음을 출력할 때 사용한다.

어디서 쓰나:

- `main`

인자:

- `out`: 출력 stream

반환:

- 없음

의미:

- `TRANSACTION ROLLED BACK`을 출력한다.

### `void printer_print_transaction_committed(FILE *out)`

언제 쓰나:

- batch 성공 후 저장까지 끝났음을 출력할 때 사용한다.

어디서 쓰나:

- `main`

인자:

- `out`: 출력 stream

반환:

- 없음

의미:

- `TRANSACTION COMMITTED`를 출력한다.

## 10.11 tools

### `tools/make_fixture.c`

역할:

- `data/default.msqldb` 같은 학습용 binary DB fixture를 만든다.

주요 함수:

- `main(int argc, char **argv)`

동작:

```text
Database 생성
-> alice, bob, carol insert
-> storage_save
```

### `tools/perf.c`

역할:

- B+ 트리 lookup과 linear scan의 차이를 관찰한다.

주요 함수:

- `now_seconds`
- `main`

동작:

```text
record N개 insert
-> id 단건 lookup 반복
-> id range lookup 반복
-> value linear scan 반복
-> 시간과 ratio 출력
```

## 10.12 tests

테스트는 구현의 학습용 예제이기도 하다.

- `tests/test_bptree.c`
  - insert/find/range/root split/validate 확인
- `tests/test_parser.c`
  - 지원 SQL과 오류 SQL 확인
- `tests/test_storage.c`
  - binary save/load round trip과 corrupt DB 확인
- `tests/test_executor.c`
  - insert, select, rollback, next_id 복구 확인
- `tests/acceptance.sh`
  - 실제 `bin/minisql` CLI 흐름 확인

## 11. 디버깅할 때 보는 지점

### INSERT가 이상할 때

1. `execute_insert`
2. `table_insert`
3. `bptree_insert`
4. `bptree_validate`

### ID 조회가 이상할 때

1. `execute_select`
2. `bptree_find`
3. `find_leaf`
4. `child_index_for`
5. `lower_bound`

### ID range 조회가 이상할 때

1. `execute_select`
2. `bptree_find_range`
3. `find_leaf`
4. leaf `next` 연결
5. `validate_leaf_chain`

### rollback이 이상할 때

1. `execute_batch`
2. `transaction_begin`
3. 실패한 query
4. `transaction_rollback`
5. `table_truncate`
6. `database_rebuild_indexes`

### 저장/로드가 이상할 때

1. `storage_save`
2. `storage_load`
3. magic/version 검사
4. `next_id` 검사
5. `database_rebuild_indexes`

## 12. 이 프로젝트에서 의도적으로 단순화한 것

- SQL parser는 tokenizer/AST 중심이 아니라 명시적 문자열 parser이다.
- table은 하나만 있다.
- schema catalog는 없다.
- B+ 트리는 메모리에만 있다.
- rollback은 INSERT-only 전제에 맞춰 row truncate + index rebuild로 처리한다.
- string literal escape는 지원하지 않는다.
- value field만 non-ID linear scan 예제로 둔다.

이 단순화 덕분에 코드를 읽을 때 DB의 핵심 흐름인 "저장된 row", "index", "query 실행", "rollback"에 집중할 수 있다.
