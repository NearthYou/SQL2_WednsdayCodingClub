#ifndef MINISQL_TRANSACTION_H
#define MINISQL_TRANSACTION_H

#include "db.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t start_row_count;
    int64_t start_next_id;
} Transaction;

void transaction_begin(Transaction *tx, const Database *db);
bool transaction_rollback(const Transaction *tx, Database *db, SqlError *error);

#endif
