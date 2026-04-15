# AGENTS.md

## 목표 요약
- 읽기 쉬운 C 기반 도서관 도서 조회 SQL 데모를 만든다.
- 테이블은 `books`로 고정한다.
- `SELECT`, `INSERT`, 배치 실행, 롤백, 바이너리 파일 I/O, 테스트, 문서, CI를 지원한다.

## 고정 요구사항
- 브랜치: `SS`
- 언어: C11
- 테이블: `books`
- 스키마: `id`, `title`, `author`, `genre`
- `id`는 자동 증가이며 사용자 `INSERT`에는 포함되지 않는다
- `WHERE`는 `column = value`만 지원한다
- `SELECT ... WHERE id = n`은 반드시 B+ 트리를 사용한다
- 그 외 조회 경로는 반드시 선형 탐색을 사용한다
- 배치 출력은 버퍼링되며 실패 시 폐기된다
- 성공한 쓰기 배치는 임시 파일 교체 방식으로 저장된다

## 저장소 규칙
- 파일은 작고 읽기 쉽게 유지한다.
- 영리한 코드보다 명확한 코드를 우선한다.
- 가능하면 이름은 짧고 직관적으로 짓는다.
- 주석은 흐름 이해에 실제로 도움이 될 때만 단순하게 단다.

## 빌드 및 테스트 명령
- 앱 빌드: `gcc -std=c11 -Wall -Wextra -pedantic -Iinclude src/*.c -o build/sql2_books.exe`
- 단위 테스트 빌드: `gcc -std=c11 -Wall -Wextra -pedantic -Iinclude tests/test_unit.c src/*.c -DNO_MAIN -o build/test_unit.exe`
- 기능 테스트 빌드: `gcc -std=c11 -Wall -Wextra -pedantic -Iinclude tests/test_func.c src/*.c -DNO_MAIN -o build/test_func.exe`
- PowerShell 도우미 스크립트는 `scripts/`에 둔다
- CI는 `make`를 사용한다

## 코딩 스타일
- C11만 사용
- 무거운 외부 라이브러리는 사용하지 않음
- 디스크 저장 행은 고정 길이 문자열 사용
- 함수 하나당 역할 하나
- `malloc`/`realloc`/`fopen` 결과를 반드시 확인
- 숨겨진 부작용을 피함

## 끝난 일
- 원격과 로컬 저장소 상태를 확인했다
- `SS` 브랜치를 만들고 전환했다
- 프로젝트 폴더 구조를 만들었다
- 배치 분리기, lexer, parser, 저장소, B+ 트리, 실행기, CLI를 구현했다
- 단위 테스트와 기능 테스트를 추가했다
- PowerShell 자동화 스크립트와 Makefile을 추가했다
- README, 바이너리 포맷 문서, 성능 문서, 리뷰 문서, GitHub 초안 문서를 작성했다
- GitHub Actions CI 워크플로를 추가했다
- 로컬 빌드, 테스트, 데모, 성능 명령을 실행했다
- `--summary-only` 옵션과 성능 출력 전용 포맷을 추가했다
- 주요 Markdown 문서를 한국어로 정리했다
- 모든 함수 정의 바로 위에 요약 주석을 추가했다
- 머메이드 클래스 다이어그램 문서를 추가했다
- `WHERE id BETWEEN a AND b` 범위 조회와 acceptance 시나리오를 추가했다
- PR 리뷰 코멘트를 반영해 롤백, 파일 검증, 스크립트, 문서를 보강했다
- PR #1을 `main`에 병합했다
- PR 리뷰 대응과 병합 절차를 정리한 `skills/gh-review-merge` 스킬을 추가했다

## 남은 일
- GitHub 이슈 초안은 준비됐지만, 현재 사용 가능한 도구 범위에서는 실제 이슈 생성은 수동으로 해야 한다

