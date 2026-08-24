#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Link Link;
typedef struct Item Item;
typedef struct Container Container;
typedef struct IndexNode IndexNode;
typedef struct Registry Registry;
typedef struct Packet Packet;

struct Link {
    Link *next;
    Link *previous;
};

struct Packet {
    uint16_t length;
    uint16_t type;
    unsigned char data[];
};

typedef int (*ScoreFunction)(Item *);

union ItemPayload {
    struct {
        uint16_t first;
        uint16_t second;
        uint8_t factor;
        char text[6];
    } kind0;

    struct {
        int16_t first;
        int16_t second;
        uint16_t scale;
        uint8_t factor;
    } kind1;

    struct {
        uint32_t initial;
        uint8_t digits[8];
    } kind2;

    struct {
        Packet *packet;
        uint32_t evaluation_count;
    } kind3;

    struct {
        Container *container;
        uint16_t discount;
    } kind4;
};

struct Item {
    uint32_t identifier;
    char name[20];
    uint16_t first_attribute;
    uint16_t second_attribute;
    uint16_t third_attribute;
    int64_t amount;
    uint16_t capacity;
    unsigned char flags[4];
    uint32_t kind;
    Link link;
    union ItemPayload payload;
    ScoreFunction score;
};

struct Container {
    char name[16];
    uint16_t first_attribute;
    uint16_t second_attribute;
    uint16_t third_attribute;
    Link items;
    uint32_t item_count;
    Container *parent;
    Container *first_child;
    Container *next_sibling;
    uint8_t slots[4][6];
};

struct IndexNode {
    uint32_t key;
    Item *item;
    IndexNode *lower;
    IndexNode *higher;
    uint32_t depth;
};

struct Registry {
    Container *root;
    IndexNode *index;
    uint32_t next_identifier;
    uint32_t item_count;
};

_Static_assert(offsetof(Item, link) == 0x38, "unexpected Item layout");
_Static_assert(offsetof(Item, payload) == 0x48, "unexpected Item layout");
_Static_assert(offsetof(Item, score) == 0x58, "unexpected Item layout");
_Static_assert(sizeof(Item) == 0x60, "unexpected Item size");
_Static_assert(offsetof(Container, items) == 0x18, "unexpected Container layout");
_Static_assert(offsetof(Container, slots) == 0x48, "unexpected Container layout");
_Static_assert(sizeof(Container) == 0x60, "unexpected Container size");

static void initialize_link(Link *link)
{
    link->next = link;
    link->previous = link;
}

static void insert_before(Link *position, Link *entry)
{
    entry->previous = position->previous;
    entry->next = position;
    position->previous->next = entry;
    position->previous = entry;
}

static void remove_link(Link *entry)
{
    entry->previous->next = entry->next;
    entry->next->previous = entry->previous;
    initialize_link(entry);
}

static Item *item_from_link(Link *link)
{
    return (Item *)((unsigned char *)link - offsetof(Item, link));
}

static int score_kind0(Item *item)
{
    uint32_t sum = (uint32_t)item->payload.kind0.first
                 + (uint32_t)item->payload.kind0.second;

    return (int)(sum * 5U + (uint32_t)item->payload.kind0.factor * 3U);
}

static int score_kind1(Item *item)
{
    int value = (int)item->payload.kind1.first
              + (int)item->payload.kind1.second;

    if (value < 0)
        value = -value;

    return (value * 2
            + (int)(item->payload.kind1.scale / 10U))
           * (int)item->payload.kind1.factor;
}

static int score_kind2(Item *item)
{
    uint32_t value = item->payload.kind2.initial;

    for (int i = 0; i < 8; ++i)
        value = value * 3U + item->payload.kind2.digits[i];

    return (int)(value % 5000U);
}

static int score_kind3(Item *item)
{
    Packet *packet = item->payload.kind3.packet;

    if (packet == NULL)
        return 0;

    return (int)((uint32_t)packet->length * 11U
                 + (uint32_t)packet->type * 7U
                 + item->payload.kind3.evaluation_count);
}

static int container_score(Container *container);

static int score_kind4(Item *item)
{
    int nested_score = item->payload.kind4.container != NULL
                     ? container_score(item->payload.kind4.container)
                     : 0;
    uint32_t percentage = 100U - item->payload.kind4.discount;

    return (int)(percentage * (uint32_t)nested_score / 100U);
}

static Container *create_container(const char *name,
                                   uint16_t first,
                                   uint16_t second,
                                   uint16_t third)
{
    Container *container = calloc(1, sizeof(*container));

    if (container == NULL)
        exit(1);

    strncpy(container->name, name, 15);
    container->first_attribute = first;
    container->second_attribute = second;
    container->third_attribute = third;
    initialize_link(&container->items);

    return container;
}

static Container *append_child(Container *parent, Container *child)
{
    Container **position = &parent->first_child;

    while (*position != NULL)
        position = &(*position)->next_sibling;

    *position = child;
    child->parent = parent;
    return child;
}

