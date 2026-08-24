#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint16_t value;
    uint8_t level;
    uint8_t enabled;
} SmallPayload;

typedef struct {
    int32_t first;
    int32_t second;
    int32_t third;
    int32_t fourth;
    uint8_t flags;
} IntegerPayload;

typedef struct {
    uint16_t width;
    uint16_t height;
    float scale;
} FloatPayload;

typedef struct {
    uint16_t length;
    char text[24];
} TextPayload;

typedef union {
    SmallPayload small;
    IntegerPayload integers;
    FloatPayload floating;
    TextPayload text;
} Payload;

typedef struct {
    uint32_t kind;
    uint64_t sequence;
    Payload payload;
} Record;

typedef void (*OverwriteCallback)(void *, void *);
typedef void (*VisitCallback)(void *, size_t, void *);

typedef struct {
    unsigned char *storage;
    size_t element_size;
    size_t capacity;
    size_t head;
    size_t count;
    size_t insertions;
    size_t overwrites;
    OverwriteCallback overwrite_callback;
    void *overwrite_context;
} Ring;

typedef struct {
    unsigned char *storage;
    size_t capacity;
    size_t offset;
    size_t peak;
    uint32_t allocations;
} Arena;

typedef struct {
    uint16_t code;
    uint8_t reserved;
    uint8_t enabled;
} ArenaObjectA;

typedef struct {
    int32_t values[4];
    uint8_t count;
} ArenaObjectB;

typedef struct {
    uint16_t width;
    uint16_t height;
    float scale;
} ArenaObjectC;

_Static_assert(sizeof(Record) == 48, "unexpected Record layout");
_Static_assert(offsetof(Record, payload) == 16, "unexpected payload offset");
_Static_assert(sizeof(Ring) == 72, "unexpected Ring layout");
_Static_assert(sizeof(Arena) == 40, "unexpected Arena layout");
_Static_assert(sizeof(ArenaObjectA) == 4, "unexpected object layout");
_Static_assert(sizeof(ArenaObjectB) == 20, "unexpected object layout");
_Static_assert(sizeof(ArenaObjectC) == 8, "unexpected object layout");

static Ring *ring_create(size_t element_size, size_t capacity)
{
    Ring *ring = calloc(1, sizeof(*ring));

    if (ring == NULL)
        exit(EXIT_FAILURE);

    ring->storage = calloc(capacity, element_size);
    if (ring->storage == NULL)
        exit(EXIT_FAILURE);

    ring->element_size = element_size;
    ring->capacity = capacity;
    return ring;
}

static void ring_push(Ring *ring, const void *element)
{
    size_t index;

    ++ring->insertions;

    if (ring->count == ring->capacity) {
        if (ring->overwrite_callback != NULL) {
            ring->overwrite_callback(
                ring->storage + ring->head * ring->element_size,
                ring->overwrite_context
            );
        }

        ring->head = (ring->head + 1) % ring->capacity;
        --ring->count;
        ++ring->overwrites;
    }

    index = (ring->head + ring->count) % ring->capacity;
    memcpy(ring->storage + index * ring->element_size,
           element,
           ring->element_size);
    ++ring->count;
}

static int ring_pop(Ring *ring, void *destination)
{
    if (ring->count == 0)
        return 0;

    memcpy(destination,
           ring->storage + ring->head * ring->element_size,
           ring->element_size);

    --ring->count;
    ring->head = (ring->head + 1) % ring->capacity;
    return 1;
}

static void ring_visit(Ring *ring, VisitCallback callback, void *context)
{
    size_t index;

    for (index = 0; index < ring->count; ++index) {
        size_t position = (ring->head + index) % ring->capacity;
        callback(ring->storage + position * ring->element_size,
                 index,
                 context);
    }
}

static void *arena_allocate(Arena *arena, size_t size, size_t alignment)
{
    size_t aligned = (arena->offset + alignment - 1) & (size_t)-alignment;
    size_t end = aligned + size;

    if (end > arena->capacity)
        return NULL;

    arena->offset = end;
    if (end > arena->peak)
        arena->peak = end;

    ++arena->allocations;
    return arena->storage + aligned;
}

static void report_overwrite(void *element, void *context)
{
    const Record *record = element;
    int *counter = context;

    ++*counter;
    printf("%u %" PRIu64 "\n",
           record->kind,
           record->sequence);
}

