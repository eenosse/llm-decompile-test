#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    NODE_FILE = 1,
    NODE_DIRECTORY = 2,
    NODE_LINK = 3
};

struct node {
    char name[32];
    int type;
    unsigned int mode;
    size_t size;
    struct node *parent;
    struct node *first_child;
    struct node *last_child;
    struct node *next_sibling;
    struct node **backlink;
    struct node *target;
};

struct statistics {
    uint32_t files;
    uint32_t directories;
    uint32_t links;
    uint32_t padding;
    size_t total_size;
    uint32_t maximum_depth;
    uint32_t reserved;
};

static struct node *append_child(struct node *parent, struct node *child)
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

static size_t subtree_size(const struct node *node)
{
    size_t total = node->size;

    for (const struct node *child = node->first_child;
         child != NULL;
         child = child->next_sibling)
        total += subtree_size(child);

    return total;
}

static void collect_statistics(const struct node *node, uint32_t depth,
                               struct statistics *result)
{
    if (node->type == NODE_FILE)
        ++result->files;
    else if (node->type == NODE_DIRECTORY)
        ++result->directories;
    else if (node->type == NODE_LINK)
        ++result->links;

    result->total_size += node->size;

    if (result->maximum_depth < depth)
        result->maximum_depth = depth;

    for (const struct node *child = node->first_child;
         child != NULL;
         child = child->next_sibling)
        collect_statistics(child, depth + 1, result);
}

static int format_path(const struct node *node, char *buffer, size_t capacity)
{
    int length;

    if (node->parent == NULL)
        return snprintf(buffer, capacity, "%s", node->name);

    length = format_path(node->parent, buffer, capacity);
    if (length < 0 || (size_t)length >= capacity)
        return length;

    return length + snprintf(buffer + length, capacity - (size_t)length,
                             "/%s", node->name);
}

static struct node *make_node(const char *name, int type, size_t size)
{
    struct node *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    strncpy(node->name, name, sizeof(node->name) - 1);
    node->type = type;
    node->size = size;
    node->mode = type == NODE_DIRECTORY ? 0755U : 0644U;

    return node;
}

static struct node *find_node(struct node *node, const char *name)
{
    if (strcmp(node->name, name) == 0)
        return node;

    for (struct node *child = node->first_child;
         child != NULL;
         child = child->next_sibling) {
        struct node *found = find_node(child, name);

        if (found != NULL)
            return found;
    }

    return NULL;
}

static void print_tree(const struct node *node, int depth)
{
    char kind = '-';

    if (node->type == NODE_DIRECTORY)
        kind = 'd';
    else if (node->type == NODE_LINK)
        kind = 'l';

    printf("%*c%s %o %zu", depth, kind, node->name, node->mode, node->size);

    if (node->type == NODE_LINK && node->target != NULL)
        printf(" %s", node->target->name);

    if (node->type == NODE_DIRECTORY)
        printf(" %zu", subtree_size(node));

    putc('\n', stdout);

    for (const struct node *child = node->first_child;
         child != NULL;
         child = child->next_sibling)
        print_tree(child, depth + 1);
}

static void destroy_tree(struct node *node)
{
    struct node *child = node->first_child;

    while (child != NULL) {
        struct node *next = child->next_sibling;
        destroy_tree(child);
        child = next;
    }

    free(node);
}

int main(void)
{
    struct node *root = make_node("root", NODE_DIRECTORY, 0);
    struct node *usr = append_child(
        root, make_node("usr", NODE_DIRECTORY, 0));
    struct node *bin = append_child(
        usr, make_node("bin", NODE_DIRECTORY, 0));
    struct node *lib = append_child(
        usr, make_node("lib", NODE_DIRECTORY, 0));
    struct node *var = append_child(
        root, make_node("var", NODE_DIRECTORY, 0));

    char name[32];

    for (size_t i = 0; i < 3; ++i) {
        snprintf(name, sizeof(name), "file%zu", i);
        append_child(bin, make_node(name, NODE_FILE, (i + 1) << 12));

        snprintf(name, sizeof(name), "archive%zu", i);
        append_child(lib, make_node(name, NODE_FILE,
                                    (size_t)0x20000 + i * 0x309));
    }

    append_child(var, make_node("notes", NODE_FILE, 220));
    append_child(var, make_node("config", NODE_FILE, 1810));

    struct node *link = append_child(
        var, make_node("latest.log", NODE_LINK, 12));
    link->target = find_node(root, "file1");

    print_tree(root, 0);

    struct statistics result = {0};
    collect_statistics(root, 0, &result);

    printf("%u %u %u %zu %u\n",
           result.files,
           result.directories,
           result.links,
           result.total_size,
           result.maximum_depth);

    if (link->target != NULL) {
        char path[256];

        format_path(link->target, path, sizeof(path));
        puts(path);

        printf("backlink=%d\n",
               link->backlink != NULL && *link->backlink == link);
    }

    destroy_tree(root);
    return 0;
}