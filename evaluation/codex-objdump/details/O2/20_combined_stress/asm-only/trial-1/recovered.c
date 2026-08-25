#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node Node;
typedef struct Item Item;
typedef struct TreeNode TreeNode;

typedef struct Link {
    struct Link *next;
    struct Link *prev;
} Link;

typedef int (*ScoreFunction)(Item *);

typedef union Payload {
    struct {
        uint16_t first;
        uint16_t second;
        uint8_t multiplier;
        char label[11];
    } kind0;

    struct {
        int16_t first;
        int16_t second;
        uint16_t scale;
        uint8_t multiplier;
        uint8_t unused[9];
    } kind1;

    struct {
        uint32_t seed;
        uint8_t bytes[8];
        uint8_t unused[4];
    } kind2;

    struct {
        uint16_t *code;
        uint32_t evaluations;
    } kind3;

    struct {
        Node *node;
        uint16_t discount;
        uint8_t unused[6];
    } kind4;

    uint8_t raw[16];
} Payload;

struct Item {
    uint32_t id;
    char name[20];
    uint16_t field_18;
    uint16_t field_1a;
    uint16_t field_1c;
    uint16_t padding_1e;
    uint64_t amount;
    uint16_t rate;
    char currency[4];
    uint16_t padding_2e;
    uint32_t type;
    uint32_t padding_34;
    Link link;
    Payload payload;
    ScoreFunction score;
};

struct Node {
    char name[16];
    uint16_t field_10;
    uint16_t field_12;
    uint16_t field_14;
    uint16_t padding_16;
    Link items;
    uint32_t item_count;
    uint32_t padding_2c;
    Node *parent;
    Node *first_child;
    Node *next_sibling;
    uint8_t slots[4][6];
};

struct TreeNode {
    uint32_t key;
    uint32_t padding_04;
    Item *item;
    TreeNode *left;
    TreeNode *right;
    uint32_t depth;
    uint32_t padding_24;
};

typedef struct Registry {
    Node *root;
    TreeNode *index;
    uint32_t next_id;
    uint32_t item_count;
} Registry;

static void allocation_failed(void)
{
    exit(1);
}

static Node *create_node(const char *name, uint16_t a, uint16_t b, uint16_t c)
{
    Node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        allocation_failed();

    strncpy(node->name, name, 15);
    node->field_10 = a;
    node->field_12 = b;
    node->field_14 = c;
    node->items.next = &node->items;
    node->items.prev = &node->items;
    return node;
}

static void append_child(Node *parent, Node *child)
{
    Node **position = &parent->first_child;

    while (*position != NULL)
        position = &(*position)->next_sibling;

    *position = child;
    child->parent = parent;
}

static int score_kind0(Item *item)
{
    uint32_t sum = (uint32_t)item->payload.kind0.first +
                   (uint32_t)item->payload.kind0.second;

    return (int)(sum * 5u +
                 (uint32_t)item->payload.kind0.multiplier * 3u);
}

static int score_kind1(Item *item)
{
    int32_t sum = (int32_t)item->payload.kind1.first +
                  (int32_t)item->payload.kind1.second;
    uint32_t magnitude = sum < 0 ? (uint32_t)(-sum) : (uint32_t)sum;
    uint32_t value = magnitude * 2u +
                     (uint32_t)item->payload.kind1.scale / 100u;

    return (int)(value * item->payload.kind1.multiplier);
}

static int score_kind2(Item *item)
{
    uint32_t value = item->payload.kind2.seed;
    unsigned i;

    for (i = 0; i < 8; ++i)
        value = value * 3u + item->payload.kind2.bytes[i];

    return (int)(value % 5000u);
}

static int score_kind3(Item *item)
{
    uint16_t *program = item->payload.kind3.code;
    int32_t stack[16];
    int count = 0;
    unsigned i;

    if (program == NULL)
        return 0;

    ++item->payload.kind3.evaluations;

    for (i = 0; i < program[0]; ++i) {
        uint8_t instruction = ((uint8_t *)program)[4 + i];

        if (instruction > 0x4f)
            continue;

        switch (instruction >> 4) {
        case 0:
            if (count <= 15)
                stack[count++] = instruction;
            break;

        case 1:
            if (count > 1) {
                stack[count - 2] += stack[count - 1];
                --count;
            }
            break;

        case 2:
            if (count > 1) {
                stack[count - 2] -= stack[count - 1];
                --count;
            }
            break;

        case 3:
            if (count > 1) {
                stack[count - 2] *= stack[count - 1];
                --count;
            }
            break;

        case 4:
            if (count != 0)
                stack[count - 1] = -stack[count - 1];
            break;
        }
    }

    return count == 0 ? 0 : stack[count - 1];
}

static int total_score(Node *node);

static int score_kind4(Item *item)
{
    uint32_t score = 0;
    uint32_t percentage = 100u - item->payload.kind4.discount;

    if (item->payload.kind4.node != NULL)
        score = (uint32_t)total_score(item->payload.kind4.node);

    return (int)(percentage * score / 100u);
}

