#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct machine Machine;
typedef void (*action_fn)(Machine *, uint32_t);

typedef union {
    struct {
        uint32_t first;
        uint32_t second;
        uint16_t third;
        uint8_t fourth;
        uint8_t reserved[21];
    } narrow;

    struct {
        uint32_t value;
        char text[24];
        uint8_t reserved[4];
    } text;

    struct {
        uint64_t first;
        uint64_t second;
        uint32_t third;
        uint16_t fourth;
        uint8_t fifth;
        uint8_t reserved[9];
    } wide;

    struct __attribute__((packed)) {
        uint32_t flags;
        char text[16];
        uint32_t value0;
        uint16_t value1;
        uint8_t value2;
        uint32_t value3;
        uint8_t value4;
    } tagged;

    uint8_t bytes[32];
} Payload;

struct machine {
    int32_t identifier;
    char name[24];
    uint32_t state;
    uint32_t previous_state;
    uint32_t transition_count;
    uint32_t no_action_count;
    uint32_t event_count[7];
    Payload payload;
};

typedef struct {
    uint32_t next_state;
    action_fn action;
    const void *opaque;
} Transition;

_Static_assert(offsetof(Machine, state) == 0x1c, "layout");
_Static_assert(offsetof(Machine, event_count) == 0x2c, "layout");
_Static_assert(offsetof(Machine, payload) == 0x48, "layout");
_Static_assert(sizeof(Payload) == 32, "layout");
_Static_assert(sizeof(Transition) == 24, "layout");

static void action_seed(Machine *machine, uint32_t event)
{
    (void)event;
    machine->payload.narrow.first =
        (uint32_t)machine->identifier * 7u + 0x1000u;
    machine->payload.narrow.second =
        (uint32_t)machine->identifier * 13u + 0x9000u;
    machine->payload.narrow.fourth = 0;
}

static void action_accumulate(Machine *machine, uint32_t event)
{
    uint32_t count = machine->transition_count;
    uint32_t first_increment = count * 8u + 0x200u;
    uint32_t second_increment = count * 3u + 0x80u;

    (void)event;
    machine->payload.wide.first += first_increment;
    machine->payload.wide.second += second_increment;
    machine->payload.wide.third +=
        (uint8_t)machine->payload.wide.first;
}

static void action_increment(Machine *machine, uint32_t event)
{
    (void)event;
    ++machine->payload.narrow.fourth;
}

static void action_reset(Machine *machine, uint32_t event)
{
    (void)event;
    memset(&machine->payload, 0, sizeof(machine->payload));
    machine->payload.narrow.third = 0x2000u;
}

static void action_convert(Machine *machine, uint32_t event)
{
    uint32_t old_value = machine->payload.narrow.first;

    (void)event;
    memset(&machine->payload, 0, sizeof(machine->payload));
    machine->payload.wide.third = old_value + 1u;
    machine->payload.wide.fourth = 0x05b4u;
    machine->payload.wide.fifth = 1;
}

static void action_format(Machine *machine, uint32_t event)
{
    uint64_t sum =
        machine->payload.wide.first + machine->payload.wide.second;

    memset(&machine->payload, 0, sizeof(machine->payload));
    machine->payload.text.value = event;
    snprintf(machine->payload.text.text,
             sizeof(machine->payload.text.text),
             "sum-%016lx", (unsigned long)sum);
}

static void action_tag(Machine *machine, uint32_t event)
{
    static const char tag[16] = "flagged";

    memset(&machine->payload, 0, sizeof(machine->payload));
    machine->payload.tagged.flags = event | 0x8000u;
    memcpy(machine->payload.tagged.text, tag, sizeof(tag));
}

#define ENTRY(next_, action_) { (next_), (action_), NULL }

