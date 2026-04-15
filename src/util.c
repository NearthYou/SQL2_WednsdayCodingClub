#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
/* This file holds small shared helpers used across the project. */
#include "sql2.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/* 요약: 문자열 버퍼가 더 담을 공간을 미리 늘린다. */
static Err sb_grow(StrBuf *sb, size_t add) {
    size_t need;
    char *next;

    need = sb->len + add + 1;
    if (need <= sb->cap) {
        return ERR_OK;
    }
    if (sb->cap == 0) {
        sb->cap = 64;
    }
    while (sb->cap < need) {
        sb->cap *= 2;
    }
    next = (char *)realloc(sb->buf, sb->cap);
    if (next == NULL) {
        return ERR_MEM;
    }
    sb->buf = next;
    return ERR_OK;
}

/* 요약: 현재 시간을 밀리초 단위로 읽는다. */
double now_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER now;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

/* 요약: 비어 있는 문자열 버퍼를 준비한다. */
void sb_init(StrBuf *sb) {
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
}

/* 요약: 문자열 버퍼 메모리를 정리한다. */
void sb_free(StrBuf *sb) {
    free(sb->buf);
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
}

/* 요약: 길이를 아는 문자열 조각을 버퍼 뒤에 붙인다. */
Err sb_addn(StrBuf *sb, const char *txt, size_t len) {
    Err err;

    err = sb_grow(sb, len);
    if (err != ERR_OK) {
        return err;
    }
    memcpy(sb->buf + sb->len, txt, len);
    sb->len += len;
    sb->buf[sb->len] = '\0';
    return ERR_OK;
}

/* 요약: C 문자열 전체를 버퍼 뒤에 붙인다. */
Err sb_add(StrBuf *sb, const char *txt) {
    return sb_addn(sb, txt, strlen(txt));
}

/* 요약: 서식 문자열 결과를 버퍼 뒤에 붙인다. */
Err sb_addf(StrBuf *sb, const char *fmt, ...) {
    va_list ap;
    va_list cp;
    int need;
    Err err;

    va_start(ap, fmt);
    va_copy(cp, ap);
    need = vsnprintf(NULL, 0, fmt, cp);
    va_end(cp);
    if (need < 0) {
        va_end(ap);
        return ERR_IO;
    }
    err = sb_grow(sb, (size_t)need);
    if (err != ERR_OK) {
        va_end(ap);
        return err;
    }
    vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, ap);
    va_end(ap);
    sb->len += (size_t)need;
    return ERR_OK;
}

/* 요약: 에러 버퍼에 사람이 읽을 메시지를 쓴다. */
void err_set(char *buf, size_t cap, const char *fmt, ...) {
    va_list ap;

    if (buf == NULL || cap == 0) {
        return;
    }
    va_start(ap, fmt);
    vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
}

/* 요약: 두 문자열을 대소문자 없이 비교한다. */
int str_ieq(const char *a, const char *b) {
    unsigned char ca;
    unsigned char cb;

    while (*a != '\0' && *b != '\0') {
        ca = (unsigned char)tolower((unsigned char)*a);
        cb = (unsigned char)tolower((unsigned char)*b);
        if (ca != cb) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

/* 요약: 문자열 앞뒤 공백을 제자리에서 지운다. */
void trim_in(char *txt) {
    size_t len;
    size_t a;
    size_t b;

    len = strlen(txt);
    a = 0;
    while (a < len && isspace((unsigned char)txt[a])) {
        ++a;
    }
    b = len;
    while (b > a && isspace((unsigned char)txt[b - 1])) {
        --b;
    }
    if (a > 0) {
        memmove(txt, txt + a, b - a);
    }
    txt[b - a] = '\0';
}

/* 요약: C 문자열 하나를 새 메모리로 복사한다. */
char *dup_txt(const char *txt) {
    size_t len;
    char *out;

    len = strlen(txt);
    out = (char *)malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, txt, len + 1);
    return out;
}

/* 요약: 문자열의 일부 구간을 새 메모리로 복사한다. */
char *dup_rng(const char *txt, size_t len) {
    char *out;

    out = (char *)malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, txt, len);
    out[len] = '\0';
    return out;
}

