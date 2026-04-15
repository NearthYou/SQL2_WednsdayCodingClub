/* This file executes parsed queries and formats user-facing output. */
#include "batch.h"
#include "exec.h"
#include "lex.h"

#include <stdlib.h>
#include <string.h>

enum {
    COL_ID = 0,
    COL_TITLE,
    COL_AUTHOR,
    COL_GENRE
};

/* 요약: 컬럼 이름을 내부 컬럼 번호로 바꾼다. */
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

/* 요약: 내부 컬럼 번호를 화면용 이름으로 바꾼다. */
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

/* 요약: 탐색 방식을 사람이 읽는 이름으로 바꾼다. */
static const char *scan_name(ScanKind scan) {
    return scan == SCAN_BTREE ? "B+Tree" : "Linear";
}

/* 요약: summary 전용 출력 제목을 만든다. */
static const char *sum_head(const Qry *qry) {
    int cid;

    if (qry->cond.kind == COND_NONE) {
        return "full scan";
    }
    cid = col_id(qry->cond.col);
    if (qry->cond.kind == COND_RANGE) {
        return cid == COL_ID ? "id range lookup" : "range lookup";
    }
    switch (cid) {
    case COL_ID:
        return "id lookup";
    case COL_TITLE:
        return "title lookup";
    case COL_AUTHOR:
        return "author lookup";
    case COL_GENRE:
        return "genre lookup";
    default:
        return "query summary";
    }
}

/* 요약: 조회 결과 집합에 행 인덱스를 하나 추가한다. */
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

/* 요약: 한 셀 값을 문자열로 바꿔 출력에 쓴다. */
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

/* 요약: 한 행이 WHERE 조건과 맞는지 검사한다. */
static int row_match(const Book *row, const Cond *cond, char *err, size_t cap) {
    int cid;

    if (cond->kind == COND_NONE) {
        return 1;
    }
    if (cond->kind != COND_EQ) {
        err_set(err, cap, "BETWEEN is only supported for id");
        return -1;
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

typedef struct {
    RowSet *set;
} RangeFill;

/* 요약: 범위 순회가 넘겨준 행 인덱스를 결과 집합에 모은다. */
static Err add_range_hit(int key, int val, void *ctx) {
    RangeFill *fill;

    (void)key;
    fill = (RangeFill *)ctx;
    return row_add(fill->set, val);
}

/* 요약: 조건에 맞는 행 인덱스를 찾고 시간도 잰다. */
static Err find_rows(Db *db, const Qry *qry, RowSet *set, char *err,
                     size_t cap) {
    double a;
    double b;
    size_t i;
    int idx;
    int hit;
    Err res;
    int cid;
    RangeFill fill;

    memset(set, 0, sizeof(*set));
    a = now_ms();
    cid = qry->cond.kind == COND_NONE ? -1 : col_id(qry->cond.col);
    if (qry->cond.kind == COND_RANGE) {
        if (cid < 0) {
            err_set(err, cap, "unknown column in WHERE: %s", qry->cond.col);
            return ERR_EXEC;
        }
        if (cid != COL_ID) {
            err_set(err, cap, "BETWEEN is only supported for id");
            return ERR_EXEC;
        }
        if (qry->cond.min_num > qry->cond.max_num) {
            err_set(err, cap, "id range start must be <= range end");
            return ERR_EXEC;
        }
        set->scan = SCAN_BTREE;
        fill.set = set;
        res = bp_visit_range(&db->idx, (int)qry->cond.min_num,
                             (int)qry->cond.max_num, add_range_hit, &fill);
        if (res != ERR_OK) {
            err_set(err, cap, "out of memory while storing row hits");
            return res;
        }
    } else if (qry->cond.kind == COND_EQ && cid == COL_ID) {
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

/* 요약: SELECT가 요청한 컬럼 목록을 내부 번호 배열로 만든다. */
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

/* 요약: 표 출력에 쓰는 가로 구분선을 추가한다. */
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

/* 요약: SELECT 결과를 표나 요약 형식으로 출력 버퍼에 쓴다. */
static Err print_sel(const Db *db, const Qry *qry, const RowSet *set,
                     int sum_only, StrBuf *out, char *err, size_t cap) {
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
    if (sum_only) {
        res = sb_addf(out, "[%s]\n", sum_head(qry));
        if (res != ERR_OK) {
            return res;
        }
        res = sb_addf(out, "rows : %zu\n", set->len);
        if (res != ERR_OK) {
            return res;
        }
        res = sb_addf(out, "scan : %s\n", scan_name(set->scan));
        if (res != ERR_OK) {
            return res;
        }
        return sb_addf(out, "time : %.3f ms\n", set->ms);
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

/* 요약: SELECT 쿼리를 실행하고 결과를 출력 버퍼에 담는다. */
static Err run_sel(Db *db, const Qry *qry, int sum_only, StrBuf *out, char *err,
                   size_t cap) {
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
    res = print_sel(db, qry, &set, sum_only, out, err, cap);
    free_rows(&set);
    return res;
}

/* 요약: INSERT 쿼리를 실행하고 새 id를 출력 버퍼에 쓴다. */
static Err run_ins(Db *db, const Qry *qry, StrBuf *out, char *err, size_t cap) {
    int new_id;
    Err res;

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
    res = db_add(db, qry->vals[0].str, qry->vals[1].str, qry->vals[2].str,
                 &new_id, err, cap);
    if (res != ERR_OK) {
        return res;
    }
    return sb_addf(out, "inserted id=%d, rows=1\n", new_id);
}

/* 요약: 쿼리 종류에 따라 SELECT 또는 INSERT를 실행한다. */
Err run_qry(Db *db, const Qry *qry, int sum_only, StrBuf *out, char *err,
            size_t cap) {
    if (qry->kind == Q_SELECT) {
        return run_sel(db, qry, sum_only, out, err, cap);
    }
    return run_ins(db, qry, out, err, cap);
}

/* 요약: 배치 하나를 트랜잭션처럼 실행하고 실패 시 롤백한다. */
Err run_batch(Db *db, const char *sql, int sum_only, StrBuf *out, char *err,
              size_t cap) {
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
        res = run_qry(db, &qry, sum_only, &tmp, msg, sizeof(msg));
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
    {
        Err roll_res;

        roll_res = db_reidx(db, msg, sizeof(msg));
        if (roll_res != ERR_OK) {
            err_set(err, cap, "rollback failed while rebuilding index: %s", msg);
            res = roll_res;
        }
    }
    free_stmts(&stmts);
    sb_free(&tmp);
    return res;
}

/* 요약: 조회 결과 집합이 들고 있는 메모리를 정리한다. */
void free_rows(RowSet *set) {
    free(set->list);
    set->list = NULL;
    set->len = 0;
    set->cap = 0;
}