static Item *create_item(Registry *registry, const char *name,
                         uint32_t type, uint64_t amount)
{
    Item *item = calloc(1, sizeof(*item));

    if (item == NULL)
        allocation_failed();

    item->id = registry->next_id++;
    strncpy(item->name, name, 19);
    item->field_18 = (uint16_t)(item->id % 3u + 1u);
    item->field_1a = (uint16_t)((item->id & 1u) + 1u);
    item->field_1c = 1;
    item->amount = amount;
    item->rate = 100;
    memcpy(item->currency, "USD", 4);
    item->type = type;
    item->link.next = &item->link;
    item->link.prev = &item->link;
    ++registry->item_count;

    switch (type) {
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
        item->score = score_kind0;
        break;
    }

    return item;
}

static void attach_item(Node *node, Item *item)
{
    Link *tail = node->items.prev;
    unsigned row;
    unsigned column;

    item->link.next = &node->items;
    item->link.prev = tail;
    tail->next = &item->link;
    node->items.prev = &item->link;
    ++node->item_count;

    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 6; ++column) {
            if (node->slots[row][column] == 0) {
                node->slots[row][column] = (uint8_t)item->id;
                return;
            }
        }
    }
}

static void detach_item(Node *node, Item *item)
{
    unsigned row;
    unsigned column;
    uint8_t id = (uint8_t)item->id;

    item->link.prev->next = item->link.next;
    item->link.next->prev = item->link.prev;
    item->link.next = &item->link;
    item->link.prev = &item->link;
    --node->item_count;

    for (row = 0; row < 4; ++row)
        for (column = 0; column < 6; ++column)
            if (node->slots[row][column] == id)
                node->slots[row][column] = 0;
}

static int total_score(Node *node)
{
    int result = 0;
    Link *link;
    Node *child;

    for (link = node->items.next; link != &node->items; link = link->next) {
        Item *item = (Item *)((char *)link - offsetof(Item, link));

        if (item->score != NULL)
            result += item->score(item);
    }

    for (child = node->first_child; child != NULL; child = child->next_sibling)
        result += total_score(child);

    return result;
}

static TreeNode *insert_tree(TreeNode *root, Item *item, uint32_t depth)
{
    if (root == NULL) {
        root = calloc(1, sizeof(*root));
        if (root == NULL)
            allocation_failed();

        root->key = item->id;
        root->item = item;
        root->depth = depth + 1;
        return root;
    }

    if (item->id < root->key)
        root->left = insert_tree(root->left, item, depth + 1);
    else
        root->right = insert_tree(root->right, item, depth + 1);

    return root;
}

static TreeNode *find_tree(TreeNode *root, uint32_t key)
{
    while (root != NULL) {
        if (root->key == key)
            return root;

        root = key < root->key ? root->left : root->right;
    }

    return NULL;
}

static void print_tree(TreeNode *root)
{
    while (root != NULL) {
        int score = 0;

        print_tree(root->left);

        if (root->item->score != NULL)
            score = root->item->score(root->item);

        printf("%u %u %s %d: %d\n",
               root->key,
               root->depth,
               root->item->name,
               (int)root->item->type,
               score);

        root = root->right;
    }
}

static void print_item(Item *item, int level)
{
    double ratio = (double)(int64_t)item->amount / (double)item->rate;
    int score = 0;

    printf("%d %u %s %u %u %u %.2f %u",
           level,
           item->id,
           item->name,
           item->field_18,
           item->field_1a,
           item->field_1c,
           ratio,
           item->type);

    switch (item->type) {
    case 0:
        printf("%u %u %u %s",
               item->payload.kind0.first,
               item->payload.kind0.second,
               item->payload.kind0.multiplier,
               item->payload.kind0.label);
        break;

    case 1:
        printf("%d %d %u %u",
               item->payload.kind1.first,
               item->payload.kind1.second,
               item->payload.kind1.scale,
               item->payload.kind1.multiplier);
        break;

    case 2:
        printf("%u%c%c",
               item->payload.kind2.seed,
               item->payload.kind2.bytes[0],
               item->payload.kind2.bytes[1]);
        break;

    case 3:
        if (item->payload.kind3.code != NULL) {
            printf("%u %u %u",
                   item->payload.kind3.code[0],
                   item->payload.kind3.code[1],
                   item->payload.kind3.evaluations);
        } else {
            printf("%u %u %u", 0u, 0u,
                   item->payload.kind3.evaluations);
        }
        break;

    case 4:
        printf("%s %u",
               item->payload.kind4.node != NULL
                   ? item->payload.kind4.node->name
                   : "",
               item->payload.kind4.discount);
        break;
    }

    if (item->score != NULL)
        score = item->score(item);

    printf(": %d\n", score);
}

