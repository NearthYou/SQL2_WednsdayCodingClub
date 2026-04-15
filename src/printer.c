#include "printer.h"

#include <stdio.h>
#include <string.h>

static size_t digits_width_int64(int64_t value) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%lld", (long long)value);
    return strlen(buffer);
}

static void print_border(FILE *out, size_t id_w, size_t name_w, size_t value_w) {
    fprintf(out, "+");
    for (size_t i = 0; i < id_w + 2; i++) {
        fputc('-', out);
    }
    fprintf(out, "+");
    for (size_t i = 0; i < name_w + 2; i++) {
        fputc('-', out);
    }
    fprintf(out, "+");
    for (size_t i = 0; i < value_w + 2; i++) {
        fputc('-', out);
    }
    fprintf(out, "+\n");
}

void printer_print_rows(FILE *out, const Table *table, const RowIndexList *rows) {
    size_t id_w = strlen("id");
    size_t name_w = strlen("name");
    size_t value_w = strlen("value");

    for (size_t i = 0; i < rows->count; i++) {
        const Record *record = &table->rows[rows->items[i]];
        size_t id_len = digits_width_int64(record->id);
        size_t name_len = strlen(record->name);
        size_t value_len = digits_width_int64(record->value);
        if (id_len > id_w) {
            id_w = id_len;
        }
        if (name_len > name_w) {
            name_w = name_len;
        }
        if (value_len > value_w) {
            value_w = value_len;
        }
    }

    print_border(out, id_w, name_w, value_w);
    fprintf(out, "| %-*s | %-*s | %-*s |\n", (int)id_w, "id", (int)name_w, "name", (int)value_w, "value");
    print_border(out, id_w, name_w, value_w);

    for (size_t i = 0; i < rows->count; i++) {
        const Record *record = &table->rows[rows->items[i]];
        fprintf(out, "| %-*lld | %-*s | %-*lld |\n",
                (int)id_w, (long long)record->id,
                (int)name_w, record->name,
                (int)value_w, (long long)record->value);
    }

    print_border(out, id_w, name_w, value_w);
    fprintf(out, "%zu row(s)\n", rows->count);
}

void printer_print_error(FILE *out, const SqlError *error) {
    fprintf(out, "ERROR [%s] statement %zu: %s\n",
            error_phase_name(error->phase),
            error->statement_index,
            error->message);
}

void printer_print_transaction_rolled_back(FILE *out) {
    fprintf(out, "TRANSACTION ROLLED BACK\n");
}

void printer_print_transaction_committed(FILE *out) {
    fprintf(out, "TRANSACTION COMMITTED\n");
}
