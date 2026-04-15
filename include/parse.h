/* Query AST types and parser entrypoint. */
#ifndef PARSE_H
#define PARSE_H

#include "base.h"
#include "lex.h"

typedef enum {
    VAL_INT = 0,
    VAL_STR
} ValKind;

typedef struct {
    ValKind kind;
    long num;
    char str[VAL_LEN];
} Val;

typedef enum {
    COND_NONE = 0,
    COND_EQ,
    COND_RANGE
} CondKind;

typedef struct {
    CondKind kind;
    char col[NAME_LEN];
    Val val;
    long min_num;
    long max_num;
} Cond;

typedef struct {
    int all;
    size_t len;
    char cols[MAX_COL][NAME_LEN];
} ColList;

typedef enum {
    Q_SELECT = 0,
    Q_INSERT
} QryKind;

typedef struct {
    QryKind kind;
    char table[NAME_LEN];
    ColList cols;
    Cond cond;
    size_t nval;
    Val vals[MAX_COL];
    size_t pos;
} Qry;

Err parse_stmt(const TokList *toks, Qry *qry, char *err, size_t cap);

#endif
