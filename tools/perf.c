#include "bptree.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_seconds(void) {
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

int main(int argc, char **argv) {
    size_t record_count = 1000000;
    if (argc > 1) {
        long parsed = strtol(argv[1], NULL, 10);
        if (parsed > 0) {
            record_count = (size_t)parsed;
        }
    }

    SqlError error;
    error_clear(&error);

    Database db;
    if (!database_init(&db, 128, &error)) {
        fprintf(stderr, "perf setup failed: %s\n", error.message);
        return 1;
    }

    double start = now_seconds();
    char name[64];
    for (size_t i = 0; i < record_count; i++) {
        snprintf(name, sizeof(name), "name%zu", i);
        if (!table_insert(&db.records, name, (int64_t)(i % 1000), &error)) {
            fprintf(stderr, "insert failed: %s\n", error.message);
            database_free(&db);
            return 1;
        }
    }
    double insert_seconds = now_seconds() - start;

    volatile size_t sink = 0;
    size_t row_index = 0;
    int64_t target_id = (int64_t)(record_count / 2);

    start = now_seconds();
    for (size_t i = 0; i < 10000; i++) {
        if (bptree_find(db.records.index, target_id, &row_index)) {
            sink += row_index;
        }
    }
    double id_lookup_seconds = now_seconds() - start;

    start = now_seconds();
    for (size_t i = 0; i < 1000; i++) {
        RowIndexList rows;
        row_index_list_init(&rows);
        bptree_find_range(db.records.index, target_id, target_id + 999, &rows);
        sink += rows.count;
        row_index_list_free(&rows);
    }
    double range_seconds = now_seconds() - start;

    start = now_seconds();
    for (size_t repeat = 0; repeat < 10; repeat++) {
        for (size_t i = 0; i < db.records.row_count; i++) {
            if (db.records.rows[i].value == 777) {
                sink += i;
            }
        }
    }
    double linear_seconds = now_seconds() - start;

    printf("records: %zu\n", record_count);
    printf("insert_total_seconds: %.6f\n", insert_seconds);
    printf("id_lookup_10000x_seconds: %.6f\n", id_lookup_seconds);
    printf("id_range_1000x_seconds: %.6f\n", range_seconds);
    printf("value_linear_scan_10x_seconds: %.6f\n", linear_seconds);
    printf("single_lookup_vs_linear_ratio: %.2fx\n", linear_seconds / (id_lookup_seconds == 0.0 ? 0.000001 : id_lookup_seconds));
    printf("range_vs_linear_ratio: %.2fx\n", linear_seconds / (range_seconds == 0.0 ? 0.000001 : range_seconds));
    printf("ignore: %zu\n", sink);

    database_free(&db);
    return 0;
}
