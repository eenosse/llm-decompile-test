#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Event Event;
typedef struct Group Group;
typedef struct TreeNode TreeNode;

typedef int32_t (*ScoreFunction)(Event *);

typedef struct {
    Event *next;
    Event *previous;
} EventLink;

typedef union {
    struct {
        uint16_t first;
        uint16_t second;
        uint8_t multiplier;
        char label[7];
    } kind0;

    struct {
        int16_t first;
        int16_t second;
        uint16_t divisor_value;
        uint8_t multiplier;
    } kind1;

    struct {
        uint32_t initial;
        uint8_t values[8];
    } kind2;

    struct {
        uint16_t length;
        uint16_t capacity;
        uint8_t instructions[];
    } *program;

    struct {
        Group *group;
        uint32_t percentage;
    } kind4;

    unsigned char raw[16];
} EventPayload;

struct Event {
    uint32_t identifier;
    char name[20];
    uint16_t coordinate_a;
    uint16_t coordinate_b;
    uint16_t coordinate_c;
    uint16_t padding_1;
    int64_t value;
    uint16_t health;
    char currency[4];
    uint16_t padding_2;
    uint32_t category;
    uint32_t padding_3;
    EventLink link;
    EventPayload payload;
    ScoreFunction score;
};

struct Group {
    char name[16];
    uint16_t extent_a;
    uint16_t extent_b;
    uint16_t extent_c;
    uint16_t padding;
    EventLink events;
    uint32_t event_count;
    uint32_t padding_2;
    Group *parent;
    Group *first_child;
    Group *next_sibling;
    uint8_t occupancy[4][6];
};

struct TreeNode {
    uint32_t key;
    uint32_t padding;
    Event *event;
    TreeNode *left;
    TreeNode *right;
    uint32_t depth;
    uint32_t padding_2;
};

typedef struct {
    Group *root;
    TreeNode *tree;
    uint32_t counter_a;
    uint32_t counter_b;
    uint64_t total;
} ProgramState;

_Static_assert(sizeof(Event) == 96, "unexpected Event layout");
_Static_assert(offsetof(Event, value) == 0x20, "unexpected Event layout");
_Static_assert(offsetof(Event, category) == 0x30, "unexpected Event layout");
_Static_assert(offsetof(Event, link) == 0x38, "unexpected Event layout");
_Static_assert(offsetof(Event, payload) == 0x48, "unexpected Event layout");
_Static_assert(offsetof(Event, score) == 0x58, "unexpected Event layout");

_Static_assert(sizeof(Group) == 96, "unexpected Group layout");
_Static_assert(offsetof(Group, events) == 0x18, "unexpected Group layout");
_Static_assert(offsetof(Group, event_count) == 0x28, "unexpected Group layout");
_Static_assert(offsetof(Group, first_child) == 0x38, "unexpected Group layout");
_Static_assert(offsetof(Group, occupancy) == 0x48, "unexpected Group layout");

_Static_assert(sizeof(TreeNode) == 40, "unexpected TreeNode layout");

static void *allocate_zeroed(size_t count, size_t size)
{
    void *result = calloc(count, size);
    if (result == NULL)
        exit(1);
    return result;
}

static int32_t score_kind0(Event *event)
{
    uint32_t sum = (uint32_t)event->payload.kind0.first
                 + (uint32_t)event->payload.kind0.second;

    return (int32_t)(sum * 5u
        + (uint32_t)event->payload.kind0.multiplier * 3u);
}

static int32_t score_kind1(Event *event)
{
    int32_t sum = (int32_t)event->payload.kind1.first
                + (int32_t)event->payload.kind1.second;
    int32_t magnitude = sum < 0 ? -sum : sum;
    uint32_t value = (uint32_t)(magnitude * 2)
                   + (uint32_t)event->payload.kind1.divisor_value / 10u;

    return (int32_t)(value
        * (uint32_t)event->payload.kind1.multiplier);
}

static int32_t score_kind2(Event *event)
{
    uint32_t value = event->payload.kind2.initial;

    for (size_t i = 0; i < 8; ++i)
        value = value * 3u + event->payload.kind2.values[i];

    return (int32_t)(value % 5000u);
}

