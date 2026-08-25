#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint16_t code;
    uint8_t modifiers;
    uint8_t pressed;
} Payload1;

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t dx;
    uint32_t dy;
    uint8_t buttons;
} Payload2;

typedef struct {
    uint16_t width;
    uint16_t height;
    float scale;
} Payload3;

typedef struct {
    uint16_t length;
    char text[24];
} Payload4;

typedef struct {
    uint32_t kind;
    uint32_t reserved;
    unsigned long long timestamp;
    union {
        Payload1 first;
        Payload2 second;
        Payload3 third;
        Payload4 fourth;
        unsigned char storage[32];
    } payload;
} Record;

typedef void (*OverwriteCallback)(void *, void *);
typedef void (*VisitCallback)(void *, size_t, void *);

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    size_t head;
    size_t count;
    size_t pushes;
    size_t overwrites;
    OverwriteCallback overwrite_callback;
    void *overwrite_context;
} Ring;

typedef struct {
    unsigned char *data;
    size_t capacity;
    size_t offset;
    size_t high_water;
    uint32_t allocations;
} Arena;

static Ring *ring_create(size_t element_size, size_t capacity)
{
    Ring *ring = calloc(1, sizeof(*ring));

    if (ring == NULL)
        exit(1);

    ring->data = calloc(capacity, element_size);
    if (ring->data == NULL)
        exit(1);

    ring->element_size = element_size;
    ring->capacity = capacity;
    return ring;
}

static void *ring_at(Ring *ring, size_t index)
{
    size_t physical_index = (ring->head + index) % ring->capacity;
    return ring->data + ring->element_size * physical_index;
}

static void ring_push(Ring *ring, const void *element)
{
    void *destination;

    ++ring->pushes;

    if (ring->count == ring->capacity) {
        if (ring->overwrite_callback != NULL) {
            ring->overwrite_callback(
                ring->data + ring->element_size * ring->head,
                ring->overwrite_context
            );
        }

        ring->head = (ring->head + 1) % ring->capacity;
        --ring->count;
        ++ring->overwrites;
    }

    destination = ring_at(ring, ring->count);
    memcpy(destination, element, ring->element_size);
    ++ring->count;
}

static int ring_pop(Ring *ring, void *output)
{
    if (ring->count == 0)
        return 0;

    memcpy(output,
           ring->data + ring->element_size * ring->head,
           ring->element_size);

    ring->head = (ring->head + 1) % ring->capacity;
    --ring->count;
    return 1;
}

static size_t ring_size(const Ring *ring)
{
    return ring->count;
}

static void ring_visit(Ring *ring, VisitCallback callback, void *context)
{
    size_t i;

    for (i = 0; i < ring->count; ++i)
        callback(ring_at(ring, i), i, context);
}

static void ring_destroy(Ring *ring)
{
    free(ring->data);
    free(ring);
}

static Arena *arena_create(size_t capacity)
{
    Arena *arena = calloc(1, sizeof(*arena));

    if (arena == NULL)
        exit(1);

    arena->data = calloc(1, capacity);
    if (arena->data == NULL)
        exit(1);

    arena->capacity = capacity;
    return arena;
}

static void *arena_allocate(Arena *arena, size_t size, size_t alignment)
{
    size_t aligned_offset =
        (arena->offset + alignment - 1) & (size_t)-(intptr_t)alignment;
    void *result;

    if (aligned_offset + size > arena->capacity)
        return NULL;

    result = arena->data + aligned_offset;
    arena->offset = aligned_offset + size;

    if (arena->offset > arena->high_water)
        arena->high_water = arena->offset;

    ++arena->allocations;
    return result;
}

static void arena_destroy(Arena *arena)
{
    free(arena->data);
    free(arena);
}

static void report_overwrite(void *element, void *context)
{
    Record *record = element;
    uint32_t *counter = context;

    ++*counter;
    printf("%u %llu\n", record->kind, record->timestamp);
}

