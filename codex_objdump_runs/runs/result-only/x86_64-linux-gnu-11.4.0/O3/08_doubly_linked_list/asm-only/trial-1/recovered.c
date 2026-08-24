#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Entry {
    struct Entry *previous;
    struct Entry *next;
    unsigned int key;
    char value[24];
    unsigned int hits;
} Entry;

typedef struct Cache {
    Entry root;
    size_t size;
    size_t capacity;
    size_t accesses;
    size_t misses;
    size_t evictions;
} Cache;

static void unlink_entry(Entry *entry)
{
    entry->previous->next = entry->next;
    entry->next->previous = entry->previous;
}

static void insert_front(Cache *cache, Entry *entry)
{
    entry->previous = &cache->root;
    entry->next = cache->root.next;
    cache->root.next->previous = entry;
    cache->root.next = entry;
}

static Entry *cache_lookup(Cache *cache, unsigned int key)
{
    Entry *entry;

    ++cache->accesses;

    for (entry = cache->root.next; entry != &cache->root;
         entry = entry->next) {
        if (entry->key == key) {
            ++entry->hits;
            unlink_entry(entry);
            insert_front(cache, entry);
            return entry;
        }
    }

    ++cache->misses;
    return NULL;
}

static void print_cache(const Cache *cache)
{
    const Entry *entry;
    size_t forward_count = 0;
    size_t reverse_count = 0;

    printf("%zu\n", cache->capacity);

    for (entry = cache->root.next; entry != &cache->root;
         entry = entry->next) {
        printf("%u %s %u\n", entry->key, entry->value, entry->hits);
        ++forward_count;
    }

    for (entry = cache->root.previous; entry != &cache->root;
         entry = entry->previous) {
        printf("%u ", entry->key);
        ++reverse_count;
    }

    printf("%zu %zu %zu\n",
           forward_count, reverse_count, cache->capacity);
}

int main(void)
{
    unsigned int requests[12] = {
        1, 2, 3, 1, 4, 5, 2, 1, 2, 3, 4, 5
    };
    Cache cache = {
        .root = { 0 },
        .size = 0,
        .capacity = 4,
        .accesses = 0,
        .misses = 0,
        .evictions = 0
    };
    char value[24];
    size_t i;

    cache.root.previous = &cache.root;
    cache.root.next = &cache.root;

    for (i = 0; i < sizeof requests / sizeof requests[0]; ++i) {
        unsigned int key = requests[i];
        Entry *entry;

        snprintf(value, sizeof value, "value-%u", key);
        printf("%u\n", key);

        entry = cache_lookup(&cache, key);
        if (entry != NULL) {
            strncpy(entry->value, value, sizeof entry->value - 1);
            continue;
        }

        if (cache.size == cache.capacity) {
            Entry *victim = cache.root.previous;

            printf("%u %s %u\n",
                   victim->key, victim->value, victim->hits);
            unlink_entry(victim);
            free(victim);
            --cache.size;
            ++cache.evictions;
        }

        entry = calloc(1, sizeof *entry);
        if (entry == NULL)
            exit(1);

        entry->key = key;
        strncpy(entry->value, value, sizeof entry->value - 1);
        insert_front(&cache, entry);
        ++cache.size;
    }

    print_cache(&cache);
    printf("%d\n", cache_lookup(&cache, 2) != NULL);
    printf("%d\n", cache_lookup(&cache, 99) != NULL);
    print_cache(&cache);

    if (cache.size > 1) {
        Entry *entry = cache.root.previous;
        unlink_entry(entry);
        insert_front(&cache, entry);

        entry = cache.root.previous;
        unlink_entry(entry);
        insert_front(&cache, entry);
    }

    print_cache(&cache);

    printf("%zu %zu %.2f%% %zu\n",
           cache.accesses,
           cache.misses,
           (double)(cache.accesses - cache.misses) * 100.0 /
               (double)cache.accesses,
           cache.evictions);

    while (cache.root.next != &cache.root) {
        Entry *entry = cache.root.next;
        cache.root.next = entry->next;
        free(entry);
    }

    return 0;
}