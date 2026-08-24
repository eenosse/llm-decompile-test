/*
 * 18_opaque_generic.c
 * Target feature: opaque handles (struct declared only in the .c body),
 * void*-based generic containers with element size known at runtime, and a
 * union-typed slot holding several user types in the same ring buffer.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

/* --- public, opaque --- */
typedef struct RingBuffer RingBuffer;   /* definition hidden below */
typedef struct Arena      Arena;

/* --- element types stored generically --- */
typedef struct EventKey {
    uint16_t code;
    uint8_t  modifiers;
    uint8_t  pressed;
} EventKey;

typedef struct EventMouse {
    int32_t  x;
    int32_t  y;
    int32_t  dx;
    int32_t  dy;
    uint8_t  buttons;
} EventMouse;

typedef struct EventResize {
    uint16_t width;
    uint16_t height;
    float    dpi_scale;
} EventResize;

typedef struct EventText {
    uint16_t length;
    char     utf8[24];
} EventText;

typedef enum EventKind {
    EV_NONE = 0, EV_KEY, EV_MOUSE, EV_RESIZE, EV_TEXT
} EventKind;

typedef struct Event {
    EventKind kind;
    uint64_t  when;
    union {
        EventKey    key;
        EventMouse  mouse;
        EventResize resize;
        EventText   text;
    } data;
} Event;

/* --- opaque definitions --- */
struct RingBuffer {
    uint8_t *storage;
    size_t   elem_size;
    size_t   capacity;
    size_t   head;
    size_t   count;
    uint64_t pushed;
    uint64_t dropped;
    void   (*on_drop)(const void *elem, void *user);
    void    *user;
};

struct Arena {
    uint8_t *base;
    size_t   size;
    size_t   used;
    size_t   high_water;
    uint32_t allocations;
};

static RingBuffer *ring_new(size_t elem_size, size_t capacity)
{
    RingBuffer *rb = (RingBuffer *)calloc(1, sizeof(RingBuffer));
    if (!rb)
        exit(1);
    rb->storage   = (uint8_t *)calloc(capacity, elem_size);
    if (!rb->storage)
        exit(1);
    rb->elem_size = elem_size;
    rb->capacity  = capacity;
    return rb;
}

static void *ring_slot(RingBuffer *rb, size_t logical)
{
    size_t idx = (rb->head + logical) % rb->capacity;
    return rb->storage + idx * rb->elem_size;
}

static void ring_push(RingBuffer *rb, const void *elem)
{
    void *dst;

    rb->pushed++;
    if (rb->count == rb->capacity) {
        if (rb->on_drop)
            rb->on_drop(rb->storage + rb->head * rb->elem_size, rb->user);
        rb->head = (rb->head + 1) % rb->capacity;
        rb->count--;
        rb->dropped++;
    }
    dst = ring_slot(rb, rb->count);
    memcpy(dst, elem, rb->elem_size);
    rb->count++;
}

static int ring_pop(RingBuffer *rb, void *out)
{
    if (rb->count == 0)
        return 0;
    memcpy(out, rb->storage + rb->head * rb->elem_size, rb->elem_size);
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    return 1;
}

static size_t ring_size(const RingBuffer *rb) { return rb->count; }

static void ring_free(RingBuffer *rb)
{
    free(rb->storage);
    free(rb);
}

/* generic map over any element type */
static void ring_foreach(RingBuffer *rb, void (*fn)(void *elem, size_t i, void *user), void *user)
{
    size_t i;
    for (i = 0; i < rb->count; i++)
        fn(ring_slot(rb, i), i, user);
}

static Arena *arena_new(size_t size)
{
    Arena *a = (Arena *)calloc(1, sizeof(Arena));
    if (!a)
        exit(1);
    a->base = (uint8_t *)calloc(1, size);
    if (!a->base)
        exit(1);
    a->size = size;
    return a;
}

static void *arena_alloc(Arena *a, size_t n, size_t align)
{
    size_t aligned = (a->used + (align - 1)) & ~(align - 1);
    void *p;

    if (aligned + n > a->size)
        return NULL;
    p = a->base + aligned;
    a->used = aligned + n;
    if (a->used > a->high_water)
        a->high_water = a->used;
    a->allocations++;
    return p;
}

static void arena_free(Arena *a)
{
    free(a->base);
    free(a);
}

static void event_dropped(const void *elem, void *user)
{
    const Event *e = (const Event *)elem;
    uint32_t *counter = (uint32_t *)user;
    (*counter)++;
    BENCH_OUTPUT(
        printf("  [drop] kind=%d when=%llu\n", (int)e->kind,
               (unsigned long long)e->when),
        printf("%d %llu\n", (int)e->kind, (unsigned long long)e->when));
}

