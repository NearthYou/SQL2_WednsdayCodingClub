/* This file executes parsed queries and formats user-facing output. */
#include "sql2.h"

#include <stdlib.h>
#include <string.h>

enum {
    COL_ID = 0,
    COL_TITLE,
    COL_AUTHOR,
    COL_GENRE
};

static int col_id(const char *name) {
    if (str_ieq(name, "id")) {
        return COL_ID;
    }
    if (str_ieq(name, "title")) {
        return COL_TITLE;
    }
    if (str_ieq(name, "author")) {
        return COL_AUTHOR;
    }
    if (str_ieq(name, "genre")) {
        return COL_GENRE;
    }
    return -1;
}

static const char *col_name(int cid) {
    switch (cid) {
    case COL_ID:
        return "id";
    case COL_TITLE:
        return "title";
    case COL_AUTHOR:
        return "author";
    case COL_GENRE:
        return "genre";
    default:
        return "?";
    }
}

static const char *scan_name(ScanKind scan) {
    return scan == SCAN_BTREE ? "B+Tree" : "Linear";
}

static Err row_add(RowSet *set, int idx) {
    int *next;

    if (set->len == set->cap) {
        set->cap = set->cap == 0 ? 8 : set->cap * 2;
        next = (int *)realloc(set->list, set->cap * sizeof(*next));
        if (next == NULL) {
            return ERR_MEM;
        }
        set->list = next;
    }
    set->list[set->len++] = idx;
    return ERR_OK;
}

static const char *cell_txt(const Book *row, int cid, char *buf, size_t cap) {
    switch (cid) {
    case COL_ID:
        snprintf(buf, cap, "%d", row->id);
        return buf;
    case COL_TITLE:
        return row->title;
    case COL_AUTHOR:
        return row->author;
    case COL_GENRE:
        return row->genre;
    default:
        return "";
    }
}

static int row_match(const Book *row, const Cond *cond, char *err, size_t cap) {
    int cid;

    if (!cond->used) {
        return 1;
    }
    cid = col_id(cond->col);
    if (cid < 0) {
        err_set(err, cap, "unknown column in WHERE: %s", cond->col);
        return -1;
    }
    if (cid == COL_ID) {
        if (cond->val.kind != VAL_INT) {
            err_set(err, cap, "id WHERE value must be an integer");
            return -1;
        }
        return row->id == (int)cond->val.num;
    }
    if (cond->val.kind != VAL_STR) {
        err_set(err, cap, "string column WHERE value must be a string");
        return -1;
    }
    if (cid == COL_TITLE) {
        return strcmp(row->title, cond->val.str) == 0;
    }
    if (cid == COL_AUTHOR) {
        return strcmp(row->author, cond->val.str) == 0;
    }
    return strcmp(row->genre, cond->val.str) == 0;
}

static Err find_rows(Db *db, const Qry *qry, RowSet *set, char *err,
                     size_t cap) {
    double a;
    double b;
    size_t i;
    int idx;
    int hit;
    Err res;

    memset(set, 0, sizeof(*set));
    a = now_ms();
    if (qry->cond.used && col_id(qry->cond.col) == COL_ID) {
        set->scan = SCAN_BTREE;
        if (qry->cond.val.kind != VAL_INT) {
            err_set(err, cap, "id WHERE value must be an integer");
            return ERR_EXEC;
        }
        hit = bp_get(&db->idx, (int)qry->cond.val.num, &idx);
        if (hit) {
            res = row_add(set, idx);
            if (res != ERR_OK) {
                err_set(err, cap, "out of memory while storing row hits");
                return res;
            }
        }
    } else {
        set->scan = SCAN_LINEAR;
        for (i = 0; i < db->len; ++i) {
            hit = row_match(&db->rows[i], &qry->cond, err, cap);
            if (hit < 0) {
                return ERR_EXEC;
            }
            if (hit) {
                res = row_add(set, (int)i);
                if (res != ERR_OK) {
                    err_set(err, cap, "out of memory while storing row hits");
                    return res;
                }
            }
        }
    }
    b = now_ms();
    set->ms = b - a;
    return ERR_OK;
}

