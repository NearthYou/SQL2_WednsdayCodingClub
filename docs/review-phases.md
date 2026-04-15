# Review Phases

## 1. Input And Parsing
- Check batch splitting around semicolons inside string literals.
- Check the final semicolon rule and empty statement rejection.
- Check lexer token coverage and case-insensitive keyword handling.
- Check parser errors point to the failing area.

## 2. Storage And Cache
- Check first-load behavior when `books.bin` is missing.
- Check binary header validation and fixed-size row reads.
- Check save-to-temp then replace flow.
- Check cache values match what is written back to disk.

## 3. Index And Executor
- Check `WHERE id = n` goes through the B+ tree.
- Check `author`, `genre`, `title`, and full scans stay linear.
- Check projection columns come from parsed SQL, not hard-coded output.
- Check pretty print layout, row count, scan mode, and time line.

## 4. Transaction And Rollback
- Check output stays buffered until the batch fully succeeds.
- Check a failed later statement removes earlier `INSERT` changes from memory.
- Check `next_id` is restored on rollback.
- Check the B+ tree is rebuilt after rollback.

## 5. Tests And Docs
- Check unit and function tests cover happy paths and error paths.
- Check README examples match actual CLI behavior.
- Check binary format docs match the implementation.
- Check perf docs list only real measured numbers.

