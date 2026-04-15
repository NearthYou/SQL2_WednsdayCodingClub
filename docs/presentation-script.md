# 라이브 시연 시나리오와 4분 발표 대본

이 문서는 README 발표 흐름 표에 맞춰 실제 시연 순서와 발표 멘트를 따로 정리한 문서다.
발표 중에는 README와 이 문서 두 개만 열어 두면 된다.

---

## 라이브 시연 시나리오

### 사전 준비

```powershell
./scripts/build.ps1
./scripts/perf.ps1 -Count 1000000
```

발표 전 확인:

- `data/perf_books.bin`이 생성되어 있을 것
- `build/sql2_books.exe`가 최신 빌드일 것
- 시연은 `--summary-only`로 진행해 `rows`, `scan`, `time`만 보여줄 것

### 시연 1. id 단건 조회

```powershell
.\build\sql2_books.exe --mode cli --data data\perf_books.bin --summary-only --batch "SELECT * FROM books WHERE id = 1000000;"
```

예상 포인트:

- `rows : 1`
- `scan : B+Tree`
- `time : 0.001 ms`

시연 멘트:

> `id` 단건 조회는 B+Tree가 바로 리프를 찾기 때문에 거의 즉시 끝납니다.

### 시연 2. id 범위 조회

```powershell
.\build\sql2_books.exe --mode cli --data data\perf_books.bin --summary-only --batch "SELECT * FROM books WHERE id BETWEEN 999001 AND 1000000;"
```

예상 포인트:

- `rows : 1000`
- `scan : B+Tree`
- `time : 0.021 ms`

시연 멘트:

> 범위 조회도 B+Tree 리프 링크를 따라가므로 1000건이어도 매우 빠르게 나옵니다.

### 시연 3. author 선형 탐색

```powershell
.\build\sql2_books.exe --mode cli --data data\perf_books.bin --summary-only --batch "SELECT * FROM books WHERE author = 'Author 999';"
```

예상 포인트:

- `rows : 1000`
- `scan : Linear`
- `time : 103.440 ms`

시연 멘트:

> 결과 수는 똑같이 1000건인데, 인덱스가 없어서 전체 row를 훑기 때문에 100ms대로 점프합니다.

### 시연 한 줄 결론

> **같은 1000건 조회라도 B+Tree 경로는 `0.021 ms`, Linear 경로는 `103.440 ms`입니다.**

---

## 4분 발표 대본

### `0:00 ~ 0:30`

안녕하세요. 저희 프로젝트는 `books` 테이블 하나를 대상으로 동작하는 C 기반 SQL 데모입니다.  
핵심은 SQL을 직접 파싱하고 실행하는 구조를 보여주는 것, 그리고 `id` 조건 조회에 B+Tree를 적용했을 때 성능 차이가 얼마나 크게 나는지 보여주는 것입니다.

### `0:30 ~ 1:10`

입력된 SQL은 먼저 배치로 분리되고, lexer에서 토큰으로 바뀌고, parser에서 `Qry` 구조체로 바뀝니다.  
그 뒤 executor가 질의를 실행합니다. 여기서 중요한 분기점은 WHERE 조건입니다.  
`id` 조건이면 B+Tree를 타고, 나머지 조건이면 row 배열을 처음부터 끝까지 선형 탐색합니다.

### `1:10 ~ 1:40`

성능 표를 보면 차이가 분명합니다.  
`WHERE id = 1000000`은 `0.001 ms`, `WHERE id BETWEEN 999001 AND 1000000`은 `0.021 ms`입니다.  
반면 같은 1000건 결과를 돌려주는 `WHERE author = 'Author 999'`는 `103.440 ms`가 걸립니다.  
즉 같은 결과 개수라도 탐색 경로가 다르면 성능 차이가 매우 크게 납니다.

### `1:40 ~ 3:10`

이제 실제로 세 가지를 시연하겠습니다.  
먼저 `id` 단건 조회를 실행하면 `scan : B+Tree`, `time : 0.001 ms`가 나옵니다.  
다음으로 `id BETWEEN` 범위 조회를 실행하면 역시 B+Tree를 사용하고, 1000건이어도 `0.021 ms` 수준입니다.  
마지막으로 `author` 조건 조회를 실행하면 결과는 1000건으로 같지만 `scan : Linear`, `time : 103.440 ms`가 나옵니다.  
여기서 B+Tree의 효과가 숫자로 바로 드러납니다.

### `3:10 ~ 3:40`

추가로 이 프로젝트는 실패한 배치를 안전하게 되돌릴 수 있습니다.  
배치 시작 전에 `len`과 `next_id`를 저장해 두고, 중간에 실패하면 그 값으로 복원한 뒤 B+Tree를 다시 구성합니다.  
출력도 버퍼에 모았다가 성공했을 때만 내보내므로 실패 배치의 중간 결과가 남지 않습니다.

### `3:40 ~ 4:00`

정리하면 이 프로젝트는 범용 DBMS가 아니라, SQL 처리 흐름과 인덱스의 성능 효과를 끝까지 직접 보여주는 교육용 엔진입니다.  
특히 `id` 조회에서 B+Tree를 적용했을 때, 선형 탐색과 비교해 얼마나 큰 차이가 나는지 가장 직관적으로 보여주는 데 초점을 맞췄습니다.

---

## 발표 직전 체크

- README 첫 화면에서 성능 숫자가 바로 보이는지 확인
- `data/perf_books.bin`이 준비되어 있는지 확인
- 명령 복붙용 터미널 창을 미리 열어 둘 것
- 시연은 `id 단건 -> id 범위 -> author 선형` 순서를 유지할 것
