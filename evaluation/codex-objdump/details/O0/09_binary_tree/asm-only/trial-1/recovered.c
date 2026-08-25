#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node Node;

struct Node {
    char text[24];
    int count;
    int position;
    Node *left;
    Node *right;
    Node *parent;
    int subtree_size;
};

typedef struct {
    Node *root;
    size_t size;
    size_t comparisons;
} Tree;

typedef struct {
    Node *items[64];
    int top;
} NodeStack;

typedef struct {
    Node *items[64];
    int read_index;
    int write_index;
} NodeQueue;

_Static_assert(sizeof(Node) == 64, "unexpected Node layout");

static const char *words[15] = {
    "the", "quick", "brown", "fox", "jumps",
    "over", "the", "lazy", "dog", "the",
    "fox", "was", "quick", "and", "smart"
};

static void stack_push(NodeStack *stack, Node *node)
{
    if (stack->top <= 63)
        stack->items[stack->top++] = node;
}

static Node *stack_pop(NodeStack *stack)
{
    if (stack->top > 0)
        return stack->items[--stack->top];
    return NULL;
}

static int stack_empty(const NodeStack *stack)
{
    return stack->top == 0;
}

static void queue_push(NodeQueue *queue, Node *node)
{
    if (queue->write_index <= 63)
        queue->items[queue->write_index++] = node;
}

static Node *queue_pop(NodeQueue *queue)
{
    if (queue->read_index < queue->write_index)
        return queue->items[queue->read_index++];
    return NULL;
}

static int queue_empty(const NodeQueue *queue)
{
    return queue->read_index >= queue->write_index;
}

static Node *new_node(const char *text, int position)
{
    Node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    strncpy(node->text, text, sizeof(node->text) - 1);
    node->count = 1;
    node->position = position;
    node->subtree_size = 1;
    return node;
}

static void tree_insert(Tree *tree, const char *text, int position)
{
    Node **link = &tree->root;
    Node *parent = NULL;

    while (*link != NULL) {
        int comparison;

        parent = *link;
        tree->comparisons++;
        comparison = strcmp(text, parent->text);

        if (comparison == 0) {
            parent->count++;
            return;
        }

        link = comparison < 0 ? &parent->left : &parent->right;
    }

    Node *node = new_node(text, position);
    node->parent = parent;
    *link = node;
    tree->size++;

    for (Node *ancestor = parent; ancestor != NULL;
         ancestor = ancestor->parent)
        ancestor->subtree_size++;
}

static Node *tree_find(Tree *tree, const char *text)
{
    Node *node = tree->root;

    while (node != NULL) {
        int comparison = strcmp(text, node->text);

        tree->comparisons++;

        if (comparison == 0)
            return node;

        node = comparison < 0 ? node->left : node->right;
    }

    return NULL;
}

static Node *minimum_node(Node *node)
{
    while (node != NULL && node->left != NULL)
        node = node->left;
    return node;
}

static Node *successor_node(Node *node)
{
    if (node->right != NULL)
        return minimum_node(node->right);

    Node *parent = node->parent;

    while (parent != NULL && node == parent->right) {
        node = parent;
        parent = parent->parent;
    }

    return parent;
}

static int tree_height(Node *node)
{
    if (node == NULL)
        return 0;

    int left_height = tree_height(node->left);
    int right_height = tree_height(node->right);

    return (left_height > right_height ? left_height : right_height) + 1;
}

static void print_tree(Node *node, int depth)
{
    if (node == NULL)
        return;

    print_tree(node->left, depth + 1);
    printf("%*s %d %d %d\n", depth, node->text, node->count,
           node->position, node->subtree_size);
    print_tree(node->right, depth + 1);
}

static void print_inorder_iterative(Tree *tree)
{
    NodeStack stack;
    Node *node = tree->root;

    stack.top = 0;

    while (node != NULL || !stack_empty(&stack)) {
        while (node != NULL) {
            stack_push(&stack, node);
            node = node->left;
        }

        node = stack_pop(&stack);
        printf("%s ", node->text);
        node = node->right;
    }

    putchar('\n');
}

static void print_level_order(Tree *tree)
{
    NodeQueue queue;
    Node *node;

    queue.read_index = 0;
    queue.write_index = 0;

    if (tree->root != NULL)
        queue_push(&queue, tree->root);

    while (!queue_empty(&queue)) {
        node = queue_pop(&queue);
        printf("%s(%d) ", node->text, node->subtree_size);

        if (node->left != NULL)
            queue_push(&queue, node->left);
        if (node->right != NULL)
            queue_push(&queue, node->right);
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

int main(void)
{
    Tree tree;
    unsigned int i;
    Node *node;

    memset(&tree, 0, sizeof(tree));

    for (i = 0; i <= 14; ++i)
        tree_insert(&tree, words[i], (int)i);

    print_tree(tree.root, 0);
    print_inorder_iterative(&tree);
    print_level_order(&tree);

    printf("%s %zu %d %zu\n", tree.root->text, tree.size,
           tree_height(tree.root), tree.comparisons);

    node = tree_find(&tree, "fox");
    if (node != NULL) {
        Node *next = successor_node(node);

        printf("%d %s %s\n", node->count,
               node->parent != NULL ? node->parent->text : "-",
               next != NULL ? next->text : "-");
    }

    for (node = minimum_node(tree.root); node != NULL;
         node = successor_node(node))
        printf("%s ", node->text);

    putchar('\n');
    destroy_tree(tree.root);
    return 0;
}