static void print_hierarchy(Node *node, int level)
{
    unsigned row;
    unsigned column;
    Link *link;
    Node *child;
    int score = total_score(node);

    printf("%d %s %u %u %u %u %d\n",
           level,
           node->name,
           node->field_10,
           node->field_12,
           node->field_14,
           node->item_count,
           score);

    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 6; ++column)
            printf("%02x ", node->slots[row][column]);
        putchar('\n');
    }

    for (link = node->items.next; link != &node->items; link = link->next) {
        Item *item = (Item *)((char *)link - offsetof(Item, link));
        print_item(item, level + 1);
    }

    for (child = node->first_child; child != NULL; child = child->next_sibling)
        print_hierarchy(child, level + 1);
}

static void free_tree(TreeNode *root)
{
    if (root == NULL)
        return;

    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

static void free_hierarchy(Node *node)
{
    Link *link;
    Node *child;

    if (node == NULL)
        return;

    link = node->items.next;
    while (link != &node->items) {
        Link *next = link->next;
        Item *item = (Item *)((char *)link - offsetof(Item, link));

        if (item->type == 3)
            free(item->payload.kind3.code);
        free(item);
        link = next;
    }

    child = node->first_child;
    while (child != NULL) {
        Node *next = child->next_sibling;
        free_hierarchy(child);
        child = next;
    }

    free(node);
}

int main(void)
{
    Registry registry = {0};
    Node *node0;
    Node *node1;
    Node *node2;
    Node *node3;
    Item *item1;
    Item *item2;
    Item *item3;
    Item *item4;
    Item *item5;
    TreeNode *found;
    uint16_t *program;
    int value;

    registry.next_id = 1;

    node0 = create_node("node0", 16, 16, 16);
    node1 = create_node("node0001", 6, 4, 2);
    node2 = create_node("node2", 8, 6, 4);
    node3 = create_node("node3", 2, 2, 1);
    registry.root = node0;

    append_child(node0, node1);
    append_child(node0, node2);
    append_child(node1, node3);

    item1 = create_item(&registry, "item000001", 0, 12500);
    item1->payload.kind0.first = 12;
    item1->payload.kind0.second = 28;
    item1->payload.kind0.multiplier = 7;
    memcpy(item1->payload.kind0.label, "steel", 6);
    attach_item(node1, item1);
    registry.index = insert_tree(registry.index, item1, 0);

    item2 = create_item(&registry, "item02", 1, 3200);
    item2->payload.kind1.first = 120;
    item2->payload.kind1.second = 45;
    item2->payload.kind1.scale = 300;
    item2->payload.kind1.multiplier = 3;
    attach_item(node1, item2);
    registry.index = insert_tree(registry.index, item2, 0);

    item3 = create_item(&registry, "item00003", 2, 500);
    item3->payload.kind2.seed = 4711;
    item3->payload.kind2.bytes[0] = 90;
    item3->payload.kind2.bytes[1] = 93;
    item3->payload.kind2.bytes[2] = 96;
    item3->payload.kind2.bytes[3] = 99;
    item3->payload.kind2.bytes[4] = 102;
    item3->payload.kind2.bytes[5] = 105;
    item3->payload.kind2.bytes[6] = 108;
    item3->payload.kind2.bytes[7] = 111;
    attach_item(node3, item3);
    registry.index = insert_tree(registry.index, item3, 0);

    item4 = create_item(&registry, "item0000004", 3, 8800);
    program = malloc(12);
    if (program == NULL)
        allocation_failed();

    program[0] = 8;
    program[1] = 8;
    ((uint8_t *)program)[4] = 0x07;
    ((uint8_t *)program)[5] = 0x03;
    ((uint8_t *)program)[6] = 0x10;
    ((uint8_t *)program)[7] = 0x05;
    ((uint8_t *)program)[8] = 0x20;
    ((uint8_t *)program)[9] = 0x02;
    ((uint8_t *)program)[10] = 0x30;
    ((uint8_t *)program)[11] = 0x40;
    item4->payload.kind3.code = program;

    attach_item(node2, item4);
    registry.index = insert_tree(registry.index, item4, 0);

    value = item4->score(item4);
    printf("%d: %d\n", value, item4->payload.kind3.evaluations);
    value = item4->score(item4);
    printf("%d: %d\n", value, item4->payload.kind3.evaluations);

    item5 = create_item(&registry, "item0000005", 4, 15000);
    item5->payload.kind4.node = node1;
    item5->payload.kind4.discount = 20;
    attach_item(node2, item5);
    registry.index = insert_tree(registry.index, item5, 0);

    print_hierarchy(node0, 0);

    detach_item(node1, item1);
    attach_item(node3, item1);

    print_hierarchy(node0, 0);
    print_tree(registry.index);

    found = find_tree(registry.index, 3);
    if (found != NULL && found->item != NULL)
        printf("%s %u\n", found->item->name, found->item->type);
    else
        printf("%s %u\n", "", UINT32_MAX);

    value = total_score(node0);
    printf("%d %u %u\n", value, registry.item_count, registry.next_id);

    free_tree(registry.index);
    free_hierarchy(node0);
    return 0;
}