#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint32_t key;
    char name[20];
    char tag[8];
    double value_a;
    double value_b;
} Record;

typedef struct Node Node;

struct Node {
    uint32_t key;
    uint32_t reserved;
    Record record;
    int8_t height;
    uint8_t padding[7];
    Node *left;
    Node *right;
};

typedef struct {
    Node *root;
    size_t count;
    uint32_t rotate_ll;
    uint32_t rotate_rr;
    uint32_t rotate_lr;
    uint32_t rotate_rl;
} Tree;

_Static_assert(sizeof(Record) == 48, "unexpected Record layout");
_Static_assert(offsetof(Node, record) == 8, "unexpected Node layout");
_Static_assert(offsetof(Node, height) == 56, "unexpected Node layout");
_Static_assert(offsetof(Node, left) == 64, "unexpected Node layout");
_Static_assert(offsetof(Node, right) == 72, "unexpected Node layout");
_Static_assert(sizeof(Node) == 80, "unexpected Node size");

/*
 * The evidence identifies an aligned array of eleven 48-byte records, but
 * does not contain its bytes. Zero initialization is the neutral completion.
 */
_Alignas(16) static const Record initial_records[11] = {{0}};

static int node_height(const Node *node)
{
    return node != NULL ? node->height : 0;
}

static int balance_factor(const Node *node)
{
    return node_height(node->left) - node_height(node->right);
}

static void update_height(Node *node)
{
    int left = node_height(node->left);
    int right = node_height(node->right);
    node->height = (int8_t)((left > right ? left : right) + 1);
}

static Node *rotate_right(Node *root)
{
    Node *replacement = root->left;
    Node *middle = replacement->right;

    replacement->right = root;
    root->left = middle;

    update_height(root);
    update_height(replacement);
    return replacement;
}

static Node *rotate_left(Node *root)
{
    Node *replacement = root->right;
    Node *middle = replacement->left;

    replacement->left = root;
    root->right = middle;

    update_height(root);
    update_height(replacement);
    return replacement;
}

static Node *rebalance(Tree *tree, Node *node)
{
    int balance;

    update_height(node);
    balance = balance_factor(node);

    if (balance > 1) {
        if (balance_factor(node->left) >= 0) {
            ++tree->rotate_ll;
            return rotate_right(node);
        }

        ++tree->rotate_lr;
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }

    if (balance < -1) {
        if (balance_factor(node->right) <= 0) {
            ++tree->rotate_rr;
            return rotate_left(node);
        }

        ++tree->rotate_rl;
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }

    return node;
}

static Node *new_node(Tree *tree, uint32_t key, const Record *record)
{
    Node *node;

    ++tree->count;
    node = calloc(1, sizeof(*node));
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
    if (node == NULL)
        return new_node(tree, key, record);

    if (key < node->key)
        node->left = insert_node(tree, node->left, key, record);
    else if (key > node->key)
        node->right = insert_node(tree, node->right, key, record);
    else {
        node->record = *record;
        return node;
    }

    return rebalance(tree, node);
}

static Node *minimum_node(Node *node)
{
    while (node->left != NULL)
        node = node->left;
    return node;
}

static Node *delete_node(Tree *tree, Node *node, uint32_t key)
{
    if (node == NULL)
        return NULL;

    if (key < node->key) {
        node->left = delete_node(tree, node->left, key);
    } else if (key > node->key) {
        node->right = delete_node(tree, node->right, key);
    } else {
        if (node->left == NULL || node->right == NULL) {
            Node *replacement =
                node->left != NULL ? node->left : node->right;

            free(node);
            --tree->count;
            return replacement;
        } else {
            Node *successor = minimum_node(node->right);

            node->key = successor->key;
            node->record = successor->record;
            node->right =
                delete_node(tree, node->right, successor->key);
        }
    }

    return rebalance(tree, node);
}

static void print_nodes(const Node *node, int depth)
{
    if (node == NULL)
        return;

    print_nodes(node->left, depth + 1);
    printf("%d %s %s %u %.2f %.2f %d %d\n",
           depth,
           node->record.name,
           node->record.tag,
           node->key,
           node->record.value_a,
           node->record.value_b,
           (int)node->height,
           balance_factor(node));
    print_nodes(node->right, depth + 1);
}

static int is_balanced(const Node *node)
{
    int balance;

    if (node == NULL)
        return 1;

    balance = balance_factor(node);
    if (balance < -1 || balance > 1)
        return 0;

    return is_balanced(node->left) && is_balanced(node->right);
}

static Node *predecessor(Node *root, uint32_t key)
{
    Node *candidate = NULL;

    while (root != NULL) {
        if (key > root->key) {
            candidate = root;
            root = root->right;
        } else {
            root = root->left;
        }
    }

    return candidate;
}

static void free_nodes(Node *node)
{
    if (node == NULL)
        return;

    free_nodes(node->left);
    free_nodes(node->right);
    free(node);
}

int main(void)
{
    Tree tree = {0};
    Node *found;
    size_t i;

    for (i = 0; i < sizeof(initial_records) / sizeof(initial_records[0]); ++i) {
        tree.root = insert_node(&tree, tree.root,
                                initial_records[i].key,
                                &initial_records[i]);
    }

    print_nodes(tree.root, 0);

    printf("%zu %s %d %d\n",
           tree.count,
           tree.root->record.name,
           (int)tree.root->height,
           is_balanced(tree.root));

    printf("%u %u %u %u\n",
           tree.rotate_ll,
           tree.rotate_rr,
           tree.rotate_lr,
           tree.rotate_rl);

    found = predecessor(tree.root, UINT32_C(0x5b8d7f));
    if (found != NULL)
        printf("%s %u\n", found->record.name, found->key);
    else
        printf("- %u\n", 0u);

    tree.root = delete_node(&tree, tree.root, UINT32_C(0x890df0));
    tree.root = delete_node(&tree, tree.root, UINT32_C(0x20f968));

    printf("%zu %s %d\n",
           tree.count,
           tree.root->record.name,
           is_balanced(tree.root));

    print_nodes(tree.root, 0);
    free_nodes(tree.root);
    return 0;
}