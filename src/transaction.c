#include "transaction.h"

void transaction_begin(Transaction *tx, const Database *db) {
    tx->start_row_count = db->records.row_count;
    tx->start_next_id = db->records.next_id;
}

bool transaction_rollback(const Transaction *tx, Database *db, SqlError *error) {
    table_truncate(&db->records, tx->start_row_count);
    db->records.next_id = tx->start_next_id;
    return database_rebuild_indexes(db, error);
}
