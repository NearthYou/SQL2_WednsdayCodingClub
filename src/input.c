#include "input.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool input_read_line(FILE *in, char *buffer, size_t buffer_size) {
    if (!fgets(buffer, (int)buffer_size, in)) {
        return false;
    }
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    return true;
}

static const char *skip_left_spaces(const char *p) {
    while (isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

bool input_unquote_cli_sql(const char *line, char **out_sql, SqlError *error) {
    const char *start = skip_left_spaces(line);
    size_t len = strlen(start);
    while (len > 0 && isspace((unsigned char)start[len - 1])) {
        len--;
    }

    if (len < 2 || start[0] != '"' || start[len - 1] != '"') {
        error_set(error, ERROR_INPUT, 0, "CLI SQL must be wrapped in double quotes");
        return false;
    }

    size_t inner_len = len - 2;
    char *sql = malloc(inner_len + 1);
    if (!sql) {
        error_set(error, ERROR_INPUT, 0, "failed to allocate SQL input");
        return false;
    }
    memcpy(sql, start + 1, inner_len);
    sql[inner_len] = '\0';
    *out_sql = sql;
    return true;
}

bool input_read_text_file(const char *path, char **out_sql, SqlError *error) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        error_set(error, ERROR_INPUT, 0, "failed to open SQL file '%s': %s", path, strerror(errno));
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        error_set(error, ERROR_INPUT, 0, "failed to seek SQL file");
        return false;
    }
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        error_set(error, ERROR_INPUT, 0, "failed to measure SQL file");
        return false;
    }
    rewind(file);

    char *sql = malloc((size_t)size + 1);
    if (!sql) {
        fclose(file);
        error_set(error, ERROR_INPUT, 0, "failed to allocate SQL file buffer");
        return false;
    }
    if (fread(sql, 1, (size_t)size, file) != (size_t)size) {
        free(sql);
        fclose(file);
        error_set(error, ERROR_INPUT, 0, "failed to read SQL file");
        return false;
    }
    sql[size] = '\0';
    fclose(file);
    *out_sql = sql;
    return true;
}
