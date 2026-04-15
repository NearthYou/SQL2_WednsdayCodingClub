#ifndef MINISQL_BPTREE_H
#define MINISQL_BPTREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BPTree BPTree;

typedef struct {
    size_t *items;
    size_t count;
    size_t capacity;
} RowIndexList;

typedef struct {
    bool ok;
    size_t height;
    size_t leaf_count;
    size_t key_count;
    char message[256];
} BPTreeValidationReport;

void row_index_list_init(RowIndexList *list);
bool row_index_list_append(RowIndexList *list, size_t row_index);
void row_index_list_free(RowIndexList *list);

BPTree *bptree_create(size_t order);
bool bptree_insert(BPTree *tree, int64_t id, size_t row_index);
bool bptree_find(const BPTree *tree, int64_t id, size_t *out_row_index);
bool bptree_find_range(const BPTree *tree, int64_t min_id, int64_t max_id, RowIndexList *out_rows);
bool bptree_validate(const BPTree *tree, BPTreeValidationReport *out_report);
size_t bptree_height(const BPTree *tree);
size_t bptree_key_count(const BPTree *tree);
void bptree_free(BPTree *tree);

#endif
