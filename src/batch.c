/* This file loads SQL input and splits one batch into statements. */
#include "batch.h"

#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char data[4];
    uint32_t ver;
    uint32_t len;
} QHdr;

/* 요약: 잘라낸 SQL 조각을 문장 목록에 추가한다. */
static Err add_stmt(StmtList *out, const char *sql, size_t a, size_t b, int no) {
    char *txt;
    Stmt *next;

    while (a < b && isspace((unsigned char)sql[a])) {
        ++a;
    }
    while (b > a && isspace((unsigned char)sql[b - 1])) {
        --b;
    }
    if (a == b) {
        return ERR_INPUT;
    }

    txt = dup_rng(sql + a, b - a);
    if (txt == NULL) {
        return ERR_MEM;
    }
    if (out->len == out->cap) {
        out->cap = out->cap == 0 ? 4 : out->cap * 2;
        next = (Stmt *)realloc(out->list, out->cap * sizeof(*next));
        if (next == NULL) {
            free(txt);
            return ERR_MEM;
        }
        out->list = next;
    }
    out->list[out->len].txt = txt;
    out->list[out->len].start = a;
    out->list[out->len].no = no;
    out->len += 1;
    return ERR_OK;
}

/* 요약: 배치를 세미콜론 기준으로 문장 목록으로 나눈다. */
Err split_sql(const char *sql, StmtList *out, char *err, size_t cap) {
    size_t i;
    size_t start;
    int in_str;
    int no;
    int found;
    Err res;

    memset(out, 0, sizeof(*out));
    if (sql == NULL) {
        err_set(err, cap, "empty batch");
        return ERR_INPUT;
    }

    start = 0;
    in_str = 0;
    no = 1;
    found = 0;
    for (i = 0; sql[i] != '\0'; ++i) {
        if (sql[i] == '\'') {
            if (in_str && sql[i + 1] == '\'') {
                ++i;
                continue;
            }
            in_str = !in_str;
            continue;
        }
        if (!in_str && sql[i] == ';') {
            res = add_stmt(out, sql, start, i, no);
            if (res != ERR_OK) {
                free_stmts(out);
                err_set(err, cap, "statement %d is empty near char %zu", no, i);
                return res;
            }
            found = 1;
            start = i + 1;
            no += 1;
        }
    }

    if (in_str) {
        free_stmts(out);
        err_set(err, cap, "unterminated string literal");
        return ERR_INPUT;
    }
    while (sql[start] != '\0' && isspace((unsigned char)sql[start])) {
        ++start;
    }
    if (sql[start] != '\0') {
        free_stmts(out);
        err_set(err, cap, "batch must end with ';'");
        return ERR_INPUT;
    }
    if (!found) {
        free_stmts(out);
        err_set(err, cap, "empty batch");
        return ERR_INPUT;
    }
    return ERR_OK;
}

/* 요약: 파일 내용을 끝까지 읽어 메모리 버퍼에 담는다. */
static Err read_all(FILE *fp, unsigned char **buf, size_t *len, char *err,
                    size_t cap) {
    unsigned char tmp[4096];
    size_t got;
    size_t old;
    unsigned char *next;

    *buf = NULL;
    *len = 0;
    while ((got = fread(tmp, 1, sizeof(tmp), fp)) > 0) {
        old = *len;
        next = (unsigned char *)realloc(*buf, old + got);
        if (next == NULL) {
            free(*buf);
            *buf = NULL;
            *len = 0;
            err_set(err, cap, "out of memory while reading file");
            return ERR_MEM;
        }
        *buf = next;
        memcpy(*buf + old, tmp, got);
        *len = old + got;
    }
    if (ferror(fp)) {
        free(*buf);
        *buf = NULL;
        *len = 0;
        err_set(err, cap, "failed while reading file");
        return ERR_IO;
    }
    return ERR_OK;
}

/* 요약: 텍스트 SQL 파일이나 QSQL 파일에서 배치를 읽는다. */
Err load_sql_file(const char *path, char **sql, char *err, size_t cap) {
    FILE *fp;
    unsigned char *raw;
    size_t len;
    Err res;
    QHdr hdr;
    char *out;
    size_t off;

    *sql = NULL;
    fp = fopen(path, "rb");
    if (fp == NULL) {
        if (errno == ENOENT) {
            err_set(err, cap, "file not found: %s", path);
            return ERR_FILE;
        }
        if (errno == EACCES) {
            err_set(err, cap, "permission denied: %s", path);
            return ERR_FILE;
        }
        err_set(err, cap, "failed to open file: %s", path);
        return ERR_FILE;
    }

    raw = NULL;
    len = 0;
    res = read_all(fp, &raw, &len, err, cap);
    fclose(fp);
    if (res != ERR_OK) {
        return res;
    }

    if (len >= sizeof(hdr) && memcmp(raw, "QSQL", 4) == 0) {
        memcpy(&hdr, raw, sizeof(hdr));
        if (hdr.ver != 1) {
            free(raw);
            err_set(err, cap, "bad qsql version: %u", hdr.ver);
            return ERR_FMT;
        }
        off = sizeof(hdr);
        if ((size_t)hdr.len != len - off) {
            free(raw);
            err_set(err, cap, "bad qsql payload length");
            return ERR_FMT;
        }
        out = dup_rng((const char *)(raw + off), (size_t)hdr.len);
        free(raw);
        if (out == NULL) {
            err_set(err, cap, "out of memory while loading qsql");
            return ERR_MEM;
        }
        *sql = out;
        return ERR_OK;
    }

    if (len >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) {
        out = dup_rng((const char *)(raw + 3), len - 3);
    } else {
        out = dup_rng((const char *)raw, len);
    }
    free(raw);
    if (out == NULL) {
        err_set(err, cap, "out of memory while loading sql");
        return ERR_MEM;
    }
    *sql = out;
    return ERR_OK;
}

/* 요약: 기본 입력 경로를 순서대로 찾아 SQL을 읽는다. */
Err load_default_sql(char **sql, char *err, size_t cap) {
    Err res;

    res = load_sql_file("data/input.qsql", sql, err, cap);
    if (res == ERR_OK) {
        return ERR_OK;
    }
    return load_sql_file("data/input.sql", sql, err, cap);
}

/* 요약: 배치 문자열을 QSQL 바이너리 파일로 저장한다. */
Err save_qsql(const char *path, const char *sql, char *err, size_t cap) {
    FILE *fp;
    QHdr hdr;
    size_t len;

    fp = fopen(path, "wb");
    if (fp == NULL) {
        err_set(err, cap, "failed to create qsql: %s", path);
        return ERR_FILE;
    }
    len = strlen(sql);
    memcpy(hdr.data, "QSQL", 4);
    hdr.ver = 1;
    hdr.len = (uint32_t)len;
    if (fwrite(&hdr, sizeof(hdr), 1, fp) != 1 ||
        fwrite(sql, 1, len, fp) != len) {
        fclose(fp);
        err_set(err, cap, "failed to write qsql");
        return ERR_IO;
    }
    fclose(fp);
    return ERR_OK;
}

/* 요약: 문장 목록이 들고 있는 메모리를 모두 정리한다. */
void free_stmts(StmtList *lst) {
    size_t i;

    if (lst == NULL) {
        return;
    }
    for (i = 0; i < lst->len; ++i) {
        free(lst->list[i].txt);
    }
    free(lst->list);
    lst->list = NULL;
    lst->len = 0;
    lst->cap = 0;
}
