/* CLI option parsing and help text. */
#ifndef CLI_H
#define CLI_H

#include "base.h"

typedef enum {
    SRC_NONE = 0,
    SRC_CLI,
    SRC_FILE
} SrcMode;

typedef struct {
    SrcMode mode;
    char batch[BATCH_LEN];
    char file[PATH_LEN];
    char data[PATH_LEN];
    int help;
    int sum_only;
} Opts;

Err parse_args(int argc, char **argv, Opts *opt, char *err, size_t cap);
void print_help(void);

#endif
