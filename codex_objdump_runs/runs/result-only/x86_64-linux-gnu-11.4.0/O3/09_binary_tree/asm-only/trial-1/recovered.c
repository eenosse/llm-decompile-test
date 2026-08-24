#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node Node;

struct Node {
    char key[24];
    int count;
    int ordinal;
    Node *left;
    Node *right;
    Node *parent;
    int subtree_size;
};

_Static_assert(sizeof(int) == 4, "requires 32-bit int");
_Static_assert(offsetof(Node, count) == 24, "unexpected layout");
_Static_assert(offsetof(Node, ordinal) == 28, "unexpected layout");
_Static_assert(offsetof(Node, left) == 32, "unexpected layout");
_Static_assert(offsetof(Node, right) == 40, "unexpected layout");
_Static_assert(offsetof(Node, parent) == 48, "unexpected layout");
_Static_assert(offsetof(Node, subtree_size) == 56, "unexpected layout");
_Static_assert(sizeof(Node) == 64, "unexpected layout");

static const char *const input_keys[15] = {
    "n07", "n03", "n11", "n01", "n05",
    "n09", "n13", "n00", "n02", "n04",
    "n06", "n08", "n10", "n12", "n14"
};

static const char query_key[] = "n07";

static Node *new_node(const char *key, int ordinal, Node *parent)
{
    Node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    strncpy(node->key, key, 23);
    node->count = 1;
    node->ordinal = ordinal;
    node->parent = parent;
    node->subtree_size = 1;
    return node;
}

static void print_recursive(const Node *node, int depth)
{
    if (node == NULL)
        return;

    print_recursive(node->left, depth + 1);
    printf("%d %s %d %d %d\n",
           depth,
           node->key,
           node->count,
           node->ordinal,
           node->subtree_size);
    print_recursive(node->right, depth + 1);
}

static void print_inorder_stack(const Node *root)
{
    const Node *stack[64];
    size_t used = 0;
    const Node *node = root;

    while (node != NULL || used != 0) {
        while (node != NULL) {
            if (used == 64)
                break;
            stack[used++] = node;
            node = node->left;
        }

        if (used == 0)
            break;

        node = stack[--used];
        printf("%s ", node->key);
        node = node->right;
    }

    putc('\n', stdout);
}

static void print_breadth_first(const Node *root)
{
    const Node *queue[64];
    size_t head = 0;
    size_t tail = 0;

    if (root != NULL)
        queue[tail++] = root;

    while (head < tail) {
        const Node *node = queue[head++];

        printf("%s:%d ", node->key, node->subtree_size);

        if (node->left != NULL && tail < 64)
            queue[tail++] = node->left;
        if (node->right != NULL && tail < 64)
            queue[tail++] = node->right;
    }

    putc('\n', stdout);
}

static int tree_height(const Node *node)
{
    int left_height;
    int right_height;

    if (node == NULL)
        return 0;

    left_height = tree_height(node->left);
    right_height = tree_height(node->right);
    return (left_height < right_height ? right_height : left_height) + 1;
}

static void print_inorder_parent_links(const Node *root)
{
    const Node *node = root;

    if (node == NULL) {
        putc('\n', stdout);
        return;
    }

    while (node->left != NULL)
        node = node->left;

    while (node != NULL) {
        const Node *next;

        printf("%s ", node->key);

        if (node->right != NULL) {
            next = node->right;
            while (next->left != NULL)
                next = next->left;
        } else {
            next = node->parent;
            while (next != NULL && node == next->right) {
                node = next;
                next = next->parent;
            }
        }

        node = next;
    }

    putc('\n', stdout);
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
    Node *root = NULL;
    size_t node_count = 0;
    size_t comparisons = 0;
    int i;

    for (i = 0; i < 15; ++i) {
        const char *key = input_keys[i];

        if (root == NULL) {
            root = new_node(key, i, NULL);
            ++node_count;
        } else {
            Node *node = root;

            for (;;) {
                Node **link;
                int order;

                ++comparisons;
                order = strcmp(key, node->key);

                if (order == 0) {
                    ++node->count;
                    break;
                }

                link = order < 0 ? &node->left : &node->right;

                if (*link != NULL) {
                    node = *link;
                } else {
                    Node *ancestor;

                    *link = new_node(key, i, node);
                    ++node_count;

                    for (ancestor = node;
                         ancestor != NULL;
                         ancestor = ancestor->parent)
                        ++ancestor->subtree_size;
                    break;
                }
            }
        }
    }

    print_recursive(root, 0);
    print_inorder_stack(root);
    print_breadth_first(root);

    printf("root=%s, nodes=%zu, height=%d, comparisons=%zu\n",
           root->key,
           node_count,
           tree_height(root),
           comparisons);

    {
        Node *node = root;

        while (node != NULL) {
            int order;

            ++comparisons;
            order = strcmp(query_key, node->key);

            if (order == 0) {
                Node *successor;
                const char *parent_key;

                parent_key = node->parent != NULL
                           ? node->parent->key
                           : "-";

                if (node->right != NULL) {
                    successor = node->right;
                    while (successor->left != NULL)
                        successor = successor->left;
                } else {
                    Node *current = node;

                    successor = current->parent;
                    while (successor != NULL &&
                           current == successor->right) {
                        current = successor;
                        successor = successor->parent;
                    }
                }

                printf("%d %s %s\n",
                       node->count,
                       parent_key,
                       successor != NULL ? successor->key : "-");
                break;
            }

            node = order < 0 ? node->left : node->right;
        }
    }

    print_inorder_parent_links(root);
    destroy_tree(root);
    return 0;
}