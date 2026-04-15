#include "db.h"
#include "executor.h"
#include "input.h"
#include "parser.h"
#include "printer.h"
#include "storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_DB_PATH "data/default.msqldb"
#define DEFAULT_ORDER 128

static size_t parse_order(int argc, char **argv) {
    for (int i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], "--order") == 0) {
            long value = strtol(argv[i + 1], NULL, 10);
            if (value >= 3) {
                return (size_t)value;
            }
        }
    }
    return DEFAULT_ORDER;
}

static bool read_sql_from_user(char **out_sql, SqlError *error) {
    char line[4096];

    printf("SQL input mode (1=quoted CLI, 2=batch file): ");
    if (!input_read_line(stdin, line, sizeof(line))) {
        error_set(error, ERROR_INPUT, 0, "failed to read input mode");
        return false;
    }

    if (strcmp(line, "1") == 0) {
        printf("SQL batch in double quotes: ");
        if (!input_read_line(stdin, line, sizeof(line))) {
            error_set(error, ERROR_INPUT, 0, "failed to read CLI SQL");
            return false;
        }
        return input_unquote_cli_sql(line, out_sql, error);
    }

    if (strcmp(line, "2") == 0) {
        printf("SQL batch file path: ");
        if (!input_read_line(stdin, line, sizeof(line))) {
            error_set(error, ERROR_INPUT, 0, "failed to read SQL file path");
            return false;
        }
        return input_read_text_file(line, out_sql, error);
    }

    error_set(error, ERROR_INPUT, 0, "input mode must be 1 or 2");
    return false;
}

int main(int argc, char **argv) {
    size_t order = parse_order(argc, argv);
    SqlError error;
    error_clear(&error);

    char db_path[1024];
    printf("DB path [data/default.msqldb]: ");
    if (!input_read_line(stdin, db_path, sizeof(db_path))) {
        error_set(&error, ERROR_INPUT, 0, "failed to read DB path");
        printer_print_error(stderr, &error);
        return 1;
    }
    if (db_path[0] == '\0') {
        snprintf(db_path, sizeof(db_path), "%s", DEFAULT_DB_PATH);
    }

    Database db;
    if (!storage_load(db_path, &db, order, &error)) {
        printer_print_error(stderr, &error);
        return 1;
    }

    char *sql = NULL;
    if (!read_sql_from_user(&sql, &error)) {
        printer_print_error(stderr, &error);
        database_free(&db);
        return 1;
    }

    QueryBatch batch;
    if (!parse_batch(sql, &batch, &error)) {
        printer_print_error(stderr, &error);
        free(sql);
        database_free(&db);
        return 1;
    }

    bool ok = execute_batch(&db, &batch, stdout, &error);
    if (ok) {
        ok = storage_save(db_path, &db, &error);
    }

    if (ok) {
        printer_print_transaction_committed(stdout);
    } else {
        printer_print_error(stderr, &error);
        printer_print_transaction_rolled_back(stderr);
    }

    query_batch_free(&batch);
    free(sql);
    database_free(&db);
    return ok ? 0 : 1;
}
