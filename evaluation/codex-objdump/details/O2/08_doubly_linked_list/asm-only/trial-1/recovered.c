#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node Node;

struct Node {
    Node *prev;
    Node *next;
    int32_t key;
    char value[24];
    int32_t hits;
};

typedef struct {
    Node root;
    size_t size;
    size_t capacity;
    size_t lookups;
    size_t misses;
    size_t evictions;
} Cache;

static Node *cache_lookup(Cache *cache, int32_t key)
{
    Node *node;

    ++cache->lookups;

    for (node = cache->root.next; node != &cache->root; node = node->next) {
        if (node->key == key) {
            ++node->hits;

            node->prev->next = node->next;
            node->next->prev = node->prev;

            node->prev = &cache->root;
            node->next = cache->root.next;
            cache->root.next->prev = node;
            cache->root.next = node;

            return node;
        }
    }

    ++cache->misses;
    return NULL;
}

static void cache_print(const Cache *cache)
{
    const Node *node;
    size_t forward_count = 0;
    size_t backward_count = 0;

    printf("%zu\n", cache->size);

    for (node = cache->root.next; node != &cache->root; node = node->next) {
        printf("%d %s get %d ", node->key, node->value, node->hits);
        ++forward_count;
    }

    for (node = cache->root.prev; node != &cache->root; node = node->prev) {
        printf("%d ", node->key);
        ++backward_count;
    }

    printf("%zu %zu: %zu\n",
           forward_count, backward_count, cache->size);
}

int main(void)
{
    static const int32_t keys[] = {
        1, 2, 3, 2, 4, 1, 5, 3, 6, 2, 2, 7
    };

    Cache cache = {0};
    char value[24];

    cache.root.prev = &cache.root;
    cache.root.next = &cache.root;
    cache.capacity = 4;

    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; ++i) {
        int32_t key = keys[i];
        Node *node;

        snprintf(value, sizeof value, "value-%d", key);
        printf("get %d ", key);

        node = cache_lookup(&cache, key);
        if (node != NULL) {
            strncpy(node->value, value, sizeof node->value - 1);
            continue;
        }

        if (cache.size == cache.capacity) {
            Node *victim = cache.root.prev;

            printf("%d %s get %d ",
                   victim->key, victim->value, victim->hits);

            victim->prev->next = victim->next;
            victim->next->prev = victim->prev;
            free(victim);

            --cache.size;
            ++cache.evictions;
        }

        node = calloc(1, sizeof *node);
        if (node == NULL)
            exit(1);

        node->key = key;
        strncpy(node->value, value, sizeof node->value - 1);

        node->prev = &cache.root;
        node->next = cache.root.next;
        cache.root.next->prev = node;
        cache.root.next = node;
        ++cache.size;
    }

    cache_print(&cache);

    printf("%d\n", cache_lookup(&cache, 2) != NULL);
    printf("%d\n", cache_lookup(&cache, 99) != NULL);

    cache_print(&cache);

    for (int i = 0; i < 2 && cache.size > 1; ++i) {
        Node *node = cache.root.prev;

        node->prev->next = node->next;
        node->next->prev = node->prev;

        node->prev = &cache.root;
        node->next = cache.root.next;
        cache.root.next->prev = node;
        cache.root.next = node;
    }

    cache_print(&cache);

    printf("%.2f%% %zu\n",
           (double)(cache.lookups - cache.misses) * 100.0
               / (double)cache.lookups,
           cache.evictions);

    while (cache.root.next != &cache.root) {
        Node *node = cache.root.next;
        cache.root.next = node->next;
        free(node);
    }

    return 0;
}