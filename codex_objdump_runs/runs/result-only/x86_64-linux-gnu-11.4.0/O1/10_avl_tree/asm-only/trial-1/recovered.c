#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint32_t key;
    char text_a[20];
    char text_b[8];
    double number_a;
    double number_b;
} Record;

typedef struct Node Node;

struct Node {
    uint32_t key;
    Record record;
    int8_t height;
    Node *child_a;
    Node *child_b;
};

typedef struct {
    Node *root;
    size_t count;
    int counter_a;
    int counter_b;
    int counter_c;
    int counter_d;
} Tree;

_Static_assert(sizeof(Record) == 48, "unexpected Record layout");
_Static_assert(offsetof(Node, record) == 8, "unexpected Node layout");
_Static_assert(offsetof(Node, height) == 56, "unexpected Node layout");
_Static_assert(offsetof(Node, child_a) == 64, "unexpected Node layout");
_Static_assert(offsetof(Node, child_b) == 72, "unexpected Node layout");
_Static_assert(sizeof(Node) == 80, "unexpected Node size");

static const Record records[11] = {
    {10230000u, "Cairo",       "EG",  30.0444,   31.2357},
    { 8336000u, "New York",    "US",  40.7128,  -74.0060},
    {12640000u, "Moscow",      "RU",  55.7558,   37.6173},
    { 5312000u, "Sydney",      "AU", -33.8688,  151.2093},
    { 9210000u, "Mexico City", "MX",  19.4326,  -99.1332},
    {12480000u, "Mumbai",      "IN",  19.0760,   72.8777},
    {13960000u, "Tokyo",       "JP",  35.6762,  139.6503},
    { 2161000u, "Paris",       "FR",  48.8566,    2.3522},
    { 8982000u, "London",      "GB",  51.5074,   -0.1278},
    {12330000u, "Sao Paulo",   "BR", -23.5505,  -46.6333},
    {21540000u, "Beijing",     "CN",  39.9042,  116.4074}
};

static int node_height(const Node *node)
{
    return node != NULL ? node->height : 0;
}

static int node_balance(const Node *node)
{
    return node_height(node->child_a) - node_height(node->child_b);
}

static void update_height(Node *node)
{
    int a = node_height(node->child_a);
    int b = node_height(node->child_b);

    node->height = (int8_t)((a < b ? b : a) + 1);
}

static Node *rotate_a(Node *node)
{
    Node *pivot = node->child_b;

    node->child_b = pivot->child_a;
    pivot->child_a = node;
    update_height(node);
    update_height(pivot);
    return pivot;
}

static Node *rotate_b(Node *node)
{
    Node *pivot = node->child_a;

    node->child_a = pivot->child_b;
    pivot->child_b = node;
    update_height(node);
    update_height(pivot);
    return pivot;
}

static Node *rebalance(Tree *tree, Node *node)
{
    int balance;

    update_height(node);
    balance = node_balance(node);

    if (balance > 1) {
        if (node_balance(node->child_a) < 0) {
            tree->counter_c++;
            node->child_a = rotate_b(node->child_a);
        }

        tree->counter_a++;
        return rotate_a(node);
    }

    if (balance < -1) {
        if (node_balance(node->child_b) > 0) {
            tree->counter_d++;
            node->child_b = rotate_a(node->child_b);
        }

        tree->counter_b++;
        return rotate_b(node);
    }

    return node;
}

static Node *insert_record(Tree *tree, Node *node, uint32_t key,
                           const Record *record)
{
    if (node == NULL) {
        node = calloc(1, sizeof(*node));
        tree->count++;

        if (node == NULL)
            exit(1);

        node->key = key;
        node->record = *record;
        node->height = 1;
        return node;
    }

    if (node->key == key) {
        node->record = *record;
        return node;
    }

    if (node->key < key)
        node->child_b = insert_record(tree, node->child_b, key, record);
    else
        node->child_a = insert_record(tree, node->child_a, key, record);

    return rebalance(tree, node);
}

static Node *delete_key(Tree *tree, Node *node, uint32_t key)
{
    Node *replacement;

    if (node == NULL)
        return NULL;

    if (node->key != key) {
        if (node->key < key)
            node->child_b = delete_key(tree, node->child_b, key);
        else
            node->child_a = delete_key(tree, node->child_a, key);

        return rebalance(tree, node);
    }

    replacement = node->child_a;
    if (replacement == NULL) {
        replacement = node->child_b;
        free(node);
        tree->count--;
        return replacement;
    }

    if (node->child_b == NULL) {
        free(node);
        tree->count--;
        return replacement;
    }

    replacement = node->child_b;
    while (replacement->child_a != NULL)
        replacement = replacement->child_a;

    node->key = replacement->key;
    node->record = replacement->record;
    node->child_b = delete_key(tree, node->child_b, replacement->key);

    return rebalance(tree, node);
}

static int validate_tree(const Node *node)
{
    int balance;

    if (node == NULL)
        return 1;

    balance = node_balance(node);
    if ((unsigned)(balance + 1) > 2u)
        return 0;

    return validate_tree(node->child_a) &&
           validate_tree(node->child_b);
}

static void print_tree(const Node *node, int depth)
{
    if (node == NULL)
        return;

    print_tree(node->child_a, depth + 1);

    printf("%*s %-7s %u %.4f %.4f %d %d\n",
           depth,
           node->record.text_a,
           node->record.text_b,
           node->key,
           node->record.number_a,
           node->record.number_b,
           (int)node->height,
           node_balance(node));

    print_tree(node->child_b, depth + 1);
}

static void free_tree(Node *node)
{
    if (node == NULL)
        return;

    free_tree(node->child_a);
    free_tree(node->child_b);
    free(node);
}

int main(void)
{
    Tree tree = {0};
    Node *node;
    Node *candidate = NULL;
    size_t i;

    for (i = 0; i < sizeof(records) / sizeof(records[0]); ++i)
        tree.root = insert_record(&tree, tree.root,
                                  records[i].key, &records[i]);

    print_tree(tree.root, 0);

    printf("%zu %s %d %d\n",
           tree.count,
           tree.root->record.text_a,
           (int)tree.root->height,
           validate_tree(tree.root));

    printf("%d %d %d %d\n",
           tree.counter_a,
           tree.counter_b,
           tree.counter_c,
           tree.counter_d);

    node = tree.root;
    while (node != NULL) {
        if (node->key <= 0x5b8d7fu) {
            node = node->child_b;
        } else {
            candidate = node;
            node = node->child_a;
        }
    }

    printf("%s %u\n",
           candidate != NULL ? candidate->record.text_a : "-",
           candidate != NULL ? candidate->key : 0u);

    tree.root = delete_key(&tree, tree.root, 0x890df0u);
    tree.root = delete_key(&tree, tree.root, 0x20f968u);

    printf("%zu %s %d\n",
           tree.count,
           tree.root->record.text_a,
           validate_tree(tree.root));

    print_tree(tree.root, 0);
    free_tree(tree.root);
    return 0;
}