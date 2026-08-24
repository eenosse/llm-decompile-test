#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct record record;

struct record {
    record *next;
    uint32_t hash;
    uint16_t text_length;
    uint16_t reserved0;
    uint32_t count;
    uint32_t reserved1;
    double total;
    uint8_t category;
    uint8_t reserved2[7];
    char text[];
};

typedef struct {
    record **buckets;
    uint32_t capacity;
    uint32_t size;
    uint32_t resize_count;
    uint32_t collision_count;
    unsigned long long probes;
} table;

typedef struct {
    uint32_t chain_length;
    uint32_t bucket_index;
} bucket_info;

_Static_assert(sizeof(record) == 40, "unexpected record layout");
_Static_assert(offsetof(record, text) == 40, "unexpected text offset");

static const char key_alpha[] = "alpha";
static const char key_beta[] = "beta";
static const char key_gamma[] = "gamma";

static const char *const input_texts[20] = {
    key_alpha,
    key_beta,
    key_gamma,
    "delta",
    "epsilon",
    "zeta",
    "eta",
    "theta",
    "iota",
    "kappa",
    key_alpha,
    key_beta,
    key_gamma,
    "lambda",
    "mu",
    "nu",
    "xi",
    "omicron",
    "pi",
    "rho"
};

static uint32_t hash_bytes(const void *data, size_t length)
{
    const unsigned char *p = data;
    uint32_t hash = UINT32_C(0x811c9dc5);

    while (length != 0) {
        hash ^= *p++;
        hash *= UINT32_C(0x01000193);
        --length;
    }

    return hash ^ (hash >> 15);
}

static record *find_record(table *t, const char *text)
{
    uint32_t hash = hash_bytes(text, strlen(text));
    uint32_t index = hash & (t->capacity - 1U);
    record *item = t->buckets[index];
    uint32_t probes = 0;

    while (item != NULL) {
        ++probes;
        if (item->hash == hash && strcmp(item->text, text) == 0)
            break;
        item = item->next;
    }

    t->probes += probes;
    return item;
}

static int remove_record(table *t, const char *text)
{
    uint32_t hash = hash_bytes(text, strlen(text));
    uint32_t index = hash & (t->capacity - 1U);
    record **link = &t->buckets[index];

    while (*link != NULL) {
        record *item = *link;

        if (item->hash == hash && strcmp(item->text, text) == 0) {
            *link = item->next;
            free(item);
            --t->size;
            return 1;
        }

        link = &item->next;
    }

    return 0;
}

static void resize_table(table *t)
{
    uint32_t old_capacity = t->capacity;
    uint32_t new_capacity = old_capacity * 2U;
    record **new_buckets = calloc(new_capacity, sizeof(*new_buckets));

    if (new_buckets == NULL)
        exit(1);

    for (uint32_t i = 0; i < old_capacity; ++i) {
        record *item = t->buckets[i];

        while (item != NULL) {
            record *next = item->next;
            uint32_t index = item->hash & (new_capacity - 1U);

            item->next = new_buckets[index];
            new_buckets[index] = item;
            item = next;
        }
    }

    free(t->buckets);
    t->buckets = new_buckets;
    t->capacity = new_capacity;
    ++t->resize_count;
}

static void insert_record(table *t, const char *text, double value,
                          uint8_t category)
{
    record *item = find_record(t, text);

    if (item != NULL) {
        ++item->count;
        item->total += value;
        return;
    }

    if (t->size * 4U >= t->capacity * 3U)
        resize_table(t);

    size_t length = strlen(text);
    uint32_t hash = hash_bytes(text, length);
    uint32_t index = hash & (t->capacity - 1U);

    item = calloc(1, sizeof(*item) + length + 1U);
    if (item == NULL)
        exit(1);

    memcpy(item->text, text, length + 1U);
    item->hash = hash;
    item->text_length = (uint16_t)length;
    item->count = 1;
    item->total = value;
    item->category = category;

    if (t->buckets[index] != NULL)
        ++t->collision_count;

    item->next = t->buckets[index];
    t->buckets[index] = item;
    ++t->size;
}

static int compare_bucket_info(const void *left, const void *right)
{
    const bucket_info *a = left;
    const bucket_info *b = right;

    if (a->chain_length != b->chain_length)
        return (int)(b->chain_length - a->chain_length);

    return (int)(a->bucket_index - b->bucket_index);
}

static void print_statistics(const table *t)
{
    bucket_info *info = calloc(t->capacity, sizeof(*info));
    uint32_t occupied = 0;
    uint32_t maximum_chain = 0;

    if (info == NULL)
        exit(1);

    for (uint32_t i = 0; i < t->capacity; ++i) {
        uint32_t length = 0;

        info[i].bucket_index = i;
        for (record *item = t->buckets[i]; item != NULL; item = item->next)
            ++length;

        info[i].chain_length = length;
        if (length != 0) {
            ++occupied;
            if (length > maximum_chain)
                maximum_chain = length;
        }
    }

    qsort(info, t->capacity, sizeof(*info), compare_bucket_info);

    printf("%u %u %.3f %u %u %u %u %llu\n",
           t->size,
           t->capacity,
           (double)t->size / (double)t->capacity,
           occupied,
           maximum_chain,
           t->resize_count,
           t->collision_count,
           t->probes);

    for (uint32_t i = 0; i < t->capacity && i < 5U; ++i)
        printf(" %u %u", info[i].bucket_index, info[i].chain_length);

    putchar('\n');
    free(info);
}

static void print_table(const table *t)
{
    for (uint32_t i = 0; i < t->capacity; ++i) {
        record *item = t->buckets[i];

        if (item == NULL)
            continue;

        printf("%u", i);
        while (item != NULL) {
            printf(" %s %u %u %.1f %u %u",
                   item->text,
                   (unsigned)item->text_length,
                   item->count,
                   item->total,
                   (unsigned)item->category,
                   item->hash);
            item = item->next;
        }
        putchar('\n');
    }
}

static void destroy_table(table *t)
{
    for (uint32_t i = 0; i < t->capacity; ++i) {
        record *item = t->buckets[i];

        while (item != NULL) {
            record *next = item->next;
            free(item);
            item = next;
        }
    }

    free(t->buckets);
}

int main(void)
{
    table t = {0};

    t.buckets = calloc(8, sizeof(*t.buckets));
    if (t.buckets == NULL)
        exit(1);
    t.capacity = 8;

    for (uint32_t i = 0; i < 20U; ++i) {
        double value = (double)(i % 5U) + 0.5;
        insert_record(&t, input_texts[i], value, (uint8_t)(i % 3U));
    }

    print_table(&t);
    print_statistics(&t);

    record *item = find_record(&t, key_alpha);
    if (item != NULL)
        printf("%u %.1f %u\n",
               item->count, item->total, (unsigned)item->category);

    item = find_record(&t, "missing");
    printf("%d\n", item != NULL);

    {
        int first = remove_record(&t, key_gamma);
        int second = remove_record(&t, "missing");
        printf("%d %d\n", first, second);
    }

    print_statistics(&t);
    destroy_table(&t);
    return 0;
}