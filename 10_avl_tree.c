/*
 * 10_avl_tree.c
 * Target feature: self-balancing tree - node struct carries height/balance
 * metadata beside the two child self-pointers, and rotations rewrite several
 * links at once (hard for a decompiler to keep coherent).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef struct GeoPoint {
    double latitude;
    double longitude;
} GeoPoint;

typedef struct City {
    uint32_t  population;
    char      name[20];
    char      country[4];
    GeoPoint  location;
} City;

typedef struct AvlNode {
    uint32_t        key;          /* population used as ordering key */
    City            value;
    int8_t          height;
    struct AvlNode *child[2];     /* [0]=left, [1]=right - array of self-pointers */
} AvlNode;

typedef struct AvlTree {
    AvlNode *root;
    size_t   count;
    uint32_t rotations_ll;
    uint32_t rotations_rr;
    uint32_t rotations_lr;
    uint32_t rotations_rl;
} AvlTree;

static int8_t node_height(const AvlNode *n) { return n ? n->height : 0; }

static int8_t max8(int8_t a, int8_t b) { return a > b ? a : b; }

static void fix_height(AvlNode *n)
{
    n->height = (int8_t)(1 + max8(node_height(n->child[0]), node_height(n->child[1])));
}

static int balance_factor(const AvlNode *n)
{
    return node_height(n->child[0]) - node_height(n->child[1]);
}

/* dir==0 rotates right, dir==1 rotates left */
static AvlNode *rotate(AvlNode *n, int dir)
{
    AvlNode *pivot = n->child[1 - dir];

    n->child[1 - dir] = pivot->child[dir];
    pivot->child[dir] = n;
    fix_height(n);
    fix_height(pivot);
    return pivot;
}

static AvlNode *rebalance(AvlTree *t, AvlNode *n)
{
    int bf;

    fix_height(n);
    bf = balance_factor(n);

    if (bf > 1) {
        if (balance_factor(n->child[0]) < 0) {
            t->rotations_lr++;
            n->child[0] = rotate(n->child[0], 1);
        } else {
            t->rotations_ll++;
        }
        return rotate(n, 0);
    }
    if (bf < -1) {
        if (balance_factor(n->child[1]) > 0) {
            t->rotations_rl++;
            n->child[1] = rotate(n->child[1], 0);
        } else {
            t->rotations_rr++;
        }
        return rotate(n, 1);
    }
    return n;
}

static AvlNode *avl_node_new(uint32_t key, const City *c)
{
    AvlNode *n = (AvlNode *)calloc(1, sizeof(AvlNode));
    if (!n)
        exit(1);
    n->key    = key;
    n->value  = *c;
    n->height = 1;
    return n;
}

static AvlNode *avl_insert_rec(AvlTree *t, AvlNode *n, uint32_t key, const City *c)
{
    int dir;

    if (!n) {
        t->count++;
        return avl_node_new(key, c);
    }
    if (key == n->key) {
        n->value = *c;
        return n;
    }
    dir = (key > n->key);
    n->child[dir] = avl_insert_rec(t, n->child[dir], key, c);
    return rebalance(t, n);
}

static void avl_insert(AvlTree *t, const City *c)
{
    t->root = avl_insert_rec(t, t->root, c->population, c);
}

static AvlNode *avl_min(AvlNode *n)
{
    while (n->child[0])
        n = n->child[0];
    return n;
}

static AvlNode *avl_erase_rec(AvlTree *t, AvlNode *n, uint32_t key)
{
    int dir;

    if (!n)
        return NULL;
    if (key != n->key) {
        dir = (key > n->key);
        n->child[dir] = avl_erase_rec(t, n->child[dir], key);
        return rebalance(t, n);
    }
    if (!n->child[0] || !n->child[1]) {
        AvlNode *kid = n->child[0] ? n->child[0] : n->child[1];
        free(n);
        t->count--;
        return kid;
    }
    {
        AvlNode *succ = avl_min(n->child[1]);
        n->key   = succ->key;
        n->value = succ->value;
        n->child[1] = avl_erase_rec(t, n->child[1], succ->key);
    }
    return rebalance(t, n);
}

static void avl_erase(AvlTree *t, uint32_t key)
{
    t->root = avl_erase_rec(t, t->root, key);
}

