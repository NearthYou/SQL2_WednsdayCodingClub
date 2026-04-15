/* B+ tree interface used for id lookups. */
#ifndef BPT_H
#define BPT_H

#include "base.h"

typedef struct BpNode BpNode;

typedef struct {
    BpNode *root;
} BpTree;

void bp_init(BpTree *tree);
void bp_free(BpTree *tree);
Err bp_put(BpTree *tree, int key, int val);
int bp_get(const BpTree *tree, int key, int *val);

#endif