static Err find_proj(const Qry *qry, int *cols, size_t *ncol, char *err,
                     size_t cap) {
    size_t i;
    int cid;

    if (qry->cols.all) {
        cols[0] = COL_ID;
        cols[1] = COL_TITLE;
        cols[2] = COL_AUTHOR;
        cols[3] = COL_GENRE;
        *ncol = 4;
        return ERR_OK;
    }
    for (i = 0; i < qry->cols.len; ++i) {
        cid = col_id(qry->cols.cols[i]);
        if (cid < 0) {
            err_set(err, cap, "unknown column in SELECT: %s", qry->cols.cols[i]);
            return ERR_EXEC;
        }
        cols[i] = cid;
    }
    *ncol = qry->cols.len;
    return ERR_OK;
}

static Err add_sep(StrBuf *out, size_t *w, size_t ncol) {
    size_t i;
    Err res;

    res = sb_add(out, "+");
    if (res != ERR_OK) {
        return res;
    }
    for (i = 0; i < ncol; ++i) {
        size_t j;

        for (j = 0; j < w[i] + 2; ++j) {
            res = sb_add(out, "-");
            if (res != ERR_OK) {
                return res;
            }
        }
        res = sb_add(out, "+");
        if (res != ERR_OK) {
            return res;
        }
    }
    return sb_add(out, "\n");
}

static Err print_sel(const Db *db, const Qry *qry, const RowSet *set, StrBuf *out,
                     char *err, size_t cap) {
    int cols[MAX_COL];
    size_t ncol;
    size_t w[MAX_COL];
    size_t i;
    size_t j;
    char tmp[32];
    const char *txt;
    Err res;

    res = find_proj(qry, cols, &ncol, err, cap);
    if (res != ERR_OK) {
        return res;
    }
    for (i = 0; i < ncol; ++i) {
        w[i] = strlen(col_name(cols[i]));
    }
    for (i = 0; i < set->len; ++i) {
        const Book *row;

        row = &db->rows[set->list[i]];
        for (j = 0; j < ncol; ++j) {
            txt = cell_txt(row, cols[j], tmp, sizeof(tmp));
            if (strlen(txt) > w[j]) {
                w[j] = strlen(txt);
            }
        }
    }

    res = add_sep(out, w, ncol);
    if (res != ERR_OK) {
        return res;
    }
    for (i = 0; i < ncol; ++i) {
        res = sb_addf(out, "| %-*s ", (int)w[i], col_name(cols[i]));
        if (res != ERR_OK) {
            return res;
        }
    }
    res = sb_add(out, "|\n");
    if (res != ERR_OK) {
        return res;
    }
    res = add_sep(out, w, ncol);
    if (res != ERR_OK) {
        return res;
    }

    if (set->len == 0) {
        res = sb_add(out, "(no rows)\n");
        if (res != ERR_OK) {
            return res;
        }
    } else {
        for (i = 0; i < set->len; ++i) {
            const Book *row;

            row = &db->rows[set->list[i]];
            for (j = 0; j < ncol; ++j) {
                txt = cell_txt(row, cols[j], tmp, sizeof(tmp));
                res = sb_addf(out, "| %-*s ", (int)w[j], txt);
                if (res != ERR_OK) {
                    return res;
                }
            }
            res = sb_add(out, "|\n");
            if (res != ERR_OK) {
                return res;
            }
        }
    }
    res = add_sep(out, w, ncol);
    if (res != ERR_OK) {
        return res;
    }
    res = sb_addf(out, "rows=%zu\n", set->len);
    if (res != ERR_OK) {
        return res;
    }
    return sb_addf(out, "scan=%s, time=%.3f ms\n", scan_name(set->scan),
                   set->ms);
}

