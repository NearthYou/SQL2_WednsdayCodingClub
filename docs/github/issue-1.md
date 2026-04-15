# 이슈 초안 1

## 제목
`books`용 핵심 SQL 실행 경로 구현

## 본문
`books` 데모 테이블을 위한 읽기 쉬운 C 기반 핵심 SQL 경로를 구현합니다.

### 범위
- 배치 분리
- lexer
- parser
- `SELECT`
- `INSERT`
- pretty print

### 리뷰 포인트
- 쿼리 파싱의 명확성
- 스키마 검증
- 출력 형식