static int32_t group_score(Group *group);

static int32_t score_kind4(Event *event)
{
    uint32_t nested = 0;

    if (event->payload.kind4.group != NULL)
        nested = (uint32_t)group_score(event->payload.kind4.group);

    return (int32_t)(
        (nested * (100u - event->payload.kind4.percentage)) / 100u
    );
}

static int32_t score_program(Event *event)
{
    uint32_t stack[16];
    int stack_size = 0;
    uint16_t length;
    const uint8_t *instructions;

    if (event->payload.program == NULL)
        return 0;

    ++event->payload.kind4.percentage;

    length = event->payload.program->length;
    if (length == 0)
        return 0;

    instructions = event->payload.program->instructions;

    for (uint16_t i = 0; i < length; ++i) {
        uint8_t instruction = instructions[i];

        if (instruction > 0x4f)
            continue;

        switch (instruction >> 4) {
        case 0:
            if (stack_size <= 15)
                stack[stack_size++] = instruction & 0x0f;
            break;

        case 1:
            if (stack_size > 1) {
                stack[stack_size - 2] += stack[stack_size - 1];
                --stack_size;
            }
            break;

        case 2:
            if (stack_size > 1) {
                stack[stack_size - 2] *= stack[stack_size - 1];
                --stack_size;
            }
            break;

        case 3:
            if (stack_size > 1) {
                stack[stack_size - 2] -= stack[stack_size - 1];
                --stack_size;
            }
            break;

        case 4:
            if (stack_size > 0)
                stack[stack_size - 1] = 0u - stack[stack_size - 1];
            break;
        }
    }

    return stack_size > 0 ? (int32_t)stack[stack_size - 1] : 0;
}

static Group *create_group(const char *name,
                           uint16_t extent_a,
                           uint16_t extent_b,
                           uint16_t extent_c)
{
    Group *group = allocate_zeroed(1, sizeof(*group));

    strncpy(group->name, name, 15);
    group->extent_a = extent_a;
    group->extent_b = extent_b;
    group->extent_c = extent_c;
    group->events.next = NULL;
    group->events.previous = NULL;

    return group;
}

static void append_child(Group *parent, Group *child)
{
    Group **position = &parent->first_child;

    while (*position != NULL)
        position = &(*position)->next_sibling;

    *position = child;
    child->parent = parent;
}

static Event *create_event(ProgramState *state,
                           const char *name,
                           uint32_t category,
                           int64_t value)
{
    Event *event = allocate_zeroed(1, sizeof(*event));
    uint32_t identifier = state->counter_a++;

    event->identifier = identifier;
    strncpy(event->name, name, 19);
    event->category = category;
    event->value = value;
    event->health = 100;
    memcpy(event->currency, "USD", 4);
    event->coordinate_a = (uint16_t)(identifier % 3u + 1u);
    event->coordinate_b = (uint16_t)(identifier % 2u + 1u);
    event->coordinate_c = 1;
    event->link.next = event;
    event->link.previous = event;

    switch (category) {
    case 0:
        event->score = score_kind0;
        break;
    case 1:
        event->score = score_kind1;
        break;
    case 2:
        event->score = score_kind2;
        break;
    case 3:
        event->score = score_program;
        break;
    case 4:
        event->score = score_kind4;
        break;
    default:
        event->score = NULL;
        break;
    }

    ++state->counter_b;
    return event;
}

static void add_event(Group *group, Event *event)
{
    Event *tail = group->events.previous;

    event->link.next = NULL;
    event->link.previous = tail;

    if (tail != NULL)
        tail->link.next = event;
    else
        group->events.next = event;

    group->events.previous = event;
    ++group->event_count;

    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 6; ++column) {
            if (group->occupancy[row][column] == 0) {
                group->occupancy[row][column] =
                    (uint8_t)event->identifier;
                return;
            }
        }
    }
}

static void remove_event(Group *group, Event *event)
{
    Event *next = event->link.next;
    Event *previous = event->link.previous;

    if (previous != NULL)
        previous->link.next = next;
    else
        group->events.next = next;

    if (next != NULL)
        next->link.previous = previous;
    else
        group->events.previous = previous;

    event->link.next = event;
    event->link.previous = event;
    --group->event_count;

    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 6; ++column) {
            if (group->occupancy[row][column]
                    == (uint8_t)event->identifier)
                group->occupancy[row][column] = 0;
        }
    }
}

