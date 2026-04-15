# Library SQL Demo In C

This project is a small SQL engine written in C for a library book lookup demo.
It supports one table, `books`, and focuses on readable code, batch execution,
rollback, binary storage, and simple performance comparison between a B+ tree
lookup and a linear scan.

## What It Does
- Loads `books` from `data/books.bin` into memory once at startup
- Runs all queries against the in-memory cache
- Supports `SELECT`, `INSERT`, and multi-statement batches
- Uses a B+ tree only for `WHERE id = ...`
- Uses linear scan for `title`, `author`, `genre`, and full table reads
- Saves writes only when the whole batch succeeds
- Rolls back cache state and discards buffered output on failure

## Schema
`books`
- `id` : auto-increment integer
- `title` : string
- `author` : string
- `genre` : string

`INSERT` values must be `title`, `author`, `genre` in that order.

## Repo Layout
- `src/` : engine source files
- `include/` : shared header
- `tests/` : unit and functional tests
- `data/` : binary data and demo queries
- `docs/` : design, review, test, perf, and GitHub draft docs
- `scripts/` : local PowerShell automation
- `.github/workflows/` : CI
- `AGENTS.md` : repo rules and running work log

## Build
### PowerShell
```powershell
./scripts/build.ps1
```

### Direct GCC
```powershell
gcc -std=c11 -Wall -Wextra -pedantic -Iinclude `
  src\util.c src\batch.c src\lex.c src\parse.c src\bpt.c src\store.c `
  src\exec.c src\main.c src\gen_perf.c -o build\sql2_books.exe
```

### Make
Used by CI on Linux:
```bash
make
```

## Run
### Interactive Mode
```powershell
.\build\sql2_books.exe
```
Then choose:
- `1` = CLI direct input
- `2` = File input

### CLI Mode Example
The direct input prompt requires one full batch inside double quotes:
```text
"SELECT * FROM books WHERE id = 1;"
"INSERT INTO books VALUES ('The Pragmatic Programmer','Andrew Hunt','SE'); SELECT * FROM books WHERE author = 'Andrew Hunt';"
```

### Non-Interactive CLI Example
```powershell
.\build\sql2_books.exe --mode cli --batch "SELECT * FROM books WHERE id = 1;"
```

### File Mode Example
```powershell
.\build\sql2_books.exe --mode file --file data\demo_queries.sql
```

### Default File Search Rule
If file mode is chosen and the path is left empty, the program searches:
1. `./data/input.qsql`
2. `./data/input.sql`

## Supported SQL
- `SELECT * FROM books;`
- `SELECT title,author FROM books;`
- `SELECT * FROM books WHERE id = 3;`
- `SELECT title,genre FROM books WHERE author = 'George Orwell';`
- `INSERT INTO books VALUES ('Book','Author','Genre');`
- Multiple statements in one batch, split by `;`

## Input Rules
- The last semicolon is required
- Empty statements like `;;` are rejected
- Semicolons inside string literals do not split the batch
- SQL string literals use single quotes
- Keywords are case-insensitive

## Demo Scenario
### Quick Demo
```powershell
./scripts/demo.ps1
```

### Demo Query File
`data/demo_queries.sql` shows:
- B+ tree lookup by `id`
- linear scan by `author`
- linear scan by `genre`
- a successful `INSERT`
- a follow-up `SELECT`

## Tests
### PowerShell
```powershell
./scripts/test.ps1
```

### Make
```bash
make test
```

## Performance
Generate 1,000,000 rows and compare lookups:
```powershell
./scripts/perf.ps1 -Count 1000000
```

The current measured numbers are recorded in [docs/perf.md](docs/perf.md).

## Binary Formats
- Query binary format: [docs/binary-format.md](docs/binary-format.md)
- Data binary format: [docs/binary-format.md](docs/binary-format.md)

## CI
GitHub Actions runs:
- build
- unit tests
- function tests
- sanitizer build and tests

Large performance runs stay out of default CI and are kept as manual/local runs.

