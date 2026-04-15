#include "db.h"
#include "executor.h"
#include "parser.h"

#include <stdio.h>

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

static int run_batch(Database *db, const char *sql, bool expect_ok) {
    QueryBatch batch;
    SqlError error;
    error_clear(&error);
    CHECK(parse_batch(sql, &batch, &error), error.message);

    FILE *out = tmpfile();
    if (!out) {
        out = stdout;
    }
    bool ok = execute_batch(db, &batch, out, &error);
    if (out != stdout) {
        fclose(out);
    }
    query_batch_free(&batch);

    if (expect_ok) {
        CHECK(ok, error.message);
    } else {
        CHECK(!ok, "batch should fail");
    }
    return 0;
}

int main(void) {
    SqlError error;
    error_clear(&error);

    Database db;
    CHECK(database_init(&db, 4, &error), error.message);

    CHECK(run_batch(&db, "INSERT INTO records VALUES ('alice', 10); INSERT INTO records VALUES ('bob', 20);", true) == 0, "insert batch should pass");
    CHECK(db.records.row_count == 2, "two rows should be inserted");
    CHECK(db.records.next_id == 3, "next_id should advance");

    size_t row_index = 0;
    CHECK(bptree_find(db.records.index, 2, &row_index), "id 2 should be indexed");
    CHECK(row_index == 1, "id 2 should point to second row");

    CHECK(run_batch(&db, "INSERT INTO records VALUES ('carol', 30); SELECT * FROM records WHERE id BETWEEN 9 AND 1;", false) == 0, "bad range should roll back");
    CHECK(db.records.row_count == 2, "failed batch should rollback row_count");
    CHECK(db.records.next_id == 3, "failed batch should rollback next_id");
    CHECK(!bptree_find(db.records.index, 3, &row_index), "rolled back id should not be indexed");

    CHECK(run_batch(&db, "SELECT * FROM records; SELECT * FROM records WHERE id = 1; SELECT * FROM records WHERE value = 20;", true) == 0, "select batch should pass");

    database_free(&db);
    printf("test_executor: ok\n");
    return 0;
}
