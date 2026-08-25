#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node Node;

struct Node {
    char name[32];
    uint32_t kind;
    uint32_t mode;
    unsigned long size;
    Node *parent;
    Node *first_child;
    Node *last_child;
    Node *next;
    Node **previous_link;
    Node *target;
};

typedef struct {
    uint32_t files;
    uint32_t directories;
    uint32_t links;
    uint32_t reserved;
    unsigned long total_size;
    uint32_t maximum_depth;
    uint32_t reserved2;
} Statistics;

static Node *new_node(const char *name, uint32_t kind, unsigned long size)
{
    Node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    strncpy(node->name, name, sizeof(node->name) - 1);
    node->kind = kind;
    node->mode = kind == 2 ? 0755U : 0644U;
    node->size = size;
    return node;
}

static void append_child(Node *parent, Node *child)
{
    child->parent = parent;

    if (parent->last_child != NULL) {
        parent->last_child->next = child;
        child->previous_link = &parent->last_child->next;
    } else {
        parent->first_child = child;
        child->previous_link = &parent->first_child;
    }

    parent->last_child = child;
}

static void collect_statistics(Node *node, uint32_t depth, Statistics *statistics)
{
    switch (node->kind) {
    case 1:
        ++statistics->files;
        break;
    case 2:
        ++statistics->directories;
        break;
    case 3:
        ++statistics->links;
        break;
    }

    statistics->total_size += node->size;

    if (statistics->maximum_depth < depth)
        statistics->maximum_depth = depth;

    for (Node *child = node->first_child; child != NULL; child = child->next)
        collect_statistics(child, depth + 1, statistics);
}

static unsigned long sum_nodes(unsigned long total, Node *node)
{
    for (; node != NULL; node = node->next) {
        total += node->size;
        total = sum_nodes(total, node->first_child);
    }

    return total;
}

static void print_tree(Node *node, int depth)
{
    char marker;

    if (node->kind == 2)
        marker = 'd';
    else if (node->kind == 3)
        marker = 'l';
    else
        marker = '-';

    printf("%*c%s %o %lu",
           depth, marker, node->name, node->mode, node->size);

    if (node->kind == 3 && node->target != NULL)
        printf(" %s", node->target->name);

    if (node->kind == 2) {
        unsigned long total = node->size;

        for (Node *child = node->first_child;
             child != NULL;
             child = child->next)
            total += sum_nodes(child->size, child->first_child);

        printf(" %lu", total);
    }

    putchar('\n');

    for (Node *child = node->first_child; child != NULL; child = child->next)
        print_tree(child, depth + 1);
}

static unsigned int build_path(Node *node, char path[256])
{
    if (node->parent != NULL) {
        unsigned int used = build_path(node->parent, path);

        if (used <= 255U) {
            int added = snprintf(path + used, 256U - used,
                                 "/%s", node->name);
            used += (unsigned int)added;
        }

        return used;
    }

    return (unsigned int)snprintf(path, 256, "%s", node->name);
}

static Node *find_node(Node *node, const char *name)
{
    if (strcmp(node->name, name) == 0)
        return node;

    for (Node *child = node->first_child; child != NULL; child = child->next) {
        Node *result = find_node(child, name);

        if (result != NULL)
            return result;
    }

    return NULL;
}

static void free_tree(Node *node)
{
    Node *child = node->first_child;

    while (child != NULL) {
        Node *next = child->next;
        free_tree(child);
        child = next;
    }

    free(node);
}

int main(void)
{
    Node *root = new_node("root", 2, 0);
    Node *group = new_node("usr", 2, 0);
    Node *first_directory = new_node("bin", 2, 0);
    Node *second_directory = new_node("lib", 2, 0);
    Node *third_directory = new_node("var", 2, 0);

    append_child(root, group);
    append_child(group, first_directory);
    append_child(group, second_directory);
    append_child(root, third_directory);

    unsigned long first_size = 0x1000UL;
    unsigned long second_size = 0x20000UL;

    for (int index = 0; index < 3; ++index) {
        char name[32];

        snprintf(name, sizeof(name), "file%d", index);
        append_child(first_directory, new_node(name, 1, first_size));

        snprintf(name, sizeof(name), "archive%d", index);
        append_child(second_directory, new_node(name, 1, second_size));

        first_size += 0x1000UL;
        second_size += 0x309UL;
    }

    append_child(third_directory, new_node("notes", 1, 220));
    append_child(third_directory, new_node("report", 1, 1810));

    Node *link = new_node("file1_link", 3, 12);
    append_child(third_directory, link);

    link->target = find_node(root, "file1");

    print_tree(root, 0);

    Statistics statistics = {0};
    collect_statistics(root, 0, &statistics);

    printf("stat %u %u %u %lu %u\n",
           statistics.files,
           statistics.directories,
           statistics.links,
           statistics.total_size,
           statistics.maximum_depth);

    if (link->target != NULL) {
        char path[256] = {0};

        build_path(link->target, path);
        puts(path);

        printf("%u\n",
               (unsigned int)(link->previous_link != NULL &&
                              *link->previous_link == link));
    }

    free_tree(root);
    return 0;
}