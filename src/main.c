/* This file is the CLI front door for the SQL demo program. */
#include "sql2.h"

#include <stdlib.h>
#include <string.h>

#ifndef NO_MAIN
static Err load_default(char **sql, char *err, size_t cap) {
    return load_default_sql(sql, err, cap);
}

static Err ask_cli(char **sql, char *err, size_t cap) {
    char *line;
    size_t len;

    puts("Enter one quoted SQL batch:");
    line = read_line(stdin);
    if (line == NULL) {
        err_set(err, cap, "failed to read input");
        return ERR_INPUT;
    }
    trim_in(line);
    len = strlen(line);
    if (len < 2 || line[0] != '"' || line[len - 1] != '"') {
        free(line);
        err_set(err, cap, "CLI input must be wrapped in double quotes");
        return ERR_INPUT;
    }
    line[len - 1] = '\0';
    *sql = dup_txt(line + 1);
    free(line);
    if (*sql == NULL) {
        err_set(err, cap, "out of memory while copying input");
        return ERR_MEM;
    }
    return ERR_OK;
}

static Err ask_file(char **sql, char *err, size_t cap) {
    char *line;
    Err res;

    puts("Enter SQL file path or press Enter for defaults:");
    line = read_line(stdin);
    if (line == NULL) {
        err_set(err, cap, "failed to read file path");
        return ERR_INPUT;
    }
    trim_in(line);
    if (line[0] == '\0') {
        free(line);
        return load_default(sql, err, cap);
    }
    res = load_sql_file(line, sql, err, cap);
    free(line);
    return res;
}

static Err pick_mode(Opts *opt, char *err, size_t cap) {
    char *line;

    puts("Select input mode:");
    puts("1 = CLI direct input");
    puts("2 = File input");
    line = read_line(stdin);
    if (line == NULL) {
        err_set(err, cap, "failed to read mode");
        return ERR_INPUT;
    }
    trim_in(line);
    if (strcmp(line, "1") == 0) {
        opt->mode = SRC_CLI;
    } else if (strcmp(line, "2") == 0) {
        opt->mode = SRC_FILE;
    } else {
        free(line);
        err_set(err, cap, "mode must be 1 or 2");
        return ERR_INPUT;
    }
    free(line);
    return ERR_OK;
}

static Err get_sql(const Opts *opt, char **sql, char *err, size_t cap) {
    if (opt->mode == SRC_CLI) {
        if (opt->batch[0] != '\0') {
            *sql = dup_txt(opt->batch);
            if (*sql == NULL) {
                err_set(err, cap, "out of memory while copying batch");
                return ERR_MEM;
            }
            return ERR_OK;
        }
        return ask_cli(sql, err, cap);
    }
    if (opt->file[0] != '\0') {
        return load_sql_file(opt->file, sql, err, cap);
    }
    return ask_file(sql, err, cap);
}

int main(int argc, char **argv) {
    Opts opt;
    Db db;
    StrBuf out;
    char err[256];
    char *sql;
    Err res;

    sql = NULL;
    err[0] = '\0';
    res = parse_args(argc, argv, &opt, err, sizeof(err));
    if (res != ERR_OK) {
        fprintf(stderr, "[ERR] %s\n", err);
        return 1;
    }
    if (opt.help) {
        print_help();
        return 0;
    }
    if (opt.mode == SRC_NONE) {
        res = pick_mode(&opt, err, sizeof(err));
        if (res != ERR_OK) {
            fprintf(stderr, "[ERR] %s\n", err);
            return 1;
        }
    }

    db_init(&db);
    res = db_set_path(&db, opt.data, err, sizeof(err));
    if (res != ERR_OK) {
        fprintf(stderr, "[ERR] %s\n", err);
        db_free(&db);
        return 1;
    }
    res = db_load(&db, err, sizeof(err));
    if (res != ERR_OK) {
        fprintf(stderr, "[ERR] %s\n", err);
        db_free(&db);
        return 1;
    }
    res = get_sql(&opt, &sql, err, sizeof(err));
    if (res != ERR_OK) {
        fprintf(stderr, "[ERR] %s\n", err);
        db_free(&db);
        return 1;
    }

    sb_init(&out);
    res = run_batch(&db, sql, &out, err, sizeof(err));
    free(sql);
    if (res != ERR_OK) {
        fprintf(stderr, "[ERR] %s\n", err);
        sb_free(&out);
        db_free(&db);
        return 1;
    }
    printf("%s", out.buf == NULL ? "" : out.buf);
    sb_free(&out);
    db_free(&db);
    return 0;
}
#endif
