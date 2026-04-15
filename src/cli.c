/* This file parses CLI options and prints CLI help text. */
#include "cli.h"

#include "util.h"

#include <string.h>

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
