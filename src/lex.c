/* This file turns one SQL statement into a list of tokens. */
#include "lex.h"

#include "util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* 요약: 단어가 예약어인지 검사해 키워드 종류를 정한다. */
static Kw find_kw(const char *txt) {
    if (str_ieq(txt, "SELECT")) {
        return KW_SELECT;
    }
    if (str_ieq(txt, "INSERT")) {
        return KW_INSERT;
    }
    if (str_ieq(txt, "FROM")) {
        return KW_FROM;
    }
    if (str_ieq(txt, "WHERE")) {
        return KW_WHERE;
    }
    if (str_ieq(txt, "BETWEEN")) {
        return KW_BETWEEN;
    }
    if (str_ieq(txt, "AND")) {
        return KW_AND;
    }
    if (str_ieq(txt, "VALUES")) {
        return KW_VALUES;
    }
    if (str_ieq(txt, "INTO")) {
        return KW_INTO;
    }
    return KW_NONE;
}

/* 요약: 토큰 하나를 토큰 목록 끝에 추가한다. */
static Err push_tok(TokList *out, Tok tok) {
    Tok *next;

    if (out->len == out->cap) {
        out->cap = out->cap == 0 ? 8 : out->cap * 2;
        next = (Tok *)realloc(out->list, out->cap * sizeof(*next));
        if (next == NULL) {
            return ERR_MEM;
        }
        out->list = next;
    }
    out->list[out->len++] = tok;
    return ERR_OK;
}

/* 요약: 문장 끝을 뜻하는 END 토큰을 넣는다. */
static Err push_end(TokList *out, size_t pos) {
    Tok tok;

    memset(&tok, 0, sizeof(tok));
    tok.kind = TK_END;
    tok.pos = pos;
    return push_tok(out, tok);
}

/* 요약: 식별자나 키워드 단어를 토큰으로 읽는다. */
static Err lex_word(const char *sql, size_t *at, TokList *out) {
    size_t pos;
    size_t len;
    Tok tok;

    pos = *at;
    len = 0;
    memset(&tok, 0, sizeof(tok));
    while (isalnum((unsigned char)sql[*at]) || sql[*at] == '_') {
        if (len + 1 >= sizeof(tok.txt)) {
            return ERR_LEX;
        }
        tok.txt[len++] = sql[*at];
        *at += 1;
    }
    tok.txt[len] = '\0';
    tok.kind = TK_WORD;
    tok.kw = find_kw(tok.txt);
    tok.pos = pos;
    return push_tok(out, tok);
}

/* 요약: 정수 리터럴을 토큰으로 읽는다. */
static Err lex_int(const char *sql, size_t *at, TokList *out) {
    size_t pos;
    size_t len;
    Tok tok;

    pos = *at;
    len = 0;
    memset(&tok, 0, sizeof(tok));
    while (isdigit((unsigned char)sql[*at])) {
        if (len + 1 >= sizeof(tok.txt)) {
            return ERR_LEX;
        }
        tok.txt[len++] = sql[*at];
        *at += 1;
    }
    tok.txt[len] = '\0';
    tok.kind = TK_INT;
    tok.num = strtol(tok.txt, NULL, 10);
    tok.pos = pos;
    return push_tok(out, tok);
}

/* 요약: 작은따옴표 문자열 리터럴을 토큰으로 읽는다. */
static Err lex_str(const char *sql, size_t *at, TokList *out, char *err,
                   size_t cap) {
    size_t pos;
    size_t len;
    Tok tok;

    pos = *at;
    len = 0;
    memset(&tok, 0, sizeof(tok));
    *at += 1;
    while (sql[*at] != '\0') {
        if (sql[*at] == '\'') {
            if (sql[*at + 1] == '\'') {
                if (len + 1 >= sizeof(tok.txt)) {
                    err_set(err, cap, "string literal too long");
                    return ERR_LEX;
                }
                tok.txt[len++] = '\'';
                *at += 2;
                continue;
            }
            *at += 1;
            tok.txt[len] = '\0';
            tok.kind = TK_STR;
            tok.pos = pos;
            return push_tok(out, tok);
        }
        if (len + 1 >= sizeof(tok.txt)) {
            err_set(err, cap, "string literal too long");
            return ERR_LEX;
        }
        tok.txt[len++] = sql[*at];
        *at += 1;
    }
    err_set(err, cap, "unterminated string literal");
    return ERR_LEX;
}

/* 요약: SQL 한 문장을 토큰 목록으로 바꾼다. */
Err lex_stmt(const char *sql, TokList *out, char *err, size_t cap) {
    size_t at;
    Tok tok;
    Err res;

    memset(out, 0, sizeof(*out));
    for (at = 0; sql[at] != '\0';) {
        if (isspace((unsigned char)sql[at])) {
            ++at;
            continue;
        }
        if (isalpha((unsigned char)sql[at]) || sql[at] == '_') {
            res = lex_word(sql, &at, out);
            if (res != ERR_OK) {
                free_toks(out);
                err_set(err, cap, "word token is too long");
                return res;
            }
            continue;
        }
        if (isdigit((unsigned char)sql[at])) {
            res = lex_int(sql, &at, out);
            if (res != ERR_OK) {
                free_toks(out);
                err_set(err, cap, "number token is too long");
                return res;
            }
            continue;
        }
        if (sql[at] == '\'') {
            res = lex_str(sql, &at, out, err, cap);
            if (res != ERR_OK) {
                free_toks(out);
                return res;
            }
            continue;
        }

        memset(&tok, 0, sizeof(tok));
        tok.pos = at;
        switch (sql[at]) {
        case ',':
            tok.kind = TK_COMMA;
            break;
        case '(':
            tok.kind = TK_LP;
            break;
        case ')':
            tok.kind = TK_RP;
            break;
        case '=':
            tok.kind = TK_EQ;
            break;
        case '*':
            tok.kind = TK_STAR;
            break;
        default:
            free_toks(out);
            err_set(err, cap, "bad char '%c' at pos %zu", sql[at], at);
            return ERR_LEX;
        }
        ++at;
        res = push_tok(out, tok);
        if (res != ERR_OK) {
            free_toks(out);
            err_set(err, cap, "out of memory while storing tokens");
            return res;
        }
    }
    return push_end(out, at);
}

/* 요약: 토큰 목록 메모리를 정리한다. */
void free_toks(TokList *lst) {
    free(lst->list);
    lst->list = NULL;
    lst->len = 0;
    lst->cap = 0;
}
