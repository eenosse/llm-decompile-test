#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    STATE_COUNT = 6,
    EVENT_COUNT = 7
};

typedef struct machine machine;
typedef void (*transition_action)(machine *, uint32_t);

typedef union {
    unsigned char raw[32];

    struct {
        uint32_t code;
        char text[24];
        unsigned char reserved[4];
    } message;

    struct {
        uint32_t first;
        uint32_t second;
        uint16_t third;
        uint8_t fourth;
        unsigned char reserved[21];
    } values32;

    struct {
        uint64_t first;
        uint64_t second;
        uint32_t third;
        uint16_t fourth;
        uint8_t fifth;
        unsigned char reserved[9];
    } values64;
} payload;

struct machine {
    uint32_t identifier;
    char name[23];
    unsigned char name_padding;
    uint32_t state;
    uint32_t previous_state;
    uint32_t transition_count;
    uint32_t invalid_count;
    uint32_t event_counts[EVENT_COUNT];
    payload data;
};

typedef struct {
    uint32_t next_state;
    transition_action action;
    const char *description;
} transition;

_Static_assert(offsetof(machine, state) == 0x1c, "machine layout");
_Static_assert(offsetof(machine, event_counts) == 0x2c, "machine layout");
_Static_assert(offsetof(machine, data) == 0x48, "machine layout");
_Static_assert(sizeof(machine) == 0x68, "machine size");
_Static_assert(sizeof(transition) == 24, "transition size");

static void action_0(machine *object, uint32_t event)
{
    (void)event;
    memset(&object->data, 0, sizeof(object->data));
    object->data.values32.third = UINT16_C(0x2000);
}

static void action_1(machine *object, uint32_t event)
{
    uint32_t id = object->identifier;

    (void)event;
    object->data.values32.first = id * UINT32_C(7) + UINT32_C(0x1000);
    object->data.values32.second = id * UINT32_C(13) + UINT32_C(0x9000);
    object->data.values32.fourth = 0;
}

static void action_2(machine *object, uint32_t event)
{
    uint32_t saved = object->data.values32.first;

    (void)event;
    memset(&object->data, 0, sizeof(object->data));
    object->data.values64.third = saved + UINT32_C(1);
    object->data.values64.fourth = UINT16_C(1460);
    object->data.values64.fifth = UINT8_C(1);
}

static void action_3(machine *object, uint32_t event)
{
    uint32_t count = object->transition_count;
    uint32_t first_increment;
    uint32_t second_increment;

    (void)event;
    first_increment = (count + UINT32_C(64)) << 3;
    second_increment = count * UINT32_C(3) + UINT32_C(128);

    object->data.values64.first += first_increment;
    object->data.values64.second += second_increment;
    object->data.values64.third +=
        (uint8_t)object->data.values64.first;
}

static void action_4(machine *object, uint32_t event)
{
    (void)event;
    object->data.values32.fourth++;
}

static void action_5(machine *object, uint32_t event)
{
    uint64_t total =
        object->data.values64.first + object->data.values64.second;

    memset(&object->data, 0, sizeof(object->data));
    object->data.message.code = event;
    snprintf(object->data.message.text,
             sizeof(object->data.message.text),
             "sum=%llu",
             (unsigned long long)total);
}

static void action_6(machine *object, uint32_t event)
{
    memset(&object->data, 0, sizeof(object->data));
    object->data.message.code = event | UINT32_C(0x8000);
    strncpy(object->data.message.text, "forced transition", 23);
}

#define ENTRY(next, function, text) { (next), (function), (text) }

