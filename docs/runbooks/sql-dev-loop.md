# SQL Dev Loop

## Default Flow

1. Read `docs/design/sql-bptree-v1-scope.md`.
2. Pick one small implementation slice.
3. Run the narrowest relevant test.
4. Run `make test`.
5. Run `make acceptance` before closeout.
6. Run `make asan` for memory-management changes.
7. Run `make perf` when index or lookup behavior changes.

## Proof Expectations

- Parser changes: parser tests plus acceptance.
- B+ tree changes: B+ tree tests plus `bptree_validate`.
- Storage changes: round-trip tests plus corrupt-file test.
- Executor changes: transaction rollback and query-path tests.
- Printer changes: acceptance output check.

## Closeout

Summarize:

- what changed
- which commands passed
- any command not run and why
- the next useful implementation slice
