# 테스트 계획

## 단위 테스트
- 문자열 리터럴 안 세미콜론을 포함한 배치 분리
- 빈 문장 거부
- `SELECT`용 lexer와 parser
- `INSERT`용 lexer와 parser
- B+ 트리 삽입과 검색
- qsql 쓰기/읽기 round-trip
- data 저장/로드 round-trip
- 잘못된 데이터 헤더 처리
- 롤백 상태 복구

## 기능 테스트
- `id` 기준 `SELECT`
- `INSERT` 후 `SELECT`
- 잘못된 테이블 이름
- 잘못된 선택 컬럼
- 0건 조회 결과
- 세미콜론 누락
- 잘못된 `INSERT` 값 개수
- `data/input.sql` 기본 탐색
- `data/input.qsql` 기본 탐색

## 수동 스모크 테스트
- 큰따옴표 입력을 사용하는 대화형 CLI 모드
- 기본 경로를 사용하는 대화형 파일 모드
- `data/demo_queries.sql` 데모 배치 실행
- 성공적인 `INSERT` 중 임시 파일 저장 경로 확인

## 성능 테스트
- `gen_perf` 빌드
- `data/perf_books.bin` 생성
- `id` 기준 B+ 트리 조회 실행
- `author` 기준 선형 탐색 실행
- `genre` 기준 선형 탐색 실행
- 실제 명령, 데이터 크기, 측정 시간을 함께 기록

