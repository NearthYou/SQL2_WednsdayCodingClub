/* This file implements a small B+ tree used only for id lookups. */
#include "bpt.h"

#include "util.h"

#include <stdlib.h>
#include <string.h>

struct BpNode {
    int leaf;
    int nkey;
    int keys[BP_MAX];
    int vals[BP_MAX];
    struct BpNode *kid[BP_ORDER];
    struct BpNode *next;
};

typedef struct {
    int has;
    int key;
    BpNode *right;
} Split;

typedef struct {
    int has;
    int min_key;
    int max_key;
} KeySpan;

typedef struct {
    size_t leaf_depth;
    size_t leaf_count;
    size_t key_count;
    int have_leaf_depth;
    int have_prev_key;
    int prev_key;
    const BpNode *prev_leaf;
} CheckCtx;

/* 요약: 새 B+ 트리 노드를 0으로 초기화해 만든다. */
static BpNode *node_new(int leaf) {
    BpNode *node;

    node = (BpNode *)calloc(1, sizeof(*node));
    if (node == NULL) {
        return NULL;
    }
    node->leaf = leaf;
    return node;
}

/* 요약: 하위 노드까지 재귀로 모두 해제한다. */
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

/* 요약: 현재 노드에서 키가 들어갈 자리를 찾는다. */
static int find_pos(BpNode *node, int key) {
    int at;

    at = 0;
    while (at < node->nkey && key >= node->keys[at]) {
        ++at;
    }
    return at;
}

/* 요약: 검색 키가 들어 있는 리프 노드를 찾는다. */
static const BpNode *find_leaf(const BpTree *tree, int key) {
    const BpNode *node;
    int at;

    node = tree->root;
    while (node != NULL && !node->leaf) {
        at = 0;
        while (at < node->nkey && key >= node->keys[at]) {
            ++at;
        }
        node = node->kid[at];
    }
    return node;
}

/* 요약: 재귀로 키를 넣고 분할 결과를 부모에게 돌려준다. */
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

/* 요약: 비어 있는 B+ 트리를 준비한다. */
void bp_init(BpTree *tree) {
    tree->root = NULL;
}

/* 요약: 트리 전체 메모리를 해제한다. */
void bp_free(BpTree *tree) {
    node_free(tree->root);
    tree->root = NULL;
}

/* 요약: id 키와 행 위치를 트리에 넣는다. */
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

