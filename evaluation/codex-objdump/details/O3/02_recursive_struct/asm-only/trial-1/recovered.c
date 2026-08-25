#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node Node;

struct Node {
    char name[32];
    uint32_t type;
    uint32_t mode;
    size_t size;
    Node *parent;
    Node *first_child;
    Node *last_child;
    Node *next;
    Node **backlink;
    Node *target;
};

typedef struct {
    uint32_t regular_count;
    uint32_t directory_count;
    uint32_t link_count;
    uint32_t padding;
    size_t total_size;
    uint32_t maximum_depth;
} Statistics;

enum {
    NODE_REGULAR = 1,
    NODE_DIRECTORY = 2,
    NODE_LINK = 3
};

static Node *make_node(const char *name, uint32_t type, size_t size)
{
    Node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    strncpy(node->name, name, sizeof(node->name) - 1);
    node->type = type;
    node->mode = type == NODE_DIRECTORY ? 0755U : 0644U;
    node->size = size;
    return node;
}

static Node *make_regular(const char *name, size_t size)
{
    Node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    strncpy(node->name, name, sizeof(node->name) - 1);
    node->type = NODE_REGULAR;
    node->mode = 0644U;
    node->size = size;
    return node;
}

static void append_child(Node *parent, Node *child)
{
    child->parent = parent;

    if (parent->last_child != NULL) {
        parent->last_child->next = child;
        child->backlink = &parent->last_child->next;
    } else {
        parent->first_child = child;
        child->backlink = &parent->first_child;
    }

    parent->last_child = child;
}

static size_t subtree_size(size_t initial, const Node *child)
{
    size_t total = initial;

    while (child != NULL) {
        total += subtree_size(child->size, child->first_child);
        child = child->next;
    }

    return total;
}

static void print_tree(const Node *node, int depth)
{
    char kind = '-';
    const Node *child;

    if (node->type == NODE_DIRECTORY)
        kind = 'd';
    else if (node->type == NODE_LINK)
        kind = 'l';

    printf("%*s [%04o] %c %zu",
           depth, node->name, node->mode, kind, node->size);

    if (node->type == NODE_LINK && node->target != NULL)
        printf(" %s", node->target->name);

    if (node->type == NODE_DIRECTORY)
        printf(" %zu", subtree_size(node->size, node->first_child));

    putc('\n', stdout);

    for (child = node->first_child; child != NULL; child = child->next)
        print_tree(child, depth + 1);
}

static Node *find_node(Node *node, const char *name)
{
    Node *child;
    Node *found;

    if (strcmp(node->name, name) == 0)
        return node;

    for (child = node->first_child; child != NULL; child = child->next) {
        found = find_node(child, name);
        if (found != NULL)
            return found;
    }

    return NULL;
}

static void collect_statistics(const Node *node, uint32_t depth,
                               Statistics *statistics)
{
    const Node *child;

    if (node->type == NODE_REGULAR)
        ++statistics->regular_count;
    else if (node->type == NODE_DIRECTORY)
        ++statistics->directory_count;
    else if (node->type == NODE_LINK)
        ++statistics->link_count;

    statistics->total_size += node->size;

    if (statistics->maximum_depth < depth)
        statistics->maximum_depth = depth;

    for (child = node->first_child; child != NULL; child = child->next)
        collect_statistics(child, depth + 1U, statistics);
}

static int build_path(const Node *node, char output[256])
{
    int length;
    int added;

    if (node->parent == NULL)
        return snprintf(output, 256, "%s", node->name);

    length = build_path(node->parent, output);
    if (length < 0 || length > 255)
        return length;

    added = snprintf(output + length, 256U - (size_t)length,
                     "/%s", node->name);
    return length + added;
}

static void destroy_tree(Node *node)
{
    Node *child = node->first_child;

    while (child != NULL) {
        Node *next = child->next;
        destroy_tree(child);
        child = next;
    }

    free(node);
}

int main(void)
{
    Node *root;
    Node *usr;
    Node *bin;
    Node *lib;
    Node *var;
    Node *link;
    Node *found;
    Statistics statistics = {0};
    char temporary[32];
    char path[256];
    size_t data_size = 0x20000U;
    size_t file_size = 0x1000U;
    int i;

    root = make_node("root", NODE_DIRECTORY, 0);
    usr = make_node("usr", NODE_DIRECTORY, 0);
    append_child(root, usr);

    bin = make_node("bin", NODE_DIRECTORY, 0);
    append_child(usr, bin);

    lib = make_node("lib", NODE_DIRECTORY, 0);
    append_child(usr, lib);

    var = make_node("var", NODE_DIRECTORY, 0);
    append_child(root, var);

    for (i = 0; i < 3; ++i) {
        Node *file;
        Node *data;

        snprintf(temporary, sizeof(temporary), "file%d", i);
        file = make_regular(temporary, file_size);
        append_child(bin, file);

        snprintf(temporary, sizeof(temporary), "data%d", i);
        data = make_regular(temporary, data_size);
        append_child(lib, data);

        file_size += 0x1000U;
        data_size += 0x309U;
    }

    append_child(var, make_regular("small", 0xdcU));
    append_child(var, make_regular("medium", 0x712U));

    link = make_node("shortcut", NODE_LINK, 12U);
    append_child(var, link);

    found = find_node(root, "file1");
    link->target = found;

    print_tree(root, 0);

    collect_statistics(root, 0, &statistics);
    printf("%u %u %u %zu %u\n",
           statistics.regular_count,
           statistics.directory_count,
           statistics.link_count,
           statistics.total_size,
           statistics.maximum_depth);

    if (link->target != NULL) {
        build_path(link->target, path);
        puts(path);

        printf("backlink %d\n",
               link->backlink != NULL && *link->backlink == link);
    }

    destroy_tree(root);
    return 0;
}