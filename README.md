# C 기반 도서관 SQL 데모

이 프로젝트는 `books` 테이블 하나를 대상으로 동작하는 작은 C 기반 SQL 데모입니다.
핵심 목표는 읽기 쉬운 구조를 유지하면서, `WHERE id = ...`와
`WHERE id BETWEEN ... AND ...`가 B+ 트리 인덱스를 타고,
그 외 조건은 선형 탐색으로 처리되는 흐름을 명확하게 보여주는 것입니다.

## 프로젝트 개요
- 시작 시 `data/books.bin`을 메모리로 불러옵니다.
- `SELECT`, `INSERT`, 다중 SQL 배치를 지원합니다.
- `WHERE id = ...`는 B+ 트리 단건 조회를 사용합니다.
- `WHERE id BETWEEN a AND b`는 B+ 트리 리프 링크 범위 조회를 사용합니다.
- `title`, `author`, `genre`, 전체 조회는 선형 탐색만 사용합니다.
- 배치 출력은 내부 버퍼에 모아 두었다가 전부 성공했을 때만 최종 출력합니다.
- 배치 도중 실패하면 행 수와 `next_id`를 되돌리고 B+ 트리를 다시 구성합니다.
- 저장은 임시 파일 교체 방식으로 처리합니다.

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
- `docs/` : 설계, 테스트, 성능, GitHub 초안 문서
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
  src\cli.c src\exec.c src\main.c src\gen_perf.c -o build\sql2_books.exe
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

### 비대화형 CLI 예시
```powershell
.\build\sql2_books.exe --mode cli --batch "SELECT * FROM books WHERE id = 1;"
.\build\sql2_books.exe --mode cli --batch "SELECT * FROM books WHERE id BETWEEN 2 AND 4;"
.\build\sql2_books.exe --mode cli --batch "INSERT INTO books VALUES ('Book','Author','Genre');"
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
- `SELECT * FROM books WHERE id BETWEEN 10 AND 20;`
- `SELECT title,genre FROM books WHERE author = 'George Orwell';`
- `INSERT INTO books VALUES ('Book','Author','Genre');`
- `;`로 구분된 다중 문장 배치

## 입력 규칙
- 마지막 세미콜론은 반드시 필요합니다.
- `;;` 같은 빈 문장은 오류로 처리합니다.
- 문자열 리터럴 안의 세미콜론은 배치를 나누지 않습니다.
- SQL 문자열 리터럴은 작은따옴표를 사용합니다.
- 키워드는 대소문자를 구분하지 않습니다.
- `BETWEEN`은 현재 `id` 컬럼에만 허용합니다.

## 빠른 데모
### 데모 실행
```powershell
./scripts/demo.ps1
```

### 데모 쿼리 흐름
`data/demo_queries.sql`
- `id` 기준 B+ 트리 단건 조회
- `id` 기준 B+ 트리 범위 조회
- `author` 기준 선형 탐색
- `genre` 기준 선형 탐색
- 성공적인 `INSERT`
- 이어지는 `SELECT`

## 테스트
### 로컬 전체 검증
```powershell
./scripts/test.ps1
```

이 스크립트는 아래를 순서대로 실행합니다.
- 빌드
- 단위 테스트
- 기능 테스트
- acceptance 시나리오
- 기본 smoke 실행

### acceptance 시나리오만 실행
```powershell
./scripts/acceptance.ps1
```

## 성능 테스트
1,000,000건 데이터를 생성하고 아래 조회를 비교합니다.
- `id` 단건 조회
- `id` 범위 조회
- `author` 선형 탐색
- `genre` 선형 탐색

```powershell
./scripts/perf.ps1 -Count 1000000
```

실측 결과는 [docs/perf.md](docs/perf.md)에 정리되어 있습니다.

## 바이너리 포맷
- Query 바이너리 포맷: [docs/binary-format.md](docs/binary-format.md)
- Data 바이너리 포맷: [docs/binary-format.md](docs/binary-format.md)

## 차별화 포인트
- `id =`뿐 아니라 `id BETWEEN`도 B+ 트리 리프 링크로 처리합니다.
- B+ 트리 검증 API가 있어 트리 높이, 리프 수, 키 수를 검사할 수 있습니다.
- 배치 출력 버퍼링과 롤백을 함께 적용해 실패 배치의 부분 출력이 남지 않습니다.

## CI
GitHub Actions에서는 아래 작업을 실행합니다.
- build
- unit tests
- function tests
- sanitizer build and tests

대규모 성능 테스트와 acceptance 시나리오는 로컬 실행용으로 유지합니다.
