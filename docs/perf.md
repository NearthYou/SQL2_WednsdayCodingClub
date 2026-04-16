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
- 관측된 벽시계 시간: `2.839 sec`

## 조회 명령
```powershell
.\build\sql2_books.exe --mode cli --data data\perf_books.bin --summary-only --batch "SELECT * FROM books WHERE id = 1000000;"
.\build\sql2_books.exe --mode cli --data data\perf_books.bin --summary-only --batch "SELECT * FROM books WHERE author = 'Author 999';"
.\build\sql2_books.exe --mode cli --data data\perf_books.bin --summary-only --batch "SELECT * FROM books WHERE genre = 'Genre 7';"
```

성능 측정에서는 `--summary-only`를 사용해, 대량 결과를 표 전체로 출력하지 않고 조회 종류, `rows`, `scan`, `time`만 요약해서 출력합니다.

예상 출력 형식:
```text
[author lookup]
rows : 1000
scan : Linear
time : 115.251 ms
```

## 조회 결과
- `WHERE id = 1000000`
  - rows: `1`
  - scan: `B+Tree`
  - time: `0.001 ms`
- `WHERE author = 'Author 999'`
  - rows: `1000`
  - scan: `Linear`
  - time: `115.251 ms`
- `WHERE genre = 'Genre 7'`
  - rows: `50000`
  - scan: `Linear`
  - time: `69.288 ms`

## 수치 해석
- 정확한 `id` 검색은 B+ 트리를 사용하므로 거의 상수 시간에 가깝게 유지됩니다.
- `author`와 `genre` 검색은 설계상 전체 행을 선형 탐색합니다.
- 선형 탐색 시간은 실행 시점에 따라 조금씩 달라질 수 있지만, 조회 경로 차이는 분명하게 유지됩니다.

## 로컬 재실행 명령
```powershell
.\scripts\perf.ps1 -Count 1000000
```
