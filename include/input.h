#ifndef MINISQL_INPUT_H
#define MINISQL_INPUT_H

#include "error.h"

#include <stdbool.h>
#include <stdio.h>

bool input_read_line(FILE *in, char *buffer, size_t buffer_size);
bool input_unquote_cli_sql(const char *line, char **out_sql, SqlError *error);
bool input_read_text_file(const char *path, char **out_sql, SqlError *error);

#endif
