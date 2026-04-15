#ifndef MINISQL_DB_H
#define MINISQL_DB_H

#include "bptree.h"
#include "error.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int64_t id;
    char *name;
    int64_t value;
} Record;

typedef struct {
    Record *rows;
    size_t row_count;
    size_t row_capacity;
    int64_t next_id;
    BPTree *index;
    size_t index_order;
} Table;

typedef struct {
    Table records;
} Database;

bool table_init(Table *table, size_t index_order, SqlError *error);
void table_free(Table *table);
bool table_append_loaded(Table *table, int64_t id, const char *name, int64_t value, SqlError *error);
bool table_insert(Table *table, const char *name, int64_t value, SqlError *error);
void table_truncate(Table *table, size_t row_count);
bool table_rebuild_index(Table *table, SqlError *error);

bool database_init(Database *db, size_t index_order, SqlError *error);
void database_free(Database *db);
bool database_rebuild_indexes(Database *db, SqlError *error);

#endif