static const AvlNode *avl_find_ge(const AvlNode *n, uint32_t key)
{
    const AvlNode *best = NULL;
    while (n) {
        if (n->key >= key) {
            best = n;
            n = n->child[0];
        } else {
            n = n->child[1];
        }
    }
    return best;
}

static void avl_print(const AvlNode *n, int depth)
{
    if (!n)
        return;
    avl_print(n->child[0], depth + 1);
    BENCH_OUTPUT(
        printf("  %*s%-16s %-3s pop=%-9u h=%d bf=%+d (%.2f,%.2f)\n",
               depth * 2, "", n->value.name, n->value.country, n->key,
               n->height, balance_factor(n), n->value.location.latitude,
               n->value.location.longitude),
        printf("%d %s %s %u %d %d %.2f %.2f\n", depth, n->value.name,
               n->value.country, n->key, n->height, balance_factor(n),
               n->value.location.latitude, n->value.location.longitude));
    avl_print(n->child[1], depth + 1);
}

static int avl_check(const AvlNode *n)
{
    int bf;
    if (!n)
        return 1;
    bf = balance_factor(n);
    if (bf < -1 || bf > 1)
        return 0;
    return avl_check(n->child[0]) && avl_check(n->child[1]);
}

static void avl_free(AvlNode *n)
{
    if (!n)
        return;
    avl_free(n->child[0]);
    avl_free(n->child[1]);
    free(n);
}

int main(void)
{
    static const City data[] = {
        { 8336817u, "New York",  "US", { 40.71, -74.01 } },
        { 3979576u, "Los Angeles","US", { 34.05, -118.24 } },
        { 9540000u, "Hanoi",     "VN", { 21.03, 105.85 } },
        { 8982000u, "London",    "GB", { 51.51, -0.13 } },
        { 2161000u, "Paris",     "FR", { 48.86, 2.35 } },
        {13960000u, "Tokyo",     "JP", { 35.68, 139.69 } },
        { 5638000u, "Singapore", "SG", { 1.35, 103.82 } },
        { 1780000u, "Vienna",    "AT", { 48.21, 16.37 } },
        {21540000u, "Shanghai",  "CN", { 31.23, 121.47 } },
        { 3645000u, "Berlin",    "DE", { 52.52, 13.40 } },
        { 9276000u, "Ho Chi Minh","VN",{ 10.82, 106.63 } }
    };
    AvlTree tree;
    const AvlNode *q;
    unsigned i;

    memset(&tree, 0, sizeof(tree));
    for (i = 0; i < sizeof(data) / sizeof(data[0]); i++)
        avl_insert(&tree, &data[i]);

    BENCH_VERBOSE(printf("in-order by population:\n"));
    avl_print(tree.root, 0);
    BENCH_OUTPUT(
        printf("count=%zu root='%s' height=%d balanced=%d\n", tree.count,
               tree.root->value.name, tree.root->height, avl_check(tree.root)),
        printf("%zu %s %d %d\n", tree.count, tree.root->value.name,
               tree.root->height, avl_check(tree.root)));
    BENCH_OUTPUT(
        printf("rotations LL=%u RR=%u LR=%u RL=%u\n", tree.rotations_ll,
               tree.rotations_rr, tree.rotations_lr, tree.rotations_rl),
        printf("%u %u %u %u\n", tree.rotations_ll, tree.rotations_rr,
               tree.rotations_lr, tree.rotations_rl));

    q = avl_find_ge(tree.root, 6000000u);
    BENCH_OUTPUT(
        printf("first city with pop >= 6M: %s (%u)\n",
               q ? q->value.name : "(none)", q ? q->key : 0u),
        printf("%s %u\n", q ? q->value.name : "-", q ? q->key : 0u));

    avl_erase(&tree, 8982000u);   /* London */
    avl_erase(&tree, 2161000u);   /* Paris  */
    BENCH_OUTPUT(
        printf("after 2 erases: count=%zu root='%s' balanced=%d\n",
               tree.count, tree.root->value.name, avl_check(tree.root)),
        printf("%zu %s %d\n", tree.count, tree.root->value.name,
               avl_check(tree.root)));
    avl_print(tree.root, 0);

    avl_free(tree.root);
    return 0;
}
