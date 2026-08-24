/*
 * 16_hash_table.c
 * Target feature: open-hashing table - a heap array of bucket-head pointers,
 * chained entry structs whose key is a trailing flexible array, plus a
 * resize that rewrites every chain. Combines FAM + linked list + array.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef struct Record {
    uint32_t visits;
    double   score;
    uint8_t  category;
} Record;

typedef struct Entry {
    struct Entry *next;      /* chain link */
    uint32_t      hash;
    uint16_t      key_len;
    Record        value;
    char          key[];     /* flexible array: NUL-terminated key */
} Entry;

typedef struct HashTable {
    Entry  **buckets;        /* array of chain heads */
    uint32_t nbuckets;
    uint32_t nentries;
    uint32_t resizes;
    uint32_t collisions;
    uint64_t probe_total;
} HashTable;

typedef struct BucketStat {
    uint32_t length;
    uint32_t index;
} BucketStat;

static uint32_t hash_key(const char *s, size_t n)
{
    uint32_t h = 0x811c9dc5u;
    size_t i;
    for (i = 0; i < n; i++) {
        h ^= (uint8_t)s[i];
        h *= 0x01000193u;
    }
    h ^= h >> 15;
    return h;
}

static void ht_init(HashTable *t, uint32_t nbuckets)
{
    memset(t, 0, sizeof(*t));
    t->buckets  = (Entry **)calloc(nbuckets, sizeof(Entry *));
    if (!t->buckets)
        exit(1);
    t->nbuckets = nbuckets;
}

static Entry *entry_new(const char *key, uint32_t h)
{
    size_t n = strlen(key);
    Entry *e = (Entry *)calloc(1, sizeof(Entry) + n + 1);
    if (!e)
        exit(1);
    memcpy(e->key, key, n + 1);
    e->key_len = (uint16_t)n;
    e->hash    = h;
    return e;
}

static void ht_grow(HashTable *t)
{
    uint32_t newn = t->nbuckets * 2;
    Entry **fresh = (Entry **)calloc(newn, sizeof(Entry *));
    uint32_t b;

    if (!fresh)
        exit(1);
    for (b = 0; b < t->nbuckets; b++) {
        Entry *e = t->buckets[b];
        while (e) {
            Entry *next = e->next;
            uint32_t idx = e->hash & (newn - 1);
            e->next = fresh[idx];
            fresh[idx] = e;
            e = next;
        }
    }
    free(t->buckets);
    t->buckets  = fresh;
    t->nbuckets = newn;
    t->resizes++;
}

static Entry *ht_lookup(HashTable *t, const char *key)
{
    uint32_t h = hash_key(key, strlen(key));
    Entry *e = t->buckets[h & (t->nbuckets - 1)];
    uint32_t probes = 0;

    while (e) {
        probes++;
        if (e->hash == h && strcmp(e->key, key) == 0)
            break;
        e = e->next;
    }
    t->probe_total += probes;
    return e;
}

static Entry *ht_put(HashTable *t, const char *key, double score, uint8_t cat)
{
    uint32_t h, idx;
    Entry *e = ht_lookup(t, key);

    if (e) {
        e->value.visits++;
        e->value.score += score;
        return e;
    }
    if (t->nentries * 4 >= t->nbuckets * 3)
        ht_grow(t);

    h = hash_key(key, strlen(key));
    idx = h & (t->nbuckets - 1);
    e = entry_new(key, h);
    e->value.visits   = 1;
    e->value.score    = score;
    e->value.category = cat;
    if (t->buckets[idx])
        t->collisions++;
    e->next = t->buckets[idx];
    t->buckets[idx] = e;
    t->nentries++;
    return e;
}

static int ht_erase(HashTable *t, const char *key)
{
    uint32_t h = hash_key(key, strlen(key));
    Entry **pp = &t->buckets[h & (t->nbuckets - 1)];

    while (*pp) {
        Entry *cur = *pp;
        if (cur->hash == h && strcmp(cur->key, key) == 0) {
            *pp = cur->next;
            free(cur);
            t->nentries--;
            return 1;
        }
        pp = &cur->next;
    }
    return 0;
}

static int cmp_bucket(const void *a, const void *b)
{
    const BucketStat *x = (const BucketStat *)a;
    const BucketStat *y = (const BucketStat *)b;
    if (x->length != y->length)
        return (int)y->length - (int)x->length;
    return (int)x->index - (int)y->index;
}

