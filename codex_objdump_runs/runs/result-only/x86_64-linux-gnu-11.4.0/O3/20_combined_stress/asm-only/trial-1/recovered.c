#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Item Item;
typedef struct Group Group;
typedef struct TreeNode TreeNode;

typedef struct Link {
    struct Link *next;
    struct Link *prev;
} Link;

typedef int32_t (*Evaluator)(Item *);

typedef union ItemData {
    struct {
        uint16_t first;
        uint16_t second;
        uint8_t third;
        char text[11];
    } kind0;

    struct {
        int16_t first;
        int16_t second;
        uint16_t third;
        uint8_t fourth;
        uint8_t unused[9];
    } kind1;

    struct {
        uint32_t seed;
        uint8_t values[12];
    } kind2;

    struct {
        void *program;
        uint32_t calls;
        uint32_t unused;
    } kind3;

    struct {
        Group *group;
        uint16_t discount;
        uint8_t unused[6];
    } kind4;

    uint8_t bytes[16];
} ItemData;

struct Item {
    uint32_t id;
    char name[20];
    uint16_t class_code;
    uint16_t variant;
    uint16_t flags;
    uint64_t amount;
    uint16_t unit;
    char currency[4];
    uint32_t kind;
    Link link;
    ItemData data;
    Evaluator evaluate;
};

struct Group {
    char name[16];
    uint16_t width;
    uint16_t height;
    uint16_t depth;
    Link items;
    uint32_t item_count;
    Group *parent;
    Group *first_child;
    Group *next_sibling;
    uint8_t slots[4][6];
};

struct TreeNode {
    uint32_t key;
    Item *item;
    TreeNode *left;
    TreeNode *right;
    uint32_t level;
};

typedef struct BuildContext {
    Group *root;
    TreeNode *tree;
    uint32_t next_id;
    uint32_t item_count;
} BuildContext;

typedef struct Program {
    uint16_t length;
    uint16_t capacity;
    uint8_t code[];
} Program;

static const uint8_t sequence_data[8] = {
    1, 2, 3, 4, 5, 6, 7, 8
};

static void allocation_failure(void)
{
    exit(1);
}

static Item *item_from_link(Link *link)
{
    return (Item *)((char *)link - offsetof(Item, link));
}

static Group *group_new(const char *name, uint16_t width,
                        uint16_t height, uint16_t depth)
{
    Group *group = calloc(1, sizeof(*group));

    if (group == NULL)
        allocation_failure();

    strncpy(group->name, name, sizeof(group->name) - 1);
    group->width = width;
    group->height = height;
    group->depth = depth;
    group->items.next = &group->items;
    group->items.prev = &group->items;
    return group;
}

static void group_add_child(Group *parent, Group *child)
{
    Group **position = &parent->first_child;

    while (*position != NULL)
        position = &(*position)->next_sibling;

    *position = child;
    child->parent = parent;
}

static int32_t evaluate_kind0(Item *item)
{
    uint32_t result;

    result = (uint32_t)item->data.kind0.first;
    result += item->data.kind0.second;
    result *= 5;
    result += (uint32_t)item->data.kind0.third * 3;
    return (int32_t)result;
}

static int32_t evaluate_kind1(Item *item)
{
    int32_t sum;
    uint32_t result;

    sum = (int32_t)item->data.kind1.first
        + (int32_t)item->data.kind1.second;

    if (sum < 0)
        sum = -sum;

    result = (uint32_t)sum * 2;
    result += item->data.kind1.third / 10U;
    result *= item->data.kind1.fourth;
    return (int32_t)result;
}

static int32_t evaluate_kind2(Item *item)
{
    uint32_t value = item->data.kind2.seed;
    unsigned int i;

    for (i = 0; i < 8; ++i)
        value = value * 3U + item->data.kind2.values[i];

    return (int32_t)(value % 5000U);
}

static int32_t evaluate_kind3(Item *item)
{
    Program *program = item->data.kind3.program;
    uint32_t stack[16];
    unsigned int stack_size = 0;
    unsigned int i;

    if (program == NULL)
        return 0;

    ++item->data.kind3.calls;

    for (i = 0; i < program->length; ++i) {
        uint8_t instruction = program->code[i];
        unsigned int operation;

        if (instruction > 0x4f)
            continue;

        operation = instruction >> 4;

        switch (operation) {
        case 0:
            if (stack_size < 16)
                stack[stack_size++] = instruction;
            break;

        case 1:
            if (stack_size > 1) {
                stack[stack_size - 2] += stack[stack_size - 1];
                --stack_size;
            }
            break;

        case 2:
            if (stack_size > 1) {
                stack[stack_size - 2] -= stack[stack_size - 1];
                --stack_size;
            }
            break;

        case 3:
            if (stack_size > 1) {
                stack[stack_size - 2] *= stack[stack_size - 1];
                --stack_size;
            }
            break;

        case 4:
            if (stack_size != 0)
                stack[stack_size - 1] = 0U - stack[stack_size - 1];
            break;
        }
    }

    if (stack_size == 0)
        return 0;

    return (int32_t)stack[stack_size - 1];
}

