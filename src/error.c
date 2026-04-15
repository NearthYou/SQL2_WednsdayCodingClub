#include "error.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void error_clear(SqlError *error) {
    if (!error) {
        return;
    }
    error->phase = ERROR_NONE;
    error->statement_index = 0;
    error->message[0] = '\0';
}

void error_set(SqlError *error, ErrorPhase phase, size_t statement_index, const char *fmt, ...) {
    if (!error) {
        return;
    }
    error->phase = phase;
    error->statement_index = statement_index;

    va_list args;
    va_start(args, fmt);
    vsnprintf(error->message, sizeof(error->message), fmt, args);
    va_end(args);
}

const char *error_phase_name(ErrorPhase phase) {
    switch (phase) {
        case ERROR_NONE:
            return "none";
        case ERROR_INPUT:
            return "input";
        case ERROR_PARSE:
            return "parse";
        case ERROR_STORAGE:
            return "storage";
        case ERROR_EXECUTE:
            return "execute";
        case ERROR_TRANSACTION:
            return "transaction";
        case ERROR_SAVE:
            return "save";
    }
    return "unknown";
}