static Item *create_item(Registry *registry,
                         const char *name,
                         uint32_t kind,
                         int64_t amount)
{
    static const unsigned char initial_flags[4] = { 0, 0, 0, 0 };
    Item *item = calloc(1, sizeof(*item));

    if (item == NULL)
        exit(1);

    item->identifier = registry->next_identifier++;
    strncpy(item->name, name, 19);
    item->kind = kind;
    item->amount = amount;
    item->capacity = 100;
    memcpy(item->flags, initial_flags, sizeof(initial_flags));

    item->first_attribute = (uint16_t)(item->identifier % 3U + 1U);
    item->second_attribute = (uint16_t)((item->identifier & 1U) + 1U);
    item->third_attribute = 1;
    initialize_link(&item->link);

    switch (kind) {
    case 0:
        item->score = score_kind0;
        break;
    case 1:
        item->score = score_kind1;
        break;
    case 2:
        item->score = score_kind2;
        break;
    case 3:
        item->score = score_kind3;
        break;
    case 4:
        item->score = score_kind4;
        break;
    default:
        item->score = NULL;
        break;
    }

    ++registry->item_count;
    return item;
}

static void add_item(Container *container, Item *item)
{
    insert_before(&container->items, &item->link);
    ++container->item_count;

    for (uint16_t row = 0; row <= 3; ++row) {
        for (uint16_t column = 0; column <= 5; ++column) {
            if (container->slots[row][column] == 0) {
                container->slots[row][column] =
                    (uint8_t)item->identifier;
                return;
            }
        }
    }
}

static void move_item(Container *source, Container *destination, Item *item)
{
    remove_link(&item->link);
    --source->item_count;

    for (uint16_t row = 0; row <= 3; ++row) {
        for (uint16_t column = 0; column <= 5; ++column) {
            if (source->slots[row][column] ==
                (uint8_t)item->identifier)
                source->slots[row][column] = 0;
        }
    }

    add_item(destination, item);
}

static Packet *create_packet(const unsigned char *data,
                             uint16_t length,
                             uint16_t type)
{
    Packet *packet = malloc((size_t)length + 4U);

    if (packet == NULL)
        exit(1);

    packet->length = length;
    packet->type = type;
    memcpy(packet->data, data, length);
    return packet;
}

static IndexNode *insert_index(IndexNode *root,
                               Item *item,
                               uint32_t depth)
{
    if (root == NULL) {
        IndexNode *node = calloc(1, sizeof(*node));

        if (node == NULL)
            exit(1);

        node->key = item->identifier;
        node->item = item;
        node->depth = depth;
        return node;
    }

    if (item->identifier < root->key)
        root->lower = insert_index(root->lower, item, depth + 1U);
    else
        root->higher = insert_index(root->higher, item, depth + 1U);

    return root;
}

static Item *find_index(IndexNode *root, uint32_t key)
{
    while (root != NULL) {
        if (key == root->key)
            return root->item;

        root = key < root->key ? root->lower : root->higher;
    }

    return NULL;
}

static void print_index(IndexNode *root)
{
    if (root == NULL)
        return;

    print_index(root->lower);

    printf("%u %u %s %u %d\n",
           root->key,
           root->depth,
           root->item->name,
           root->item->kind,
           root->item->score != NULL ? root->item->score(root->item) : 0);

    print_index(root->higher);
}

static void free_index(IndexNode *root)
{
    if (root == NULL)
        return;

    free_index(root->lower);
    free_index(root->higher);
    free(root);
}

static int container_score(Container *container)
{
    int result = 0;

    for (Link *link = container->items.next;
         link != &container->items;
         link = link->next) {
        Item *item = item_from_link(link);

        if (item->score != NULL)
            result += item->score(item);
    }

    for (Container *child = container->first_child;
         child != NULL;
         child = child->next_sibling)
        result += container_score(child);

    return result;
}

static void print_item(Item *item, int depth)
{
    printf("%d %u %s %u %u %u %.2f %u\n",
           depth,
           item->identifier,
           item->name,
           item->first_attribute,
           item->second_attribute,
           item->third_attribute,
           (double)item->amount / (double)item->capacity,
           item->kind);

    switch (item->kind) {
    case 0:
        printf("%u %u %u %s\n",
               item->payload.kind0.first,
               item->payload.kind0.second,
               item->payload.kind0.factor,
               item->payload.kind0.text);
        break;

    case 1:
        printf("%d %d %u %u\n",
               item->payload.kind1.first,
               item->payload.kind1.second,
               item->payload.kind1.scale,
               item->payload.kind1.factor);
        break;

    case 2:
        printf("%u %u %u\n",
               item->payload.kind2.initial,
               item->payload.kind2.digits[0],
               item->payload.kind2.digits[1]);
        break;

    case 3:
        printf("%u %u %u\n",
               item->payload.kind3.packet != NULL
                   ? item->payload.kind3.packet->length : 0,
               item->payload.kind3.packet != NULL
                   ? item->payload.kind3.packet->type : 0,
               item->payload.kind3.evaluation_count);
        break;

    case 4:
        printf("%s %u\n",
               item->payload.kind4.container != NULL
                   ? item->payload.kind4.container->name : "",
               item->payload.kind4.discount);
        break;
    }

    printf("%d\n", item->score != NULL ? item->score(item) : 0);
}

