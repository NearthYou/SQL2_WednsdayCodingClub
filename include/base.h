/* Shared constants and error codes for the books SQL demo. */
#ifndef BASE_H
#define BASE_H

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
#define BATCH_LEN 4096

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

#endif
