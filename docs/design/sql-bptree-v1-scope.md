# SQL B+tree V1 Scope

## Summary

- [confirmed] Implement a C mini SQL DB with `SELECT`, `INSERT`, `WHERE`,
  multi-statement batches, rollback, binary DB files, and cache-first execution.
- [confirmed] The B+ tree stores record positions only.
- [confirmed] IDs start at `1`, increase by `1`, are unique, and are never reused
  by future delete support.
- [confirmed] `WHERE id = N` and `WHERE id BETWEEN A AND B` use the B+ tree.
- [inferred] V1 is intentionally single-table to keep the implementation
  readable for learning and review.

## Table

V1 has one table:

```text
records(
  id INT64 implicit,
  name STRING,
  value INT64
)
```

`INSERT` callers provide only `name` and `value`. The database assigns `id`.

## SQL

Supported:

```sql
SELECT * FROM records;
SELECT * FROM records WHERE id = 10;
SELECT * FROM records WHERE id BETWEEN 10 AND 20;
SELECT * FROM records WHERE value = 100;
INSERT INTO records VALUES ('alice', 100);
```

Out of scope:

- `CREATE TABLE`
- multiple tables
- `UPDATE` and `DELETE`
- joins, ordering, aggregates, `NULL`
- secondary indexes
- `AND`, `>=`, `<=`, or non-ID range search

## Storage

The binary DB stores:

- magic `MSQLDB1`
- version `1`
- `next_id`
- row count
- each row: `id`, string length + `name`, `value`

B+ tree nodes are not persisted. On load, rows are read into memory and the ID
index is rebuilt.

## B+ Tree

- Order `m` means max child count for an internal node.
- Internal nodes have at most `m - 1` keys.
- Leaves store sorted `keys[]`, `record_ptrs[]`, and `next`.
- `record_ptrs[]` stores row indexes into the in-memory table.
- Leaf split promotes the right leaf's first key to the parent.
- Internal split promotes the middle key and removes it from both children.
- Root split creates a new root and increases height.

## Transaction

One SQL batch is one transaction. Since v1 only mutates through `INSERT`,
rollback stores the starting `row_count` and `next_id`. On failure it truncates
rows, restores `next_id`, and rebuilds the B+ tree.

## Proof

Required checks:

- `make test`
- `make acceptance`
- `make asan` after memory-management changes
- `make perf` for B+ tree or executor lookup changes
