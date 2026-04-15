#include "parser.h"

#include <stdio.h>

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

static int test_valid_batch(void) {
    QueryBatch batch;
    SqlError error;
    error_clear(&error);

    const char *sql =
        "SELECT * FROM records;"
        "SELECT * FROM records WHERE id = 7;"
        "SELECT * FROM records WHERE id BETWEEN 3 AND 9;"
        "SELECT * FROM records WHERE value = 10;"
        "INSERT INTO records VALUES ('alice', 10);";

    CHECK(parse_batch(sql, &batch, &error), error.message);
    CHECK(batch.count == 5, "batch should contain five queries");
    CHECK(batch.items[0].type == QUERY_SELECT_ALL, "first query should be SELECT all");
    CHECK(batch.items[1].type == QUERY_SELECT_ID_EQ && batch.items[1].id == 7, "id equality should parse");
    CHECK(batch.items[2].type == QUERY_SELECT_ID_RANGE && batch.items[2].min_id == 3 && batch.items[2].max_id == 9, "id range should parse");
    CHECK(batch.items[3].type == QUERY_SELECT_VALUE_EQ && batch.items[3].value == 10, "value equality should parse");
    CHECK(batch.items[4].type == QUERY_INSERT && batch.items[4].insert_value == 10, "insert should parse");

    query_batch_free(&batch);
    return 0;
}

static int test_invalid_inputs(void) {
    QueryBatch batch;
    SqlError error;

    error_clear(&error);
    CHECK(!parse_batch("", &batch, &error), "empty input should fail");
    CHECK(error.phase == ERROR_PARSE, "empty input should be parse error");

    error_clear(&error);
    CHECK(!parse_batch("SELECT * FROM records", &batch, &error), "missing semicolon should fail");

    error_clear(&error);
    CHECK(!parse_batch("SELECT * FROM records;;", &batch, &error), "empty statement should fail");

    error_clear(&error);
    CHECK(!parse_batch("SELECT * FROM records WHERE name = 'alice';", &batch, &error), "unsupported column should fail");

    return 0;
}

int main(void) {
    if (test_valid_batch() != 0) {
        return 1;
    }
    if (test_invalid_inputs() != 0) {
        return 1;
    }
    printf("test_parser: ok\n");
    return 0;
}