static const transition transition_table[STATE_COUNT][EVENT_COUNT] = {
    {
        ENTRY(1, action_0, "state 0 event 0"),
        ENTRY(0, NULL,     "state 0 event 1"),
        ENTRY(0, NULL,     "state 0 event 2"),
        ENTRY(0, NULL,     "state 0 event 3"),
        ENTRY(0, NULL,     "state 0 event 4"),
        ENTRY(0, NULL,     "state 0 event 5"),
        ENTRY(5, action_6, "state 0 event 6")
    },
    {
        ENTRY(1, action_4, "state 1 event 0"),
        ENTRY(2, action_1, "state 1 event 1"),
        ENTRY(1, NULL,     "state 1 event 2"),
        ENTRY(1, NULL,     "state 1 event 3"),
        ENTRY(1, NULL,     "state 1 event 4"),
        ENTRY(1, NULL,     "state 1 event 5"),
        ENTRY(5, action_6, "state 1 event 6")
    },
    {
        ENTRY(2, NULL,     "state 2 event 0"),
        ENTRY(2, NULL,     "state 2 event 1"),
        ENTRY(3, action_2, "state 2 event 2"),
        ENTRY(2, NULL,     "state 2 event 3"),
        ENTRY(2, NULL,     "state 2 event 4"),
        ENTRY(2, NULL,     "state 2 event 5"),
        ENTRY(5, action_6, "state 2 event 6")
    },
    {
        ENTRY(3, NULL,     "state 3 event 0"),
        ENTRY(3, NULL,     "state 3 event 1"),
        ENTRY(3, action_3, "state 3 event 2"),
        ENTRY(4, NULL,     "state 3 event 3"),
        ENTRY(3, NULL,     "state 3 event 4"),
        ENTRY(0, action_5, "state 3 event 5"),
        ENTRY(5, action_6, "state 3 event 6")
    },
    {
        ENTRY(4, NULL,     "state 4 event 0"),
        ENTRY(4, NULL,     "state 4 event 1"),
        ENTRY(4, NULL,     "state 4 event 2"),
        ENTRY(4, NULL,     "state 4 event 3"),
        ENTRY(3, NULL,     "state 4 event 4"),
        ENTRY(0, action_5, "state 4 event 5"),
        ENTRY(5, action_6, "state 4 event 6")
    },
    {
        ENTRY(1, action_0, "state 5 event 0"),
        ENTRY(5, NULL,     "state 5 event 1"),
        ENTRY(5, NULL,     "state 5 event 2"),
        ENTRY(5, NULL,     "state 5 event 3"),
        ENTRY(5, NULL,     "state 5 event 4"),
        ENTRY(0, action_5, "state 5 event 5"),
        ENTRY(5, action_6, "state 5 event 6")
    }
};

#undef ENTRY

static void machine_init(machine *object, uint32_t identifier,
                         const char *name)
{
    memset(object, 0, sizeof(*object));
    object->identifier = identifier;
    strncpy(object->name, name, 23);
    object->state = 0;
    object->previous_state = 0;
}

static void machine_dispatch(machine *object, uint32_t event)
{
    const transition *selected =
        &transition_table[object->state][event];

    object->event_counts[event]++;

    if (selected->next_state == object->state && selected->action == NULL) {
        object->invalid_count++;
        printf("%u %u %u\n", object->state, event, object->state);
        return;
    }

    if (selected->action != NULL)
        selected->action(object, event);

    object->previous_state = object->state;
    object->state = selected->next_state;
    object->transition_count++;

    printf("%u %u %u\n",
           object->previous_state, event, object->state);
}

static void machine_print(const machine *object)
{
    uint32_t i;

    printf("%u %s %u %u %u %u\n",
           object->identifier,
           object->name,
           object->state,
           object->previous_state,
           object->transition_count,
           object->invalid_count);

    switch (object->state) {
    case 0:
    case 5:
        printf("%u %s\n",
               object->data.message.code,
               object->data.message.text);
        break;

    case 1:
    case 2:
        printf("%u %u %hu %hhu\n",
               object->data.values32.first,
               object->data.values32.second,
               object->data.values32.third,
               object->data.values32.fourth);
        break;

    case 3:
    case 4:
        printf("%llu %llu %u %hu %hhu\n",
               (unsigned long long)object->data.values64.first,
               (unsigned long long)object->data.values64.second,
               object->data.values64.third,
               object->data.values64.fourth,
               object->data.values64.fifth);
        break;

    default:
        break;
    }

    for (i = 0; i <= 6; ++i)
        printf("%u ", object->event_counts[i]);

    putchar('\n');
}

int main(void)
{
    machine first;
    machine second;
    static const uint32_t first_events[8] = {
        0, 1, 2, 2, 3, 4, 5, 6
    };
    static const uint32_t second_events[8] = {
        6, 0, 1, 2, 3, 2, 4, 5
    };
    uint32_t configured_actions = 0;
    uint32_t state;
    uint32_t event;
    uint32_t i;

    machine_init(&first, 1, "primary-node");
    machine_init(&second, 2, "secondary-node");

    for (i = 0; i <= 7; ++i)
        machine_dispatch(&first, first_events[i]);

    machine_print(&first);

    for (i = 0; i <= 7; ++i)
        machine_dispatch(&second, second_events[i]);

    machine_print(&second);

    for (state = 0; state <= 5; ++state) {
        for (event = 0; event <= 6; ++event) {
            if (transition_table[state][event].action != NULL)
                configured_actions++;
        }
    }

    printf("callbacks %u/%u\n", 42U, configured_actions);
    return 0;
}