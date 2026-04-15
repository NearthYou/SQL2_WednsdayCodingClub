# PR Title
Implement a C-based library SQL demo with cache, rollback, and B+ tree id lookup

## Summary
- build a readable SQL demo engine in C for one `books` table
- support `SELECT`, `INSERT`, multi-statement batches, and rollback
- add binary query/data formats, tests, docs, performance tooling, and CI

## Main Design Choices
- reduced schema: `id`, `title`, `author`, `genre`
- fixed-size binary row format for simpler file I/O
- one B+ tree only for `id`
- linear scan for other columns
- rollback by restoring row count and `next_id`, then rebuilding the B+ tree

## Test Results
- unit tests: pass
- function tests: pass
- manual smoke queries: pass

## Performance
- dataset: `1,000,000` rows
- `WHERE id = 1000000`: `scan=B+Tree, time=0.001 ms`
- `WHERE author = 'Author 999'`: `scan=Linear, time=115.251 ms`
- `WHERE genre = 'Genre 7'`: `scan=Linear, time=69.288 ms`

## Review Focus
- batch split and parser clarity
- rollback and save consistency
- B+ tree scope limited to `id`
- docs and tests matching real behavior

## Suggested GitHub Commands
```bash
gh issue create --title "Build the core SQL path for books" --body-file docs/github/issue-1.md
gh issue create --title "Add binary storage, cache sync, rollback, and B+ tree index" --body-file docs/github/issue-2.md
gh issue create --title "Finish tests, docs, perf tooling, and CI" --body-file docs/github/issue-3.md
gh pr create --base main --head SS --title "Implement a C-based library SQL demo" --body-file docs/github/pr-body.md
```
