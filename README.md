# C 기반 도서관 SQL 데모

이 프로젝트는 도서관 도서 조회 데모를 위한 작은 C 기반 SQL 엔진입니다.
현재는 `books` 테이블 하나를 지원하며, 읽기 쉬운 코드, 배치 실행, 롤백,
바이너리 저장, 그리고 B+ 트리 조회와 선형 탐색의 간단한 성능 비교에
초점을 맞추고 있습니다.

## 프로젝트 개요
- 시작 시 `data/books.bin`의 `books` 데이터를 한 번만 메모리로 불러옵니다.
- 모든 쿼리는 메모리 캐시를 기준으로 실행합니다.
- `SELECT`, `INSERT`, 다중 SQL 배치를 지원합니다.
- `WHERE id = ...` 조건은 B+ 트리만 사용합니다.
- `title`, `author`, `genre`, 전체 조회는 선형 탐색만 사용합니다.
- 쓰기 작업은 배치 전체가 성공했을 때만 저장합니다.
- 배치 도중 실패하면 캐시 상태를 되돌리고 버퍼링된 출력도 버립니다.

## 스키마
`books`
- `id` : 자동 증가 정수
- `title` : 문자열
- `author` : 문자열
- `genre` : 문자열

`INSERT` 값은 반드시 `title`, `author`, `genre` 순서여야 합니다.

## 저장소 구조
- `src/` : 엔진 소스 코드
- `include/` : 공용 헤더
- `tests/` : 단위 테스트와 기능 테스트
- `data/` : 바이너리 데이터와 데모 쿼리
- `docs/` : 설계, 리뷰, 테스트, 성능, GitHub 초안 문서
- `scripts/` : 로컬 PowerShell 자동화 스크립트
- `.github/workflows/` : CI 설정
- `AGENTS.md` : 저장소 규칙과 작업 로그

## 빌드 방법
### PowerShell
```powershell
./scripts/build.ps1
```

### GCC 직접 실행
```powershell
gcc -std=c11 -Wall -Wextra -pedantic -Iinclude `
  src\util.c src\batch.c src\lex.c src\parse.c src\bpt.c src\store.c `
  src\cli.c `
  src\exec.c src\main.c src\gen_perf.c -o build\sql2_books.exe
```

### Make
Linux 기반 CI에서 사용합니다.
```bash
make
```

## 실행 방법
### 대화형 모드
```powershell
.\build\sql2_books.exe
```
실행 후 아래 중 하나를 선택합니다.
- `1` = CLI 직접 입력
- `2` = 파일 입력

### CLI 모드 예시
직접 입력 모드에서는 SQL 배치 전체를 큰따옴표로 감싸야 합니다.
```text
"SELECT * FROM books WHERE id = 1;"
"INSERT INTO books VALUES ('The Pragmatic Programmer','Andrew Hunt','SE'); SELECT * FROM books WHERE author = 'Andrew Hunt';"
```

### 비대화형 CLI 예시
```powershell
.\build\sql2_books.exe --mode cli --batch "SELECT * FROM books WHERE id = 1;"
```

### 파일 모드 예시
```powershell
.\build\sql2_books.exe --mode file --file data\demo_queries.sql
```

### 기본 파일 탐색 규칙
파일 모드에서 경로를 비워 두면 아래 순서대로 찾습니다.
1. `./data/input.qsql`
2. `./data/input.sql`

## 지원 SQL 문법
- `SELECT * FROM books;`
- `SELECT title,author FROM books;`
- `SELECT * FROM books WHERE id = 3;`
- `SELECT title,genre FROM books WHERE author = 'George Orwell';`
- `INSERT INTO books VALUES ('Book','Author','Genre');`
- `;`로 구분된 다중 문장 배치

## 입력 규칙
- 마지막 세미콜론은 반드시 필요합니다.
- `;;` 같은 빈 문장은 오류로 처리합니다.
- 문자열 리터럴 안의 세미콜론은 배치를 나누지 않습니다.
- SQL 문자열 리터럴은 작은따옴표를 사용합니다.
- 키워드는 대소문자를 구분하지 않습니다.

## 데모 시나리오
### 빠른 데모 실행
```powershell
./scripts/demo.ps1
```

### 데모 쿼리 파일
`data/demo_queries.sql`에는 아래 흐름이 들어 있습니다.
- `id` 기준 B+ 트리 조회
- `author` 기준 선형 탐색
- `genre` 기준 선형 탐색
- 성공적인 `INSERT`
- 이어지는 `SELECT`

## 테스트
### PowerShell
```powershell
./scripts/test.ps1
```

### Make
```bash
make test
```

## 성능 테스트
1,000,000건 데이터를 생성하고 조회 성능을 비교합니다.
```powershell
./scripts/perf.ps1 -Count 1000000
```

현재 실측 성능 결과는 [docs/perf.md](docs/perf.md)에 정리되어 있습니다.

## 바이너리 포맷
- Query 바이너리 포맷: [docs/binary-format.md](docs/binary-format.md)
- Data 바이너리 포맷: [docs/binary-format.md](docs/binary-format.md)

## CI
GitHub Actions에서는 아래 작업을 실행합니다.
- build
- unit tests
- function tests
- sanitizer build and tests

대규모 성능 테스트는 기본 CI에 넣지 않고, 수동 또는 로컬 실행용으로 유지합니다.