static int32_t group_score(Group *group)
{
    uint32_t total = 0;

    for (Event *event = group->events.next;
         event != NULL;
         event = event->link.next) {
        if (event->score != NULL)
            total += (uint32_t)event->score(event);
    }

    for (Group *child = group->first_child;
         child != NULL;
         child = child->next_sibling)
        total += (uint32_t)group_score(child);

    return (int32_t)total;
}

static TreeNode *insert_tree(TreeNode *node, Event *event, uint32_t depth)
{
    if (node == NULL) {
        node = allocate_zeroed(1, sizeof(*node));
        node->key = event->identifier;
        node->event = event;
        node->depth = depth;
        return node;
    }

    if (event->identifier < node->key)
        node->left = insert_tree(node->left, event, depth + 1);
    else
        node->right = insert_tree(node->right, event, depth + 1);

    return node;
}

static TreeNode *find_tree(TreeNode *node, uint32_t key)
{
    while (node != NULL) {
        if (node->key == key)
            return node;
        if (node->key < key)
            node = node->right;
        else
            node = node->left;
    }

    return NULL;
}

static void print_tree(TreeNode *node)
{
    int32_t score = 0;

    if (node == NULL)
        return;

    print_tree(node->left);

    if (node->event->score != NULL)
        score = node->event->score(node->event);

    printf("%u %u %s %u %d\n",
           node->key,
           node->depth,
           node->event->name,
           node->event->category,
           score);

    print_tree(node->right);
}

static void print_event_payload(Event *event)
{
    switch (event->category) {
    case 0:
        printf("%u %u %u %s",
               (unsigned)event->payload.kind0.first,
               (unsigned)event->payload.kind0.second,
               (unsigned)event->payload.kind0.multiplier,
               event->payload.kind0.label);
        break;

    case 1:
        printf("%d %d %u %u",
               (int)event->payload.kind1.first,
               (int)event->payload.kind1.second,
               (unsigned)event->payload.kind1.divisor_value,
               (unsigned)event->payload.kind1.multiplier);
        break;

    case 2:
        printf("%u %u %u",
               event->payload.kind2.initial,
               (unsigned)event->payload.kind2.values[0],
               (unsigned)event->payload.kind2.values[1]);
        break;

    case 3:
        if (event->payload.program != NULL) {
            printf("%u %u %u",
                   (unsigned)event->payload.program->length,
                   (unsigned)event->payload.program->capacity,
                   event->payload.kind4.percentage);
        } else {
            printf("0 0 %u", event->payload.kind4.percentage);
        }
        break;

    case 4:
        printf("%s %u",
               event->payload.kind4.group != NULL
                   ? event->payload.kind4.group->name
                   : "(null)",
               event->payload.kind4.percentage);
        break;
    }
}

static void print_group(Group *group, unsigned depth)
{
    printf("%u %s %u %u %u %u %d\n",
           depth,
           group->name,
           (unsigned)group->extent_a,
           (unsigned)group->extent_b,
           (unsigned)group->extent_c,
           group->event_count,
           group_score(group));

    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 6; ++column)
            printf("%u ", (unsigned)group->occupancy[row][column]);
        putc('\n', stdout);
    }

    for (Event *event = group->events.next;
         event != NULL;
         event = event->link.next) {
        double ratio = (double)event->value / (double)event->health;
        int32_t score = 0;

        printf("%u %u %s %u %u %u %u %.6f ",
               depth + 1,
               event->identifier,
               event->name,
               (unsigned)event->coordinate_a,
               (unsigned)event->coordinate_b,
               (unsigned)event->coordinate_c,
               event->category,
               ratio);

        print_event_payload(event);

        if (event->score != NULL)
            score = event->score(event);

        printf(" %d\n", score);
    }

    for (Group *child = group->first_child;
         child != NULL;
         child = child->next_sibling)
        print_group(child, depth + 1);
}