static Err run_sel(Db *db, const Qry *qry, StrBuf *out, char *err, size_t cap) {
    RowSet set;
    Err res;

    if (!str_ieq(qry->table, "books")) {
        err_set(err, cap, "unknown table: %s", qry->table);
        return ERR_EXEC;
    }
    res = find_rows(db, qry, &set, err, cap);
    if (res != ERR_OK) {
        return res;
    }
    res = print_sel(db, qry, &set, out, err, cap);
    free_rows(&set);
    return res;
}

static Err run_ins(Db *db, const Qry *qry, StrBuf *out, char *err, size_t cap) {
    int new_id;

    if (!str_ieq(qry->table, "books")) {
        err_set(err, cap, "unknown table: %s", qry->table);
        return ERR_EXEC;
    }
    if (qry->nval != 3) {
        err_set(err, cap, "INSERT for books needs 3 values");
        return ERR_EXEC;
    }
    if (qry->vals[0].kind != VAL_STR || qry->vals[1].kind != VAL_STR ||
        qry->vals[2].kind != VAL_STR) {
        err_set(err, cap, "books INSERT values must be strings");
        return ERR_EXEC;
    }
    if (db_add(db, qry->vals[0].str, qry->vals[1].str, qry->vals[2].str, &new_id,
               err, cap) != ERR_OK) {
        return ERR_EXEC;
    }
    return sb_addf(out, "inserted id=%d, rows=1\n", new_id);
}

Err run_qry(Db *db, const Qry *qry, StrBuf *out, char *err, size_t cap) {
    if (qry->kind == Q_SELECT) {
        return run_sel(db, qry, out, err, cap);
    }
    return run_ins(db, qry, out, err, cap);
}

Err run_batch(Db *db, const char *sql, StrBuf *out, char *err, size_t cap) {
    StmtList stmts;
    TokList toks;
    Qry qry;
    StrBuf tmp;
    size_t old_len;
    int old_id;
    size_t i;
    int dirty;
    Err res;
    char msg[256];

    old_len = db->len;
    old_id = db->next_id;
    dirty = 0;
    sb_init(&tmp);
    memset(&stmts, 0, sizeof(stmts));

    res = split_sql(sql, &stmts, msg, sizeof(msg));
    if (res != ERR_OK) {
        err_set(err, cap, "%s", msg);
        return res;
    }

    for (i = 0; i < stmts.len; ++i) {
        memset(&toks, 0, sizeof(toks));
        res = lex_stmt(stmts.list[i].txt, &toks, msg, sizeof(msg));
        if (res != ERR_OK) {
            err_set(err, cap, "statement %d lexer error: %s", stmts.list[i].no,
                    msg);
            free_toks(&toks);
            goto fail;
        }
        res = parse_stmt(&toks, &qry, msg, sizeof(msg));
        free_toks(&toks);
        if (res != ERR_OK) {
            err_set(err, cap, "statement %d parse error: %s", stmts.list[i].no,
                    msg);
            goto fail;
        }
        if (qry.kind == Q_INSERT) {
            dirty = 1;
        }
        res = run_qry(db, &qry, &tmp, msg, sizeof(msg));
        if (res != ERR_OK) {
            err_set(err, cap, "statement %d exec error: %s", stmts.list[i].no,
                    msg);
            goto fail;
        }
        if (i + 1 < stmts.len) {
            res = sb_add(&tmp, "\n");
            if (res != ERR_OK) {
                err_set(err, cap, "out of memory while buffering output");
                goto fail;
            }
        }
    }

    if (dirty) {
        res = db_save(db, msg, sizeof(msg));
        if (res != ERR_OK) {
            err_set(err, cap, "save failed after successful batch: %s", msg);
            goto fail;
        }
    }
    res = sb_add(out, tmp.buf == NULL ? "" : tmp.buf);
    if (res != ERR_OK) {
        err_set(err, cap, "out of memory while copying output");
        goto fail;
    }
    free_stmts(&stmts);
    sb_free(&tmp);
    return ERR_OK;

fail:
    db->len = old_len;
    db->next_id = old_id;
    db_reidx(db, msg, sizeof(msg));
    free_stmts(&stmts);
    sb_free(&tmp);
    return res;
}

