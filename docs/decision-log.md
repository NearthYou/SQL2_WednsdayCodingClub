# Decision Log

## 1. One Working Log File
- Problem: The original request mentioned both `AGENTS.md` and `agent.md`.
- Options: keep both, mirror content, merge into one.
- Choice: merge into `AGENTS.md`.
- Why: one source of truth is easier to maintain in a long session.
- Impact: progress tracking lives in the repo root and is always updated in one file.

## 2. Reduced Demo Schema
- Problem: The earlier draft schema had more columns than needed for the current demo.
- Options: keep the larger schema, reduce to a smaller demo schema.
- Choice: `id`, `title`, `author`, `genre`.
- Why: the smaller schema keeps code, tests, and output easier to follow.
- Impact: `INSERT` takes 3 values and docs/examples match the reduced table.

## 3. Fixed-Size Binary Records
- Problem: Variable-length strings save space but make I/O and debugging harder.
- Options: variable-length strings, fixed-size strings.
- Choice: fixed-size strings.
- Why: simpler code and safer beginner-level file parsing.
- Impact: data files are larger, but logic is clear and stable.

## 4. B+ Tree Scope
- Problem: Which lookups should use the B+ tree.
- Options: all searchable columns, only `id`.
- Choice: only `id`.
- Why: the requirement is strict and the code stays readable.
- Impact: `author`, `genre`, and `title` always use linear scan.

## 5. Rollback Strategy
- Problem: How to roll back writes without deep-copying the whole cache.
- Options: full cache copy, undo log, truncate-plus-rebuild.
- Choice: truncate rows back to original count, restore `next_id`, rebuild B+ tree.
- Why: only `INSERT` mutates state today, so this is the clearest approach.
- Impact: rollback code is short and easy to explain.

