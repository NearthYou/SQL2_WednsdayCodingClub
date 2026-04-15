# AGENTS.md

## Goal Summary
- Build a readable C SQL demo for a library book lookup system.
- Keep the table fixed to `books`.
- Support `SELECT`, `INSERT`, batch execution, rollback, binary file I/O, tests, docs, and CI.

## Fixed Requirements
- Branch: `SS`
- Language: C11
- Table: `books`
- Schema: `id`, `title`, `author`, `genre`
- `id` is auto-increment and is not part of user `INSERT`
- `WHERE` supports only `column = value`
- `SELECT WHERE id = n` must use the B+ tree
- Other lookup paths must use linear scan
- Batch output is buffered and discarded on failure
- Successful write batches are saved through a temp file swap

## Repo Rules
- Keep files small and easy to read.
- Prefer clear code over clever code.
- Use short names when possible.
- Add simple comments only where they help a reader follow the flow.

## Build And Test Commands
- Build app: `gcc -std=c11 -Wall -Wextra -pedantic -Iinclude src/*.c -o build/sql2_books.exe`
- Build unit tests: `gcc -std=c11 -Wall -Wextra -pedantic -Iinclude tests/test_unit.c src/*.c -DNO_MAIN -o build/test_unit.exe`
- Build function tests: `gcc -std=c11 -Wall -Wextra -pedantic -Iinclude tests/test_func.c src/*.c -DNO_MAIN -o build/test_func.exe`
- PowerShell helpers live in `scripts/`
- CI uses `make`

## Coding Style
- C11 only
- No heavy external libraries
- Fixed-size strings for on-disk rows
- One clear job per function
- Check malloc/realloc/fopen results
- Avoid hidden side effects

## Done
- Confirmed remote and local repo state
- Created and switched to `SS`
- Created project folders
- Implemented batch split, lexer, parser, storage, B+ tree, executor, and CLI
- Added unit tests and function tests
- Added PowerShell automation scripts and Makefile
- Added README, binary format docs, perf docs, review docs, and GitHub draft docs
- Added GitHub Actions CI workflow
- Ran local build, test, demo, and perf commands

## Remaining
- GitHub issue drafts are ready, but actual issue creation is still manual with the current available tool set

## Command Log
- `git status --short --branch` -> clean `main`
- `git branch --all --verbose --no-abbrev` -> only `main`
- `git switch -c SS` -> success after elevated git access
- `New-Item -ItemType Directory ...` -> created repo layout folders
- `gcc ... -o build/sql2_books.exe` -> app build passed
- `gcc ... -DNO_MAIN ... tests/test_unit.c ...` -> unit test build passed
- `gcc ... -DNO_MAIN ... tests/test_func.c ...` -> function test build passed
- `build/test_unit.exe` -> 9 unit tests passed
- `build/test_func.exe` -> 9 function tests passed
- `powershell -File scripts/test.ps1` -> local scripted build and tests passed
- `powershell -File scripts/demo.ps1` -> demo batch ran successfully
- `powershell -File scripts/perf.ps1 -Count 1000000` -> generated 1,000,000 rows and measured lookup timings
- `git push -u origin SS` -> pushed local branch to remote
- GitHub PR created -> `https://github.com/NearthYou/SQL2_WednsdayCodingClub/pull/1`

## Decision Log
- Use one working log file: `AGENTS.md`
- Keep one demo table only: `books`
- Use reduced schema: `id`, `title`, `author`, `genre`
- Keep query binary and data binary formats simple and fixed-size
- Keep B+ tree scope limited to `id`
- Use truncate-plus-rebuild rollback because only `INSERT` mutates state today

## Errors Or Blocks
- `git switch -c SS` first failed with a ref lock permission error inside the sandbox
- Resolved by rerunning the git command with elevated permission
- PowerShell build script first had bad array passing and hid gcc failures behind old binaries
- Resolved by switching to explicit argument arrays and exit-code checks
- The current GitHub tool set in this session supports PR creation, but not direct issue creation
- Issue drafts were left in `docs/github/issue-*.md`

## Current State
- Implementation, docs, scripts, and CI are in place
- Local validation is complete
- `SS` is pushed and PR #1 is open

## Next Work
- Wait for review on PR #1
- Create GitHub issues manually from `docs/github/issue-*.md` if issue tracking is still needed
