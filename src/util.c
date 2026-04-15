#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
/* This file holds small shared helpers used across the project. */
#include "util.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/* 요약: 문자열 버퍼가 더 자랄 수 있게 공간을 미리 늘린다. */
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

/* 요약: 에러 버퍼에 사람이 읽을 메시지를 만든다. */
void err_set(char *buf, size_t cap, const char *fmt, ...) {
    va_list ap;

    if (buf == NULL || cap == 0) {
        return;
    }
    va_start(ap, fmt);
    vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
}

/* 요약: 두 문자열을 대소문자 구분 없이 비교한다. */
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

/* 요약: 문자열의 일부 구간만 새 메모리로 복사한다. */
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
