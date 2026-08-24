#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct arena {
    unsigned char *data;
    size_t capacity;
    size_t offset;
    size_t peak;
    uint32_t allocation_count;
};

struct queue {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    size_t head;
    size_t count;
    size_t push_count;
    size_t overwrite_count;
    void (*overwrite_callback)(void *, void *);
    void *overwrite_context;
};

struct record {
    uint32_t kind;
    uint32_t reserved;
    uint64_t identifier;
    union {
        struct {
            uint16_t code;
            uint8_t modifiers;
            uint8_t pressed;
        } key;
        struct {
            int32_t x;
            int32_t y;
            int32_t dx;
            int32_t dy;
            uint8_t buttons;
        } pointer;
        struct {
            uint16_t width;
            uint16_t height;
            float scale;
        } resize;
        struct {
            uint16_t length;
            char text[24];
        } text;
        unsigned char raw[32];
    } value;
};

struct arena_item_a {
    uint16_t value;
    uint8_t reserved;
    uint8_t enabled;
};

struct arena_item_b {
    int32_t first;
    int32_t second;
    int32_t reserved_a;
    int32_t reserved_b;
    uint8_t flags;
};

struct arena_item_c {
    uint16_t width;
    uint16_t height;
    float scale;
};

_Static_assert(sizeof(struct arena) == 40, "unexpected arena layout");
_Static_assert(sizeof(struct queue) == 72, "unexpected queue layout");
_Static_assert(sizeof(struct record) == 48, "unexpected record layout");
_Static_assert(sizeof(struct arena_item_a) == 4, "unexpected item layout");
_Static_assert(sizeof(struct arena_item_b) == 20, "unexpected item layout");
_Static_assert(sizeof(struct arena_item_c) == 8, "unexpected item layout");

static void *arena_allocate(struct arena *arena, size_t size, size_t alignment)
{
    size_t aligned_offset =
        (arena->offset + alignment - 1) & ((size_t)0 - alignment);
    size_t new_offset = aligned_offset + size;

    if (new_offset > arena->capacity)
        return NULL;

    void *result = arena->data + aligned_offset;
    arena->offset = new_offset;

    if (new_offset > arena->peak)
        arena->peak = new_offset;

    ++arena->allocation_count;
    return result;
}

static struct queue *queue_create(size_t element_size, size_t capacity)
{
    struct queue *queue = calloc(1, sizeof(*queue));
    if (queue == NULL)
        exit(1);

    queue->data = calloc(capacity, element_size);
    if (queue->data == NULL)
        exit(1);

    queue->element_size = element_size;
    queue->capacity = capacity;
    return queue;
}

static void queue_push(struct queue *queue, const void *element)
{
    ++queue->push_count;

    if (queue->count == queue->capacity) {
        if (queue->overwrite_callback != NULL) {
            void *oldest = queue->data + queue->head * queue->element_size;
            queue->overwrite_callback(oldest, queue->overwrite_context);
        }

        queue->head = (queue->head + 1) % queue->capacity;
        --queue->count;
        ++queue->overwrite_count;
    }

    size_t index = (queue->head + queue->count) % queue->capacity;
    memcpy(queue->data + index * queue->element_size,
           element,
           queue->element_size);
    ++queue->count;
}

static int queue_pop(struct queue *queue, void *destination)
{
    if (queue->count == 0)
        return 0;

    memcpy(destination,
           queue->data + queue->head * queue->element_size,
           queue->element_size);

    queue->head = (queue->head + 1) % queue->capacity;
    --queue->count;
    return 1;
}

static void queue_for_each(
    struct queue *queue,
    void (*callback)(void *, size_t, void *),
    void *context)
{
    for (size_t i = 0; i < queue->count; ++i) {
        size_t index = (queue->head + i) % queue->capacity;
        callback(queue->data + index * queue->element_size, i, context);
    }
}

static void overwrite_record(void *element, void *context)
{
    struct record *record = element;
    int *overwrite_counter = context;

    ++*overwrite_counter;
    printf("%u %lu\n",
           record->kind,
           (unsigned long)record->identifier);
}

