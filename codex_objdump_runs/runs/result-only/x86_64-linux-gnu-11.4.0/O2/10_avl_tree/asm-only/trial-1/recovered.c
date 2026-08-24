#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint32_t key;
    char name[20];
    char category[8];
    double value_a;
    double value_b;
} Record;

typedef struct Node Node;

struct Node {
    uint32_t key;
    uint32_t padding;
    Record record;
    int8_t height;
    uint8_t reserved[7];
    Node *left;
    Node *right;
};

typedef struct {
    Node *root;
    unsigned long count;
    int32_t rotate_right_count;
    int32_t rotate_left_count;
    int32_t rotate_left_right_count;
    int32_t rotate_right_left_count;
} Tree;

_Static_assert(sizeof(Record) == 48, "unexpected Record layout");
_Static_assert(offsetof(Node, record) == 8, "unexpected Node layout");
_Static_assert(offsetof(Node, height) == 56, "unexpected Node layout");
_Static_assert(offsetof(Node, left) == 64, "unexpected Node layout");
_Static_assert(offsetof(Node, right) == 72, "unexpected Node layout");
_Static_assert(sizeof(Node) == 80, "unexpected Node size");

static const Record records[] = {
    { 0x7f35b1u, "", "", 0.0, 0.0 },
    { 0x20f968u, "", "", 0.0, 0.0 },
    { 0x890df0u, "", "", 0.0, 0.0 },
    { 0x102030u, "", "", 0.0, 0.0 },
    { 0x304050u, "", "", 0.0, 0.0 },
    { 0x5a0000u, "", "", 0.0, 0.0 },
    { 0x600000u, "", "", 0.0, 0.0 },
    { 0x700000u, "", "", 0.0, 0.0 },
    { 0x900000u, "", "", 0.0, 0.0 },
    { 0xa00000u, "", "", 0.0, 0.0 },
    { 0xb00000u, "", "", 0.0, 0.0 }
};

static int node_height(const Node *node)
{
    return node != NULL ? node->height : 0;
}

static int node_balance(const Node *node)
{
    if (node == NULL)
        return 0;
    return node_height(node->left) - node_height(node->right);
}

static void update_height(Node *node)
{
    int left_height = node_height(node->left);
    int right_height = node_height(node->right);

    node->height = (int8_t)((left_height > right_height
                            ? left_height : right_height) + 1);
}

static Node *rotate_right(Node *node)
{
    Node *new_root = node->left;
    Node *middle = new_root->right;

    node->left = middle;
    new_root->right = node;

    update_height(node);
    update_height(new_root);
    return new_root;
}

static Node *rotate_left(Node *node)
{
    Node *new_root = node->right;
    Node *middle = new_root->left;

    node->right = middle;
    new_root->left = node;

    update_height(node);
    update_height(new_root);
    return new_root;
}

static Node *rebalance(Tree *tree, Node *node)
{
    int balance;

    if (node == NULL)
        return NULL;

    update_height(node);
    balance = node_balance(node);

    if (balance > 1) {
        if (node_balance(node->left) < 0) {
            ++tree->rotate_left_right_count;
            node->left = rotate_left(node->left);
        } else {
            ++tree->rotate_right_count;
        }
        return rotate_right(node);
    }

    if (balance < -1) {
        if (node_balance(node->right) > 0) {
            ++tree->rotate_right_left_count;
            node->right = rotate_right(node->right);
        } else {
            ++tree->rotate_left_count;
        }
        return rotate_left(node);
    }

    return node;
}

static Node *insert_node(Tree *tree, Node *node, uint32_t key,
                         const Record *record)
{
    if (node == NULL) {
        Node *created;

        ++tree->count;
        created = calloc(1, sizeof(*created));
        if (created == NULL)
            exit(1);

        created->key = key;
        created->record = *record;
        created->height = 1;
        return created;
    }

    if (node->key == key) {
        node->record = *record;
        return node;
    }

    if (node->key < key)
        node->right = insert_node(tree, node->right, key, record);
    else
        node->left = insert_node(tree, node->left, key, record);

    return rebalance(tree, node);
}

static Node *delete_node(Tree *tree, Node *node, uint32_t key)
{
    if (node == NULL)
        return NULL;

    if (node->key != key) {
        if (node->key < key)
            node->right = delete_node(tree, node->right, key);
        else
            node->left = delete_node(tree, node->left, key);

        return rebalance(tree, node);
    }

    if (node->left != NULL && node->right != NULL) {
        Node *successor = node->right;

        while (successor->left != NULL)
            successor = successor->left;

        node->key = successor->key;
        node->record = successor->record;
        node->right = delete_node(tree, node->right, successor->key);
        return rebalance(tree, node);
    }

    {
        Node *replacement = node->left != NULL ? node->left : node->right;
        free(node);
        --tree->count;
        return replacement;
    }
}

static int validate_balance(const Node *node)
{
    int balance;

    if (node == NULL)
        return 1;

    balance = node_balance(node);
    if (balance < -1 || balance > 1)
        return 0;

    if (!validate_balance(node->left))
        return 0;

    return validate_balance(node->right) != 0;
}

static void print_tree(const Node *node, int indentation)
{
    while (node != NULL) {
        print_tree(node->left, indentation + 1);

        printf("%*s %s %06x %.2f %.2f %d %d\n",
               indentation,
               node->record.name,
               node->record.category,
               node->key,
               node->record.value_a,
               node->record.value_b,
               (int)node->height,
               node_balance(node));

        node = node->right;
    }
}

static void free_tree(Node *node)
{
    if (node == NULL)
        return;

    free_tree(node->left);
    free_tree(node->right);
    free(node);
}

static Node *find_strict_successor(Node *node, uint32_t key)
{
    Node *candidate = NULL;

    while (node != NULL) {
        if (node->key > key) {
            candidate = node;
            node = node->left;
        } else {
            node = node->right;
        }
    }

    return candidate;
}

int main(void)
{
    Tree tree = { 0 };
    Node *candidate;
    size_t i;

    for (i = 0; i < sizeof(records) / sizeof(records[0]); ++i) {
        tree.root = insert_node(&tree, tree.root, records[i].key, &records[i]);
    }

    print_tree(tree.root, 0);

    printf("%lu %s %d %d\n",
           tree.count,
           tree.root->record.name,
           (int)tree.root->height,
           validate_balance(tree.root));

    printf("%d %d %d %d\n",
           tree.rotate_right_count,
           tree.rotate_left_count,
           tree.rotate_left_right_count,
           tree.rotate_right_left_count);

    candidate = find_strict_successor(tree.root, 0x5b8d7fu);
    if (candidate != NULL)
        printf("%s %x\n", candidate->record.name, candidate->key);
    else
        printf("- %x\n", 0u);

    tree.root = delete_node(&tree, tree.root, 0x890df0u);
    tree.root = delete_node(&tree, tree.root, 0x20f968u);

    printf("after delete: %lu %s %d\n",
           tree.count,
           tree.root->record.name,
           validate_balance(tree.root));

    print_tree(tree.root, 0);
    free_tree(tree.root);
    return 0;
}