#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t key;
    char name[20];
    char code[8];
    double value1;
    double value2;
} Record;

typedef struct Node Node;

struct Node {
    uint32_t key;
    uint32_t padding;
    Record record;
    signed char height;
    unsigned char reserved[7];
    Node *child[2];
};

typedef struct {
    Node *root;
    unsigned long size;
    int rotations[4];
} Tree;

_Static_assert(sizeof(Record) == 48, "unexpected Record layout");
_Static_assert(offsetof(Node, record) == 8, "unexpected Node layout");
_Static_assert(offsetof(Node, height) == 56, "unexpected Node layout");
_Static_assert(offsetof(Node, child) == 64, "unexpected Node layout");
_Static_assert(sizeof(Node) == 80, "unexpected Node size");
_Static_assert(sizeof(Tree) == 32, "unexpected Tree size");

/*
 * The supplied disassembly identifies eleven 48-byte records, but does not
 * expose their bytes. Zero-filled records conservatively represent the
 * unavailable contents without introducing unsupported names or values.
 */
static const Record records[11] = {
    {0}, {0}, {0}, {0}, {0}, {0},
    {0}, {0}, {0}, {0}, {0}
};

static signed char node_height(const Node *node)
{
    return node != NULL ? node->height : 0;
}

static signed char maximum_height(signed char a, signed char b)
{
    return a > b ? a : b;
}

static void update_height(Node *node)
{
    node->height = (signed char)
        (maximum_height(node_height(node->child[0]),
                        node_height(node->child[1])) + 1);
}

static int balance_factor(const Node *node)
{
    return (int)node_height(node->child[0])
         - (int)node_height(node->child[1]);
}

static Node *rotate_node(Node *node, int direction)
{
    Node *replacement = node->child[1 - direction];

    node->child[1 - direction] = replacement->child[direction];
    replacement->child[direction] = node;

    update_height(node);
    update_height(replacement);
    return replacement;
}

static Node *rebalance(Tree *tree, Node *node)
{
    int balance;

    update_height(node);
    balance = balance_factor(node);

    if (balance > 1) {
        if (balance_factor(node->child[0]) < 0) {
            ++tree->rotations[2];
            node->child[0] = rotate_node(node->child[0], 1);
        }

        ++tree->rotations[0];
        return rotate_node(node, 0);
    }

    if (balance < -1) {
        if (balance_factor(node->child[1]) > 0) {
            ++tree->rotations[3];
            node->child[1] = rotate_node(node->child[1], 0);
        }

        ++tree->rotations[1];
        return rotate_node(node, 1);
    }

    return node;
}

static Node *new_node(uint32_t key, const Record *record)
{
    Node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    node->key = key;
    node->record = *record;
    node->height = 1;
    return node;
}

static Node *insert_node(Tree *tree, Node *node, uint32_t key,
                         const Record *record)
{
    int direction;

    if (node == NULL) {
        ++tree->size;
        return new_node(key, record);
    }

    if (key == node->key) {
        node->record = *record;
        return node;
    }

    direction = key > node->key;
    node->child[direction] =
        insert_node(tree, node->child[direction], key, record);

    return rebalance(tree, node);
}

static void insert_record(Tree *tree, const Record *record)
{
    tree->root = insert_node(tree, tree->root, record->key, record);
}

static Node *leftmost_node(Node *node)
{
    while (node->child[0] != NULL)
        node = node->child[0];

    return node;
}

static Node *delete_node(Tree *tree, Node *node, uint32_t key)
{
    int direction;

    if (node == NULL)
        return NULL;

    if (key != node->key) {
        direction = key > node->key;
        node->child[direction] =
            delete_node(tree, node->child[direction], key);
        return rebalance(tree, node);
    }

    if (node->child[0] == NULL || node->child[1] == NULL) {
        Node *replacement =
            node->child[0] != NULL ? node->child[0] : node->child[1];

        free(node);
        --tree->size;
        return replacement;
    }

    {
        Node *successor = leftmost_node(node->child[1]);

        node->key = successor->key;
        node->record = successor->record;
        node->child[1] =
            delete_node(tree, node->child[1], successor->key);
    }

    return rebalance(tree, node);
}

static void delete_key(Tree *tree, uint32_t key)
{
    tree->root = delete_node(tree, tree->root, key);
}

static Node *lower_bound_node(Node *node, uint32_t key)
{
    Node *result = NULL;

    while (node != NULL) {
        if (key <= node->key) {
            result = node;
            node = node->child[0];
        } else {
            node = node->child[1];
        }
    }

    return result;
}

static void print_tree(const Node *node, int depth)
{
    if (node == NULL)
        return;

    print_tree(node->child[0], depth + 1);

    printf("%d %s %s %u %d %d %.2f %.2f\n",
           depth,
           node->record.name,
           node->record.code,
           node->key,
           (int)node->height,
           balance_factor(node),
           node->record.value1,
           node->record.value2);

    print_tree(node->child[1], depth + 1);
}

static int is_balanced(const Node *node)
{
    int balance;

    if (node == NULL)
        return 1;

    balance = balance_factor(node);
    if (balance < -1 || balance > 1)
        return 0;

    return is_balanced(node->child[0]) &&
           is_balanced(node->child[1]);
}

static void destroy_tree(Node *node)
{
    if (node != NULL) {
        destroy_tree(node->child[0]);
        destroy_tree(node->child[1]);
        free(node);
    }
}

int main(void)
{
    Tree tree;
    Node *found;
    unsigned int i;

    memset(&tree, 0, sizeof(tree));

    for (i = 0; i <= 10; ++i)
        insert_record(&tree, &records[i]);

    print_tree(tree.root, 0);

    printf("%lu %s %d %d\n",
           tree.size,
           tree.root->record.name,
           (int)tree.root->height,
           is_balanced(tree.root));

    printf("%d %d %d %d\n",
           tree.rotations[0],
           tree.rotations[1],
           tree.rotations[2],
           tree.rotations[3]);

    found = lower_bound_node(tree.root, UINT32_C(6000000));
    printf("%s %u\n",
           found != NULL ? found->record.name : "",
           found != NULL ? found->key : 0U);

    delete_key(&tree, UINT32_C(8982000));
    delete_key(&tree, UINT32_C(2161000));

    printf("%lu %s %d\n",
           tree.size,
           tree.root->record.name,
           is_balanced(tree.root));

    print_tree(tree.root, 0);
    destroy_tree(tree.root);
    return 0;
}