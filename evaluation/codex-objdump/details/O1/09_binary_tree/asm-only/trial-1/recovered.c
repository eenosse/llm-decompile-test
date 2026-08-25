#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node Node;

struct Node {
    char text[24];
    int32_t occurrences;
    int32_t first_position;
    Node *left;
    Node *right;
    Node *parent;
    int32_t subtree_nodes;
    int32_t padding;
};

typedef struct {
    Node *root;
    uint64_t distinct_nodes;
    uint64_t comparisons;
} Tree;

_Static_assert(sizeof(Node) == 64, "unexpected Node layout");
_Static_assert(offsetof(Node, occurrences) == 0x18, "unexpected layout");
_Static_assert(offsetof(Node, first_position) == 0x1c, "unexpected layout");
_Static_assert(offsetof(Node, left) == 0x20, "unexpected layout");
_Static_assert(offsetof(Node, right) == 0x28, "unexpected layout");
_Static_assert(offsetof(Node, parent) == 0x30, "unexpected layout");
_Static_assert(offsetof(Node, subtree_nodes) == 0x38, "unexpected layout");

static const char *const input_values[15] = {
    "u07", "u03", "u11", "u01", "u05",
    "u09", "u13", "u00", "u02", "u04",
    "u06", "u08", "u10", "u12", "u14"
};

static const char query_value[] = "u05";
static const char empty_text[] = "";

static Node *successor(Node *node)
{
    Node *cursor;

    if (node->right != NULL) {
        cursor = node->right;
        while (cursor->left != NULL)
            cursor = cursor->left;
        return cursor;
    }

    cursor = node->parent;
    while (cursor != NULL && cursor->right == node) {
        node = cursor;
        cursor = cursor->parent;
    }

    return cursor;
}

static int32_t tree_height(const Node *node)
{
    int32_t left_height;
    int32_t right_height;

    if (node == NULL)
        return 0;

    left_height = tree_height(node->left);
    right_height = tree_height(node->right);

    return (left_height >= right_height ? left_height : right_height) + 1;
}

static void print_detailed(const Node *node, int32_t depth)
{
    if (node == NULL)
        return;

    print_detailed(node->left, depth + 1);
    printf("%d %s %d %d %d\n",
           depth,
           node->text,
           node->occurrences,
           node->first_position,
           node->subtree_nodes);
    print_detailed(node->right, depth + 1);
}

static void print_in_order(const Tree *tree)
{
    const Node *stack[64];
    const Node *node = tree->root;
    int32_t count = 0;

    while (node != NULL || count != 0) {
        while (node != NULL) {
            if (count <= 63)
                stack[count++] = node;
            node = node->left;
        }

        if (count <= 0)
            break;

        node = stack[--count];
        printf("%s ", node->text);
        node = node->right;
    }

    putchar('\n');
}

static void print_breadth_first(const Tree *tree)
{
    const Node *queue[64];
    int32_t used = 0;
    int32_t index = 0;

    if (tree->root != NULL)
        queue[used++] = tree->root;

    while (index < used) {
        const Node *node = queue[index++];

        printf("%s:%d ", node->text, node->subtree_nodes);

        if (used <= 63 && node->left != NULL)
            queue[used++] = node->left;

        if (used <= 63 && node->right != NULL)
            queue[used++] = node->right;
    }

    putchar('\n');
}

static void destroy_tree(Node *node)
{
    if (node == NULL)
        return;

    destroy_tree(node->left);
    destroy_tree(node->right);
    free(node);
}

static void insert_value(Tree *tree, const char *text, int32_t position)
{
    Node *parent = NULL;
    Node *cursor = tree->root;
    Node **link = &tree->root;

    while (cursor != NULL) {
        int result;

        parent = cursor;
        ++tree->comparisons;
        result = strcmp(text, cursor->text);

        if (result == 0) {
            ++cursor->occurrences;
            return;
        }

        if (result < 0) {
            link = &cursor->left;
            cursor = cursor->left;
        } else {
            link = &cursor->right;
            cursor = cursor->right;
        }
    }

    cursor = calloc(1, sizeof(*cursor));
    if (cursor == NULL)
        exit(1);

    strncpy(cursor->text, text, 23);
    cursor->occurrences = 1;
    cursor->first_position = position;
    cursor->subtree_nodes = 1;
    cursor->parent = parent;
    *link = cursor;
    ++tree->distinct_nodes;

    while (parent != NULL) {
        ++parent->subtree_nodes;
        parent = parent->parent;
    }
}

static Node *find_value(Tree *tree, const char *text)
{
    Node *node = tree->root;

    while (node != NULL) {
        int result;

        ++tree->comparisons;
        result = strcmp(text, node->text);

        if (result == 0)
            return node;

        node = result < 0 ? node->left : node->right;
    }

    return NULL;
}

int main(void)
{
    Tree tree = { NULL, 0, 0 };
    Node *node;
    Node *minimum;

    for (int32_t i = 0; i < 15; ++i)
        insert_value(&tree, input_values[i], i);

    print_detailed(tree.root, 0);
    print_in_order(&tree);
    print_breadth_first(&tree);

    printf("%s %lu %lu %d\n",
           tree.root->text,
           (unsigned long)tree.distinct_nodes,
           (unsigned long)tree.comparisons,
           tree_height(tree.root));

    node = find_value(&tree, query_value);
    if (node != NULL) {
        Node *next = successor(node);

        printf("%d %s %s\n",
               node->occurrences,
               node->parent != NULL ? node->parent->text : empty_text,
               next != NULL ? next->text : empty_text);
    }

    minimum = tree.root;
    while (minimum != NULL && minimum->left != NULL)
        minimum = minimum->left;

    for (node = minimum; node != NULL; node = successor(node))
        printf("%s ", node->text);
    putchar('\n');

    destroy_tree(tree.root);
    return 0;
}