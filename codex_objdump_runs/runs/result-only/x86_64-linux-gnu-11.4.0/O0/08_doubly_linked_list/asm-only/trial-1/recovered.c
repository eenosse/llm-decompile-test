#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Entry {
    struct Entry *previous;
    struct Entry *next;
    int key;
    char value[24];
    int hits;
} Entry;

typedef struct Cache {
    Entry root;
    size_t size;
    size_t capacity;
    size_t lookups;
    size_t misses;
    size_t evictions;
} Cache;

static void cache_init(Cache *cache, size_t capacity)
{
    memset(cache, 0, sizeof(*cache));
    cache->root.previous = &cache->root;
    cache->root.next = &cache->root;
    cache->capacity = capacity;
}

static void entry_remove(Entry *entry)
{
    entry->previous->next = entry->next;
    entry->next->previous = entry->previous;
    entry->previous = NULL;
    entry->next = NULL;
}

static void entry_insert_after(Entry *position, Entry *entry)
{
    entry->previous = position;
    entry->next = position->next;
    position->next->previous = entry;
    position->next = entry;
}

static void entry_insert_before(Entry *position, Entry *entry)
{
    entry_insert_after(position->previous, entry);
}

static Entry *cache_find(Cache *cache, int key)
{
    Entry *entry;

    ++cache->lookups;

    for (entry = cache->root.next; entry != &cache->root;
         entry = entry->next) {
        if (entry->key == key) {
            ++entry->hits;
            entry_remove(entry);
            entry_insert_after(&cache->root, entry);
            return entry;
        }
    }

    ++cache->misses;
    return NULL;
}

static void cache_put(Cache *cache, int key, const char *value)
{
    Entry *entry = cache_find(cache, key);

    if (entry != NULL) {
        strncpy(entry->value, value, sizeof(entry->value) - 1);
        return;
    }

    if (cache->size == cache->capacity) {
        entry = cache->root.previous;
        printf("%d %s %d\n", entry->key, entry->value, entry->hits);
        entry_remove(entry);
        free(entry);
        --cache->size;
        ++cache->evictions;
    }

    entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        exit(1);

    entry->key = key;
    strncpy(entry->value, value, sizeof(entry->value) - 1);
    entry_insert_after(&cache->root, entry);
    ++cache->size;
}

static void cache_print(Cache *cache, const char *unused_label)
{
    size_t forward_count = 0;
    size_t backward_count = 0;
    Entry *entry;

    (void)unused_label;

    printf("%zu\n", cache->size);

    for (entry = cache->root.next; entry != &cache->root;
         entry = entry->next) {
        printf("%d %s %d\n", entry->key, entry->value, entry->hits);
        ++forward_count;
    }

    for (entry = cache->root.previous; entry != &cache->root;
         entry = entry->previous) {
        printf("%d\n", entry->key);
        ++backward_count;
    }

    printf("%zu %zu %zu\n",
           forward_count, backward_count, cache->size);
}

static void cache_rotate(Cache *cache, int count)
{
    while (count-- > 0 && cache->size > 1) {
        Entry *entry = cache->root.next;
        entry_remove(entry);
        entry_insert_before(&cache->root, entry);
    }
}

static void cache_destroy(Cache *cache)
{
    Entry *entry = cache->root.next;

    while (entry != &cache->root) {
        Entry *next = entry->next;
        free(entry);
        entry = next;
    }

    cache->root.next = &cache->root;
    cache->root.previous = &cache->root;
    cache->size = 0;
}

int main(void)
{
    int keys[12] = { 1, 2, 3, 2, 4, 1, 5, 3, 6, 2, 2, 7 };
    Cache cache;
    char value[24];
    unsigned int i;
    double hit_rate;

    cache_init(&cache, 4);

    for (i = 0; i <= 11; ++i) {
        snprintf(value, sizeof(value), "value-%d", keys[i]);
        printf("%d\n", keys[i]);
        cache_put(&cache, keys[i], value);
    }

    cache_print(&cache, "after inserts");

    printf("%d\n", cache_find(&cache, 2) != NULL);
    printf("%d\n", cache_find(&cache, 99) != NULL);

    cache_print(&cache, "after lookups");
    cache_rotate(&cache, 2);
    cache_print(&cache, "after rotation");

    hit_rate = (double)(cache.lookups - cache.misses) * 100.0
             / (double)cache.lookups;

    printf("stats %zu %zu %zu %.2f\n",
           cache.lookups, cache.misses, cache.evictions, hit_rate);

    cache_destroy(&cache);
    return 0;
}