#include "db.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static char *string_duplicate(const char *value) {
    size_t len = strlen(value);
    char *copy = malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, value, len + 1);
    return copy;
}

bool table_init(Table *table, size_t index_order, SqlError *error) {
    table->rows = NULL;
    table->row_count = 0;
    table->row_capacity = 0;
    table->next_id = 1;
    table->index_order = index_order;
    table->index = bptree_create(index_order);
    if (!table->index) {
        error_set(error, ERROR_STORAGE, 0, "failed to create B+ tree index");
        return false;
    }
    return true;
}

void table_free(Table *table) {
    if (!table) {
        return;
    }
    for (size_t i = 0; i < table->row_count; i++) {
        free(table->rows[i].name);
    }
    free(table->rows);
    bptree_free(table->index);
    table->rows = NULL;
    table->row_count = 0;
    table->row_capacity = 0;
    table->next_id = 1;
    table->index = NULL;
}

static bool table_reserve(Table *table, size_t needed, SqlError *error) {
    if (needed <= table->row_capacity) {
        return true;
    }

    size_t next_capacity = table->row_capacity == 0 ? 8 : table->row_capacity * 2;
    while (next_capacity < needed) {
        next_capacity *= 2;
    }

    Record *next_rows = realloc(table->rows, next_capacity * sizeof(Record));
    if (!next_rows) {
        error_set(error, ERROR_STORAGE, 0, "failed to grow records table");
        return false;
    }

    table->rows = next_rows;
    table->row_capacity = next_capacity;
    return true;
}

bool table_append_loaded(Table *table, int64_t id, const char *name, int64_t value, SqlError *error) {
    if (id <= 0) {
        error_set(error, ERROR_STORAGE, 0, "record id must be positive");
        return false;
    }
    if (!table_reserve(table, table->row_count + 1, error)) {
        return false;
    }

    char *name_copy = string_duplicate(name);
    if (!name_copy) {
        error_set(error, ERROR_STORAGE, 0, "failed to copy record name");
        return false;
    }

    size_t row_index = table->row_count;
    table->rows[row_index].id = id;
    table->rows[row_index].name = name_copy;
    table->rows[row_index].value = value;
    table->row_count++;

    if (id >= table->next_id) {
        table->next_id = id + 1;
    }

    return true;
}

bool table_insert(Table *table, const char *name, int64_t value, SqlError *error) {
    int64_t id = table->next_id;
    if (!table_append_loaded(table, id, name, value, error)) {
        return false;
    }
    table->next_id = id + 1;

    size_t row_index = table->row_count - 1;
    if (!bptree_insert(table->index, id, row_index)) {
        error_set(error, ERROR_EXECUTE, 0, "duplicate or failed B+ tree insert for id %lld", (long long)id);
        table_truncate(table, row_index);
        table->next_id = id;
        return false;
    }
    return true;
}

void table_truncate(Table *table, size_t row_count) {
    if (!table || row_count >= table->row_count) {
        return;
    }

    for (size_t i = row_count; i < table->row_count; i++) {
        free(table->rows[i].name);
        table->rows[i].name = NULL;
    }
    table->row_count = row_count;
}

bool table_rebuild_index(Table *table, SqlError *error) {
    bptree_free(table->index);
    table->index = bptree_create(table->index_order);
    if (!table->index) {
        error_set(error, ERROR_TRANSACTION, 0, "failed to rebuild B+ tree index");
        return false;
    }

    for (size_t i = 0; i < table->row_count; i++) {
        if (!bptree_insert(table->index, table->rows[i].id, i)) {
            error_set(error, ERROR_TRANSACTION, 0, "duplicate id %lld while rebuilding index", (long long)table->rows[i].id);
            return false;
        }
    }

    BPTreeValidationReport report;
    if (!bptree_validate(table->index, &report)) {
        error_set(error, ERROR_TRANSACTION, 0, "rebuilt B+ tree is invalid: %s", report.message);
        return false;
    }
    return true;
}

bool database_init(Database *db, size_t index_order, SqlError *error) {
    return table_init(&db->records, index_order, error);
}

void database_free(Database *db) {
    if (!db) {
        return;
    }
    table_free(&db->records);
}

bool database_rebuild_indexes(Database *db, SqlError *error) {
    return table_rebuild_index(&db->records, error);
}
