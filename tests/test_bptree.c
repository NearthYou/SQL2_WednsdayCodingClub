#include "bptree.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

static int test_insert_find_and_validate(void) {
    BPTree *tree = bptree_create(4);
    CHECK(tree != NULL, "tree should be created");

    for (int64_t id = 1; id <= 100; id++) {
        CHECK(bptree_insert(tree, id, (size_t)(id - 1)), "insert should succeed");
    }
    CHECK(!bptree_insert(tree, 50, 999), "duplicate key should be rejected");

    for (int64_t id = 1; id <= 100; id++) {
        size_t row_index = 0;
        CHECK(bptree_find(tree, id, &row_index), "inserted id should be found");
        CHECK(row_index == (size_t)(id - 1), "row index should match inserted value");
    }

    BPTreeValidationReport report;
    CHECK(bptree_validate(tree, &report), report.message);
    CHECK(bptree_height(tree) > 1, "many inserts should split the root");
    CHECK(bptree_key_count(tree) == 100, "tree should track key count");

    bptree_free(tree);
    return 0;
}

static int test_reverse_insert_and_range(void) {
    BPTree *tree = bptree_create(5);
    CHECK(tree != NULL, "tree should be created");

    for (int64_t id = 40; id >= 1; id--) {
        CHECK(bptree_insert(tree, id, (size_t)(id * 10)), "reverse insert should succeed");
    }

    RowIndexList rows;
    row_index_list_init(&rows);
    CHECK(bptree_find_range(tree, 10, 15, &rows), "range search should succeed");
    CHECK(rows.count == 6, "range should include both endpoints");
    for (size_t i = 0; i < rows.count; i++) {
        CHECK(rows.items[i] == (size_t)((10 + (int64_t)i) * 10), "range rows should be ordered by id");
    }
    row_index_list_free(&rows);

    BPTreeValidationReport report;
    CHECK(bptree_validate(tree, &report), report.message);
    bptree_free(tree);
    return 0;
}

int main(void) {
    if (test_insert_find_and_validate() != 0) {
        return 1;
    }
    if (test_reverse_insert_and_range() != 0) {
        return 1;
    }
    printf("test_bptree: ok\n");
    return 0;
}
