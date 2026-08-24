#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Entry Entry;

struct Entry {
    Entry *next;
    uint32_t hash;
    uint16_t length;
    uint16_t padding;
    uint32_t count;
    uint32_t reserved;
    double total;
    uint8_t category;
    uint8_t unused[7];
    char key[];
};

typedef struct {
    Entry **buckets;
    uint32_t capacity;
    uint32_t size;
    uint32_t growths;
    uint32_t collisions;
    unsigned long long probes;
} Table;

typedef struct {
    uint32_t count;
    uint32_t bucket;
} BucketInfo;

static uint32_t hash_text(const char *text)
{
    size_t length = strlen(text);
    uint32_t hash = UINT32_C(0x811c9dc5);

    for (size_t i = 0; i < length; ++i) {
        hash ^= (unsigned char)text[i];
        hash *= UINT32_C(0x01000193);
    }

    return hash ^ (hash >> 15);
}

static Entry *find_entry(Table *table, const char *key)
{
    uint32_t hash = hash_text(key);
    Entry *entry = table->buckets[hash & (table->capacity - 1)];
    unsigned long long examined = 0;

    while (entry != NULL) {
        ++examined;
        if (entry->hash == hash && strcmp(entry->key, key) == 0)
            break;
        entry = entry->next;
    }

    table->probes += examined;
    return entry;
}

static int remove_entry(Table *table, const char *key)
{
    uint32_t hash = hash_text(key);
    Entry **link = &table->buckets[hash & (table->capacity - 1)];

    while (*link != NULL) {
        Entry *entry = *link;

        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            *link = entry->next;
            free(entry);
            --table->size;
            return 1;
        }

        link = &entry->next;
    }

    return 0;
}

static int compare_buckets(const void *left, const void *right)
{
    const BucketInfo *a = left;
    const BucketInfo *b = right;

    if (a->count != b->count)
        return (int)(b->count - a->count);

    return (int)(a->bucket - b->bucket);
}

static void print_statistics(const Table *table)
{
    BucketInfo *info = calloc(table->capacity, sizeof(*info));
    uint32_t occupied = 0;
    uint32_t maximum = 0;

    if (info == NULL)
        exit(1);

    for (uint32_t i = 0; i < table->capacity; ++i) {
        info[i].bucket = i;

        for (Entry *entry = table->buckets[i];
             entry != NULL;
             entry = entry->next) {
            ++info[i].count;
        }

        if (info[i].count != 0) {
            ++occupied;
            if (info[i].count > maximum)
                maximum = info[i].count;
        }
    }

    qsort(info, table->capacity, sizeof(*info), compare_buckets);

    printf("%u %u %.3f %u %u %u %u %llu\n",
           table->size,
           table->capacity,
           (double)table->size / (double)table->capacity,
           occupied,
           maximum,
           table->growths,
           table->collisions,
           table->probes);

    for (uint32_t i = 0; i < table->capacity && i < 5; ++i)
        printf("%u: %u ", info[i].bucket, info[i].count);

    putc('\n', stdout);
    free(info);
}

static void grow_table(Table *table)
{
    uint32_t new_capacity = table->capacity * 2;
    Entry **new_buckets = calloc(new_capacity, sizeof(*new_buckets));

    if (new_buckets == NULL)
        exit(1);

    for (uint32_t i = 0; i < table->capacity; ++i) {
        Entry *entry = table->buckets[i];

        while (entry != NULL) {
            Entry *next = entry->next;
            uint32_t bucket = entry->hash & (new_capacity - 1);

            entry->next = new_buckets[bucket];
            new_buckets[bucket] = entry;
            entry = next;
        }
    }

    free(table->buckets);
    table->buckets = new_buckets;
    table->capacity = new_capacity;
    ++table->growths;
}

int main(void)
{
    static const char *const inputs[20] = {
        "",
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
        "banana"
    };

    Table table = {0};

    table.capacity = 8;
    table.buckets = calloc(table.capacity, sizeof(*table.buckets));
    if (table.buckets == NULL)
        exit(1);

    for (uint32_t i = 0; i < 20; ++i) {
        const char *key = inputs[i];
        double value = (double)(i % 5) + 0.5;
        Entry *entry = find_entry(&table, key);

        if (entry != NULL) {
            ++entry->count;
            entry->total += value;
            continue;
        }

        if (table.size * 4 >= table.capacity * 3)
            grow_table(&table);

        size_t length = strlen(key);
        uint32_t hash = hash_text(key);
        uint32_t bucket = hash & (table.capacity - 1);

        entry = calloc(1, sizeof(*entry) + length + 1);
        if (entry == NULL)
            exit(1);

        memcpy(entry->key, key, length + 1);
        entry->hash = hash;
        entry->length = (uint16_t)length;
        entry->count = 1;
        entry->total = value;
        entry->category = (uint8_t)(i % 3);

        if (table.buckets[bucket] != NULL)
            ++table.collisions;

        entry->next = table.buckets[bucket];
        table.buckets[bucket] = entry;
        ++table.size;
    }

    for (uint32_t i = 0; i < table.capacity; ++i) {
        Entry *entry = table.buckets[i];

        if (entry == NULL)
            continue;

        printf("%u ", i);

        while (entry != NULL) {
            printf(" %s(%hu,%u,%.1f,%u,%x)",
                   entry->key,
                   entry->length,
                   entry->count,
                   entry->total,
                   entry->category,
                   entry->hash);
            entry = entry->next;
        }

        putc('\n', stdout);
    }

    print_statistics(&table);

    Entry *entry = find_entry(&table, "banana");
    if (entry != NULL)
        printf("%u %.1f %u", entry->count, entry->total, entry->category);

    printf(" %u\n", find_entry(&table, "missing") != NULL);

    int removed_present = remove_entry(&table, "apple");
    int removed_missing = remove_entry(&table, "missing");
    printf(" %u %u\n", removed_present, removed_missing);

    print_statistics(&table);

    for (uint32_t i = 0; i < table.capacity; ++i) {
        Entry *entry = table.buckets[i];

        while (entry != NULL) {
            Entry *next = entry->next;
            free(entry);
            entry = next;
        }
    }

    free(table.buckets);
    return 0;
}