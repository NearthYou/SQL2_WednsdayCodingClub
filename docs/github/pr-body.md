# PR 제목
캐시, 롤백, B+ 트리 id 조회를 포함한 C 기반 도서관 SQL 데모 구현

## 요약
- `books` 테이블 하나를 위한 읽기 쉬운 C 기반 SQL 데모 엔진 구현
- `SELECT`, `INSERT`, 다중 문장 배치, 롤백 지원
- query/data 바이너리 포맷, 테스트, 문서, 성능 도구, CI 추가

## 주요 설계 결정
- 축소 스키마 사용: `id`, `title`, `author`, `genre`
- 파일 I/O 단순화를 위해 고정 길이 바이너리 레코드 사용
- B+ 트리는 `id`에만 적용
- 다른 컬럼은 선형 탐색 사용
- 롤백은 행 수와 `next_id`를 복원한 뒤 B+ 트리를 재구성하는 방식 사용

## 테스트 결과
- 단위 테스트: 통과
- 기능 테스트: 통과
- 수동 스모크 쿼리: 통과

## 성능
- 데이터셋: `1,000,000` rows
- `WHERE id = 1000000`: `scan=B+Tree, time=0.001 ms`
- `WHERE author = 'Author 999'`: `scan=Linear, time=115.251 ms`
- `WHERE genre = 'Genre 7'`: `scan=Linear, time=69.288 ms`

## 리뷰 포인트
- 배치 분리와 parser 흐름의 명확성
- 롤백 및 저장 일관성
- B+ 트리 적용 범위가 `id`로 제한되는 점
- 문서와 테스트가 실제 동작과 맞는지

## 권장 GitHub 명령
```bash
gh issue create --title "books용 핵심 SQL 실행 경로 구현" --body-file docs/github/issue-1.md
gh issue create --title "바이너리 저장소, 캐시 동기화, 롤백, B+ 트리 인덱스 추가" --body-file docs/github/issue-2.md
gh issue create --title "테스트, 문서, 성능 도구, CI 마무리" --body-file docs/github/issue-3.md
gh pr create --base main --head SS --title "C 기반 도서관 SQL 데모 구현" --body-file docs/github/pr-body.md
```

