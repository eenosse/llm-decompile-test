#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Entry Entry;

struct Entry {
    Entry *next;
    uint32_t hash;
    uint16_t key_length;
    uint16_t reserved;
    uint32_t count;
    uint32_t padding;
    double total;
    uint8_t category;
    uint8_t unused[7];
    char key[];
};

typedef struct {
    Entry **buckets;
    uint32_t capacity;
    uint32_t size;
    uint32_t resize_count;
    uint32_t collision_count;
    uint64_t probes;
} Table;

typedef struct {
    uint32_t count;
    uint32_t index;
} BucketInfo;

/*
 * The supplied evidence does not contain the bytes of these string literals
 * or of the floating-point constant. Neutral stand-ins preserve the recovered
 * aggregate shape and control flow.
 */
static const char *const input_values[20] = {
    "sample0000",
    "key000",
    "sample000",
    "sample0001",
    "sample0002",
    "key001",
    "sample0003",
    "sample0004",
    "key000",
    "sample000",
    "sample0005",
    "sample0006",
    "sample0000",
    "sample0007",
    "sample0008",
    "key002",
    "sample0009",
    "sample0010",
    "key000",
    "sample000"
};

static uint32_t hash_string(const char *text)
{
    uint32_t hash = UINT32_C(0x811c9dc5);

    while (*text != '\0') {
        hash ^= (unsigned char)*text++;
        hash *= UINT32_C(0x01000193);
    }

    return hash ^ (hash >> 15);
}

static Entry *table_find(Table *table, const char *key)
{
    uint32_t hash = hash_string(key);
    uint32_t index = hash & (table->capacity - 1);
    Entry *entry = table->buckets[index];
    uint64_t probes = 0;

    while (entry != NULL) {
        ++probes;
        if (entry->hash == hash && strcmp(entry->key, key) == 0)
            break;
        entry = entry->next;
    }

    table->probes += probes;
    return entry;
}

static void table_resize(Table *table)
{
    uint32_t old_capacity = table->capacity;
    uint32_t new_capacity = old_capacity * 2;
    Entry **new_buckets = calloc(new_capacity, sizeof(*new_buckets));

    if (new_buckets == NULL)
        exit(1);

    for (uint32_t i = 0; i < old_capacity; ++i) {
        Entry *entry = table->buckets[i];

        while (entry != NULL) {
            Entry *next = entry->next;
            uint32_t index = entry->hash & (new_capacity - 1);

            entry->next = new_buckets[index];
            new_buckets[index] = entry;
            entry = next;
        }
    }

    free(table->buckets);
    table->buckets = new_buckets;
    table->capacity = new_capacity;
    ++table->resize_count;
}

static void table_insert(Table *table, const char *key, double value,
                         uint8_t category)
{
    size_t length = strlen(key);
    uint32_t hash = hash_string(key);
    uint32_t index = hash & (table->capacity - 1);
    Entry *entry = table->buckets[index];
    uint64_t probes = 0;

    while (entry != NULL) {
        ++probes;
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            table->probes += probes;
            ++entry->count;
            entry->total += value;
            return;
        }
        entry = entry->next;
    }

    table->probes += probes;

    if (table->size * 4U >= table->capacity * 3U)
        table_resize(table);

    length = strlen(key);
    hash = hash_string(key);
    index = hash & (table->capacity - 1);

    entry = calloc(1, sizeof(*entry) + length + 1);
    if (entry == NULL)
        exit(1);

    memcpy(entry->key, key, length + 1);
    entry->hash = hash;
    entry->key_length = (uint16_t)length;
    entry->count = 1;
    entry->total = value;
    entry->category = category;

    if (table->buckets[index] != NULL)
        ++table->collision_count;

    entry->next = table->buckets[index];
    table->buckets[index] = entry;
    ++table->size;
}

static int table_remove(Table *table, const char *key)
{
    uint32_t hash = hash_string(key);
    uint32_t index = hash & (table->capacity - 1);
    Entry **link = &table->buckets[index];

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

    return (int)(a->index - b->index);
}

static void print_statistics(const Table *table)
{
    BucketInfo *info = calloc(table->capacity, sizeof(*info));
    uint32_t nonempty = 0;
    uint32_t maximum = 0;

    if (info == NULL)
        exit(1);

    for (uint32_t i = 0; i < table->capacity; ++i) {
        uint32_t count = 0;

        info[i].index = i;
        for (Entry *entry = table->buckets[i];
             entry != NULL;
             entry = entry->next)
            ++count;

        info[i].count = count;
        if (count != 0) {
            ++nonempty;
            if (count > maximum)
                maximum = count;
        }
    }

    qsort(info, table->capacity, sizeof(*info), compare_buckets);

    printf("%u %u %u %u %.2f %u %u %llu",
           table->size,
           table->capacity,
           nonempty,
           maximum,
           (double)table->size / (double)table->capacity,
           table->resize_count,
           table->collision_count,
           (unsigned long long)table->probes);

    for (uint32_t i = 0; i < table->capacity && i < 5; ++i)
        printf(" %u:%u", info[i].index, info[i].count);

    putchar('\n');
    free(info);
}

static void print_table(const Table *table)
{
    for (uint32_t i = 0; i < table->capacity; ++i) {
        Entry *entry = table->buckets[i];

        if (entry == NULL)
            continue;

        printf("%u", i);
        while (entry != NULL) {
            printf(" %s %u %u %.1f %u %08x",
                   entry->key,
                   (unsigned)entry->key_length,
                   entry->count,
                   entry->total,
                   (unsigned)entry->category,
                   entry->hash);
            entry = entry->next;
        }
        putchar('\n');
    }
}

static void table_destroy(Table *table)
{
    for (uint32_t i = 0; i < table->capacity; ++i) {
        Entry *entry = table->buckets[i];

        while (entry != NULL) {
            Entry *next = entry->next;
            free(entry);
            entry = next;
        }
    }

    free(table->buckets);
}

int main(void)
{
    Table table = {0};
    const double base_value = 0.5;

    table.capacity = 8;
    table.buckets = calloc(table.capacity, sizeof(*table.buckets));
    if (table.buckets == NULL)
        exit(1);

    for (uint32_t i = 0; i < 20; ++i) {
        double value = (double)(i % 5U) + base_value;
        table_insert(&table, input_values[i], value, (uint8_t)(i % 3U));
    }

    print_table(&table);
    print_statistics(&table);

    {
        Entry *entry = table_find(&table, "key000");
        if (entry != NULL)
            printf("%u %.2f %u\n",
                   entry->count, entry->total, (unsigned)entry->category);
    }

    {
        Entry *entry = table_find(&table, "absent00");
        printf("%d\n", entry != NULL);
    }

    {
        int first = table_remove(&table, "sample000");
        int second = table_remove(&table, "absent00");
        printf("%d %d\n", first, second);
    }

    print_statistics(&table);
    table_destroy(&table);
    return 0;
}