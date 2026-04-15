# Test Plan

## Unit Tests
- batch split with string-literal semicolons
- empty statement rejection
- lexer and parser for `SELECT`
- lexer and parser for `INSERT`
- B+ tree insert and search
- qsql write and read round-trip
- data save and load round-trip
- bad data header handling
- rollback state restore

## Function Tests
- `SELECT` by `id`
- `INSERT` then `SELECT`
- bad table name
- bad selected column
- zero-row result
- missing semicolon
- bad `INSERT` value count
- default file search for `data/input.sql`
- default file search for `data/input.qsql`

## Manual Smoke Tests
- interactive CLI mode with quoted input
- interactive file mode with default path
- demo batch from `data/demo_queries.sql`
- temp-file save path during successful `INSERT`

## Performance Tests
- build `gen_perf`
- generate `data/perf_books.bin`
- run B+ tree lookup by `id`
- run linear scan by `author`
- run linear scan by `genre`
- record exact command, data size, and measured time