## 실행 명령 로그
- `git status --short --branch` -> 깨끗한 `main` 확인
- `git branch --all --verbose --no-abbrev` -> `main`만 존재
- `git switch -c SS` -> 권한 상승 후 성공
- `New-Item -ItemType Directory ...` -> 저장소 폴더 구조 생성
- `gcc ... -o build/sql2_books.exe` -> 앱 빌드 성공
- `gcc ... -DNO_MAIN ... tests/test_unit.c ...` -> 단위 테스트 빌드 성공
- `gcc ... -DNO_MAIN ... tests/test_func.c ...` -> 기능 테스트 빌드 성공
- `build/test_unit.exe` -> 단위 테스트 9개 통과
- `build/test_func.exe` -> 기능 테스트 9개 통과
- `powershell -File scripts/test.ps1` -> 스크립트 기반 로컬 빌드 및 테스트 통과
- `powershell -File scripts/demo.ps1` -> 데모 배치 실행 성공
- `powershell -File scripts/perf.ps1 -Count 1000000` -> 1,000,000건 생성 및 조회 시간 측정
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1` -> 함수 주석 정리 후 빌드 성공
- `.\build\test_unit.exe` -> 단위 테스트 9개 통과
- `.\build\test_func.exe` -> 기능 테스트 10개 통과
- `git push -u origin SS` -> 원격 브랜치 푸시 성공
- GitHub PR 생성 -> `https://github.com/NearthYou/SQL2_WednsdayCodingClub/pull/1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\test.ps1` -> unit 12, func 14, acceptance 통과
- `powershell -ExecutionPolicy Bypass -File .\scripts\perf.ps1 -Count 1000000` -> id 단건, id 범위, author, genre 성능 재측정
- `git merge --no-ff SS -m "Merge pull request #1 from NearthYou/SS"` -> 로컬 `main` 병합 성공
- `git push origin main` -> 원격 `main` 반영 성공

## 의사결정 기록
- 작업 로그 파일은 `AGENTS.md` 하나만 사용한다
- 데모 테이블은 `books` 하나로 유지한다
- 축소 스키마 `id`, `title`, `author`, `genre`를 사용한다
- Query/Data 바이너리 포맷은 단순하고 고정 길이로 유지한다
- B+ 트리 적용 범위는 `id`로 제한한다
- 현재 변경 연산이 `INSERT`뿐이므로 행 수 복구 + 인덱스 재구성 방식으로 롤백한다
- 문서 미리보기에서 잘 렌더되도록 클래스 다이어그램은 단순한 머메이드 문법으로 유지한다

## 오류 및 막힘
- `git switch -c SS`가 처음에는 샌드박스 내부 ref lock 권한 문제로 실패했다
- 권한 상승 후 같은 Git 명령을 다시 실행해 해결했다
- PowerShell 빌드 스크립트 초안에는 배열 전달 문제가 있어 오래된 바이너리 때문에 실패가 가려졌다
- 명시적 인자 배열과 종료 코드 검사로 수정했다
- 현재 세션의 GitHub 도구는 PR 생성은 지원하지만 이슈 생성은 직접 지원하지 않는다
- 이슈 초안은 `docs/github/issue-*.md`에 남겨 두었다
- 일부 머메이드 렌더러는 배열과 포인터 표기가 많은 클래스 다이어그램을 제대로 그리지 못해, 더 단순한 표기로 다시 정리했다
- 이 환경에는 Python 런타임이 없어 `skill-creator`의 `init_skill.py`, `quick_validate.py`를 직접 실행할 수 없다

## 현재 상태
- 구현, 문서, 스크립트, CI가 모두 `main`에 병합되어 있다
- 로컬 검증과 PR 리뷰 반영이 끝났다
- PR #1은 병합 완료 상태다
- `skills/gh-review-merge`가 저장소에 추가되어 있다

## 다음 작업
- 새 스킬을 실제 PR 정리 작업에 다시 써 보며 보완점을 찾는다
- 이슈 추적이 필요하면 `docs/github/issue-*.md`를 바탕으로 GitHub 이슈를 수동 생성한다
