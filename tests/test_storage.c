#include "db.h"
#include "storage.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

int main(void) {
    const char *path = "build/test_storage.msqldb";
    SqlError error;
    error_clear(&error);

    Database db;
    CHECK(database_init(&db, 4, &error), error.message);
    CHECK(table_insert(&db.records, "alice", 10, &error), error.message);
    CHECK(table_insert(&db.records, "bob", 20, &error), error.message);
    CHECK(storage_save(path, &db, &error), error.message);
    database_free(&db);

    Database loaded;
    error_clear(&error);
    CHECK(storage_load(path, &loaded, 4, &error), error.message);
    CHECK(loaded.records.row_count == 2, "loaded row count should match");
    CHECK(loaded.records.next_id == 3, "next_id should round trip");
    CHECK(strcmp(loaded.records.rows[0].name, "alice") == 0, "first row name should round trip");

    size_t row_index = 0;
    CHECK(bptree_find(loaded.records.index, 2, &row_index), "loaded index should find id 2");
    CHECK(row_index == 1, "loaded index should point to second row");
    database_free(&loaded);

    FILE *bad = fopen("build/corrupt.msqldb", "wb");
    CHECK(bad != NULL, "corrupt fixture should open");
    fputs("bad", bad);
    fclose(bad);

    Database corrupt;
    error_clear(&error);
    CHECK(!storage_load("build/corrupt.msqldb", &corrupt, 4, &error), "corrupt DB should fail");
    CHECK(error.phase == ERROR_STORAGE, "corrupt DB should report storage error");

    printf("test_storage: ok\n");
    return 0;
}
