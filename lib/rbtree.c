// Red-Black Tree implementation for SUB-OS (Linux-compatible semantics)
#include <lib/rbtree.h>

static void rb_rotate_left(struct rb_node* node, struct rb_root* root) {
    struct rb_node* right  = node->rb_right;
    struct rb_node* parent = node->rb_parent;

    node->rb_right = right->rb_left;
    if (right->rb_left) right->rb_left->rb_parent = node;

    right->rb_left   = node;
    right->rb_parent = parent;

    if (parent) {
        if (node == parent->rb_left) parent->rb_left = right;
        else                         parent->rb_right = right;
    } else {
        root->rb_node = right;
    }
    node->rb_parent = right;
}

static void rb_rotate_right(struct rb_node* node, struct rb_root* root) {
    struct rb_node* left   = node->rb_left;
    struct rb_node* parent = node->rb_parent;

    node->rb_left = left->rb_right;
    if (left->rb_right) left->rb_right->rb_parent = node;

    left->rb_right  = node;
    left->rb_parent = parent;

    if (parent) {
        if (node == parent->rb_right) parent->rb_right = left;
        else                          parent->rb_left = left;
    } else {
        root->rb_node = left;
    }
    node->rb_parent = left;
}

void rb_insert_color(struct rb_node* node, struct rb_root* root) {
    struct rb_node *parent, *gparent;

    while ((parent = node->rb_parent) && parent->rb_color == RB_RED) {
        gparent = parent->rb_parent;
        if (!gparent) break;

        if (parent == gparent->rb_left) {
            struct rb_node* uncle = gparent->rb_right;
            if (uncle && uncle->rb_color == RB_RED) {
                uncle->rb_color   = RB_BLACK;
                parent->rb_color  = RB_BLACK;
                gparent->rb_color = RB_RED;
                node = gparent;
                continue;
            }
            if (parent->rb_right == node) {
                rb_rotate_left(parent, root);
                struct rb_node* tmp = parent;
                parent = node;
                node = tmp;
            }
            parent->rb_color  = RB_BLACK;
            gparent->rb_color = RB_RED;
            rb_rotate_right(gparent, root);
        } else {
            struct rb_node* uncle = gparent->rb_left;
            if (uncle && uncle->rb_color == RB_RED) {
                uncle->rb_color   = RB_BLACK;
                parent->rb_color  = RB_BLACK;
                gparent->rb_color = RB_RED;
                node = gparent;
                continue;
            }
            if (parent->rb_left == node) {
                rb_rotate_right(parent, root);
                struct rb_node* tmp = parent;
                parent = node;
                node = tmp;
            }
            parent->rb_color  = RB_BLACK;
            gparent->rb_color = RB_RED;
            rb_rotate_left(gparent, root);
        }
    }

    root->rb_node->rb_color = RB_BLACK;
}

// Rebalance after removing a black node. `node` may be NULL, hence the
// explicit parent pointer.
static void rb_erase_color(struct rb_node* node, struct rb_node* parent,
                           struct rb_root* root) {
    struct rb_node* other;

    while ((!node || node->rb_color == RB_BLACK) && node != root->rb_node) {
        if (parent->rb_left == node) {
            other = parent->rb_right;
            if (!other) break;

            if (other->rb_color == RB_RED) {
                other->rb_color  = RB_BLACK;
                parent->rb_color = RB_RED;
                rb_rotate_left(parent, root);
                other = parent->rb_right;
                if (!other) break;
            }
            if ((!other->rb_left  || other->rb_left->rb_color  == RB_BLACK) &&
                (!other->rb_right || other->rb_right->rb_color == RB_BLACK)) {
                other->rb_color = RB_RED;
                node = parent;
                parent = node->rb_parent;
            } else {
                if (!other->rb_right || other->rb_right->rb_color == RB_BLACK) {
                    if (other->rb_left) other->rb_left->rb_color = RB_BLACK;
                    other->rb_color = RB_RED;
                    rb_rotate_right(other, root);
                    other = parent->rb_right;
                    if (!other) break;
                }
                other->rb_color  = parent->rb_color;
                parent->rb_color = RB_BLACK;
                if (other->rb_right) other->rb_right->rb_color = RB_BLACK;
                rb_rotate_left(parent, root);
                node = root->rb_node;
                break;
            }
        } else {
            other = parent->rb_left;
            if (!other) break;

            if (other->rb_color == RB_RED) {
                other->rb_color  = RB_BLACK;
                parent->rb_color = RB_RED;
                rb_rotate_right(parent, root);
                other = parent->rb_left;
                if (!other) break;
            }
            if ((!other->rb_left  || other->rb_left->rb_color  == RB_BLACK) &&
                (!other->rb_right || other->rb_right->rb_color == RB_BLACK)) {
                other->rb_color = RB_RED;
                node = parent;
                parent = node->rb_parent;
            } else {
                if (!other->rb_left || other->rb_left->rb_color == RB_BLACK) {
                    if (other->rb_right) other->rb_right->rb_color = RB_BLACK;
                    other->rb_color = RB_RED;
                    rb_rotate_left(other, root);
                    other = parent->rb_left;
                    if (!other) break;
                }
                other->rb_color  = parent->rb_color;
                parent->rb_color = RB_BLACK;
                if (other->rb_left) other->rb_left->rb_color = RB_BLACK;
                rb_rotate_right(parent, root);
                node = root->rb_node;
                break;
            }
        }
    }

    if (node) node->rb_color = RB_BLACK;
}

