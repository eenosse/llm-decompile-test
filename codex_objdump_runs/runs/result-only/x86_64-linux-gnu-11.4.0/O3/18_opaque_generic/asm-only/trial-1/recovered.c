#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef RODATA_FLOAT_4020B0_BITS
#define RODATA_FLOAT_4020B0_BITS UINT32_C(0)
#endif

#ifndef RODATA_FLOAT_4020B4_BITS
#define RODATA_FLOAT_4020B4_BITS UINT32_C(0)
#endif

#ifndef RODATA_WORD0_4020B8
#define RODATA_WORD0_4020B8 UINT32_C(0)
#endif

#ifndef RODATA_WORD1_4020BC
#define RODATA_WORD1_4020BC UINT32_C(0)
#endif

typedef struct {
    uint16_t first;
    uint8_t second;
    uint8_t third;
} Payload1;

typedef struct {
    int32_t first;
    int32_t second;
    int32_t third;
    int32_t fourth;
    uint8_t mask;
} Payload2;

typedef struct {
    uint16_t first;
    uint16_t second;
    float value;
} Payload3;

typedef struct {
    uint16_t length;
    char text[24];
} Payload4;

typedef union {
    Payload1 p1;
    Payload2 p2;
    Payload3 p3;
    Payload4 p4;
} Payload;

typedef struct {
    uint32_t kind;
    uint64_t timestamp;
    Payload payload;
} Record;

typedef void (*DiscardCallback)(const void *, void *);

typedef struct {
    unsigned char *storage;
    size_t element_size;
    size_t capacity;
    size_t head;
    size_t count;
    size_t pushes;
    size_t discards;
    DiscardCallback discard_callback;
    void *callback_context;
} Ring;

typedef struct {
    unsigned char *storage;
    size_t capacity;
    size_t used;
    size_t high_water;
    uint32_t allocations;
} Arena;

typedef struct {
    uint16_t value;
    uint8_t unused;
    uint8_t flag;
} ArenaItem1;

typedef struct {
    uint32_t first;
    uint32_t second;
    uint32_t third;
    uint32_t fourth;
    uint8_t kind;
} ArenaItem2;

typedef struct {
    uint16_t width;
    uint16_t height;
    float scale;
} ArenaItem3;

_Static_assert(sizeof(Record) == 48, "unexpected Record layout");
_Static_assert(offsetof(Record, timestamp) == 8, "unexpected timestamp offset");
_Static_assert(offsetof(Record, payload) == 16, "unexpected payload offset");
_Static_assert(sizeof(Ring) == 72, "unexpected Ring layout");
_Static_assert(sizeof(Arena) == 40, "unexpected Arena layout");
_Static_assert(sizeof(ArenaItem1) == 4, "unexpected ArenaItem1 layout");
_Static_assert(sizeof(ArenaItem2) == 20, "unexpected ArenaItem2 layout");
_Static_assert(sizeof(ArenaItem3) == 8, "unexpected ArenaItem3 layout");

static void fail_allocation(void)
{
    exit(1);
}

static Ring *ring_create(size_t element_size, size_t capacity)
{
    Ring *ring = calloc(1, sizeof(*ring));

    if (ring == NULL)
        fail_allocation();

    ring->storage = calloc(capacity, element_size);
    if (ring->storage == NULL)
        fail_allocation();

    ring->element_size = element_size;
    ring->capacity = capacity;
    return ring;
}

static void *ring_at(Ring *ring, size_t index)
{
    size_t physical = (ring->head + index) % ring->capacity;
    return ring->storage + physical * ring->element_size;
}

static void ring_push(Ring *ring, const void *element)
{
    size_t physical;

    ++ring->pushes;

    if (ring->count == ring->capacity) {
        if (ring->discard_callback != NULL)
            ring->discard_callback(ring_at(ring, 0), ring->callback_context);

        ring->head = (ring->head + 1) % ring->capacity;
        --ring->count;
        ++ring->discards;
    }

    physical = (ring->head + ring->count) % ring->capacity;
    memcpy(ring->storage + physical * ring->element_size,
           element, ring->element_size);
    ++ring->count;
}

static int ring_pop(Ring *ring, void *destination)
{
    if (ring->count == 0)
        return 0;

    memcpy(destination, ring_at(ring, 0), ring->element_size);
    ring->head = (ring->head + 1) % ring->capacity;
    --ring->count;
    return 1;
}

static void print_record_identity(const Record *record)
{
    printf("%u %lu\n",
           record->kind,
           (unsigned long)record->timestamp);
}

static void discard_record(const void *element, void *context)
{
    int32_t *discard_count = context;

    ++*discard_count;
    print_record_identity(element);
}

