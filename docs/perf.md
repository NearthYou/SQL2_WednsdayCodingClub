# Performance Notes

These numbers were measured locally in this workspace on `2026-04-15`.
They are real observed values from this session and are not estimated.

## Dataset
- file: `data/perf_books.bin`
- row count: `1,000,000`
- file size: `196,000,020` bytes
- schema: `id`, `title`, `author`, `genre`

## Generation Command
```powershell
.\build\gen_perf.exe data\perf_books.bin 1000000
```

## Generation Result
- observed wall time: `2.839 sec`

## Query Commands
```powershell
.\build\sql2_books.exe --mode cli --data data\perf_books.bin --batch "SELECT * FROM books WHERE id = 1000000;"
.\build\sql2_books.exe --mode cli --data data\perf_books.bin --batch "SELECT * FROM books WHERE author = 'Author 999';"
.\build\sql2_books.exe --mode cli --data data\perf_books.bin --batch "SELECT * FROM books WHERE genre = 'Genre 7';"
```

## Query Results
- `WHERE id = 1000000`
  - rows: `1`
  - scan: `B+Tree`
  - time: `0.001 ms`
- `WHERE author = 'Author 999'`
  - rows: `1000`
  - scan: `Linear`
  - time: `115.251 ms`
- `WHERE genre = 'Genre 7'`
  - rows: `50000`
  - scan: `Linear`
  - time: `69.288 ms`

## Reading The Numbers
- The B+ tree lookup stays near constant time for exact `id` search.
- The `author` and `genre` lookups scan all rows by design.
- Linear timings can move around between runs because this is a single local observation, but the lookup path difference stays clear.

## Local Repeat Command
```powershell
.\scripts\perf.ps1 -Count 1000000
```