void rb_erase(struct rb_node* node, struct rb_root* root) {
    struct rb_node *child, *parent;
    int color;

    if (!node->rb_left) {
        child = node->rb_right;
    } else if (!node->rb_right) {
        child = node->rb_left;
    } else {
        // Two children: splice in the in-order successor.
        struct rb_node* old = node;
        struct rb_node* left;

        node = node->rb_right;
        while ((left = node->rb_left) != NULL) node = left;

        // Put the successor where `old` used to hang.
        if (old->rb_parent) {
            if (old->rb_parent->rb_left == old) old->rb_parent->rb_left = node;
            else                                old->rb_parent->rb_right = node;
        } else {
            root->rb_node = node;
        }

        child  = node->rb_right;
        parent = node->rb_parent;
        color  = node->rb_color;

        if (parent == old) {
            // The successor is `old`'s direct right child, so it keeps its own
            // right subtree and becomes the parent of the deficient side.
            parent = node;
        } else {
            if (child) child->rb_parent = parent;
            parent->rb_left = child;

            node->rb_right = old->rb_right;
            old->rb_right->rb_parent = node;
        }

        node->rb_parent = old->rb_parent;
        node->rb_color  = old->rb_color;
        node->rb_left   = old->rb_left;
        old->rb_left->rb_parent = node;

        if (color == RB_BLACK) rb_erase_color(child, parent, root);
        return;
    }

    parent = node->rb_parent;
    color  = node->rb_color;

    if (child) child->rb_parent = parent;
    if (parent) {
        if (parent->rb_left == node) parent->rb_left = child;
        else                         parent->rb_right = child;
    } else {
        root->rb_node = child;
    }

    if (color == RB_BLACK) rb_erase_color(child, parent, root);
}

struct rb_node* rb_first(const struct rb_root* root) {
    struct rb_node* n = root->rb_node;
    if (!n) return NULL;
    while (n->rb_left) n = n->rb_left;
    return n;
}

struct rb_node* rb_last(const struct rb_root* root) {
    struct rb_node* n = root->rb_node;
    if (!n) return NULL;
    while (n->rb_right) n = n->rb_right;
    return n;
}

struct rb_node* rb_next(const struct rb_node* node) {
    if (!node) return NULL;

    if (node->rb_right) {
        struct rb_node* n = node->rb_right;
        while (n->rb_left) n = n->rb_left;
        return n;
    }

    struct rb_node* parent;
    while ((parent = node->rb_parent) && node == parent->rb_right) {
        node = parent;
    }
    return parent;
}

struct rb_node* rb_prev(const struct rb_node* node) {
    if (!node) return NULL;

    if (node->rb_left) {
        struct rb_node* n = node->rb_left;
        while (n->rb_right) n = n->rb_right;
        return n;
    }

    struct rb_node* parent;
    while ((parent = node->rb_parent) && node == parent->rb_left) {
        node = parent;
    }
    return parent;
}

size_t rb_count(const struct rb_root* root) {
    size_t count = 0;
    for (struct rb_node* n = rb_first(root); n; n = rb_next(n)) count++;
    return count;
}

// Returns the number of black nodes on any root-to-leaf path, or -1 when the
// black-height invariant is violated.
static int rb_black_height_of(const struct rb_node* node) {
    if (!node) return 1;

    int left  = rb_black_height_of(node->rb_left);
    int right = rb_black_height_of(node->rb_right);
    if (left < 0 || right < 0 || left != right) return -1;

    if (node->rb_color == RB_RED) {
        if ((node->rb_left  && node->rb_left->rb_color  == RB_RED) ||
            (node->rb_right && node->rb_right->rb_color == RB_RED)) {
            return -1;
        }
        return left;
    }
    return left + 1;
}

int rb_black_height(const struct rb_root* root) {
    return rb_black_height_of(root->rb_node);
}

bool rb_validate(const struct rb_root* root) {
    if (!root->rb_node) return true;
    if (root->rb_node->rb_color != RB_BLACK) return false;
    if (root->rb_node->rb_parent != NULL) return false;
    return rb_black_height_of(root->rb_node) > 0;
}