static float float_from_bits(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void print_record(size_t index, const Record *record)
{
    printf("#%zu %lu %u ",
           index,
           (unsigned long)record->timestamp,
           record->kind);

    switch (record->kind) {
    case 0:
        putchar('\n');
        break;

    case 1:
        printf("%u %u %u\n",
               (unsigned)record->payload.p1.first,
               (unsigned)record->payload.p1.second,
               (unsigned)record->payload.p1.third);
        break;

    case 2:
        printf("%d %d %d %d 0x%x\n",
               record->payload.p2.first,
               record->payload.p2.second,
               record->payload.p2.third,
               record->payload.p2.fourth,
               (unsigned)record->payload.p2.mask);
        break;

    case 3:
        printf("%u %u %.2f\n",
               (unsigned)record->payload.p3.first,
               (unsigned)record->payload.p3.second,
               (double)record->payload.p3.value);
        break;

    case 4:
        printf("%u %s\n",
               (unsigned)record->payload.p4.length,
               record->payload.p4.text);
        break;

    default:
        break;
    }
}

static Arena *arena_create(size_t capacity)
{
    Arena *arena = calloc(1, sizeof(*arena));

    if (arena == NULL)
        fail_allocation();

    arena->storage = calloc(1, capacity);
    if (arena->storage == NULL)
        fail_allocation();

    arena->capacity = capacity;
    return arena;
}

static void *arena_allocate(Arena *arena, size_t size, size_t alignment)
{
    size_t aligned = (arena->used + alignment - 1) & ~(alignment - 1);
    size_t end = aligned + size;
    void *result;

    if (end > arena->capacity)
        return NULL;

    result = arena->storage + aligned;
    arena->used = end;

    if (end > arena->high_water)
        arena->high_water = end;

    ++arena->allocations;
    return result;
}

int main(void)
{
    Ring *records = ring_create(sizeof(Record), 5);
    Ring *integers = ring_create(sizeof(int32_t), 8);
    Arena *arena = arena_create(1024);
    int32_t discard_count = 0;
    float scale = float_from_bits(RODATA_FLOAT_4020B0_BITS);
    float bias = float_from_bits(RODATA_FLOAT_4020B4_BITS);

    records->discard_callback = discard_record;
    records->callback_context = &discard_count;

    for (uint32_t i = 0; i < 9; ++i) {
        Record record;

        memset(&record, 0, sizeof(record));
        record.kind = (i & 3U) == 0 ? 1U :
                      (i & 3U) == 1 ? 2U :
                      (i & 3U) == 2 ? 3U : 4U;
        record.timestamp = UINT64_C(5000) + (uint64_t)i * 17U;

        switch (record.kind) {
        case 1:
            record.payload.p1.first = (uint16_t)(i + 64U);
            record.payload.p1.second = (uint8_t)(i & 7U);
            record.payload.p1.third = (uint8_t)(i & 1U);
            break;

        case 2:
            record.payload.p2.first = (int32_t)i * 30;
            record.payload.p2.second = 200 - (int32_t)i * 12;
            record.payload.p2.third = (int32_t)i;
            record.payload.p2.fourth = -(int32_t)i;
            record.payload.p2.mask = (uint8_t)(1U << (i % 3U));
            break;

        case 3:
            record.payload.p3.first = (uint16_t)(800U + i * 16U);
            record.payload.p3.second = (uint16_t)(600U + i * 9U);
            record.payload.p3.value = (float)(i % 3U) * scale + bias;
            break;

        case 4:
            snprintf(record.payload.p4.text,
                     sizeof(record.payload.p4.text),
                     "message-%u", i);
            record.payload.p4.length =
                (uint16_t)strlen(record.payload.p4.text);
            break;
        }

        ring_push(records, &record);
    }

    printf("R %zu %zu %zu %d\n",
           records->count,
           records->pushes,
           records->discards,
           discard_count);

    for (size_t i = 0; i < records->count; ++i)
        print_record(i, ring_at(records, i));

    {
        Record record;

        while (ring_pop(records, &record))
            print_record_identity(&record);
    }

    for (int32_t i = 0; i < 6; ++i) {
        int32_t value = i * i;
        ring_push(integers, &value);
    }

    for (size_t i = 0; i < integers->count; ++i) {
        int32_t *value = ring_at(integers, i);
        *value += (int32_t)i * 10;
    }

    {
        int32_t value;

        while (ring_pop(integers, &value))
            printf("%d\n", value);
    }

    putchar('\n');

    {
        ArenaItem1 *first = arena_allocate(arena, sizeof(*first), 4);
        ArenaItem2 *second = arena_allocate(arena, sizeof(*second), 8);
        ArenaItem3 *third = arena_allocate(arena, sizeof(*third), 8);
        char *text = arena_allocate(arena, 32, 1);

        first->value = 27;
        first->flag = 1;

        second->first = RODATA_WORD0_4020B8;
        second->second = RODATA_WORD1_4020BC;
        second->kind = 4;

        third->width = 1920;
        third->height = 1080;
        third->scale = 2.0f;

        memcpy(text, "arena string", sizeof("arena string"));

        printf("%zu %zu %u %u %u %u %u %u %s\n",
               arena->used,
               arena->high_water,
               arena->allocations,
               (unsigned)first->value,
               second->first,
               second->second,
               (unsigned)third->width,
               (unsigned)third->height,
               text);
    }

    free(records->storage);
    free(records);
    free(integers->storage);
    free(integers);
    free(arena->storage);
    free(arena);

    return 0;
}