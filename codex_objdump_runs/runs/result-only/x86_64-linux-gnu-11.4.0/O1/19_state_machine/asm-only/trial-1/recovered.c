#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    STATE_COUNT = 7,
    EVENT_COUNT = 7
};

typedef struct object object;

typedef union {
    struct {
        uint32_t value0;
        uint32_t value1;
        uint16_t value2;
        uint8_t value3;
        uint8_t padding[21];
    } compact;

    struct {
        uint64_t value0;
        uint64_t value1;
        uint32_t value2;
        uint16_t value3;
        uint8_t value4;
        uint8_t padding[9];
    } wide;

    struct {
        uint32_t code;
        char text[28];
    } message;

    unsigned char bytes[32];
} payload;

struct object {
    uint32_t identifier;
    char endpoint[24];
    uint32_t state;
    uint32_t previous_state;
    uint32_t transition_count;
    uint32_t ignored_count;
    uint32_t event_count[EVENT_COUNT];
    payload data;
};

typedef void (*action_fn)(object *, uint32_t);

typedef struct {
    uint32_t next_state;
    uint32_t padding;
    action_fn action;
    const char *tag;
} transition;

_Static_assert(offsetof(object, endpoint) == 0x04, "layout");
_Static_assert(offsetof(object, state) == 0x1c, "layout");
_Static_assert(offsetof(object, event_count) == 0x2c, "layout");
_Static_assert(offsetof(object, data) == 0x48, "layout");
_Static_assert(sizeof(payload) == 0x20, "layout");
_Static_assert(sizeof(object) == 0x68, "layout");
_Static_assert(sizeof(transition) == 0x18, "layout");

static void initialize_compact(object *item, uint32_t event)
{
    uint32_t identifier = item->identifier;

    (void)event;
    item->data.compact.value0 = identifier * UINT32_C(7) + UINT32_C(0x1000);
    item->data.compact.value1 = identifier * UINT32_C(13) + UINT32_C(0x9000);
    item->data.compact.value3 = 0;
}

static void accumulate_wide(object *item, uint32_t event)
{
    uint32_t count = item->transition_count;
    uint32_t increment0 = count * UINT32_C(8) + UINT32_C(0x200);
    uint32_t increment1 = count * UINT32_C(3) + UINT32_C(0x80);

    (void)event;
    item->data.wide.value0 += (uint64_t)increment0;
    item->data.wide.value1 += (uint64_t)increment1;
    item->data.wide.value2 += (uint8_t)item->data.wide.value0;
}

static void increment_compact(object *item, uint32_t event)
{
    (void)event;
    ++item->data.compact.value3;
}

static void initialize_wide(object *item, uint32_t event)
{
    (void)event;
    memset(&item->data, 0, sizeof(item->data));
    item->data.wide.value1 = UINT64_C(0x2000);
}

static void convert_to_wide(object *item, uint32_t event)
{
    uint32_t old_value = (uint32_t)item->data.wide.value0;

    (void)event;
    memset(&item->data, 0, sizeof(item->data));
    item->data.wide.value2 = old_value + UINT32_C(1);
    item->data.wide.value3 = UINT16_C(1460);
    item->data.wide.value4 = UINT8_C(1);
}

static void format_total(object *item, uint32_t event)
{
    uint64_t total = item->data.wide.value0 + item->data.wide.value1;

    memset(&item->data, 0, sizeof(item->data));
    item->data.message.code = event;
    snprintf(item->data.message.text, 24, "sum=%" PRIu64, total);
}

static void record_peer_reset(object *item, uint32_t event)
{
    memset(&item->data, 0, sizeof(item->data));
    item->data.message.code = event | UINT32_C(0x8000);
    memcpy(item->data.message.text, "peer reset", sizeof("peer reset"));
}

static const char tag0[] = "event0";
static const char tag1[] = "event1";
static const char tag2[] = "event2";
static const char tag3[] = "event3";
static const char tag4[] = "event4";
static const char tag5[] = "event5";
static const char tag6[] = "event6";

#define ENTRY(n, f, t) { (n), 0, (f), (t) }
#define SAME(n, t)     { (n), 0, NULL, (t) }

