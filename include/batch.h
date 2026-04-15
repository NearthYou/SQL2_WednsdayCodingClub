/* Batch statement parsing and SQL/QSQL file helpers. */
#ifndef BATCH_H
#define BATCH_H

#include "base.h"

typedef struct {
    char *txt;
    size_t start;
    int no;
} Stmt;

typedef struct {
    Stmt *list;
    size_t len;
    size_t cap;
} StmtList;

void free_stmts(StmtList *lst);
Err split_sql(const char *sql, StmtList *out, char *err, size_t cap);
Err load_sql_file(const char *path, char **sql, char *err, size_t cap);
Err load_default_sql(char **sql, char *err, size_t cap);
Err save_qsql(const char *path, const char *sql, char *err, size_t cap);

#endif
