#ifndef MINISQL_ERROR_H
#define MINISQL_ERROR_H

#include <stddef.h>

typedef enum {
    ERROR_NONE = 0,
    ERROR_INPUT,
    ERROR_PARSE,
    ERROR_STORAGE,
    ERROR_EXECUTE,
    ERROR_TRANSACTION,
    ERROR_SAVE
} ErrorPhase;

typedef struct {
    ErrorPhase phase;
    size_t statement_index;
    char message[256];
} SqlError;

void error_clear(SqlError *error);
void error_set(SqlError *error, ErrorPhase phase, size_t statement_index, const char *fmt, ...);
const char *error_phase_name(ErrorPhase phase);

#endif
