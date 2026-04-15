# B+tree Mini SQL DB

This is a learning-first C project that implements a small SQL-like database
with a B+ tree index over implicit record IDs.

## Goals

- Keep the code readable and review-friendly.
- Show the path from SQL input to parsing, execution, indexing, output, and rollback.
- Use a B+ tree for `id` lookups and range scans.
- Compare indexed lookup against linear search over another field.

## Supported SQL

The v1 database has one table:

```text
records(
  id INT64 implicit,
  name STRING,
  value INT64
)
```

Supported statements:

```sql
SELECT * FROM records;
SELECT * FROM records WHERE id = 10;
SELECT * FROM records WHERE id BETWEEN 10 AND 20;
SELECT * FROM records WHERE value = 100;
INSERT INTO records VALUES ('alice', 100);
```

Multiple statements can be provided in one batch. The whole batch is treated as
one transaction. If any statement fails, inserted rows from the batch are rolled
back and the binary DB file is not saved.

## Commands

```sh
make
make fixtures
make test
make acceptance
make asan
make perf
```

Run the program:

```sh
./bin/minisql
```

The default binary DB path is `data/default.msqldb`.

## Learning Notes

- The B+ tree stores `id -> row_index`, not full records.
- B+ tree nodes are in memory only. They are rebuilt from rows after loading.
- Binary persistence stores rows and `next_id`.
- The parser intentionally supports a tiny grammar so the execution path stays
  easy to inspect.
