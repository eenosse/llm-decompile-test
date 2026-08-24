#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Record {
    struct Record *next;
    uint32_t hash;
    uint16_t length;
    uint8_t padding1[2];
    uint32_t count;
    uint8_t padding2[4];
    double total;
    uint8_t tag;
    uint8_t padding3[7];
    char key[];
} Record;

typedef struct {
    Record **buckets;
    uint32_t capacity;
    uint32_t size;
    uint32_t rehashes;
    uint32_t collisions;
    uint64_t probes;
} Map;

typedef struct {
    uint32_t count;
    uint32_t index;
} BucketStat;

static const char *sample_keys[20] = {
    "apple",
    "banana",
    "cherry",
    "date",
    "elderberry",
    "fig",
    "grape",
    "honeydew",
    "kiwi",
    "lemon",
    "mango",
    "nectarine",
    "orange",
    "papaya",
    "quince",
    "raspberry",
    "strawberry",
    "tangerine",
    "ugli",
    "watermelon"
};

static uint32_t hash_bytes(const unsigned char *data, size_t length)
{
    uint32_t hash = UINT32_C(0x811c9dc5);
    size_t i;

    for (i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= UINT32_C(0x01000193);
    }

    hash ^= hash >> 15;
    return hash;
}

static void map_init(Map *map, uint32_t capacity)
{
    memset(map, 0, sizeof(*map));
    map->buckets = calloc(capacity, sizeof(*map->buckets));
    if (map->buckets == NULL)
        exit(1);

    map->capacity = capacity;
}

static Record *record_create(const char *key, uint32_t hash)
{
    size_t length = strlen(key);
    Record *record = calloc(1, sizeof(*record) + length + 1);

    if (record == NULL)
        exit(1);

    memcpy(record->key, key, length + 1);
    record->length = (uint16_t)length;
    record->hash = hash;
    return record;
}

static void map_resize(Map *map)
{
    uint32_t new_capacity = map->capacity * 2;
    Record **new_buckets = calloc(new_capacity, sizeof(*new_buckets));
    uint32_t i;

    if (new_buckets == NULL)
        exit(1);

    for (i = 0; i < map->capacity; ++i) {
        Record *record = map->buckets[i];

        while (record != NULL) {
            Record *next = record->next;
            uint32_t index = record->hash & (new_capacity - 1);

            record->next = new_buckets[index];
            new_buckets[index] = record;
            record = next;
        }
    }

    free(map->buckets);
    map->buckets = new_buckets;
    map->capacity = new_capacity;
    ++map->rehashes;
}

static Record *map_find(Map *map, const char *key)
{
    uint32_t hash = hash_bytes((const unsigned char *)key, strlen(key));
    Record *record = map->buckets[hash & (map->capacity - 1)];
    uint32_t probes = 0;

    while (record != NULL) {
        ++probes;

        if (record->hash == hash && strcmp(record->key, key) == 0)
            break;

        record = record->next;
    }

    map->probes += probes;
    return record;
}

static Record *map_add(Map *map, const char *key, double value, uint8_t tag)
{
    Record *record = map_find(map, key);

    if (record != NULL) {
        ++record->count;
        record->total += value;
        return record;
    }

    if (map->size * 4 >= map->capacity * 3)
        map_resize(map);

    {
        uint32_t hash = hash_bytes((const unsigned char *)key, strlen(key));
        uint32_t index = hash & (map->capacity - 1);

        record = record_create(key, hash);
        record->count = 1;
        record->total = value;
        record->tag = tag;

        if (map->buckets[index] != NULL)
            ++map->collisions;

        record->next = map->buckets[index];
        map->buckets[index] = record;
        ++map->size;
    }

    return record;
}

static int map_remove(Map *map, const char *key)
{
    uint32_t hash = hash_bytes((const unsigned char *)key, strlen(key));
    Record **link = &map->buckets[hash & (map->capacity - 1)];

    while (*link != NULL) {
        Record *record = *link;

        if (record->hash == hash && strcmp(record->key, key) == 0) {
            *link = record->next;
            free(record);
            --map->size;
            return 1;
        }

        link = &record->next;
    }

    return 0;
}

static int compare_bucket_stats(const void *left, const void *right)
{
    const BucketStat *a = left;
    const BucketStat *b = right;

    if (a->count != b->count)
        return (int)(b->count - a->count);

    return (int)(a->index - b->index);
}

static void map_print_stats(Map *map)
{
    BucketStat *stats = calloc(map->capacity, sizeof(*stats));
    uint32_t nonempty = 0;
    uint32_t maximum = 0;
    uint32_t i;

    if (stats == NULL)
        exit(1);

    for (i = 0; i < map->capacity; ++i) {
        Record *record;

        stats[i].index = i;
        record = map->buckets[i];

        while (record != NULL) {
            ++stats[i].count;
            record = record->next;
        }

        if (stats[i].count != 0) {
            ++nonempty;
            if (stats[i].count > maximum)
                maximum = stats[i].count;
        }
    }

    qsort(stats, map->capacity, sizeof(*stats), compare_bucket_stats);

    printf("%u %u %u %u %.2f %u %u %lu\n",
           map->size,
           map->capacity,
           nonempty,
           maximum,
           (double)map->size / (double)map->capacity,
           map->rehashes,
           map->collisions,
           (unsigned long)map->probes);

    for (i = 0; i <= 4 && i < map->capacity; ++i)
        printf("%u:%u ", stats[i].index, stats[i].count);

    putchar('\n');
    free(stats);
}

static void map_dump(Map *map)
{
    uint32_t i;

    for (i = 0; i < map->capacity; ++i) {
        Record *record = map->buckets[i];

        if (record != NULL) {
            printf("%u", i);

            while (record != NULL) {
                printf(" %s(%hu,%u,%.1f,%u,%x)",
                       record->key,
                       record->length,
                       record->count,
                       record->total,
                       (unsigned)record->tag,
                       record->hash);
                record = record->next;
            }

            putchar('\n');
        }
    }
}

static void map_destroy(Map *map)
{
    uint32_t i;

    for (i = 0; i < map->capacity; ++i) {
        Record *record = map->buckets[i];

        while (record != NULL) {
            Record *next = record->next;
            free(record);
            record = next;
        }
    }

    free(map->buckets);
    memset(map, 0, sizeof(*map));
}

int main(void)
{
    Map map;
    Record *record;
    uint32_t i;
    int removed_present;
    int removed_absent;

    map_init(&map, 8);

    for (i = 0; i < 20; ++i) {
        map_add(&map,
                sample_keys[i],
                (double)(i % 5) + 0.5,
                (uint8_t)(i % 3));
    }

    map_dump(&map);
    map_print_stats(&map);

    record = map_find(&map, "banana");
    if (record != NULL)
        printf("%u %.1f %u\n",
               record->count,
               record->total,
               (unsigned)record->tag);

    record = map_find(&map, "notfound");
    printf("%d\n", record != NULL);

    removed_present = map_remove(&map, "raspberry");
    removed_absent = map_remove(&map, "notfound");
    printf("%d %d\n", removed_present, removed_absent);

    map_print_stats(&map);
    map_destroy(&map);
    return 0;
}