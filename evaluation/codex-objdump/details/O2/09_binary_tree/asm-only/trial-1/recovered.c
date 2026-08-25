#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node Node;

struct Node {
    char text[24];
    int count;
    int first_index;
    Node *left;
    Node *right;
    Node *parent;
    int subtree_size;
    int reserved;
};

static const char *const words[15] = {
    "the", "quick", "brown", "fox", "jumps",
    "over", "the", "lazy", "dog", "the",
    "quick", "brown", "fox", "jumps", "over"
};

static Node *make_node(const char *text, int index, Node *parent)
{
    Node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    strncpy(node->text, text, 23);
    node->count = 1;
    node->first_index = index;
    node->parent = parent;
    node->subtree_size = 1;
    return node;
}

static void print_recursive(Node *node, int depth)
{
    if (node == NULL)
        return;

    print_recursive(node->left, depth + 1);
    printf("%*s (%d,%d,%d)\n", depth, node->text, node->count,
           node->first_index, node->subtree_size);
    print_recursive(node->right, depth + 1);
}

static void print_inorder(Node *root)
{
    Node *stack[64];
    int count = 0;
    Node *node = root;

    while (node != NULL || count != 0) {
        while (node != NULL) {
            if (count <= 63)
                stack[count++] = node;
            node = node->left;
        }

        if (count == 0)
            break;

        node = stack[--count];
        printf("%s ", node->text);
        node = node->right;
    }

    putchar('\n');
}

static void print_breadth_first(Node *root)
{
    Node *queue[64];
    int next = 1;
    size_t position = 0;
    Node *node = root;

    while (node != NULL) {
        printf("%s %d\n", node->text, node->subtree_size);

        if (node->left != NULL && next <= 63)
            queue[next++] = node->left;
        if (node->right != NULL && next <= 63)
            queue[next++] = node->right;

        ++position;
        if ((size_t)next <= position)
            break;
        node = queue[position];
    }

    putchar('\n');
}

static int tree_height(const Node *node)
{
    int left_height;
    int right_height;

    if (node == NULL)
        return 0;

    left_height = tree_height(node->left);
    right_height = tree_height(node->right);
    return (left_height > right_height ? left_height : right_height) + 1;
}

static void free_tree(Node *node)
{
    if (node == NULL)
        return;

    free_tree(node->left);
    free_tree(node->right);
    free(node);
}

static void print_using_parents(Node *root)
{
    Node *node = root;

    if (node != NULL) {
        while (node->left != NULL)
            node = node->left;

        for (;;) {
            Node *parent;

            printf("%s ", node->text);

            if (node->right != NULL) {
                node = node->right;
                while (node->left != NULL)
                    node = node->left;
                continue;
            }

            parent = node->parent;
            while (parent != NULL && parent->right == node) {
                node = parent;
                parent = parent->parent;
            }

            if (parent == NULL)
                break;
            node = parent;
        }
    }

    putchar('\n');
}

int main(void)
{
    Node *root = NULL;
    size_t unique_count = 0;
    size_t comparison_count = 0;
    int index;

    for (index = 0; index < 15; ++index) {
        const char *text = words[index];

        if (root == NULL) {
            root = make_node(text, index, NULL);
            ++unique_count;
        } else {
            Node *node = root;

            for (;;) {
                int comparison;
                Node **link;

                ++comparison_count;
                comparison = strcmp(text, node->text);

                if (comparison == 0) {
                    ++node->count;
                    break;
                }

                link = comparison < 0 ? &node->left : &node->right;
                if (*link != NULL) {
                    node = *link;
                    continue;
                }

                *link = make_node(text, index, node);
                ++unique_count;

                while (node != NULL) {
                    ++node->subtree_size;
                    node = node->parent;
                }
                break;
            }
        }
    }

    print_recursive(root, 0);
    print_inorder(root);
    print_breadth_first(root);

    printf("%s %zu %d %zu\n", root->text, unique_count,
           tree_height(root), comparison_count);

    {
        const char *target = "fox";
        Node *found = root;

        while (found != NULL) {
            int comparison;

            ++comparison_count;
            comparison = strcmp(target, found->text);
            if (comparison == 0)
                break;

            found = comparison < 0 ? found->left : found->right;
        }

        if (found != NULL) {
            Node *successor;
            const char *parent_text =
                found->parent != NULL ? found->parent->text : "-";

            if (found->right != NULL) {
                successor = found->right;
                while (successor->left != NULL)
                    successor = successor->left;
            } else {
                Node *child = found;

                successor = found->parent;
                while (successor != NULL && successor->right == child) {
                    child = successor;
                    successor = successor->parent;
                }
            }

            printf("%d %s %s\n", found->count, parent_text,
                   successor != NULL ? successor->text : "-");
        }
    }

    print_using_parents(root);
    free_tree(root);
    return 0;
}