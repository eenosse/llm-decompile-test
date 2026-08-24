#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    STATE_COUNT = 6,
    EVENT_COUNT = 7,
    TRACE_LENGTH = 8
};

struct small_payload {
    uint32_t first;
    uint32_t second;
    uint16_t third;
    uint8_t fourth;
};

struct wide_payload {
    uint64_t first;
    uint64_t second;
    uint32_t third;
    uint16_t fourth;
    uint8_t fifth;
};

struct text_payload {
    uint32_t code;
    char text[24];
};

union payload {
    struct small_payload small;
    struct wide_payload wide;
    struct text_payload text;
    unsigned char bytes[32];
};

struct machine {
    uint32_t identifier;
    char label[24];
    uint32_t state;
    uint32_t previous_state;
    uint32_t transition_count;
    uint32_t ignored_count;
    uint32_t event_count[EVENT_COUNT];
    union payload payload;
};

typedef void (*action_fn)(struct machine *, uint32_t);

struct transition {
    uint32_t destination;
    action_fn action;
    uintptr_t metadata;
};

_Static_assert(offsetof(struct machine, state) == 0x1c, "layout");
_Static_assert(offsetof(struct machine, event_count) == 0x2c, "layout");
_Static_assert(offsetof(struct machine, payload) == 0x48, "layout");
_Static_assert(sizeof(struct machine) == 0x68, "layout");
_Static_assert(sizeof(struct transition) == 0x18, "layout");

static void initialize_small(struct machine *machine, uint32_t event)
{
    (void)event;
    machine->payload.small.first =
        UINT32_C(0x1000) + machine->identifier * UINT32_C(7);
    machine->payload.small.second =
        UINT32_C(0x9000) + machine->identifier * UINT32_C(13);
    machine->payload.small.fourth = 0;
}

static void accumulate_wide(struct machine *machine, uint32_t event)
{
    uint32_t first_increment;
    uint32_t second_increment;

    (void)event;
    first_increment =
        machine->transition_count * UINT32_C(8) + UINT32_C(0x200);
    second_increment =
        machine->transition_count * UINT32_C(3) + UINT32_C(0x80);

    machine->payload.wide.first += (uint64_t)first_increment;
    machine->payload.wide.second += (uint64_t)second_increment;
    machine->payload.wide.third +=
        (uint8_t)machine->payload.wide.first;
}

static void increment_small_byte(struct machine *machine, uint32_t event)
{
    (void)event;
    ++machine->payload.small.fourth;
}

static void initialize_wide(struct machine *machine, uint32_t event)
{
    (void)event;
    memset(&machine->payload, 0, sizeof(machine->payload));
    machine->payload.wide.second = UINT64_C(0x2000);
}

static void convert_to_wide(struct machine *machine, uint32_t event)
{
    uint32_t previous_value = machine->payload.small.first;

    (void)event;
    memset(&machine->payload, 0, sizeof(machine->payload));
    machine->payload.wide.third = previous_value + UINT32_C(1);
    machine->payload.wide.fourth = UINT16_C(0x05b4);
    machine->payload.wide.fifth = UINT8_C(1);
}

static void convert_to_text(struct machine *machine, uint32_t event)
{
    uint64_t total =
        machine->payload.wide.first + machine->payload.wide.second;

    memset(&machine->payload, 0, sizeof(machine->payload));
    machine->payload.text.code = event;
    (void)snprintf(machine->payload.text.text,
                   sizeof(machine->payload.text.text),
                   "sum=%" PRIu64, total);
}

static void initialize_flagged_small(struct machine *machine, uint32_t event)
{
    memset(&machine->payload, 0, sizeof(machine->payload));
    machine->payload.small.first = event | UINT32_C(0x8000);
    machine->payload.small.second = UINT32_C(0x12345678);
    machine->payload.small.third = UINT16_C(0x3456);
    machine->payload.small.fourth = UINT8_C(1);
}

#define T(destination_, action_) \
    { (destination_), (action_), UINTPTR_C(0) }

