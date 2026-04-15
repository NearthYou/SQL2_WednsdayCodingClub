/* Shared helper types and string/time utilities. */
#ifndef UTIL_H
#define UTIL_H

#include "base.h"

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} StrBuf;

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

#endif
