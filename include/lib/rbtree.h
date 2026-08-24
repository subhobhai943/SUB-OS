#ifndef _LIB_RBTREE_H
#define _LIB_RBTREE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Linux-style Red-Black Tree: O(log n) ordered container used by the
// CFS-style scheduler index, VMA lookup and the page cache radix index.

#define RB_RED   0
#define RB_BLACK 1

struct rb_node {
    struct rb_node* rb_parent;
    struct rb_node* rb_left;
    struct rb_node* rb_right;
    int             rb_color;
};

struct rb_root {
    struct rb_node* rb_node;
};

#define RB_ROOT_INIT { NULL }

#define rb_entry(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

static inline void rb_root_init(struct rb_root* root) {
    root->rb_node = NULL;
}

static inline bool rb_empty(const struct rb_root* root) {
    return root->rb_node == NULL;
}

// Attach a freshly allocated node into the tree, then recolour.
static inline void rb_link_node(struct rb_node* node, struct rb_node* parent,
                                struct rb_node** rb_link) {
    node->rb_parent = parent;
    node->rb_color  = RB_RED;
    node->rb_left   = NULL;
    node->rb_right  = NULL;
    *rb_link = node;
}

void rb_insert_color(struct rb_node* node, struct rb_root* root);
void rb_erase(struct rb_node* node, struct rb_root* root);

struct rb_node* rb_first(const struct rb_root* root);
struct rb_node* rb_last(const struct rb_root* root);
struct rb_node* rb_next(const struct rb_node* node);
struct rb_node* rb_prev(const struct rb_node* node);

size_t rb_count(const struct rb_root* root);
int    rb_black_height(const struct rb_root* root);
bool   rb_validate(const struct rb_root* root);

#define rb_for_each(pos, root) \
    for (pos = rb_first(root); pos != NULL; pos = rb_next(pos))

#endif // _LIB_RBTREE_H
