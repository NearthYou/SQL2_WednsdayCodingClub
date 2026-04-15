/* B+ tree interface used for id lookups. */
#ifndef BPT_H
#define BPT_H

#include "base.h"

typedef struct BpNode BpNode;

typedef struct {
    BpNode *root;
} BpTree;

typedef struct {
    size_t height;
    size_t leaf_count;
    size_t key_count;
} BpStat;

typedef Err (*BpVisitFn)(int key, int val, void *ctx);

void bp_init(BpTree *tree);
void bp_free(BpTree *tree);
Err bp_put(BpTree *tree, int key, int val);
int bp_get(const BpTree *tree, int key, int *val);
Err bp_visit_range(const BpTree *tree, int min_key, int max_key, BpVisitFn visit,
                   void *ctx);
Err bp_check(const BpTree *tree, BpStat *out, char *err, size_t cap);

#endif