static void print_record(void *element, size_t index, void *context)
{
    Record *record = element;

    (void)context;
    printf("%zu %llu %u ", index, record->timestamp, record->kind);

    switch (record->kind) {
    case 0:
        putchar('\n');
        break;

    case 1:
        printf("%u %u %u\n",
               (unsigned)record->payload.first.code,
               (unsigned)record->payload.first.modifiers,
               (unsigned)record->payload.first.pressed);
        break;

    case 2:
        printf("%u %u %u %u %u\n",
               record->payload.second.x,
               record->payload.second.y,
               record->payload.second.dx,
               record->payload.second.dy,
               (unsigned)record->payload.second.buttons);
        break;

    case 3:
        printf("%u %u %.2f\n",
               (unsigned)record->payload.third.width,
               (unsigned)record->payload.third.height,
               (double)record->payload.third.scale);
        break;

    case 4:
        printf("%u %s\n",
               (unsigned)record->payload.fourth.length,
               record->payload.fourth.text);
        break;
    }
}

static void transform_integer(void *element, size_t index, void *context)
{
    int *value = element;
    int *factor = context;

    *value += (int)index * *factor;
}

int main(void)
{
    Ring *records = ring_create(sizeof(Record), 5);
    Ring *integers = ring_create(sizeof(int), 8);
    Arena *arena = arena_create(1024);
    uint32_t overwritten = 0;
    int factor = 10;
    uint32_t i;
    Record record;

    records->overwrite_callback = report_overwrite;
    records->overwrite_context = &overwritten;

    for (i = 0; i <= 8; ++i) {
        memset(&record, 0, sizeof(record));
        record.timestamp = (unsigned long long)(5000u + 17u * i);

        switch (i & 3u) {
        case 0:
            record.kind = 1;
            record.payload.first.code = (uint16_t)(i + 64u);
            record.payload.first.modifiers = (uint8_t)(i & 7u);
            record.payload.first.pressed = (uint8_t)(i & 1u);
            break;

        case 1:
            record.kind = 2;
            record.payload.second.x = i * 30u;
            record.payload.second.y = 200u - i * 12u;
            record.payload.second.dx = i;
            record.payload.second.dy = 0u - i;
            record.payload.second.buttons = (uint8_t)(1u << (i % 3u));
            break;

        case 2:
            record.kind = 3;
            record.payload.third.width = (uint16_t)((i + 50u) << 4);
            record.payload.third.height = (uint16_t)(600u + 9u * i);
            record.payload.third.scale = 1.0f + 0.25f * (float)(i % 3u);
            break;

        default:
            record.kind = 4;
            snprintf(record.payload.fourth.text,
                     sizeof(record.payload.fourth.text),
                     "message-%u", i);
            record.payload.fourth.length =
                (uint16_t)strlen(record.payload.fourth.text);
            break;
        }

        ring_push(records, &record);
    }

    printf("%zu %zu %zu %u\n",
           ring_size(records),
           records->pushes,
           records->overwrites,
           overwritten);

    ring_visit(records, print_record, "");

    while (ring_pop(records, &record))
        printf("%u %llu\n", record.kind, record.timestamp);

    for (i = 0; i <= 5; ++i) {
        int value = (int)(i * i);
        ring_push(integers, &value);
    }

    ring_visit(integers, transform_integer, &factor);

    {
        int value;

        while (ring_pop(integers, &value))
            printf("%d ", value);
    }
    putchar('\n');

    {
        Payload1 *first = arena_allocate(arena, 4, 4);
        Payload2 *second = arena_allocate(arena, 20, 8);
        Payload3 *third = arena_allocate(arena, 8, 8);
        char *text = arena_allocate(arena, 32, 1);

        first->code = 27;
        first->pressed = 1;

        second->x = 11;
        second->y = 22;
        second->buttons = 4;

        third->width = 1920;
        third->height = 1080;
        third->scale = 60.0f;

        snprintf(text, 32, "arena-backed");

        printf("%zu %zu %u %u %u %u %u %u %s\n",
               arena->offset,
               arena->high_water,
               arena->allocations,
               (unsigned)first->code,
               second->x,
               second->y,
               (unsigned)third->width,
               (unsigned)third->height,
               text);
    }

    ring_destroy(records);
    ring_destroy(integers);
    arena_destroy(arena);
    return 0;
}