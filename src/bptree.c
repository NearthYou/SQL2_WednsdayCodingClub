#include "bptree.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct BPTreeNode {
    bool is_leaf;
    size_t num_keys;
    int64_t *keys;
    struct BPTreeNode **children;
    size_t *record_ptrs;
    struct BPTreeNode *next;
} BPTreeNode;

struct BPTree {
    size_t order;
    size_t height;
    size_t key_count;
    BPTreeNode *root;
};

typedef struct {
    bool did_split;
    int64_t promoted_key;
    BPTreeNode *right;
} SplitResult;

static size_t max_keys(const BPTree *tree) {
    return tree->order - 1;
}

void row_index_list_init(RowIndexList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

bool row_index_list_append(RowIndexList *list, size_t row_index) {
    if (list->count == list->capacity) {
        size_t next_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        size_t *next_items = realloc(list->items, next_capacity * sizeof(size_t));
        if (!next_items) {
            return false;
        }
        list->items = next_items;
        list->capacity = next_capacity;
    }
    list->items[list->count++] = row_index;
    return true;
}

void row_index_list_free(RowIndexList *list) {
    if (!list) {
        return;
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static BPTreeNode *node_create(size_t order, bool is_leaf) {
    BPTreeNode *node = calloc(1, sizeof(BPTreeNode));
    if (!node) {
        return NULL;
    }

    node->is_leaf = is_leaf;
    node->keys = calloc(order, sizeof(int64_t));
    if (!node->keys) {
        free(node);
        return NULL;
    }

    if (is_leaf) {
        node->record_ptrs = calloc(order, sizeof(size_t));
        if (!node->record_ptrs) {
            free(node->keys);
            free(node);
            return NULL;
        }
    } else {
        node->children = calloc(order + 1, sizeof(BPTreeNode *));
        if (!node->children) {
            free(node->keys);
            free(node);
            return NULL;
        }
    }

    return node;
}

BPTree *bptree_create(size_t order) {
    if (order < 3) {
        order = 3;
    }

    BPTree *tree = calloc(1, sizeof(BPTree));
    if (!tree) {
        return NULL;
    }

    tree->order = order;
    tree->height = 1;
    tree->root = node_create(order, true);
    if (!tree->root) {
        free(tree);
        return NULL;
    }
    return tree;
}

static void node_free(BPTreeNode *node) {
    if (!node) {
        return;
    }
    if (!node->is_leaf) {
        for (size_t i = 0; i <= node->num_keys; i++) {
            node_free(node->children[i]);
        }
    }
    free(node->keys);
    free(node->children);
    free(node->record_ptrs);
    free(node);
}

void bptree_free(BPTree *tree) {
    if (!tree) {
        return;
    }
    node_free(tree->root);
    free(tree);
}

static size_t lower_bound(const int64_t *keys, size_t count, int64_t key) {
    size_t pos = 0;
    while (pos < count && keys[pos] < key) {
        pos++;
    }
    return pos;
}

static size_t child_index_for(const BPTreeNode *node, int64_t key) {
    size_t pos = 0;
    while (pos < node->num_keys && key >= node->keys[pos]) {
        pos++;
    }
    return pos;
}

static bool leaf_insert_sorted(BPTreeNode *leaf, int64_t key, size_t row_index) {
    size_t pos = lower_bound(leaf->keys, leaf->num_keys, key);
    if (pos < leaf->num_keys && leaf->keys[pos] == key) {
        return false;
    }

    for (size_t i = leaf->num_keys; i > pos; i--) {
        leaf->keys[i] = leaf->keys[i - 1];
        leaf->record_ptrs[i] = leaf->record_ptrs[i - 1];
    }
    leaf->keys[pos] = key;
    leaf->record_ptrs[pos] = row_index;
    leaf->num_keys++;
    return true;
}

static SplitResult split_leaf(BPTree *tree, BPTreeNode *leaf) {
    SplitResult result = {0};
    BPTreeNode *right = node_create(tree->order, true);
    if (!right) {
        return result;
    }

    size_t split_at = (leaf->num_keys + 1) / 2;
    size_t right_count = leaf->num_keys - split_at;

    for (size_t i = 0; i < right_count; i++) {
        right->keys[i] = leaf->keys[split_at + i];
        right->record_ptrs[i] = leaf->record_ptrs[split_at + i];
    }
    right->num_keys = right_count;
    leaf->num_keys = split_at;

    right->next = leaf->next;
    leaf->next = right;

    result.did_split = true;
    result.promoted_key = right->keys[0];
    result.right = right;
    return result;
}

static bool internal_insert_child(BPTreeNode *node, size_t pos, int64_t key, BPTreeNode *right_child) {
    for (size_t i = node->num_keys; i > pos; i--) {
        node->keys[i] = node->keys[i - 1];
    }
    for (size_t i = node->num_keys + 1; i > pos + 1; i--) {
        node->children[i] = node->children[i - 1];
    }

    node->keys[pos] = key;
    node->children[pos + 1] = right_child;
    node->num_keys++;
    return true;
}

static SplitResult split_internal(BPTree *tree, BPTreeNode *node) {
    SplitResult result = {0};
    BPTreeNode *right = node_create(tree->order, false);
    if (!right) {
        return result;
    }

    size_t promote_index = node->num_keys / 2;
    int64_t promoted_key = node->keys[promote_index];
    size_t right_key_count = node->num_keys - promote_index - 1;

    for (size_t i = 0; i < right_key_count; i++) {
        right->keys[i] = node->keys[promote_index + 1 + i];
    }
    for (size_t i = 0; i <= right_key_count; i++) {
        right->children[i] = node->children[promote_index + 1 + i];
        node->children[promote_index + 1 + i] = NULL;
    }

    right->num_keys = right_key_count;
    node->num_keys = promote_index;

    result.did_split = true;
    result.promoted_key = promoted_key;
    result.right = right;
    return result;
}

static SplitResult insert_recursive(BPTree *tree, BPTreeNode *node, int64_t key, size_t row_index, bool *inserted) {
    SplitResult result = {0};

    if (node->is_leaf) {
        if (!leaf_insert_sorted(node, key, row_index)) {
            *inserted = false;
            return result;
        }
        *inserted = true;
        if (node->num_keys > max_keys(tree)) {
            return split_leaf(tree, node);
        }
        return result;
    }

    size_t child_pos = child_index_for(node, key);
    SplitResult child_split = insert_recursive(tree, node->children[child_pos], key, row_index, inserted);
    if (!*inserted || !child_split.did_split) {
        return result;
    }

    internal_insert_child(node, child_pos, child_split.promoted_key, child_split.right);
    if (node->num_keys > max_keys(tree)) {
        return split_internal(tree, node);
    }

    return result;
}

bool bptree_insert(BPTree *tree, int64_t id, size_t row_index) {
    if (!tree || !tree->root) {
        return false;
    }

    bool inserted = false;
    SplitResult split = insert_recursive(tree, tree->root, id, row_index, &inserted);
    if (!inserted) {
        return false;
    }
    tree->key_count++;

    if (split.did_split) {
        BPTreeNode *new_root = node_create(tree->order, false);
        if (!new_root) {
            return false;
        }
        new_root->keys[0] = split.promoted_key;
        new_root->children[0] = tree->root;
        new_root->children[1] = split.right;
        new_root->num_keys = 1;
        tree->root = new_root;
        tree->height++;
    }

    return true;
}

static const BPTreeNode *find_leaf(const BPTree *tree, int64_t key) {
    const BPTreeNode *node = tree->root;
    while (node && !node->is_leaf) {
        node = node->children[child_index_for(node, key)];
    }
    return node;
}

bool bptree_find(const BPTree *tree, int64_t id, size_t *out_row_index) {
    if (!tree || !out_row_index) {
        return false;
    }

    const BPTreeNode *leaf = find_leaf(tree, id);
    if (!leaf) {
        return false;
    }

    size_t pos = lower_bound(leaf->keys, leaf->num_keys, id);
    if (pos < leaf->num_keys && leaf->keys[pos] == id) {
        *out_row_index = leaf->record_ptrs[pos];
        return true;
    }
    return false;
}

bool bptree_find_range(const BPTree *tree, int64_t min_id, int64_t max_id, RowIndexList *out_rows) {
    if (!tree || !out_rows || min_id > max_id) {
        return false;
    }

    const BPTreeNode *leaf = find_leaf(tree, min_id);
    if (!leaf) {
        return true;
    }

    size_t pos = lower_bound(leaf->keys, leaf->num_keys, min_id);
    while (leaf) {
        while (pos < leaf->num_keys) {
            if (leaf->keys[pos] > max_id) {
                return true;
            }
            if (!row_index_list_append(out_rows, leaf->record_ptrs[pos])) {
                return false;
            }
            pos++;
        }
        leaf = leaf->next;
        pos = 0;
    }

    return true;
}

size_t bptree_height(const BPTree *tree) {
    return tree ? tree->height : 0;
}

size_t bptree_key_count(const BPTree *tree) {
    return tree ? tree->key_count : 0;
}

static bool report_fail(BPTreeValidationReport *report, const char *message) {
    if (report) {
        report->ok = false;
        snprintf(report->message, sizeof(report->message), "%s", message);
    }
    return false;
}

static bool validate_node(const BPTree *tree, const BPTreeNode *node, size_t depth, size_t *leaf_depth, BPTreeValidationReport *report) {
    if (!node) {
        return report_fail(report, "null node");
    }
    if (node->num_keys > max_keys(tree)) {
        return report_fail(report, "node has too many keys");
    }
    for (size_t i = 1; i < node->num_keys; i++) {
        if (node->keys[i - 1] >= node->keys[i]) {
            return report_fail(report, "node keys are not strictly sorted");
        }
    }

    if (node->is_leaf) {
        if (*leaf_depth == 0) {
            *leaf_depth = depth;
        } else if (*leaf_depth != depth) {
            return report_fail(report, "leaves are not at the same depth");
        }
        report->leaf_count++;
        report->key_count += node->num_keys;
        return true;
    }

    for (size_t i = 0; i <= node->num_keys; i++) {
        if (!node->children[i]) {
            return report_fail(report, "internal node has null child");
        }
        if (!validate_node(tree, node->children[i], depth + 1, leaf_depth, report)) {
            return false;
        }
    }
    return true;
}

static const BPTreeNode *leftmost_leaf(const BPTreeNode *node) {
    while (node && !node->is_leaf) {
        node = node->children[0];
    }
    return node;
}

static bool validate_leaf_chain(const BPTree *tree, BPTreeValidationReport *report) {
    const BPTreeNode *leaf = leftmost_leaf(tree->root);
    bool have_previous = false;
    int64_t previous = 0;
    size_t chain_key_count = 0;
    size_t chain_leaf_count = 0;

    while (leaf) {
        chain_leaf_count++;
        for (size_t i = 0; i < leaf->num_keys; i++) {
            if (have_previous && previous >= leaf->keys[i]) {
                return report_fail(report, "leaf chain keys are not globally sorted");
            }
            previous = leaf->keys[i];
            have_previous = true;
            chain_key_count++;
        }
        leaf = leaf->next;
    }

    if (chain_key_count != tree->key_count || chain_key_count != report->key_count) {
        return report_fail(report, "leaf chain key count mismatch");
    }
    if (chain_leaf_count != report->leaf_count) {
        return report_fail(report, "leaf chain leaf count mismatch");
    }
    return true;
}

bool bptree_validate(const BPTree *tree, BPTreeValidationReport *out_report) {
    BPTreeValidationReport local = {0};
    BPTreeValidationReport *report = out_report ? out_report : &local;
    memset(report, 0, sizeof(*report));

    if (!tree || !tree->root) {
        return report_fail(report, "empty tree pointer");
    }

    size_t leaf_depth = 0;
    report->height = tree->height;
    if (!validate_node(tree, tree->root, 1, &leaf_depth, report)) {
        return false;
    }
    if (!validate_leaf_chain(tree, report)) {
        return false;
    }

    report->ok = true;
    snprintf(report->message, sizeof(report->message), "ok");
    return true;
}