static void print_record(void *element, size_t index, void *context)
{
    struct record *record = element;
    (void)context;

    printf("%zu %lu %u ",
           index,
           (unsigned long)record->identifier,
           record->kind);

    switch (record->kind) {
    case 0:
        putc('\n', stdout);
        break;

    case 1:
        printf("%u %u %u\n",
               (unsigned)record->value.key.code,
               (unsigned)record->value.key.modifiers,
               (unsigned)record->value.key.pressed);
        break;

    case 2:
        printf("%d %d %d %d %u\n",
               record->value.pointer.x,
               record->value.pointer.y,
               record->value.pointer.dx,
               record->value.pointer.dy,
               (unsigned)record->value.pointer.buttons);
        break;

    case 3:
        printf("%u %u %.2f\n",
               (unsigned)record->value.resize.width,
               (unsigned)record->value.resize.height,
               (double)record->value.resize.scale);
        break;

    case 4:
        printf("%u %s\n",
               (unsigned)record->value.text.length,
               record->value.text.text);
        break;

    default:
        break;
    }
}

static void adjust_integer(void *element, size_t index, void *context)
{
    int32_t *value = element;
    int32_t factor = *(int32_t *)context;
    *value += (int32_t)index * factor;
}

int main(void)
{
    static const float scale_step = 0.25f;
    static const float scale_base = 1.0f;

    struct queue *records = queue_create(sizeof(struct record), 5);
    struct queue *integers = queue_create(sizeof(int32_t), 8);

    struct arena *arena = calloc(1, sizeof(*arena));
    if (arena == NULL)
        exit(1);

    arena->data = calloc(1, 1024);
    if (arena->data == NULL)
        exit(1);
    arena->capacity = 1024;

    int overwritten = 0;
    int32_t adjustment = 10;

    records->overwrite_callback = overwrite_record;
    records->overwrite_context = &overwritten;

    for (unsigned i = 0; i < 9; ++i) {
        struct record record = {0};

        record.identifier = 5000u + 17u * i;
        record.kind = i & 3u;

        switch (record.kind) {
        case 0:
            break;

        case 1:
            record.value.key.code = (uint16_t)(i + 64u);
            record.value.key.modifiers = (uint8_t)(i & 7u);
            record.value.key.pressed = (uint8_t)(i & 1u);
            break;

        case 2:
            record.value.pointer.x = (int32_t)(i * 30u);
            record.value.pointer.y = 200 - (int32_t)(i * 12u);
            record.value.pointer.dx = (int32_t)i;
            record.value.pointer.dy = -(int32_t)i;
            record.value.pointer.buttons = (uint8_t)(1u << (i % 3u));
            break;

        case 3:
            record.value.resize.width = (uint16_t)(800u + i * 16u);
            record.value.resize.height = (uint16_t)(600u + i * 9u);
            record.value.resize.scale =
                (float)(i % 3u) * scale_step + scale_base;
            break;

        case 4:
            snprintf(record.value.text.text,
                     sizeof(record.value.text.text),
                     "message-%u",
                     i);
            record.value.text.length =
                (uint16_t)strlen(record.value.text.text);
            break;
        }

        queue_push(records, &record);
    }

    printf("%zu %zu %zu %d\n",
           records->count,
           records->push_count,
           records->overwrite_count,
           overwritten);

    queue_for_each(records, print_record, "Q");

    struct record record;
    while (queue_pop(records, &record))
        printf("%u %lu\n", record.kind, (unsigned long)record.identifier);

    for (int32_t i = 0; i < 6; ++i) {
        int32_t square = i * i;
        queue_push(integers, &square);
    }

    queue_for_each(integers, adjust_integer, &adjustment);

    int32_t value;
    while (queue_pop(integers, &value))
        printf("%d ", value);
    putc('\n', stdout);

    struct arena_item_a *item_a =
        arena_allocate(arena, sizeof(*item_a), 4);
    struct arena_item_b *item_b =
        arena_allocate(arena, sizeof(*item_b), 8);
    struct arena_item_c *item_c =
        arena_allocate(arena, sizeof(*item_c), 8);
    char *text = arena_allocate(arena, 32, 1);

    item_a->value = 27;
    item_a->enabled = 1;

    item_b->first = 11;
    item_b->second = 22;
    item_b->flags = 4;

    item_c->width = 1920;
    item_c->height = 1080;
    item_c->scale = 2.0f;

    memcpy(text, "arena string", sizeof("arena string"));

    printf("%zu %zu %u %u %d %d %u %u %s\n",
           arena->offset,
           arena->peak,
           arena->allocation_count,
           (unsigned)item_a->value,
           item_b->first,
           item_b->second,
           (unsigned)item_c->width,
           (unsigned)item_c->height,
           text);

    free(records->data);
    free(records);
    free(integers->data);
    free(integers);
    free(arena->data);
    free(arena);

    return 0;
}