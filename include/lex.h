/* Token types and lexer entrypoints. */
#ifndef LEX_H
#define LEX_H

#include "base.h"

typedef enum {
    TK_END = 0,
    TK_WORD,
    TK_INT,
    TK_STR,
    TK_COMMA,
    TK_LP,
    TK_RP,
    TK_EQ,
    TK_STAR
} TokKind;

typedef enum {
    KW_NONE = 0,
    KW_SELECT,
    KW_INSERT,
    KW_FROM,
    KW_WHERE,
    KW_BETWEEN,
    KW_AND,
    KW_VALUES,
    KW_INTO
} Kw;

typedef struct {
    TokKind kind;
    Kw kw;
    char txt[TOK_LEN];
    long num;
    size_t pos;
} Tok;

typedef struct {
    Tok *list;
    size_t len;
    size_t cap;
} TokList;

void free_toks(TokList *lst);
Err lex_stmt(const char *sql, TokList *out, char *err, size_t cap);

#endif
