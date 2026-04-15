/* This file defines shared types and helper functions for the SQL demo. */
#ifndef SQL2_H
#define SQL2_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define PATH_LEN 260
#define NAME_LEN 16
#define TOK_LEN 128
#define VAL_LEN 128
#define MAX_COL 8
#define TITLE_LEN 96
#define AUTH_LEN 64
#define GENRE_LEN 32
#define BP_ORDER 32
#define BP_MAX (BP_ORDER - 1)

typedef enum {
    ERR_OK = 0,
    ERR_MEM,
    ERR_IO,
    ERR_ARG,
    ERR_INPUT,
    ERR_FILE,
    ERR_FMT,
    ERR_LEX,
    ERR_PARSE,
    ERR_EXEC
} Err;

typedef enum {
    SRC_NONE = 0,
    SRC_CLI,
    SRC_FILE
} SrcMode;

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
    KW_VALUES,
    KW_INTO
} Kw;

typedef enum {
    VAL_INT = 0,
    VAL_STR
} ValKind;

typedef enum {
    Q_SELECT = 0,
    Q_INSERT
} QryKind;

typedef enum {
    SCAN_LINEAR = 0,
    SCAN_BTREE
} ScanKind;

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} StrBuf;

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

typedef struct {
    ValKind kind;
    long num;
    char str[VAL_LEN];
} Val;

typedef struct {
    int used;
    char col[NAME_LEN];
    Val val;
} Cond;

typedef struct {
    int all;
    size_t len;
    char cols[MAX_COL][NAME_LEN];
} ColList;

typedef struct {
    QryKind kind;
    char table[NAME_LEN];
    ColList cols;
    Cond cond;
    size_t nval;
    Val vals[MAX_COL];
    size_t pos;
} Qry;

typedef struct {
    int id;
    char title[TITLE_LEN];
    char author[AUTH_LEN];
    char genre[GENRE_LEN];
} Book;

typedef struct BpNode {
    int leaf;
    int nkey;
    int keys[BP_MAX];
    int vals[BP_MAX];
    struct BpNode *kid[BP_ORDER];
    struct BpNode *next;
} BpNode;

typedef struct {
    BpNode *root;
} BpTree;

typedef struct {
    Book *rows;
    size_t len;
    size_t cap;
    int next_id;
    char path[PATH_LEN];
    BpTree idx;
} Db;

typedef struct {
    int *list;
    size_t len;
    size_t cap;
    ScanKind scan;
    double ms;
} RowSet;

typedef struct {
    char data[4];
    uint32_t ver;
    uint32_t len;
} QHdr;

typedef struct {
    char data[4];
    uint32_t ver;
    uint16_t rec_sz;
    uint16_t keep;
    uint32_t cnt;
    uint32_t next_id;
} DHdr;

typedef struct {
    SrcMode mode;
    char batch[4096];
    char file[PATH_LEN];
    char data[PATH_LEN];
    int help;
    int sum_only;
} Opts;

double now_ms(void);
void sb_init(StrBuf *sb);
void sb_free(StrBuf *sb);
Err sb_add(StrBuf *sb, const char *txt);
Err sb_addn(StrBuf *sb, const char *txt, size_t len);
Err sb_addf(StrBuf *sb, const char *fmt, ...);
void err_set(char *buf, size_t cap, const char *fmt, ...);
int str_ieq(const char *a, const char *b);
void trim_in(char *txt);
char *dup_txt(const char *txt);
char *dup_rng(const char *txt, size_t len);
char *read_line(FILE *fp);
void free_stmts(StmtList *lst);
void free_toks(TokList *lst);
void free_rows(RowSet *set);

Err split_sql(const char *sql, StmtList *out, char *err, size_t cap);
Err load_sql_file(const char *path, char **sql, char *err, size_t cap);
Err load_default_sql(char **sql, char *err, size_t cap);
Err save_qsql(const char *path, const char *sql, char *err, size_t cap);
Err parse_args(int argc, char **argv, Opts *opt, char *err, size_t cap);
void print_help(void);

Err lex_stmt(const char *sql, TokList *out, char *err, size_t cap);
Err parse_stmt(const TokList *toks, Qry *qry, char *err, size_t cap);

void bp_init(BpTree *tree);
void bp_free(BpTree *tree);
Err bp_put(BpTree *tree, int key, int val);
int bp_get(const BpTree *tree, int key, int *val);

void db_init(Db *db);
void db_free(Db *db);
Err db_set_path(Db *db, const char *path, char *err, size_t cap);
Err db_load(Db *db, char *err, size_t cap);
Err db_save(Db *db, char *err, size_t cap);
Err db_seed(Db *db, char *err, size_t cap);
Err db_add(Db *db, const char *title, const char *author, const char *genre,
           int *new_id, char *err, size_t cap);
Err db_reidx(Db *db, char *err, size_t cap);

Err run_qry(Db *db, const Qry *qry, int sum_only, StrBuf *out, char *err,
            size_t cap);
Err run_batch(Db *db, const char *sql, int sum_only, StrBuf *out, char *err,
              size_t cap);

#endif
