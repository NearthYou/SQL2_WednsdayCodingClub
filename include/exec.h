/* Query execution and result buffering API. */
#ifndef EXEC_H
#define EXEC_H

#include "parse.h"
#include "store.h"
#include "util.h"

typedef enum {
    SCAN_LINEAR = 0,
    SCAN_BTREE
} ScanKind;

typedef struct {
    int *list;
    size_t len;
    size_t cap;
    ScanKind scan;
    double ms;
} RowSet;

void free_rows(RowSet *set);
Err run_qry(Db *db, const Qry *qry, int sum_only, StrBuf *out, char *err,
            size_t cap);
Err run_batch(Db *db, const char *sql, int sum_only, StrBuf *out, char *err,
              size_t cap);

#endif