static void print_record(void *element, size_t index, void *context)
{
    const Record *record = element;

    (void)context;

    printf("%zu %" PRIu64 " %u ",
           index,
           record->sequence,
           record->kind);

    switch (record->kind) {
    case 0:
        putchar('\n');
        break;

    case 1:
        printf("%u %u %u\n",
               (unsigned)record->payload.small.value,
               (unsigned)record->payload.small.level,
               (unsigned)record->payload.small.enabled);
        break;

    case 2:
        printf("%d %d %d %d %u\n",
               record->payload.integers.first,
               record->payload.integers.second,
               record->payload.integers.third,
               record->payload.integers.fourth,
               (unsigned)record->payload.integers.flags);
        break;

    case 3:
        printf("%u %u %.2f\n",
               (unsigned)record->payload.floating.width,
               (unsigned)record->payload.floating.height,
               (double)record->payload.floating.scale);
        break;

    case 4:
        printf("%u %s\n",
               (unsigned)record->payload.text.length,
               record->payload.text.text);
        break;

    default:
        break;
    }
}

static void adjust_integer(void *element, size_t index, void *context)
{
    int32_t *value = element;
    const int32_t *factor = context;

    *value += (int32_t)index * *factor;
}

int main(void)
{
    Ring *records = ring_create(sizeof(Record), 5);
    Ring *integers = ring_create(sizeof(int32_t), 8);
    Arena *arena;
    int overwritten = 0;
    uint32_t i;

    arena = calloc(1, sizeof(*arena));
    if (arena == NULL)
        exit(EXIT_FAILURE);

    arena->storage = calloc(1, 1024);
    if (arena->storage == NULL)
        exit(EXIT_FAILURE);

    arena->capacity = 1024;

    records->overwrite_callback = report_overwrite;
    records->overwrite_context = &overwritten;

    for (i = 0; i < 9; ++i) {
        Record record = {0};

        record.sequence = UINT64_C(5000) + UINT64_C(17) * i;

        switch (i & 3U) {
        case 0:
            record.kind = 1;
            record.payload.small.value = (uint16_t)(64U + i);
            record.payload.small.level = (uint8_t)(i & 7U);
            record.payload.small.enabled = (uint8_t)(i & 1U);
            break;

        case 1:
            record.kind = 2;
            record.payload.integers.first = (int32_t)(30U * i);
            record.payload.integers.second = 200 - (int32_t)(12U * i);
            record.payload.integers.third = (int32_t)i;
            record.payload.integers.fourth = -(int32_t)i;
            record.payload.integers.flags =
                (uint8_t)((i & 3U) << (i % 3U));
            break;

        case 2:
            record.kind = 3;
            record.payload.floating.width =
                (uint16_t)(800U + 16U * i);
            record.payload.floating.height =
                (uint16_t)(600U + 9U * i);
            record.payload.floating.scale =
                (float)(i % 3U) * 0.25f + 1.0f;
            break;

        default:
            record.kind = 4;
            snprintf(record.payload.text.text,
                     sizeof(record.payload.text.text),
                     "message-%u",
                     i);
            record.payload.text.length =
                (uint16_t)strlen(record.payload.text.text);
            break;
        }

        ring_push(records, &record);
    }

    printf("%zu %zu %zu %d\n",
           records->count,
           records->insertions,
           records->overwrites,
           overwritten);

    ring_visit(records, print_record, "x");

    while (records->count != 0) {
        Record record;

        ring_pop(records, &record);
        printf("%u %" PRIu64 "\n",
               record.kind,
               record.sequence);
    }

    for (i = 0; i < 6; ++i) {
        int32_t value = (int32_t)(i * i);
        ring_push(integers, &value);
    }

    {
        int32_t factor = 10;
        int32_t value;

        ring_visit(integers, adjust_integer, &factor);

        while (ring_pop(integers, &value))
            printf("%d ", value);
    }

    putchar('\n');

    {
        ArenaObjectA *a = arena_allocate(arena, 4, 4);
        ArenaObjectB *b = arena_allocate(arena, 20, 8);
        ArenaObjectC *c = arena_allocate(arena, 8, 8);
        char *text = arena_allocate(arena, 32, 1);

        a->code = 27;
        a->enabled = 1;

        b->values[0] = 11;
        b->values[1] = 22;
        b->count = 4;

        c->width = 1920;
        c->height = 1080;
        c->scale = 2.0f;

        memcpy(text, "arena string", sizeof("arena string"));

        printf("%zu %zu %u %u %d %d %u %u %s\n",
               arena->capacity,
               arena->peak,
               arena->allocations,
               (unsigned)a->code,
               b->values[0],
               b->values[1],
               (unsigned)c->width,
               (unsigned)c->height,
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