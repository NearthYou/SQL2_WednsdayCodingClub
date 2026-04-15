/* This file implements a small B+ tree used only for id lookups. */
#include "sql2.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int has;
    int key;
    BpNode *right;
} Split;

static BpNode *node_new(int leaf) {
    BpNode *node;

    node = (BpNode *)calloc(1, sizeof(*node));
    if (node == NULL) {
        return NULL;
    }
    node->leaf = leaf;
    return node;
}

static void node_free(BpNode *node) {
    int i;

    if (node == NULL) {
        return;
    }
    if (!node->leaf) {
        for (i = 0; i <= node->nkey; ++i) {
            node_free(node->kid[i]);
        }
    }
    free(node);
}

static int find_pos(BpNode *node, int key) {
    int at;

    at = 0;
    while (at < node->nkey && key >= node->keys[at]) {
        ++at;
    }
    return at;
}

static Split put_rec(BpNode *node, int key, int val, Err *err) {
    Split sp;
    int at;
    int i;

    memset(&sp, 0, sizeof(sp));
    if (node->leaf) {
        int keys[BP_ORDER];
        int vals[BP_ORDER];
        int total;

        at = 0;
        while (at < node->nkey && node->keys[at] < key) {
            ++at;
        }
        if (at < node->nkey && node->keys[at] == key) {
            node->vals[at] = val;
            return sp;
        }
        total = node->nkey + 1;
        for (i = 0; i < at; ++i) {
            keys[i] = node->keys[i];
            vals[i] = node->vals[i];
        }
        keys[at] = key;
        vals[at] = val;
        for (i = at; i < node->nkey; ++i) {
            keys[i + 1] = node->keys[i];
            vals[i + 1] = node->vals[i];
        }

        if (total <= BP_MAX) {
            node->nkey = total;
            for (i = 0; i < total; ++i) {
                node->keys[i] = keys[i];
                node->vals[i] = vals[i];
            }
            return sp;
        }

        sp.right = node_new(1);
        if (sp.right == NULL) {
            *err = ERR_MEM;
            return sp;
        }
        at = total / 2;
        node->nkey = at;
        sp.right->nkey = total - at;
        for (i = 0; i < node->nkey; ++i) {
            node->keys[i] = keys[i];
            node->vals[i] = vals[i];
        }
        for (i = 0; i < sp.right->nkey; ++i) {
            sp.right->keys[i] = keys[at + i];
            sp.right->vals[i] = vals[at + i];
        }
        sp.right->next = node->next;
        node->next = sp.right;
        sp.has = 1;
        sp.key = sp.right->keys[0];
        return sp;
    }

    at = find_pos(node, key);
    sp = put_rec(node->kid[at], key, val, err);
    if (*err != ERR_OK || !sp.has) {
        return sp;
    }
    {
        int keys[BP_ORDER];
        BpNode *kid[BP_ORDER + 1];
        int total;
        Split out;

        total = node->nkey + 1;
        memset(&out, 0, sizeof(out));
        for (i = 0; i < at; ++i) {
            keys[i] = node->keys[i];
        }
        keys[at] = sp.key;
        for (i = at; i < node->nkey; ++i) {
            keys[i + 1] = node->keys[i];
        }
        for (i = 0; i <= at; ++i) {
            kid[i] = node->kid[i];
        }
        kid[at + 1] = sp.right;
        for (i = at + 1; i <= node->nkey; ++i) {
            kid[i + 1] = node->kid[i];
        }

        if (total <= BP_MAX) {
            node->nkey = total;
            for (i = 0; i < total; ++i) {
                node->keys[i] = keys[i];
            }
            for (i = 0; i <= total; ++i) {
                node->kid[i] = kid[i];
            }
            sp.has = 0;
            return sp;
        }

        out.right = node_new(0);
        if (out.right == NULL) {
            *err = ERR_MEM;
            sp.has = 0;
            return sp;
        }
        at = total / 2;
        out.key = keys[at];
        out.has = 1;
        node->nkey = at;
        for (i = 0; i < at; ++i) {
            node->keys[i] = keys[i];
        }
        for (i = 0; i <= at; ++i) {
            node->kid[i] = kid[i];
        }
        out.right->nkey = total - at - 1;
        for (i = 0; i < out.right->nkey; ++i) {
            out.right->keys[i] = keys[at + 1 + i];
        }
        for (i = 0; i <= out.right->nkey; ++i) {
            out.right->kid[i] = kid[at + 1 + i];
        }
        return out;
    }
}

void bp_init(BpTree *tree) {
    tree->root = NULL;
}

void bp_free(BpTree *tree) {
    node_free(tree->root);
    tree->root = NULL;
}

Err bp_put(BpTree *tree, int key, int val) {
    Split sp;
    Err err;
    BpNode *root;

    err = ERR_OK;
    if (tree->root == NULL) {
        tree->root = node_new(1);
        if (tree->root == NULL) {
            return ERR_MEM;
        }
        tree->root->nkey = 1;
        tree->root->keys[0] = key;
        tree->root->vals[0] = val;
        return ERR_OK;
    }

    sp = put_rec(tree->root, key, val, &err);
    if (err != ERR_OK) {
        return err;
    }
    if (!sp.has) {
        return ERR_OK;
    }

    root = node_new(0);
    if (root == NULL) {
        return ERR_MEM;
    }
    root->nkey = 1;
    root->keys[0] = sp.key;
    root->kid[0] = tree->root;
    root->kid[1] = sp.right;
    tree->root = root;
    return ERR_OK;
}

int bp_get(const BpTree *tree, int key, int *val) {
    BpNode *node;
    int at;

    node = tree->root;
    while (node != NULL) {
        if (node->leaf) {
            for (at = 0; at < node->nkey; ++at) {
                if (node->keys[at] == key) {
                    *val = node->vals[at];
                    return 1;
                }
            }
            return 0;
        }
        at = 0;
        while (at < node->nkey && key >= node->keys[at]) {
            ++at;
        }
        node = node->kid[at];
    }
    return 0;
}