static void print_container(Container *container, int depth)
{
    printf("%d %s %u %u %u %u %d\n",
           depth,
           container->name,
           container->first_attribute,
           container->second_attribute,
           container->third_attribute,
           container->item_count,
           container_score(container));

    for (uint16_t row = 0; row <= 3; ++row) {
        for (uint16_t column = 0; column <= 5; ++column)
            printf("%u ", container->slots[row][column]);
        putchar('\n');
    }

    for (Link *link = container->items.next;
         link != &container->items;
         link = link->next)
        print_item(item_from_link(link), depth + 1);

    for (Container *child = container->first_child;
         child != NULL;
         child = child->next_sibling)
        print_container(child, depth + 1);
}

static int evaluate_packet(Item *item)
{
    int32_t stack[16];
    int top = 0;
    Packet *packet = item->payload.kind3.packet;

    if (packet == NULL)
        return 0;

    ++item->payload.kind3.evaluation_count;

    for (uint16_t i = 0; i < packet->length; ++i) {
        uint8_t instruction = packet->data[i];

        switch (instruction >> 4) {
        case 0:
            if (top <= 15)
                stack[top++] = instruction & 15;
            break;

        case 1:
            if (top > 1) {
                stack[top - 2] += stack[top - 1];
                --top;
            }
            break;

        case 2:
            if (top > 1) {
                stack[top - 2] *= stack[top - 1];
                --top;
            }
            break;

        case 3:
            if (top > 1) {
                stack[top - 2] -= stack[top - 1];
                --top;
            }
            break;

        case 4:
            if (top > 0)
                stack[top - 1] = -stack[top - 1];
            break;
        }
    }

    return top > 0 ? stack[top - 1] : 0;
}

static void free_container(Container *container)
{
    Link *link = container->items.next;

    while (link != &container->items) {
        Link *next = link->next;
        Item *item = item_from_link(link);

        if (item->kind == 3)
            free(item->payload.kind3.packet);

        free(item);
        link = next;
    }

    Container *child = container->first_child;

    while (child != NULL) {
        Container *next = child->next_sibling;
        free_container(child);
        child = next;
    }

    free(container);
}

int main(void)
{
    Registry registry;
    memset(&registry, 0, sizeof(registry));
    registry.next_identifier = 1;

    Container *root = create_container("root0", 16, 16, 16);
    Container *branch_a =
        append_child(root, create_container("branch_a", 6, 4, 2));
    Container *branch_b =
        append_child(root, create_container("nodeB", 8, 6, 4));
    Container *leaf =
        append_child(branch_a, create_container("leaf0", 2, 2, 1));

    registry.root = root;

    Item *first = create_item(&registry, "record_001", 0, 12500);
    first->payload.kind0.first = 12;
    first->payload.kind0.second = 28;
    first->payload.kind0.factor = 7;
    memcpy(first->payload.kind0.text, "alpha", 6);
    add_item(branch_a, first);
    registry.index = insert_index(registry.index, first, 0);

    Item *second = create_item(&registry, "item02", 1, 3200);
    second->payload.kind1.first = 120;
    second->payload.kind1.second = 45;
    second->payload.kind1.scale = 300;
    second->payload.kind1.factor = 3;
    add_item(branch_a, second);
    registry.index = insert_index(registry.index, second, 0);

    Item *third = create_item(&registry, "record003", 2, 500);
    third->payload.kind2.initial = 4711;

    for (int i = 0; i <= 7; ++i)
        third->payload.kind2.digits[i] = (uint8_t)(90 + i * 3);

    add_item(leaf, third);
    registry.index = insert_index(registry.index, third, 0);

    Item *fourth = create_item(&registry, "expression0", 3, 8800);
    {
        static const unsigned char bytecode[8] = { 0 };
        fourth->payload.kind3.packet = create_packet(bytecode, 8, 8);
    }
    add_item(branch_b, fourth);
    registry.index = insert_index(registry.index, fourth, 0);

    int evaluation = evaluate_packet(fourth);
    printf("%d %u\n", evaluation, fourth->payload.kind3.evaluation_count);

    evaluation = evaluate_packet(fourth);
    printf("%d %u\n", evaluation, fourth->payload.kind3.evaluation_count);

    Item *fifth = create_item(&registry, "reference00", 4, 15000);
    fifth->payload.kind4.container = branch_a;
    fifth->payload.kind4.discount = 20;
    add_item(branch_b, fifth);
    registry.index = insert_index(registry.index, fifth, 0);

    print_container(root, 0);

    move_item(branch_a, leaf, first);

    print_container(root, 0);
    print_index(registry.index);

    Item *found = find_index(registry.index, 3);
    printf("%s %d\n",
           found != NULL ? found->name : "",
           found != NULL ? (int)found->kind : -1);

    unsigned long total = (uint32_t)container_score(root);
    printf("%lu %u %u\n",
           total,
           registry.item_count,
           registry.next_identifier);

    free_index(registry.index);
    free_container(root);
    return 0;
}