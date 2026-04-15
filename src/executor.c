#include "executor.h"

#include "printer.h"
#include "transaction.h"

#include <stdbool.h>

static bool append_all_rows(const Table *table, RowIndexList *rows) {
    for (size_t i = 0; i < table->row_count; i++) {
        if (!row_index_list_append(rows, i)) {
            return false;
        }
    }
    return true;
}

static bool append_value_matches(const Table *table, int64_t value, RowIndexList *rows) {
    for (size_t i = 0; i < table->row_count; i++) {
        if (table->rows[i].value == value) {
            if (!row_index_list_append(rows, i)) {
                return false;
            }
        }
    }
    return true;
}

static bool execute_select(Database *db, const Query *query, size_t statement_index, FILE *out, SqlError *error) {
    RowIndexList rows;
    row_index_list_init(&rows);
    bool ok = true;

    switch (query->type) {
        case QUERY_SELECT_ALL:
            ok = append_all_rows(&db->records, &rows);
            break;
        case QUERY_SELECT_ID_EQ: {
            size_t row_index = 0;
            if (bptree_find(db->records.index, query->id, &row_index)) {
                ok = row_index_list_append(&rows, row_index);
            }
            break;
        }
        case QUERY_SELECT_ID_RANGE:
            if (query->min_id > query->max_id) {
                error_set(error, ERROR_EXECUTE, statement_index, "id range start must be <= range end");
                row_index_list_free(&rows);
                return false;
            }
            ok = bptree_find_range(db->records.index, query->min_id, query->max_id, &rows);
            break;
        case QUERY_SELECT_VALUE_EQ:
            ok = append_value_matches(&db->records, query->value, &rows);
            break;
        case QUERY_INSERT:
            ok = false;
            break;
    }

    if (!ok) {
        error_set(error, ERROR_EXECUTE, statement_index, "failed to build SELECT result");
        row_index_list_free(&rows);
        return false;
    }

    printer_print_rows(out, &db->records, &rows);
    row_index_list_free(&rows);
    return true;
}

static bool execute_insert(Database *db, const Query *query, size_t statement_index, SqlError *error) {
    SqlError inner;
    error_clear(&inner);
    if (!table_insert(&db->records, query->insert_name, query->insert_value, &inner)) {
        error_set(error, ERROR_EXECUTE, statement_index, "%s", inner.message[0] ? inner.message : "insert failed");
        return false;
    }

    BPTreeValidationReport report;
    if (!bptree_validate(db->records.index, &report)) {
        error_set(error, ERROR_EXECUTE, statement_index, "B+ tree invariant failed after insert: %s", report.message);
        return false;
    }
    return true;
}

bool execute_batch(Database *db, const QueryBatch *batch, FILE *out, SqlError *error) {
    Transaction tx;
    transaction_begin(&tx, db);

    for (size_t i = 0; i < batch->count; i++) {
        const Query *query = &batch->items[i];
        size_t statement_index = i + 1;
        bool ok = false;

        if (query->type == QUERY_INSERT) {
            ok = execute_insert(db, query, statement_index, error);
        } else {
            ok = execute_select(db, query, statement_index, out, error);
        }

        if (!ok) {
            SqlError rollback_error;
            error_clear(&rollback_error);
            if (!transaction_rollback(&tx, db, &rollback_error)) {
                error_set(error, ERROR_TRANSACTION, statement_index, "rollback failed: %s", rollback_error.message);
            }
            return false;
        }
    }

    return true;
}