static const Transition transitions[7][7] = {
    {
        ENTRY(1, action_reset),
        ENTRY(2, action_seed),
        ENTRY(3, action_convert),
        ENTRY(4, action_accumulate),
        ENTRY(5, action_tag),
        ENTRY(6, action_increment),
        ENTRY(0, NULL)
    },
    {
        ENTRY(1, NULL),
        ENTRY(2, action_seed),
        ENTRY(3, action_convert),
        ENTRY(4, action_accumulate),
        ENTRY(5, action_tag),
        ENTRY(6, action_increment),
        ENTRY(0, action_format)
    },
    {
        ENTRY(1, action_reset),
        ENTRY(2, NULL),
        ENTRY(3, action_convert),
        ENTRY(4, action_accumulate),
        ENTRY(5, action_tag),
        ENTRY(6, action_increment),
        ENTRY(0, action_format)
    },
    {
        ENTRY(1, action_reset),
        ENTRY(2, action_seed),
        ENTRY(3, NULL),
        ENTRY(4, action_accumulate),
        ENTRY(5, action_tag),
        ENTRY(6, action_increment),
        ENTRY(0, action_format)
    },
    {
        ENTRY(1, action_reset),
        ENTRY(2, action_seed),
        ENTRY(3, action_convert),
        ENTRY(4, NULL),
        ENTRY(5, action_tag),
        ENTRY(6, action_increment),
        ENTRY(0, action_format)
    },
    {
        ENTRY(1, action_reset),
        ENTRY(2, action_seed),
        ENTRY(3, action_convert),
        ENTRY(4, action_accumulate),
        ENTRY(5, NULL),
        ENTRY(6, action_increment),
        ENTRY(0, action_format)
    },
    {
        ENTRY(1, action_reset),
        ENTRY(2, action_seed),
        ENTRY(3, action_convert),
        ENTRY(4, action_accumulate),
        ENTRY(5, action_tag),
        ENTRY(6, NULL),
        ENTRY(0, action_format)
    }
};

static void dispatch(Machine *machine, uint32_t event)
{
    uint32_t old_state = machine->state;
    const Transition *transition;

    ++machine->event_count[event];
    transition = &transitions[old_state][event];

    if (old_state == transition->next_state && transition->action == NULL) {
        ++machine->no_action_count;
        printf("=%u/%u/%u\n", old_state, event, old_state);
        return;
    }

    if (transition->action != NULL)
        transition->action(machine, event);

    ++machine->transition_count;
    old_state = machine->state;

    printf(">%u/%u/%u\n", old_state, event, transition->next_state);

    machine->state = transition->next_state;
    machine->previous_state = old_state;
}

static void print_machine(const Machine *machine)
{
    uint32_t i;

    printf("%d %s %u %u %u %u",
           machine->identifier,
           machine->name,
           machine->state,
           machine->previous_state,
           machine->transition_count,
           machine->no_action_count);

    if (machine->state == 1 || machine->state == 2) {
        printf(" %u/%u/%u/%u",
               machine->payload.narrow.first,
               machine->payload.narrow.second,
               (unsigned)machine->payload.narrow.third,
               (unsigned)machine->payload.narrow.fourth);
    } else if (machine->state == 0 || machine->state == 5) {
        printf(" %u:%s",
               machine->payload.text.value,
               machine->payload.text.text);
    } else if (machine->state == 3 || machine->state == 4) {
        printf(" %lu/%lu/%u/%u/%u",
               (unsigned long)machine->payload.wide.first,
               (unsigned long)machine->payload.wide.second,
               machine->payload.wide.third,
               (unsigned)machine->payload.wide.fourth,
               (unsigned)machine->payload.wide.fifth);
    }

    for (i = 0; i < 7; ++i)
        printf(" %u", machine->event_count[i]);

    putc('\n', stdout);
}

int main(void)
{
    static const uint32_t first_events[8] =
        { 0, 1, 2, 3, 4, 5, 6, 0 };
    static const uint32_t second_events[8] =
        { 0, 2, 4, 6, 1, 3, 5, 0 };

    Machine first = {
        .identifier = 1,
        .name = "machine_one"
    };
    Machine second = {
        .identifier = 2,
        .name = "machine_two"
    };

    uint32_t i;
    uint32_t state;
    uint32_t event;
    uint32_t action_count = 0;

    for (i = 0; i < 8; ++i)
        dispatch(&first, first_events[i]);
    print_machine(&first);

    for (i = 0; i < 8; ++i)
        dispatch(&second, second_events[i]);
    print_machine(&second);

    for (state = 0; state < 7; ++state)
        for (event = 0; event < 7; ++event)
            if (transitions[state][event].action != NULL)
                ++action_count;

    printf("actions: expected=%u actual=%u\n", 42u, action_count);
    return 0;
}