static const struct transition transitions[STATE_COUNT][EVENT_COUNT] = {
    {
        T(1, initialize_small),
        T(0, NULL),
        T(0, NULL),
        T(3, initialize_wide),
        T(0, NULL),
        T(0, NULL),
        T(2, initialize_flagged_small)
    },
    {
        T(1, initialize_small),
        T(1, NULL),
        T(1, increment_small_byte),
        T(3, initialize_wide),
        T(4, convert_to_wide),
        T(5, convert_to_text),
        T(2, initialize_flagged_small)
    },
    {
        T(1, initialize_small),
        T(2, NULL),
        T(2, increment_small_byte),
        T(3, initialize_wide),
        T(4, convert_to_wide),
        T(5, convert_to_text),
        T(2, initialize_flagged_small)
    },
    {
        T(1, initialize_small),
        T(3, accumulate_wide),
        T(3, NULL),
        T(3, initialize_wide),
        T(4, NULL),
        T(5, convert_to_text),
        T(2, initialize_flagged_small)
    },
    {
        T(1, initialize_small),
        T(4, accumulate_wide),
        T(4, NULL),
        T(3, initialize_wide),
        T(4, NULL),
        T(5, convert_to_text),
        T(2, initialize_flagged_small)
    },
    {
        T(1, initialize_small),
        T(5, NULL),
        T(5, NULL),
        T(3, initialize_wide),
        T(4, convert_to_wide),
        T(5, convert_to_text),
        T(2, initialize_flagged_small)
    }
};

#undef T

static void process_event(struct machine *machine, uint32_t event)
{
    const struct transition *transition;
    uint32_t old_state;

    ++machine->event_count[event];
    transition = &transitions[machine->state][event];

    if (machine->state == transition->destination &&
        transition->action == NULL) {
        ++machine->ignored_count;
        printf("%" PRIu32 " %" PRIu32 " %" PRIu32 "\n",
               machine->state, event, machine->state);
        return;
    }

    if (transition->action != NULL)
        transition->action(machine, event);

    old_state = machine->state;
    ++machine->transition_count;
    machine->previous_state = old_state;
    machine->state = transition->destination;

    printf("%" PRIu32 " %" PRIu32 " %" PRIu32 "\n",
           old_state, event, transition->destination);
}

static void print_machine(const struct machine *machine)
{
    unsigned int i;

    printf("%" PRIu32 " %s %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32,
           machine->identifier,
           machine->label,
           machine->state,
           machine->previous_state,
           machine->transition_count,
           machine->ignored_count);

    switch (machine->state) {
    case 0:
    case 5:
        printf(" %" PRIu32 " %s",
               machine->payload.text.code,
               machine->payload.text.text);
        break;

    case 1:
    case 2:
        printf(" %" PRIu32 " %" PRIu32 " %" PRIu16 " %" PRIu8,
               machine->payload.small.first,
               machine->payload.small.second,
               machine->payload.small.third,
               machine->payload.small.fourth);
        break;

    case 3:
    case 4:
        printf(" %" PRIu64 " %" PRIu64 " %" PRIu32 " %" PRIu16 " %" PRIu8,
               machine->payload.wide.first,
               machine->payload.wide.second,
               machine->payload.wide.third,
               machine->payload.wide.fourth,
               machine->payload.wide.fifth);
        break;

    default:
        break;
    }

    for (i = 0; i < EVENT_COUNT; ++i)
        printf(" %" PRIu32, machine->event_count[i]);

    putc('\n', stdout);
}

int main(void)
{
    static const uint32_t second_trace[TRACE_LENGTH] =
        { 6, 2, 3, 1, 1, 5, 0, 2 };
    static const uint32_t first_trace[TRACE_LENGTH] =
        { 0, 2, 2, 4, 1, 1, 5, 5 };

    struct machine first = {
        .identifier = 1,
        .label = "object-a"
    };
    struct machine second = {
        .identifier = 2,
        .label = "object-b"
    };
    uint32_t action_count = 0;
    unsigned int state;
    unsigned int event;

    for (event = 0; event < TRACE_LENGTH; ++event)
        process_event(&first, first_trace[event]);
    print_machine(&first);

    for (event = 0; event < TRACE_LENGTH; ++event)
        process_event(&second, second_trace[event]);
    print_machine(&second);

    for (state = 0; state < STATE_COUNT; ++state)
        for (event = 0; event < EVENT_COUNT; ++event)
            if (transitions[state][event].action != NULL)
                ++action_count;

    printf("%u %" PRIu32 "\n", STATE_COUNT * EVENT_COUNT, action_count);
    return 0;
}