static void event_print(void *elem, size_t i, void *user)
{
    Event *e = (Event *)elem;
    const char *prefix = (const char *)user;
    BENCH_VERBOSE_ARG(prefix);

    BENCH_OUTPUT(
        printf("%s[%zu] t=%llu ", prefix, i, (unsigned long long)e->when),
        printf("%zu %llu %d ", i, (unsigned long long)e->when, (int)e->kind));
    switch (e->kind) {
    case EV_KEY:
        BENCH_OUTPUT(
            printf("KEY code=%u mods=%02x %s\n", e->data.key.code,
                   e->data.key.modifiers, e->data.key.pressed ? "down" : "up"),
            printf("%u %02x %u\n", e->data.key.code,
                   e->data.key.modifiers, e->data.key.pressed));
        break;
    case EV_MOUSE:
        BENCH_OUTPUT(
            printf("MOUSE (%d,%d) d=(%d,%d) btn=%02x\n", e->data.mouse.x,
                   e->data.mouse.y, e->data.mouse.dx, e->data.mouse.dy,
                   e->data.mouse.buttons),
            printf("%d %d %d %d %02x\n", e->data.mouse.x,
                   e->data.mouse.y, e->data.mouse.dx, e->data.mouse.dy,
                   e->data.mouse.buttons));
        break;
    case EV_RESIZE:
        BENCH_OUTPUT(
            printf("RESIZE %ux%u dpi=%.2f\n", e->data.resize.width,
                   e->data.resize.height, e->data.resize.dpi_scale),
            printf("%u %u %.2f\n", e->data.resize.width,
                   e->data.resize.height, e->data.resize.dpi_scale));
        break;
    case EV_TEXT:
        BENCH_OUTPUT(
            printf("TEXT len=%u '%s'\n", e->data.text.length, e->data.text.utf8),
            printf("%u|%s\n", e->data.text.length, e->data.text.utf8));
        break;
    case EV_NONE:
        BENCH_OUTPUT(printf("NONE\n"), putchar('\n'));
        break;
    }
}

static void bump_ints(void *elem, size_t i, void *user)
{
    int32_t *v = (int32_t *)elem;
    int32_t k = *(const int32_t *)user;
    *v += (int32_t)i * k;
}

int main(void)
{
    RingBuffer *events = ring_new(sizeof(Event), 5);
    RingBuffer *ints   = ring_new(sizeof(int32_t), 8);
    Arena *arena = arena_new(1024);
    uint32_t drops = 0;
    Event ev;
    int32_t bump = 10;
    unsigned i;

    events->on_drop = event_dropped;
    events->user    = &drops;

    for (i = 0; i < 9; i++) {
        memset(&ev, 0, sizeof(ev));
        ev.when = 5000 + i * 17;
        switch (i % 4) {
        case 0:
            ev.kind = EV_KEY;
            ev.data.key.code = (uint16_t)(0x40 + i);
            ev.data.key.modifiers = (uint8_t)(i & 7);
            ev.data.key.pressed = (uint8_t)(i % 2);
            break;
        case 1:
            ev.kind = EV_MOUSE;
            ev.data.mouse.x = (int32_t)(i * 30);
            ev.data.mouse.y = (int32_t)(200 - i * 12);
            ev.data.mouse.dx = (int32_t)i;
            ev.data.mouse.dy = -(int32_t)i;
            ev.data.mouse.buttons = (uint8_t)(1u << (i % 3));
            break;
        case 2:
            ev.kind = EV_RESIZE;
            ev.data.resize.width = (uint16_t)(800 + i * 16);
            ev.data.resize.height = (uint16_t)(600 + i * 9);
            ev.data.resize.dpi_scale = 1.0f + 0.25f * (float)(i % 3);
            break;
        default:
            ev.kind = EV_TEXT;
            snprintf(ev.data.text.utf8, sizeof(ev.data.text.utf8), "chunk-%02u", i);
            ev.data.text.length = (uint16_t)strlen(ev.data.text.utf8);
            break;
        }
        ring_push(events, &ev);
    }

    BENCH_OUTPUT(
        printf("event ring: size=%zu pushed=%llu dropped=%llu (callback saw %u)\n",
               ring_size(events), (unsigned long long)events->pushed,
               (unsigned long long)events->dropped, drops),
        printf("%zu %llu %llu %u\n", ring_size(events),
               (unsigned long long)events->pushed,
               (unsigned long long)events->dropped, drops));
    ring_foreach(events, event_print, (void *)"  ");

    while (ring_pop(events, &ev))
        BENCH_OUTPUT(
            printf("  popped kind=%d when=%llu\n", (int)ev.kind,
                   (unsigned long long)ev.when),
            printf("%d %llu\n", (int)ev.kind, (unsigned long long)ev.when));

    for (i = 0; i < 6; i++) {
        int32_t v = (int32_t)(i * i);
        ring_push(ints, &v);
    }
    ring_foreach(ints, bump_ints, &bump);
    BENCH_VERBOSE(printf("int ring:"));
    {
        int32_t v;
        while (ring_pop(ints, &v))
            printf(" %d", v);
    }
    putchar('\n');

    {
        EventKey    *k = (EventKey *)arena_alloc(arena, sizeof(EventKey), 4);
        EventMouse  *m = (EventMouse *)arena_alloc(arena, sizeof(EventMouse), 8);
        EventResize *r = (EventResize *)arena_alloc(arena, sizeof(EventResize), 8);
        char        *s = (char *)arena_alloc(arena, 32, 1);

        k->code = 0x1b; k->pressed = 1;
        m->x = 11; m->y = 22; m->buttons = 4;
        r->width = 1920; r->height = 1080; r->dpi_scale = 2.0f;
        snprintf(s, 32, "arena string");
        BENCH_OUTPUT(
            printf("arena: used=%zu high=%zu allocs=%u key=%u mouse=(%d,%d) res=%ux%u '%s'\n",
                   arena->used, arena->high_water, arena->allocations,
                   k->code, m->x, m->y, r->width, r->height, s),
            printf("%zu %zu %u %u %d %d %u %u|%s\n",
                   arena->used, arena->high_water, arena->allocations,
                   k->code, m->x, m->y, r->width, r->height, s));
    }

    ring_free(events);
    ring_free(ints);
    arena_free(arena);
    return 0;
}
