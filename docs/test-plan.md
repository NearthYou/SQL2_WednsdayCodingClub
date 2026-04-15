# 테스트 계획

## 단위 테스트
- 문자열 리터럴 안 세미콜론을 포함한 배치 분리
- 빈 문장 거절
- `SELECT ... WHERE id = ...` lexer / parser
- `SELECT ... WHERE id BETWEEN ... AND ...` lexer / parser
- `INSERT` lexer / parser
- B+ 트리 단건 삽입 / 조회
- B+ 트리 범위 순회
- B+ 트리 검증 API와 통계 계산
- qsql 저장 / 로드 round-trip
- data 저장 / 로드 round-trip
- 잘못된 데이터 헤더 거절
- 실패 배치 rollback 복구

## 기능 테스트
- `id` 기준 `SELECT`
- `id BETWEEN` 기준 `SELECT`
- `INSERT` 후 `SELECT`
- 잘못된 테이블 이름
- 잘못된 선택 컬럼
- 0건 조회 결과
- `--summary-only` 일반 조회
- `--summary-only` 범위 조회
- 마지막 세미콜론 누락
- 잘못된 `INSERT` 값 개수
- `id BETWEEN` 역순 범위 거절 및 rollback
- `id` 외 컬럼에 대한 `BETWEEN` 거절
- `data/input.sql` 기본 탐색
- `data/input.qsql` 기본 탐색

## Acceptance 시나리오
- 단건 조회 + 범위 조회 + 선형 조회를 한 배치에서 확인
- `INSERT` 후 같은 배치에서 범위 조회 확인
- 실패 배치 후 데이터 rollback 확인
- 손상된 DB 파일 거절

## 성능 테스트
- `gen_perf` 빌드
- `data/perf_books.bin` 생성
- `id` 단건 B+ 트리 조회
- `id` 범위 B+ 트리 조회
- `author` 선형 탐색
- `genre` 선형 탐색
- 데이터 수, 조회 종류, 측정 시간 기록
