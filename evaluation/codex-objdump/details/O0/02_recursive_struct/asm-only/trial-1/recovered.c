#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    NODE_FILE = 1,
    NODE_DIRECTORY = 2,
    NODE_LINK = 3
};

typedef struct Node Node;

struct Node {
    char name[32];
    uint32_t type;
    uint32_t mode;
    size_t size;
    Node *parent;
    Node *first_child;
    Node *last_child;
    Node *next_sibling;
    Node **backlink;
    Node *target;
};

typedef struct {
    uint32_t files;
    uint32_t directories;
    uint32_t links;
    size_t total_size;
    uint32_t maximum_depth;
} Statistics;

static Node *node_create(const char *name, uint32_t type, size_t size)
{
    Node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    strncpy(node->name, name, sizeof(node->name) - 1);
    node->type = type;
    node->size = size;
    node->mode = type == NODE_DIRECTORY ? 0755U : 0644U;
    return node;
}

static Node *node_append(Node *parent, Node *child)
{
    child->parent = parent;

    if (parent->last_child != NULL) {
        parent->last_child->next_sibling = child;
        child->backlink = &parent->last_child->next_sibling;
    } else {
        parent->first_child = child;
        child->backlink = &parent->first_child;
    }

    parent->last_child = child;
    return child;
}

static size_t node_total_size(const Node *node)
{
    size_t total = node->size;
    const Node *child;

    for (child = node->first_child; child != NULL;
         child = child->next_sibling)
        total += node_total_size(child);

    return total;
}

static void collect_statistics(const Node *node, uint32_t depth,
                               Statistics *statistics)
{
    const Node *child;

    switch (node->type) {
    case NODE_FILE:
        ++statistics->files;
        break;
    case NODE_DIRECTORY:
        ++statistics->directories;
        break;
    case NODE_LINK:
        ++statistics->links;
        break;
    }

    statistics->total_size += node->size;

    if (depth > statistics->maximum_depth)
        statistics->maximum_depth = depth;

    for (child = node->first_child; child != NULL;
         child = child->next_sibling)
        collect_statistics(child, depth + 1, statistics);
}

static Node *node_find(Node *node, const char *name)
{
    Node *child;

    if (strcmp(node->name, name) == 0)
        return node;

    for (child = node->first_child; child != NULL;
         child = child->next_sibling) {
        Node *result = node_find(child, name);

        if (result != NULL)
            return result;
    }

    return NULL;
}

static int node_path(const Node *node, char *buffer, size_t capacity)
{
    int length;

    if (node->parent == NULL)
        return snprintf(buffer, capacity, "%s", node->name);

    length = node_path(node->parent, buffer, capacity);
    if (length < 0 || capacity <= (size_t)length)
        return length;

    return length + snprintf(buffer + length, capacity - (size_t)length,
                             "/%s", node->name);
}

static void node_print(const Node *node, int depth)
{
    const Node *child;
    int marker;

    if (node->type == NODE_DIRECTORY)
        marker = 'd';
    else if (node->type == NODE_LINK)
        marker = 'l';
    else
        marker = '-';

    printf("%*c %-10s %04o %zu",
           depth, marker, node->name, node->mode, node->size);

    if (node->type == NODE_LINK && node->target != NULL)
        printf(" %s", node->target->name);

    if (node->type == NODE_DIRECTORY)
        printf(" %zu", node_total_size(node));

    putchar('\n');

    for (child = node->first_child; child != NULL;
         child = child->next_sibling)
        node_print(child, depth + 1);
}

static void node_destroy(Node *node)
{
    Node *child = node->first_child;

    while (child != NULL) {
        Node *next = child->next_sibling;
        node_destroy(child);
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
    Node *etc;
    Node *link;
    Statistics statistics;
    char buffer[256];
    unsigned int i;

    root = node_create("root", NODE_DIRECTORY, 0);
    usr = node_append(root, node_create("usr", NODE_DIRECTORY, 0));
    bin = node_append(usr, node_create("bin", NODE_DIRECTORY, 0));
    lib = node_append(usr, node_create("lib", NODE_DIRECTORY, 0));
    etc = node_append(root, node_create("etc", NODE_DIRECTORY, 0));

    for (i = 0; i <= 2; ++i) {
        snprintf(buffer, 32, "tool%d", i);
        node_append(bin,
                    node_create(buffer, NODE_FILE,
                                (size_t)(i + 1U) << 12));

        snprintf(buffer, 32, "lib%d.so", i);
        node_append(lib,
                    node_create(buffer, NODE_FILE,
                                (size_t)i * 777U + 0x20000U));
    }

    node_append(etc, node_create("hosts", NODE_FILE, 220));
    node_append(etc, node_create("passwd", NODE_FILE, 1810));
    link = node_append(etc, node_create("localtime", NODE_LINK, 12));
    link->target = node_find(root, "tool1");

    node_print(root, 0);

    memset(&statistics, 0, sizeof(statistics));
    collect_statistics(root, 0, &statistics);

    printf("%u %u %u %zu %u\n",
           statistics.files,
           statistics.directories,
           statistics.links,
           statistics.total_size,
           statistics.maximum_depth);

    if (link->target != NULL) {
        node_path(link->target, buffer, sizeof(buffer));
        puts(buffer);
        printf("backlink %d\n",
               link->backlink != NULL && *link->backlink == link);
    }

    node_destroy(root);
    return 0;
}