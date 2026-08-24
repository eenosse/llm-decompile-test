/*
 * 09_binary_tree.c
 * Target feature: binary search tree - two child self-pointers plus a parent
 * back-pointer, recursive and iterative traversals, an explicit stack of
 * node pointers, and a level-order queue.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef struct WordCount {
    char     word[24];
    uint32_t count;
    uint32_t first_pos;
} WordCount;

typedef struct BstNode {
    WordCount       key;
    struct BstNode *left;
    struct BstNode *right;
    struct BstNode *parent;
    uint32_t        subtree_size;
} BstNode;

typedef struct Bst {
    BstNode *root;
    size_t   nodes;
    size_t   comparisons;
} Bst;

typedef struct NodeStack {
    BstNode *slots[64];
    int      top;
} NodeStack;

typedef struct NodeQueue {
    BstNode *slots[64];
    int      head;
    int      tail;
} NodeQueue;

static void stack_push(NodeStack *s, BstNode *n) { if (s->top < 64) s->slots[s->top++] = n; }
static BstNode *stack_pop(NodeStack *s)          { return s->top > 0 ? s->slots[--s->top] : NULL; }
static int stack_empty(const NodeStack *s)       { return s->top == 0; }

static void queue_push(NodeQueue *q, BstNode *n) { if (q->tail < 64) q->slots[q->tail++] = n; }
static BstNode *queue_pop(NodeQueue *q)          { return q->head < q->tail ? q->slots[q->head++] : NULL; }
static int queue_empty(const NodeQueue *q)       { return q->head >= q->tail; }

static BstNode *bst_node_new(const char *word, uint32_t pos)
{
    BstNode *n = (BstNode *)calloc(1, sizeof(BstNode));
    if (!n)
        exit(1);
    strncpy(n->key.word, word, sizeof(n->key.word) - 1);
    n->key.count     = 1;
    n->key.first_pos = pos;
    n->subtree_size  = 1;
    return n;
}

static void bst_insert(Bst *t, const char *word, uint32_t pos)
{
    BstNode **slot = &t->root;
    BstNode  *parent = NULL;
    BstNode  *fresh;
    BstNode  *walk;

    while (*slot) {
        int cmp;
        parent = *slot;
        t->comparisons++;
        cmp = strcmp(word, parent->key.word);
        if (cmp == 0) {
            parent->key.count++;
            return;
        }
        slot = (cmp < 0) ? &parent->left : &parent->right;
    }
    fresh = bst_node_new(word, pos);
    fresh->parent = parent;
    *slot = fresh;
    t->nodes++;
    for (walk = parent; walk; walk = walk->parent)
        walk->subtree_size++;
}

static BstNode *bst_find(Bst *t, const char *word)
{
    BstNode *p = t->root;
    while (p) {
        int cmp = strcmp(word, p->key.word);
        t->comparisons++;
        if (cmp == 0)
            return p;
        p = (cmp < 0) ? p->left : p->right;
    }
    return NULL;
}

static BstNode *bst_min(BstNode *n)
{
    while (n && n->left)
        n = n->left;
    return n;
}

/* successor via parent pointers - exercises the back link */
static BstNode *bst_successor(BstNode *n)
{
    BstNode *p;
    if (n->right)
        return bst_min(n->right);
    p = n->parent;
    while (p && n == p->right) {
        n = p;
        p = p->parent;
    }
    return p;
}

static int bst_height(const BstNode *n)
{
    int hl, hr;
    if (!n)
        return 0;
    hl = bst_height(n->left);
    hr = bst_height(n->right);
    return 1 + (hl > hr ? hl : hr);
}

static void bst_inorder_recursive(const BstNode *n, int depth)
{
    if (!n)
        return;
    bst_inorder_recursive(n->left, depth + 1);
    BENCH_OUTPUT(
        printf("  %*s%-12s x%-2u pos=%-3u size=%u\n", depth * 2, "",
               n->key.word, n->key.count, n->key.first_pos, n->subtree_size),
        printf("%d %s %u %u %u\n", depth, n->key.word, n->key.count,
               n->key.first_pos, n->subtree_size));
    bst_inorder_recursive(n->right, depth + 1);
}

static void bst_inorder_iterative(Bst *t)
{
    NodeStack st;
    BstNode *cur = t->root;

    st.top = 0;
    BENCH_VERBOSE(printf("  iterative:"));
    while (cur || !stack_empty(&st)) {
        while (cur) {
            stack_push(&st, cur);
            cur = cur->left;
        }
        cur = stack_pop(&st);
        printf(" %s", cur->key.word);
        cur = cur->right;
    }
    putchar('\n');
}

static void bst_levelorder(Bst *t)
{
    NodeQueue q;
    q.head = q.tail = 0;

    if (t->root)
        queue_push(&q, t->root);
    BENCH_VERBOSE(printf("  levels:"));
    while (!queue_empty(&q)) {
        BstNode *n = queue_pop(&q);
        BENCH_OUTPUT(printf(" %s(%u)", n->key.word, n->subtree_size),
                     printf(" %s %u", n->key.word, n->subtree_size));
        if (n->left)  queue_push(&q, n->left);
        if (n->right) queue_push(&q, n->right);
    }
    putchar('\n');
}

static void bst_free(BstNode *n)
{
    if (!n)
        return;
    bst_free(n->left);
    bst_free(n->right);
    free(n);
}

int main(void)
{
    static const char *text[] = {
        "the", "quick", "brown", "fox", "jumps", "over", "the", "lazy",
        "dog", "while", "the", "brown", "fox", "naps", "quietly"
    };
    Bst tree;
    BstNode *n;
    uint32_t i;

    memset(&tree, 0, sizeof(tree));
    for (i = 0; i < sizeof(text) / sizeof(text[0]); i++)
        bst_insert(&tree, text[i], i);

    BENCH_VERBOSE(printf("in-order (recursive):\n"));
    bst_inorder_recursive(tree.root, 0);
    bst_inorder_iterative(&tree);
    bst_levelorder(&tree);

    BENCH_OUTPUT(
        printf("root='%s' nodes=%zu height=%d comparisons=%zu\n",
               tree.root->key.word, tree.nodes, bst_height(tree.root),
               tree.comparisons),
        printf("%s %zu %d %zu\n", tree.root->key.word, tree.nodes,
               bst_height(tree.root), tree.comparisons));

    n = bst_find(&tree, "fox");
    if (n) {
        BstNode *s = bst_successor(n);
        BENCH_OUTPUT(
            printf("found 'fox' count=%u parent=%s successor=%s\n", n->key.count,
                   n->parent ? n->parent->key.word : "(root)",
                   s ? s->key.word : "(none)"),
            printf("%u %s %s\n", n->key.count,
                   n->parent ? n->parent->key.word : "-",
                   s ? s->key.word : "-"));
    }

    BENCH_VERBOSE(printf("ascending via successor:"));
    for (n = bst_min(tree.root); n; n = bst_successor(n))
        printf(" %s", n->key.word);
    putchar('\n');

    bst_free(tree.root);
    return 0;
}