/* 요약: id 키로 행 위치를 찾아 돌려준다. */
int bp_get(const BpTree *tree, int key, int *val) {
    const BpNode *node;
    int at;

    node = find_leaf(tree, key);
    while (node != NULL) {
        for (at = 0; at < node->nkey; ++at) {
            if (node->keys[at] == key) {
                *val = node->vals[at];
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

/* 요약: 시작 키 이상이 들어 있는 첫 리프부터 범위를 순회한다. */
Err bp_visit_range(const BpTree *tree, int min_key, int max_key, BpVisitFn visit,
                   void *ctx) {
    const BpNode *node;
    int i;
    Err res;

    if (min_key > max_key) {
        return ERR_ARG;
    }
    if (tree == NULL || visit == NULL) {
        return ERR_ARG;
    }
    node = find_leaf(tree, min_key);
    while (node != NULL) {
        for (i = 0; i < node->nkey; ++i) {
            if (node->keys[i] < min_key) {
                continue;
            }
            if (node->keys[i] > max_key) {
                return ERR_OK;
            }
            res = visit(node->keys[i], node->vals[i], ctx);
            if (res != ERR_OK) {
                return res;
            }
        }
        node = node->next;
    }
    return ERR_OK;
}

/* 요약: 리프 하나와 리프 체인 정렬 상태를 검사한다. */
static Err check_leaf(const BpNode *node, size_t depth, CheckCtx *ctx, char *err,
                      size_t cap, KeySpan *span) {
    int i;

    if (!ctx->have_leaf_depth) {
        ctx->leaf_depth = depth;
        ctx->have_leaf_depth = 1;
    } else if (ctx->leaf_depth != depth) {
        err_set(err, cap, "leaf depth mismatch");
        return ERR_FMT;
    }
    if (ctx->prev_leaf != NULL && ctx->prev_leaf->next != node) {
        err_set(err, cap, "leaf next link is broken");
        return ERR_FMT;
    }
    for (i = 1; i < node->nkey; ++i) {
        if (node->keys[i - 1] >= node->keys[i]) {
            err_set(err, cap, "leaf keys are not strictly increasing");
            return ERR_FMT;
        }
    }
    if (node->nkey > 0) {
        if (ctx->have_prev_key && ctx->prev_key >= node->keys[0]) {
            err_set(err, cap, "leaf chain order is not strictly increasing");
            return ERR_FMT;
        }
        ctx->prev_key = node->keys[node->nkey - 1];
        ctx->have_prev_key = 1;
        span->has = 1;
        span->min_key = node->keys[0];
        span->max_key = node->keys[node->nkey - 1];
        ctx->key_count += (size_t)node->nkey;
    } else {
        span->has = 0;
    }
    ctx->leaf_count += 1;
    ctx->prev_leaf = node;
    return ERR_OK;
}

/* 요약: 내부 노드와 하위 서브트리의 키 범위를 재귀 검사한다. */
static Err check_node(const BpNode *node, size_t depth, int is_root,
                      CheckCtx *ctx, char *err, size_t cap, KeySpan *span) {
    int i;
    KeySpan first;
    KeySpan prev;
    KeySpan child;
    Err res;

    memset(span, 0, sizeof(*span));
    if (node == NULL) {
        return ERR_OK;
    }
    if (node->nkey < 0 || node->nkey > BP_MAX) {
        err_set(err, cap, "node key count is out of range");
        return ERR_FMT;
    }
    for (i = 1; i < node->nkey; ++i) {
        if (node->keys[i - 1] >= node->keys[i]) {
            err_set(err, cap, "node keys are not strictly increasing");
            return ERR_FMT;
        }
    }
    if (node->leaf) {
        return check_leaf(node, depth, ctx, err, cap, span);
    }
    if (!is_root && node->nkey == 0) {
        err_set(err, cap, "internal node must contain at least one key");
        return ERR_FMT;
    }

    memset(&first, 0, sizeof(first));
    memset(&prev, 0, sizeof(prev));
    for (i = 0; i <= node->nkey; ++i) {
        if (node->kid[i] == NULL) {
            err_set(err, cap, "internal node child is missing");
            return ERR_FMT;
        }
        res = check_node(node->kid[i], depth + 1, 0, ctx, err, cap, &child);
        if (res != ERR_OK) {
            return res;
        }
        if (!child.has) {
            err_set(err, cap, "empty subtree under internal node");
            return ERR_FMT;
        }
        if (i == 0) {
            first = child;
        } else {
            if (prev.max_key >= child.min_key) {
                err_set(err, cap, "subtree key ranges overlap");
                return ERR_FMT;
            }
            if (node->keys[i - 1] != child.min_key) {
                err_set(err, cap, "separator key does not match right subtree");
                return ERR_FMT;
            }
        }
        prev = child;
    }
    span->has = 1;
    span->min_key = first.min_key;
    span->max_key = prev.max_key;
    return ERR_OK;
}

/* 요약: B+ 트리 구조와 리프 링크를 검사하고 통계를 계산한다. */
Err bp_check(const BpTree *tree, BpStat *out, char *err, size_t cap) {
    CheckCtx ctx;
    KeySpan span;
    Err res;

    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (tree == NULL) {
        err_set(err, cap, "tree is null");
        return ERR_ARG;
    }
    if (tree->root == NULL) {
        return ERR_OK;
    }
    memset(&ctx, 0, sizeof(ctx));
    memset(&span, 0, sizeof(span));
    res = check_node(tree->root, 1, 1, &ctx, err, cap, &span);
    if (res != ERR_OK) {
        return res;
    }
    if (ctx.prev_leaf != NULL && ctx.prev_leaf->next != NULL) {
        err_set(err, cap, "last leaf next link must be null");
        return ERR_FMT;
    }
    if (out != NULL) {
        out->height = ctx.have_leaf_depth ? ctx.leaf_depth : 0;
        out->leaf_count = ctx.leaf_count;
        out->key_count = ctx.key_count;
    }
    return ERR_OK;
}