static uint32_t group_total_unsigned(Group *group);

static int32_t evaluate_kind4(Item *item)
{
    uint32_t total = 0;
    uint32_t factor;

    if (item->data.kind4.group != NULL)
        total = group_total_unsigned(item->data.kind4.group);

    factor = 100U - item->data.kind4.discount;
    return (int32_t)((factor * total) / 100U);
}

static Item *item_new(BuildContext *context, const char *name,
                      uint32_t kind, uint64_t amount)
{
    Item *item = calloc(1, sizeof(*item));
    uint32_t id;

    if (item == NULL)
        allocation_failure();

    id = context->next_id++;
    ++context->item_count;

    item->id = id;
    strncpy(item->name, name, sizeof(item->name) - 1);
    item->class_code = (uint16_t)(id % 3U + 1U);
    item->variant = (uint16_t)((id & 1U) + 1U);
    item->flags = 1;
    item->amount = amount;
    item->unit = 100;
    memcpy(item->currency, "USD", 4);
    item->kind = kind;
    item->link.next = &item->link;
    item->link.prev = &item->link;

    switch (kind) {
    case 1:
        item->evaluate = evaluate_kind1;
        break;
    case 2:
        item->evaluate = evaluate_kind2;
        break;
    case 3:
        item->evaluate = evaluate_kind3;
        break;
    case 4:
        item->evaluate = evaluate_kind4;
        break;
    default:
        item->evaluate = evaluate_kind0;
        break;
    }

    return item;
}

static void group_add_item(Group *group, Item *item)
{
    Link *tail = group->items.prev;
    unsigned int row;
    unsigned int column;

    item->link.next = &group->items;
    item->link.prev = tail;
    tail->next = &item->link;
    group->items.prev = &item->link;
    ++group->item_count;

    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 6; ++column) {
            if (group->slots[row][column] == 0) {
                group->slots[row][column] = (uint8_t)item->id;
                return;
            }
        }
    }
}

static void group_remove_item(Group *group, Item *item)
{
    unsigned int row;
    unsigned int column;
    uint8_t id = (uint8_t)item->id;

    item->link.prev->next = item->link.next;
    item->link.next->prev = item->link.prev;
    item->link.next = &item->link;
    item->link.prev = &item->link;

    --group->item_count;

    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 6; ++column) {
            if (group->slots[row][column] == id)
                group->slots[row][column] = 0;
        }
    }
}

static TreeNode *tree_insert(TreeNode *root, Item *item, uint32_t level)
{
    if (root == NULL) {
        root = calloc(1, sizeof(*root));
        if (root == NULL)
            allocation_failure();

        root->key = item->id;
        root->item = item;
        root->level = level;
        return root;
    }

    if (item->id < root->key)
        root->left = tree_insert(root->left, item, level + 1);
    else
        root->right = tree_insert(root->right, item, level + 1);

    return root;
}

static TreeNode *tree_find(TreeNode *root, uint32_t key)
{
    while (root != NULL) {
        if (root->key == key)
            return root;

        if (key < root->key)
            root = root->left;
        else
            root = root->right;
    }

    return NULL;
}

static void tree_print(TreeNode *root)
{
    if (root == NULL)
        return;

    tree_print(root->left);

    printf("%u %u %s %u %d\n",
           root->key,
           root->level,
           root->item->name,
           root->item->kind,
           root->item->evaluate != NULL
               ? root->item->evaluate(root->item)
               : 0);

    tree_print(root->right);
}

static void tree_destroy(TreeNode *root)
{
    if (root == NULL)
        return;

    tree_destroy(root->left);
    tree_destroy(root->right);
    free(root);
}

static uint32_t group_total_unsigned(Group *group)
{
    uint32_t total = 0;
    Link *link;
    Group *child;

    for (link = group->items.next;
         link != &group->items;
         link = link->next) {
        Item *item = item_from_link(link);

        if (item->evaluate != NULL)
            total += (uint32_t)item->evaluate(item);
    }

    for (child = group->first_child;
         child != NULL;
         child = child->next_sibling) {
        total += group_total_unsigned(child);
    }

    return total;
}

static int32_t group_total(Group *group)
{
    return (int32_t)group_total_unsigned(group);
}

static void print_item_details(Item *item)
{
    switch (item->kind) {
    case 0:
        printf("%u %u %u %s\n",
               item->data.kind0.first,
               item->data.kind0.second,
               item->data.kind0.third,
               item->data.kind0.text);
        break;

    case 1:
        printf("%d %d %u %u\n",
               item->data.kind1.first,
               item->data.kind1.second,
               item->data.kind1.third,
               item->data.kind1.fourth);
        break;

    case 2:
        printf("%u %u %u\n",
               item->data.kind2.seed,
               item->data.kind2.values[0],
               item->data.kind2.values[1]);
        break;

    case 3: {
        Program *program = item->data.kind3.program;

        printf("%u %u %u\n",
               program != NULL ? program->length : 0,
               program != NULL ? program->capacity : 0,
               item->data.kind3.calls);
        break;
    }

    case 4:
        printf("%s %u\n",
               item->data.kind4.group != NULL
                   ? item->data.kind4.group->name
                   : "",
               item->data.kind4.discount);
        break;
    }
}