static void free_tree(TreeNode *node)
{
    if (node == NULL)
        return;

    free_tree(node->left);
    free_tree(node->right);
    free(node);
}

static void free_groups(Group *group)
{
    if (group == NULL)
        return;

    Event *event = group->events.next;
    while (event != NULL) {
        Event *next = event->link.next;

        if (event->category == 3)
            free(event->payload.program);

        free(event);
        event = next;
    }

    Group *child = group->first_child;
    while (child != NULL) {
        Group *next = child->next_sibling;
        free_groups(child);
        child = next;
    }

    free(group);
}

int main(void)
{
    ProgramState state = {0};
    Group *root;
    Group *group_1;
    Group *group_2;
    Group *group_3;
    Event *event_0;
    Event *event_1;
    Event *event_2;
    Event *event_3;
    Event *event_4;
    TreeNode *found;
    static const uint8_t instructions[8] = {
        0x07, 0x03, 0x10, 0x05, 0x20, 0x02, 0x30, 0x40
    };

    state.counter_a = 1;

    root = create_group("group0", 16, 16, 16);
    group_1 = create_group("group1", 6, 4, 2);
    group_2 = create_group("group2", 8, 6, 4);
    group_3 = create_group("group3", 2, 2, 1);

    append_child(root, group_1);
    append_child(root, group_2);
    append_child(group_1, group_3);
    state.root = root;

    event_0 = create_event(&state, "event0", 0, 12500);
    event_0->payload.kind0.first = 12;
    event_0->payload.kind0.second = 28;
    event_0->payload.kind0.multiplier = 7;
    memcpy(event_0->payload.kind0.label, "steel", 6);
    add_event(group_1, event_0);
    state.tree = insert_tree(state.tree, event_0, 0);

    event_1 = create_event(&state, "event1", 1, 3200);
    event_1->payload.kind1.first = 120;
    event_1->payload.kind1.second = 45;
    event_1->payload.kind1.divisor_value = 300;
    event_1->payload.kind1.multiplier = 3;
    add_event(group_1, event_1);
    state.tree = insert_tree(state.tree, event_1, 0);

    event_2 = create_event(&state, "event2", 2, 500);
    event_2->payload.kind2.initial = 4711;
    event_2->payload.kind2.values[0] = 90;
    event_2->payload.kind2.values[1] = 93;
    event_2->payload.kind2.values[2] = 96;
    event_2->payload.kind2.values[3] = 99;
    event_2->payload.kind2.values[4] = 102;
    event_2->payload.kind2.values[5] = 105;
    event_2->payload.kind2.values[6] = 108;
    event_2->payload.kind2.values[7] = 111;
    add_event(group_3, event_2);
    state.tree = insert_tree(state.tree, event_2, 0);

    event_3 = create_event(&state, "event3", 3, 8800);
    event_3->payload.program = malloc(12);
    if (event_3->payload.program == NULL)
        exit(1);
    event_3->payload.program->length = 8;
    event_3->payload.program->capacity = 8;
    memcpy(event_3->payload.program->instructions,
           instructions, sizeof(instructions));
    add_event(group_2, event_3);
    state.tree = insert_tree(state.tree, event_3, 0);

    printf("%d %u\n",
           score_program(event_3),
           event_3->payload.kind4.percentage);
    printf("%d %u\n",
           score_program(event_3),
           event_3->payload.kind4.percentage);

    event_4 = create_event(&state, "event4", 4, 15000);
    event_4->payload.kind4.group = group_1;
    event_4->payload.kind4.percentage = 20;
    add_event(group_2, event_4);
    state.tree = insert_tree(state.tree, event_4, 0);

    print_group(root, 0);

    remove_event(group_1, event_0);
    add_event(group_3, event_0);

    print_group(root, 0);
    print_tree(state.tree);

    found = find_tree(state.tree, 3);
    if (found != NULL && found->event != NULL)
        printf("%s %u\n", found->event->name, found->event->category);
    else
        printf("(null) %d\n", -1);

    state.total = (uint32_t)group_score(root);
    printf("%u %u %llu\n",
           state.counter_a,
           state.counter_b,
           (unsigned long long)state.total);

    free_tree(state.tree);
    free_groups(root);
    return 0;
}