static void ht_stats(HashTable *t)
{
    BucketStat *stats = (BucketStat *)calloc(t->nbuckets, sizeof(BucketStat));
    uint32_t b, used = 0, longest = 0;

    if (!stats)
        exit(1);
    for (b = 0; b < t->nbuckets; b++) {
        Entry *e;
        stats[b].index = b;
        for (e = t->buckets[b]; e; e = e->next)
            stats[b].length++;
        if (stats[b].length) {
            used++;
            if (stats[b].length > longest)
                longest = stats[b].length;
        }
    }
    qsort(stats, t->nbuckets, sizeof(BucketStat), cmp_bucket);

    BENCH_OUTPUT(
        printf("table: entries=%u buckets=%u used=%u load=%.2f longest=%u resizes=%u collisions=%u probes=%llu\n",
               t->nentries, t->nbuckets, used,
               (double)t->nentries / (double)t->nbuckets, longest, t->resizes,
               t->collisions, (unsigned long long)t->probe_total),
        printf("%u %u %u %.2f %u %u %u %llu\n", t->nentries,
               t->nbuckets, used, (double)t->nentries / (double)t->nbuckets,
               longest, t->resizes, t->collisions,
               (unsigned long long)t->probe_total));
    BENCH_VERBOSE(printf("top buckets:"));
    for (b = 0; b < 5 && b < t->nbuckets; b++)
        BENCH_OUTPUT(printf(" [%u]=%u", stats[b].index, stats[b].length),
                     printf(" %u %u", stats[b].index, stats[b].length));
    putchar('\n');
    free(stats);
}

static void ht_dump(HashTable *t)
{
    uint32_t b;
    for (b = 0; b < t->nbuckets; b++) {
        Entry *e = t->buckets[b];
        if (!e)
            continue;
        BENCH_OUTPUT(printf("  bucket %2u:", b), printf("%u", b));
        for (; e; e = e->next)
            BENCH_OUTPUT(
                printf(" {%s len=%u v=%u s=%.1f c=%u h=%08x}", e->key,
                       e->key_len, e->value.visits, e->value.score,
                       e->value.category, e->hash),
                printf(" %s %u %u %.1f %u %08x", e->key, e->key_len,
                       e->value.visits, e->value.score, e->value.category,
                       e->hash));
        putchar('\n');
    }
}

static void ht_free(HashTable *t)
{
    uint32_t b;
    for (b = 0; b < t->nbuckets; b++) {
        Entry *e = t->buckets[b];
        while (e) {
            Entry *n = e->next;
            free(e);
            e = n;
        }
    }
    free(t->buckets);
    memset(t, 0, sizeof(*t));
}

int main(void)
{
    static const char *keys[] = {
        "index.html", "style.css", "app.js", "logo.png", "index.html",
        "api/users", "api/orders", "api/users", "favicon.ico", "app.js",
        "robots.txt", "sitemap.xml", "api/health", "index.html", "app.js",
        "fonts/inter.woff2", "img/hero.jpg", "api/orders", "sw.js", "manifest.json"
    };
    HashTable table;
    Entry *e;
    unsigned i;

    ht_init(&table, 8);
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        ht_put(&table, keys[i], 1.5 + (double)(i % 5), (uint8_t)(i % 3));

    ht_dump(&table);
    ht_stats(&table);

    e = ht_lookup(&table, "app.js");
    if (e)
        BENCH_OUTPUT(
            printf("lookup app.js -> visits=%u score=%.1f cat=%u\n",
                   e->value.visits, e->value.score, e->value.category),
            printf("%u %.1f %u\n", e->value.visits, e->value.score,
                   e->value.category));
    {
        Entry *missing = ht_lookup(&table, "nope.txt");
        BENCH_OUTPUT(printf("lookup missing -> %p\n", (void *)missing),
                     printf("%d\n", missing != NULL));
    }

    {
        int erased_api = ht_erase(&table, "api/users");
        int erased_nope = ht_erase(&table, "nope.txt");
        BENCH_OUTPUT(printf("erase api/users = %d, erase nope = %d\n",
                           erased_api, erased_nope),
                     printf("%d %d\n", erased_api, erased_nope));
    }
    ht_stats(&table);

    ht_free(&table);
    return 0;
}