static void group_print(Group *group, unsigned int level)
{
    Link *link;
    Group *child;
    unsigned int row;
    unsigned int column;

    printf("%u %s %u %u %u %u %d\n",
           level,
           group->name,
           group->width,
           group->height,
           group->depth,
           group->item_count,
           group_total(group));

    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 6; ++column)
            printf("%02u ", group->slots[row][column]);
        putchar('\n');
    }

    for (link = group->items.next;
         link != &group->items;
         link = link->next) {
        Item *item = item_from_link(link);
        double ratio = (double)(int64_t)item->amount / item->unit;

        printf("%u %u %s %u %f %u %u %u\n",
               level + 1,
               item->id,
               item->name,
               item->class_code,
               ratio,
               item->variant,
               item->flags,
               item->kind);

        print_item_details(item);

        printf("%d\n",
               item->evaluate != NULL ? item->evaluate(item) : 0);
    }

    for (child = group->first_child;
         child != NULL;
         child = child->next_sibling) {
        group_print(child, level + 1);
    }
}

static void item_destroy(Item *item)
{
    if (item->kind == 3)
        free(item->data.kind3.program);
    free(item);
}

static void group_destroy(Group *group)
{
    Link *link;
    Group *child;

    if (group == NULL)
        return;

    link = group->items.next;
    while (link != &group->items) {
        Link *next = link->next;
        item_destroy(item_from_link(link));
        link = next;
    }

    child = group->first_child;
    while (child != NULL) {
        Group *next = child->next_sibling;
        group_destroy(child);
        child = next;
    }

    free(group);
}

int main(void)
{
    BuildContext context = {0};
    Group *root;
    Group *group1;
    Group *group2;
    Group *group3;
    Item *item1;
    Item *item2;
    Item *item3;
    Item *item4;
    Item *item5;
    Program *program;
    TreeNode *found;

    context.next_id = 1;

    root = group_new("group0", 16, 16, 16);
    group1 = group_new("group1", 6, 4, 2);
    group2 = group_new("group2", 8, 6, 4);
    group3 = group_new("group3", 2, 2, 1);

    context.root = root;

    group_add_child(root, group1);
    group_add_child(root, group2);
    group_add_child(group1, group3);

    item1 = item_new(&context, "Steel Beam", 0, 12500);
    item1->data.kind0.first = 12;
    item1->data.kind0.second = 28;
    item1->data.kind0.third = 7;
    memcpy(item1->data.kind0.text, "steel", 6);
    group_add_item(group1, item1);
    context.tree = tree_insert(context.tree, item1, 0);

    item2 = item_new(&context, "Widget", 1, 3200);
    item2->data.kind1.first = 120;
    item2->data.kind1.second = 45;
    item2->data.kind1.third = 300;
    item2->data.kind1.fourth = 3;
    group_add_item(group1, item2);
    context.tree = tree_insert(context.tree, item2, 0);

    item3 = item_new(&context, "Hash Item", 2, 500);
    item3->data.kind2.seed = 4711;
    memcpy(item3->data.kind2.values,
           sequence_data, sizeof(sequence_data));
    group_add_item(group3, item3);
    context.tree = tree_insert(context.tree, item3, 0);

    item4 = item_new(&context, "Expression", 3, 8800);
    program = malloc(sizeof(*program) + 8);
    if (program == NULL)
        allocation_failure();

    program->length = 8;
    program->capacity = 8;
    program->code[0] = 0x07;
    program->code[1] = 0x03;
    program->code[2] = 0x10;
    program->code[3] = 0x05;
    program->code[4] = 0x20;
    program->code[5] = 0x02;
    program->code[6] = 0x30;
    program->code[7] = 0x40;
    item4->data.kind3.program = program;

    group_add_item(group2, item4);
    context.tree = tree_insert(context.tree, item4, 0);

    printf("%d %u\n", item4->evaluate(item4),
           item4->data.kind3.calls);
    printf("%d %u\n", item4->evaluate(item4),
           item4->data.kind3.calls);

    item5 = item_new(&context, "Bundle Item", 4, 15000);
    item5->data.kind4.group = group1;
    item5->data.kind4.discount = 20;
    group_add_item(group2, item5);
    context.tree = tree_insert(context.tree, item5, 0);

    group_print(root, 0);

    group_remove_item(group1, item1);
    group_add_item(group3, item1);

    group_print(root, 0);
    tree_print(context.tree);

    found = tree_find(context.tree, 3);
    if (found != NULL && found->item != NULL)
        printf("%s %u\n", found->item->name, found->item->kind);
    else
        printf(" %d\n", -1);

    printf("%d %u %u\n",
           group_total(root),
           context.next_id,
           context.item_count);

    tree_destroy(context.tree);
    group_destroy(root);
    return 0;
}