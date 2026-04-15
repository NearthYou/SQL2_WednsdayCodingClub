# Issue Draft 2

## Title
Add binary storage, cache sync, rollback, and B+ tree index

## Body
Implement the persistent storage and transaction path for the `books` demo.

### Scope
- `BKDB` storage format
- cache load/save
- temp-file replace flow
- rollback on failed batch
- `id` B+ tree build and search

### Review Points
- data format checks
- rollback consistency
- `id` lookup path

