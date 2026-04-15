# Binary Formats

## Query Binary Format: `QSQL`
This format stores one full SQL batch.

### Header
- magic: 4 bytes, ASCII `QSQL`
- version: `uint32`, current value `1`
- sql_len: `uint32`, byte length of the SQL payload

### Payload
- raw SQL batch bytes
- the payload is not split ahead of time
- the final semicolon must still be present in the SQL text

### Validation
- magic must be `QSQL`
- version must be `1`
- `sql_len` must exactly match the remaining file size

## Data Binary Format: `BKDB`
This format stores the persistent `books` table.

### Header
- magic: 4 bytes, ASCII `BKDB`
- version: `uint32`, current value `1`
- rec_sz: `uint16`, current value `196`
- keep: `uint16`, reserved, currently `0`
- cnt: `uint32`, row count
- next_id: `uint32`, next auto-increment id

### One Book Record
- `id`: `uint32`
- `title`: `char[96]`
- `author`: `char[64]`
- `genre`: `char[32]`

Strings are fixed-size and zero-padded.

## Cache And Save Policy
- The program loads `data/books.bin` once at startup.
- All queries run against the in-memory cache.
- A batch is one transaction.
- `SELECT` output is buffered until the whole batch succeeds.
- `INSERT` changes stay only in memory until the whole batch succeeds.
- If a statement fails, row count and `next_id` are restored and the B+ tree is rebuilt from the current cache state.
- If save fails, the same rollback path runs.
- Save writes to `*.tmp` first and swaps it into place only after a full successful write.

## Why Fixed Strings
- simpler file I/O
- simpler format checks
- easier debugging with hex editors
- easier beginner-friendly code