/* 요약: 줄바꿈 전까지 한 줄 입력을 읽는다. */
char *read_line(FILE *fp) {
    int ch;
    StrBuf sb;
    char one;

    sb_init(&sb);
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            break;
        }
        one = (char)ch;
        if (sb_addn(&sb, &one, 1) != ERR_OK) {
            sb_free(&sb);
            return NULL;
        }
    }
    if (ch == EOF && sb.len == 0) {
        sb_free(&sb);
        return NULL;
    }
    if (sb.buf == NULL) {
        return dup_txt("");
    }
    return sb.buf;
}

/* 요약: 문장 목록이 잡고 있는 메모리를 모두 정리한다. */
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

/* 요약: 토큰 목록 메모리를 정리한다. */
void free_toks(TokList *lst) {
    free(lst->list);
    lst->list = NULL;
    lst->len = 0;
    lst->cap = 0;
}

/* 요약: 조회 결과 집합 메모리를 정리한다. */
void free_rows(RowSet *set) {
    free(set->list);
    set->list = NULL;
    set->len = 0;
    set->cap = 0;
}

/* 요약: 옵션 뒤에 값이 꼭 있는지 검사한다. */
static int arg_need(int i, int argc, char *err, size_t cap, const char *name) {
    if (i + 1 >= argc) {
        err_set(err, cap, "missing value after %s", name);
        return 0;
    }
    return 1;
}

/* 요약: 길이 제한 안에서 문자열을 안전하게 복사한다. */
static void copy_str(char *dst, size_t cap, const char *src) {
    if (cap == 0) {
        return;
    }
    snprintf(dst, cap, "%s", src);
}

/* 요약: 명령줄 인자를 읽어 실행 옵션 구조체를 채운다. */
Err parse_args(int argc, char **argv, Opts *opt, char *err, size_t cap) {
    int i;

    memset(opt, 0, sizeof(*opt));
    copy_str(opt->data, sizeof(opt->data), "data/books.bin");
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            opt->help = 1;
        } else if (strcmp(argv[i], "--summary-only") == 0) {
            opt->sum_only = 1;
        } else if (strcmp(argv[i], "--mode") == 0) {
            if (!arg_need(i, argc, err, cap, "--mode")) {
                return ERR_ARG;
            }
            ++i;
            if (strcmp(argv[i], "cli") == 0) {
                opt->mode = SRC_CLI;
            } else if (strcmp(argv[i], "file") == 0) {
                opt->mode = SRC_FILE;
            } else {
                err_set(err, cap, "bad mode: %s", argv[i]);
                return ERR_ARG;
            }
        } else if (strcmp(argv[i], "--batch") == 0) {
            if (!arg_need(i, argc, err, cap, "--batch")) {
                return ERR_ARG;
            }
            ++i;
            copy_str(opt->batch, sizeof(opt->batch), argv[i]);
        } else if (strcmp(argv[i], "--file") == 0) {
            if (!arg_need(i, argc, err, cap, "--file")) {
                return ERR_ARG;
            }
            ++i;
            copy_str(opt->file, sizeof(opt->file), argv[i]);
        } else if (strcmp(argv[i], "--data") == 0) {
            if (!arg_need(i, argc, err, cap, "--data")) {
                return ERR_ARG;
            }
            ++i;
            copy_str(opt->data, sizeof(opt->data), argv[i]);
        } else {
            err_set(err, cap, "unknown arg: %s", argv[i]);
            return ERR_ARG;
        }
    }
    return ERR_OK;
}

/* 요약: 실행 가능한 옵션 목록을 화면에 보여준다. */
void print_help(void) {
    puts("sql2_books");
    puts("  --mode cli|file");
    puts("  --batch \"SELECT * FROM books;\"");
    puts("  --file data/input.sql");
    puts("  --data data/books.bin");
    puts("  --summary-only");
    puts("  --help");
}