static const transition transition_table[STATE_COUNT][EVENT_COUNT] = {
    {
        SAME(0, tag0),
        ENTRY(1, initialize_compact, tag1),
        ENTRY(2, increment_compact, tag2),
        ENTRY(3, initialize_wide, tag3),
        ENTRY(4, convert_to_wide, tag4),
        ENTRY(5, format_total, tag5),
        ENTRY(6, accumulate_wide, tag6)
    },
    {
        ENTRY(0, record_peer_reset, tag0),
        SAME(1, tag1),
        ENTRY(2, increment_compact, tag2),
        ENTRY(3, initialize_wide, tag3),
        ENTRY(4, convert_to_wide, tag4),
        ENTRY(5, format_total, tag5),
        ENTRY(6, accumulate_wide, tag6)
    },
    {
        ENTRY(0, record_peer_reset, tag0),
        ENTRY(1, initialize_compact, tag1),
        SAME(2, tag2),
        ENTRY(3, initialize_wide, tag3),
        ENTRY(4, convert_to_wide, tag4),
        ENTRY(5, format_total, tag5),
        ENTRY(6, accumulate_wide, tag6)
    },
    {
        ENTRY(0, record_peer_reset, tag0),
        ENTRY(1, initialize_compact, tag1),
        ENTRY(2, increment_compact, tag2),
        SAME(3, tag3),
        ENTRY(4, convert_to_wide, tag4),
        ENTRY(5, format_total, tag5),
        ENTRY(6, accumulate_wide, tag6)
    },
    {
        ENTRY(0, record_peer_reset, tag0),
        ENTRY(1, initialize_compact, tag1),
        ENTRY(2, increment_compact, tag2),
        ENTRY(3, initialize_wide, tag3),
        SAME(4, tag4),
        ENTRY(5, format_total, tag5),
        ENTRY(6, accumulate_wide, tag6)
    },
    {
        ENTRY(0, record_peer_reset, tag0),
        ENTRY(1, initialize_compact, tag1),
        ENTRY(2, increment_compact, tag2),
        ENTRY(3, initialize_wide, tag3),
        ENTRY(4, convert_to_wide, tag4),
        SAME(5, tag5),
        ENTRY(6, accumulate_wide, tag6)
    },
    {
        ENTRY(0, record_peer_reset, tag0),
        ENTRY(1, initialize_compact, tag1),
        ENTRY(2, increment_compact, tag2),
        ENTRY(3, initialize_wide, tag3),
        ENTRY(4, convert_to_wide, tag4),
        ENTRY(5, format_total, tag5),
        SAME(6, tag6)
    }
};

static const uint32_t sequence0[8] = { 1, 2, 2, 3, 4, 4, 5, 6 };
static const uint32_t sequence1[8] = { 6, 5, 4, 3, 2, 1, 0, 0 };

static void process_event(object *item, uint32_t event)
{
    uint32_t old_state = item->state;
    const transition *entry;

    ++item->event_count[event];
    entry = &transition_table[old_state][event];

    if (old_state == entry->next_state) {
        if (entry->action != NULL) {
            entry->action(item, event);
        } else {
            ++item->ignored_count;
            printf("%u == %u\n", event, old_state);
        }
        return;
    }

    if (entry->action != NULL)
        entry->action(item, event);

    item->previous_state = old_state;
    item->state = entry->next_state;
    ++item->transition_count;

    printf("%u -> %u\n", event, entry->next_state);
}

static void report_object(const object *item)
{
    uint32_t i;

    printf("%u %s %u %u %u %u\n",
           item->identifier,
           item->endpoint,
           item->state,
           item->previous_state,
           item->transition_count,
           item->ignored_count);

    if (item->state <= 2) {
        if (item->state == 0) {
            printf("%u %s\n",
                   item->data.message.code,
                   item->data.message.text);
        } else {
            printf("%u %u %u %u\n",
                   item->data.compact.value0,
                   item->data.compact.value1,
                   (unsigned)item->data.compact.value2,
                   (unsigned)item->data.compact.value3);
        }
    } else if (item->state <= 4) {
        printf("%" PRIu64 " %" PRIu64 " %u %u %u\n",
               item->data.wide.value0,
               item->data.wide.value1,
               item->data.wide.value2,
               (unsigned)item->data.wide.value3,
               (unsigned)item->data.wide.value4);
    } else if (item->state == 5) {
        printf("%u %s\n",
               item->data.message.code,
               item->data.message.text);
    }

    for (i = 0; i < EVENT_COUNT; ++i)
        printf("%u\n", item->event_count[i]);

    putc('\n', stdout);
}

int main(void)
{
    object first = { 0 };
    object second = { 0 };
    uint32_t active_actions = 0;
    size_t state_index;
    size_t event_index;

    first.identifier = 1;
    memcpy(first.endpoint, "10.0.0.7:443", sizeof("10.0.0.7:443"));

    second.identifier = 2;
    memcpy(second.endpoint, "192.168.1.9:80", sizeof("192.168.1.9:80"));

    for (event_index = 0; event_index < 8; ++event_index)
        process_event(&first, sequence0[event_index]);

    report_object(&first);

    for (event_index = 0; event_index < 8; ++event_index)
        process_event(&second, sequence1[event_index]);

    report_object(&second);

    for (state_index = 0; state_index < STATE_COUNT; ++state_index) {
        for (event_index = 0; event_index < EVENT_COUNT; ++event_index) {
            if (transition_table[state_index][event_index].action != NULL)
                ++active_actions;
        }
    }

    printf("%u %u\n", 42U, active_actions);
    return 0;
}