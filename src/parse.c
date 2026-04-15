/* This file builds a simple query tree from a token list. */
#include "sql2.h"

#include <string.h>

typedef struct {
    const TokList *toks;
    size_t at;
} Psr;

static const Tok *cur(Psr *ps) {
    return &ps->toks->list[ps->at];
}

static const Tok *take(Psr *ps) {
    const Tok *tok;

    tok = cur(ps);
    if (tok->kind != TK_END) {
        ps->at += 1;
    }
    return tok;
}

static int tok_is(Psr *ps, TokKind kind, Kw kw) {
    const Tok *tok;

    tok = cur(ps);
    if (tok->kind != kind) {
        return 0;
    }
    if (kw != KW_NONE && tok->kw != kw) {
        return 0;
    }
    return 1;
}

static Err need(Psr *ps, TokKind kind, Kw kw, char *err, size_t cap,
                const char *what) {
    if (!tok_is(ps, kind, kw)) {
        err_set(err, cap, "expected %s near pos %zu", what, cur(ps)->pos);
        return ERR_PARSE;
    }
    take(ps);
    return ERR_OK;
}

static Err parse_word(Psr *ps, char *dst, size_t cap, char *err, size_t ecap,
                      const char *what) {
    const Tok *tok;

    if (!tok_is(ps, TK_WORD, KW_NONE)) {
        err_set(err, ecap, "expected %s near pos %zu", what, cur(ps)->pos);
        return ERR_PARSE;
    }
    tok = take(ps);
    snprintf(dst, cap, "%s", tok->txt);
    return ERR_OK;
}

static Err parse_val(Psr *ps, Val *val, char *err, size_t cap) {
    const Tok *tok;

    tok = cur(ps);
    if (tok->kind == TK_INT) {
        val->kind = VAL_INT;
        val->num = tok->num;
        val->str[0] = '\0';
        take(ps);
        return ERR_OK;
    }
    if (tok->kind == TK_STR) {
        val->kind = VAL_STR;
        val->num = 0;
        snprintf(val->str, sizeof(val->str), "%s", tok->txt);
        take(ps);
        return ERR_OK;
    }
    err_set(err, cap, "expected value near pos %zu", tok->pos);
    return ERR_PARSE;
}

static Err parse_cols(Psr *ps, ColList *cols, char *err, size_t cap) {
    Err res;

    memset(cols, 0, sizeof(*cols));
    if (tok_is(ps, TK_STAR, KW_NONE)) {
        cols->all = 1;
        take(ps);
        return ERR_OK;
    }
    while (1) {
        if (cols->len >= MAX_COL) {
            err_set(err, cap, "too many columns near pos %zu", cur(ps)->pos);
            return ERR_PARSE;
        }
        res = parse_word(ps, cols->cols[cols->len], sizeof(cols->cols[0]), err,
                         cap, "column name");
        if (res != ERR_OK) {
            return res;
        }
        cols->len += 1;
        if (!tok_is(ps, TK_COMMA, KW_NONE)) {
            break;
        }
        take(ps);
    }
    return ERR_OK;
}

static Err parse_cond(Psr *ps, Cond *cond, char *err, size_t cap) {
    Err res;

    memset(cond, 0, sizeof(*cond));
    if (!tok_is(ps, TK_WORD, KW_WHERE)) {
        return ERR_OK;
    }
    take(ps);
    cond->used = 1;
    res = parse_word(ps, cond->col, sizeof(cond->col), err, cap, "column name");
    if (res != ERR_OK) {
        return res;
    }
    res = need(ps, TK_EQ, KW_NONE, err, cap, "'='");
    if (res != ERR_OK) {
        return res;
    }
    return parse_val(ps, &cond->val, err, cap);
}

static Err parse_sel(Psr *ps, Qry *qry, char *err, size_t cap) {
    Err res;

    memset(qry, 0, sizeof(*qry));
    qry->kind = Q_SELECT;
    qry->pos = cur(ps)->pos;
    res = need(ps, TK_WORD, KW_SELECT, err, cap, "SELECT");
    if (res != ERR_OK) {
        return res;
    }
    res = parse_cols(ps, &qry->cols, err, cap);
    if (res != ERR_OK) {
        return res;
    }
    res = need(ps, TK_WORD, KW_FROM, err, cap, "FROM");
    if (res != ERR_OK) {
        return res;
    }
    res = parse_word(ps, qry->table, sizeof(qry->table), err, cap,
                     "table name");
    if (res != ERR_OK) {
        return res;
    }
    res = parse_cond(ps, &qry->cond, err, cap);
    if (res != ERR_OK) {
        return res;
    }
    if (!tok_is(ps, TK_END, KW_NONE)) {
        err_set(err, cap, "extra token near pos %zu", cur(ps)->pos);
        return ERR_PARSE;
    }
    return ERR_OK;
}

static Err parse_ins(Psr *ps, Qry *qry, char *err, size_t cap) {
    Err res;

    memset(qry, 0, sizeof(*qry));
    qry->kind = Q_INSERT;
    qry->pos = cur(ps)->pos;
    res = need(ps, TK_WORD, KW_INSERT, err, cap, "INSERT");
    if (res != ERR_OK) {
        return res;
    }
    res = need(ps, TK_WORD, KW_INTO, err, cap, "INTO");
    if (res != ERR_OK) {
        return res;
    }
    res = parse_word(ps, qry->table, sizeof(qry->table), err, cap,
                     "table name");
    if (res != ERR_OK) {
        return res;
    }
    res = need(ps, TK_WORD, KW_VALUES, err, cap, "VALUES");
    if (res != ERR_OK) {
        return res;
    }
    res = need(ps, TK_LP, KW_NONE, err, cap, "'('");
    if (res != ERR_OK) {
        return res;
    }
    while (1) {
        if (qry->nval >= MAX_COL) {
            err_set(err, cap, "too many values near pos %zu", cur(ps)->pos);
            return ERR_PARSE;
        }
        res = parse_val(ps, &qry->vals[qry->nval], err, cap);
        if (res != ERR_OK) {
            return res;
        }
        qry->nval += 1;
        if (!tok_is(ps, TK_COMMA, KW_NONE)) {
            break;
        }
        take(ps);
    }
    res = need(ps, TK_RP, KW_NONE, err, cap, "')'");
    if (res != ERR_OK) {
        return res;
    }
    if (!tok_is(ps, TK_END, KW_NONE)) {
        err_set(err, cap, "extra token near pos %zu", cur(ps)->pos);
        return ERR_PARSE;
    }
    return ERR_OK;
}

Err parse_stmt(const TokList *toks, Qry *qry, char *err, size_t cap) {
    Psr ps;

    ps.toks = toks;
    ps.at = 0;
    if (tok_is(&ps, TK_WORD, KW_SELECT)) {
        return parse_sel(&ps, qry, err, cap);
    }
    if (tok_is(&ps, TK_WORD, KW_INSERT)) {
        return parse_ins(&ps, qry, err, cap);
    }
    err_set(err, cap, "unsupported statement near pos %zu", cur(&ps)->pos);
    return ERR_PARSE;
}
