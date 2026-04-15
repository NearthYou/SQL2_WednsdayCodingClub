# 성능 기록

아래 수치는 `2026-04-15`에 현재 워크스페이스에서 직접 측정한 값입니다.
추정값이 아니라 이 세션에서 실제로 관측한 결과만 적었습니다.

## 데이터셋
- 파일: `data/perf_books.bin`
- 행 수: `1,000,000`
- 파일 크기: `196,000,020` bytes
- 스키마: `id`, `title`, `author`, `genre`

## 생성 명령
```powershell
.\build\gen_perf.exe data\perf_books.bin 1000000
```

## 생성 결과
- 관측된 벽시계 시간: `3.533 sec`

## 조회 명령
```powershell
.\build\sql2_books.exe --mode cli --data data\perf_books.bin --summary-only --batch "SELECT * FROM books WHERE id = 1000000;"
.\build\sql2_books.exe --mode cli --data data\perf_books.bin --summary-only --batch "SELECT * FROM books WHERE id BETWEEN 999001 AND 1000000;"
.\build\sql2_books.exe --mode cli --data data\perf_books.bin --summary-only --batch "SELECT * FROM books WHERE author = 'Author 999';"
.\build\sql2_books.exe --mode cli --data data\perf_books.bin --summary-only --batch "SELECT * FROM books WHERE genre = 'Genre 7';"
```

성능 측정에서는 `--summary-only`를 사용해 대량 결과를 표 전체로 출력하지 않고,
조회 종류, `rows`, `scan`, `time`만 요약해서 확인합니다.

## 예시 출력 형식
```text
[id range lookup]
rows : 1000
scan : B+Tree
time : 0.021 ms
```

## 조회 결과
- `WHERE id = 1000000`
  - rows: `1`
  - scan: `B+Tree`
  - time: `0.001 ms`
- `WHERE id BETWEEN 999001 AND 1000000`
  - rows: `1000`
  - scan: `B+Tree`
  - time: `0.021 ms`
- `WHERE author = 'Author 999'`
  - rows: `1000`
  - scan: `Linear`
  - time: `103.440 ms`
- `WHERE genre = 'Genre 7'`
  - rows: `50000`
  - scan: `Linear`
  - time: `108.874 ms`

## 수치 해석
- 정확한 `id` 단건 조회는 B+ 트리에서 바로 리프를 찾아 거의 상수 시간에 가깝게 동작합니다.
- `id BETWEEN`도 B+ 트리 리프 링크를 따라가므로, 같은 `1000`건 조회라도 선형 탐색보다 훨씬 빠릅니다.
- `author`와 `genre` 검색은 설계상 전체 행을 선형 탐색하므로 데이터가 커질수록 비용이 그대로 증가합니다.
- 단건 인덱스 조회와 범위 인덱스 조회, 선형 탐색 사이의 경로 차이가 숫자로 분명하게 드러납니다.

## 로컬 재실행 명령
```powershell
.\scripts\perf.ps1 -Count 1000000
```
