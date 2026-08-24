/*
 * 08_doubly_linked_list.c
 * Target feature: doubly linked list with an embedded sentinel node
 * (circular), giving prev/next self-references plus an LRU cache built on
 * top of it - moves nodes between positions, so the decompiler must track
 * both link directions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef struct CacheEntry {
    uint32_t key;
    char     value[24];
    uint32_t hits;
} CacheEntry;

typedef struct DNode {
    struct DNode *prev;
    struct DNode *next;
    CacheEntry    entry;
} DNode;

/* The sentinel is an embedded DNode, not a pointer: head.next is the first
 * real element and head.prev is the last one. */
typedef struct LruCache {
    DNode    head;
    size_t   size;
    size_t   capacity;
    uint64_t lookups;
    uint64_t misses;
    uint64_t evictions;
} LruCache;

static void lru_init(LruCache *c, size_t capacity)
{
    memset(c, 0, sizeof(*c));
    c->head.prev = &c->head;
    c->head.next = &c->head;
    c->capacity  = capacity;
}

static void dnode_unlink(DNode *n)
{
    n->prev->next = n->next;
    n->next->prev = n->prev;
    n->prev = NULL;
    n->next = NULL;
}

static void dnode_insert_after(DNode *pos, DNode *n)
{
    n->prev = pos;
    n->next = pos->next;
    pos->next->prev = n;
    pos->next = n;
}

static void dnode_insert_before(DNode *pos, DNode *n)
{
    dnode_insert_after(pos->prev, n);
}

static DNode *lru_lookup(LruCache *c, uint32_t key)
{
    DNode *p;

    c->lookups++;
    for (p = c->head.next; p != &c->head; p = p->next) {
        if (p->entry.key == key) {
            p->entry.hits++;
            dnode_unlink(p);
            dnode_insert_after(&c->head, p);   /* promote to MRU */
            return p;
        }
    }
    c->misses++;
    return NULL;
}

static void lru_put(LruCache *c, uint32_t key, const char *value)
{
    DNode *n = lru_lookup(c, key);

    if (n) {
        strncpy(n->entry.value, value, sizeof(n->entry.value) - 1);
        return;
    }
    if (c->size == c->capacity) {
        DNode *victim = c->head.prev;    /* LRU end */
        BENCH_OUTPUT(
            printf("  evict key=%u ('%s', hits=%u)\n", victim->entry.key,
                   victim->entry.value, victim->entry.hits),
            printf("%u|%s|%u\n", victim->entry.key,
                   victim->entry.value, victim->entry.hits));
        dnode_unlink(victim);
        free(victim);
        c->size--;
        c->evictions++;
    }
    n = (DNode *)calloc(1, sizeof(DNode));
    if (!n)
        exit(1);
    n->entry.key = key;
    strncpy(n->entry.value, value, sizeof(n->entry.value) - 1);
    dnode_insert_after(&c->head, n);
    c->size++;
}

static void lru_dump(const LruCache *c, const char *label)
{
    const DNode *p;
    size_t fwd = 0, bwd = 0;
    BENCH_VERBOSE_ARG(label);

    BENCH_OUTPUT(printf("%s: MRU -> ", label), printf("%zu\n", c->size));
    for (p = c->head.next; p != &c->head; p = p->next, fwd++)
        BENCH_OUTPUT(printf("[%u:%s/%u] ", p->entry.key, p->entry.value,
                           p->entry.hits),
                     printf("%u|%s|%u\n", p->entry.key, p->entry.value,
                            p->entry.hits));
    BENCH_VERBOSE(printf("<- LRU\n"));

    BENCH_VERBOSE(printf("  reverse walk: "));
    for (p = c->head.prev; p != &c->head; p = p->prev, bwd++)
        printf("%u ", p->entry.key);
    BENCH_OUTPUT(printf("(fwd=%zu bwd=%zu size=%zu)\n", fwd, bwd, c->size),
                 printf("\n%zu %zu %zu\n", fwd, bwd, c->size));
}

static void lru_rotate(LruCache *c, int steps)
{
    while (steps-- > 0 && c->size > 1) {
        DNode *first = c->head.next;
        dnode_unlink(first);
        dnode_insert_before(&c->head, first);
    }
}

static void lru_free(LruCache *c)
{
    DNode *p = c->head.next;
    while (p != &c->head) {
        DNode *n = p->next;
        free(p);
        p = n;
    }
    c->head.next = &c->head;
    c->head.prev = &c->head;
    c->size = 0;
}

int main(void)
{
    LruCache cache;
    const uint32_t access[] = {1, 2, 3, 2, 4, 1, 5, 3, 6, 2, 2, 7};
    unsigned i;

    lru_init(&cache, 4);

    for (i = 0; i < sizeof(access) / sizeof(access[0]); i++) {
        char val[24];
        snprintf(val, sizeof(val), "page-%02u", access[i]);
        BENCH_OUTPUT(printf("put key=%u\n", access[i]), printf("%u\n", access[i]));
        lru_put(&cache, access[i], val);
    }
    lru_dump(&cache, "after workload");

    BENCH_OUTPUT(printf("lookup 2 -> %s\n", lru_lookup(&cache, 2) ? "HIT" : "MISS"),
                 printf("%d\n", lru_lookup(&cache, 2) != NULL));
    BENCH_OUTPUT(printf("lookup 99 -> %s\n", lru_lookup(&cache, 99) ? "HIT" : "MISS"),
                 printf("%d\n", lru_lookup(&cache, 99) != NULL));
    lru_dump(&cache, "after lookups");

    lru_rotate(&cache, 2);
    lru_dump(&cache, "after rotate(2)");

    BENCH_OUTPUT(
        printf("lookups=%llu misses=%llu evictions=%llu hit_rate=%.1f%%\n",
               (unsigned long long)cache.lookups,
               (unsigned long long)cache.misses,
               (unsigned long long)cache.evictions,
               100.0 * (double)(cache.lookups - cache.misses) /
                   (double)cache.lookups),
        printf("%llu %llu %llu %.1f\n", (unsigned long long)cache.lookups,
               (unsigned long long)cache.misses,
               (unsigned long long)cache.evictions,
               100.0 * (double)(cache.lookups - cache.misses) /
                   (double)cache.lookups));

    lru_free(&cache);
    return 0;
}
