#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node Node;

struct Node {
    Node *next;
    Node *prev;
    int key;
    char value[24];
    int hits;
};

typedef struct {
    Node head;
    size_t size;
    size_t capacity;
    size_t accesses;
    size_t misses;
    size_t evictions;
} Cache;

static void unlink_node(Node *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;
}

static void append_mru(Cache *cache, Node *node)
{
    node->next = &cache->head;
    node->prev = cache->head.prev;
    node->prev->next = node;
    cache->head.prev = node;
}

static Node *cache_access(Cache *cache, int key)
{
    Node *node;

    cache->accesses++;

    for (node = cache->head.prev;
         node != &cache->head;
         node = node->prev) {
        if (node->key == key) {
            node->hits++;
            unlink_node(node);
            append_mru(cache, node);
            return node;
        }
    }

    cache->misses++;
    return NULL;
}

static void print_cache(Cache *cache, const char *unused_label)
{
    Node *node;
    size_t reverse_count = 0;
    size_t forward_count = 0;

    (void)unused_label;

    printf("%zu\n", cache->size);

    for (node = cache->head.prev;
         node != &cache->head;
         node = node->prev) {
        printf("%d %s %d\n", node->key, node->value, node->hits);
        reverse_count++;
    }

    for (node = cache->head.next;
         node != &cache->head;
         node = node->next) {
        printf("%d ", node->key);
        forward_count++;
    }

    printf("\n%zu %zu %zu\n",
           reverse_count, forward_count, cache->size);
}

int main(void)
{
    int sequence[] = { 1, 2, 3, 2, 4, 1, 5, 3, 6, 2, 2, 7 };
    Cache cache = {0};
    char text[24];

    cache.head.next = &cache.head;
    cache.head.prev = &cache.head;
    cache.capacity = 4;

    for (size_t i = 0; i < sizeof sequence / sizeof sequence[0]; i++) {
        int key = sequence[i];
        Node *node;

        snprintf(text, sizeof text, "value-%d", key);
        printf("%d\n", key);

        node = cache_access(&cache, key);
        if (node != NULL) {
            strncpy(node->value, text, sizeof node->value - 1);
            continue;
        }

        if (cache.size == cache.capacity) {
            node = cache.head.next;
            printf("%d %s %d\n", node->key, node->value, node->hits);
            unlink_node(node);
            free(node);
            cache.size--;
            cache.evictions++;
        }

        node = calloc(1, sizeof *node);
        if (node == NULL)
            exit(1);

        node->key = key;
        strncpy(node->value, text, sizeof node->value - 1);
        append_mru(&cache, node);
        cache.size++;
    }

    print_cache(&cache, "after sequence");

    printf("%d\n", cache_access(&cache, 2) != NULL);
    printf("%d\n", cache_access(&cache, 99) != NULL);

    print_cache(&cache, "after lookups");

    if (cache.size > 1) {
        for (int i = 0; i < 2; i++) {
            Node *node = cache.head.prev;

            unlink_node(node);

            node->next = cache.head.next;
            node->prev = &cache.head;
            cache.head.next->prev = node;
            cache.head.next = node;
        }
    }

    print_cache(&cache, "after rotations");

    {
        double hit_percentage =
            (double)(cache.accesses - cache.misses) * 100.0 /
            (double)cache.accesses;

        printf("%zu %zu %.2f%% %zu\n",
               cache.accesses, cache.misses,
               hit_percentage, cache.evictions);
    }

    while (cache.head.prev != &cache.head) {
        Node *node = cache.head.prev;
        cache.head.prev = node->prev;
        free(node);
    }

    return 0;
}