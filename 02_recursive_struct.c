/*
 * 02_recursive_struct.c
 * Target feature: self-referential (recursive) struct types - parent /
 * first-child / next-sibling tree, plus a pointer-to-pointer back link.
 * Exercises deep pointer chasing and recursive traversal.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef enum FsKind {
    FS_FILE = 1,
    FS_DIR  = 2,
    FS_LINK = 3
} FsKind;

typedef struct FsNode FsNode;

struct FsNode {
    char      name[32];
    FsKind    kind;
    uint32_t  mode;
    uint64_t  size;
    FsNode   *parent;
    FsNode   *first_child;
    FsNode   *last_child;
    FsNode   *next_sibling;
    FsNode  **self_slot;   /* pointer to the pointer that references this node */
    FsNode   *link_target; /* only for FS_LINK: another node of the same type */
};

typedef struct FsStats {
    uint32_t n_files;
    uint32_t n_dirs;
    uint32_t n_links;
    uint64_t total_bytes;
    uint32_t max_depth;
} FsStats;

static FsNode *fs_new(const char *name, FsKind kind, uint64_t size)
{
    FsNode *n = (FsNode *)calloc(1, sizeof(FsNode));
    if (!n)
        exit(1);
    strncpy(n->name, name, sizeof(n->name) - 1);
    n->kind = kind;
    n->size = size;
    n->mode = (kind == FS_DIR) ? 0755u : 0644u;
    return n;
}

static FsNode *fs_attach(FsNode *dir, FsNode *child)
{
    child->parent = dir;
    if (dir->last_child) {
        dir->last_child->next_sibling = child;
        child->self_slot = &dir->last_child->next_sibling;
    } else {
        dir->first_child = child;
        child->self_slot = &dir->first_child;
    }
    dir->last_child = child;
    return child;
}

static uint64_t fs_recursive_size(const FsNode *n)
{
    uint64_t total = n->size;
    const FsNode *c;

    for (c = n->first_child; c != NULL; c = c->next_sibling)
        total += fs_recursive_size(c);
    return total;
}

static void fs_collect(const FsNode *n, unsigned depth, FsStats *st)
{
    const FsNode *c;

    switch (n->kind) {
    case FS_FILE: st->n_files++; break;
    case FS_DIR:  st->n_dirs++;  break;
    case FS_LINK: st->n_links++; break;
    }
    st->total_bytes += n->size;
    if (depth > st->max_depth)
        st->max_depth = depth;

    for (c = n->first_child; c != NULL; c = c->next_sibling)
        fs_collect(c, depth + 1, st);
}

static FsNode *fs_find(FsNode *root, const char *name)
{
    FsNode *c;
    FsNode *hit;

    if (strcmp(root->name, name) == 0)
        return root;
    for (c = root->first_child; c != NULL; c = c->next_sibling) {
        hit = fs_find(c, name);
        if (hit)
            return hit;
    }
    return NULL;
}

static int fs_path(const FsNode *n, char *buf, size_t cap)
{
    int used;

    if (n->parent == NULL)
        return snprintf(buf, cap, "%s", n->name);
    used = fs_path(n->parent, buf, cap);
    if (used < 0 || (size_t)used >= cap)
        return used;
    return used + snprintf(buf + used, cap - (size_t)used, "/%s", n->name);
}

static void fs_print(const FsNode *n, unsigned depth)
{
    const FsNode *c;

#ifdef RESULT_ONLY
    printf("%u %c %s %04o %llu", depth,
           n->kind == FS_DIR ? 'd' : (n->kind == FS_LINK ? 'l' : '-'),
           n->name, n->mode, (unsigned long long)n->size);
    if (n->kind == FS_LINK && n->link_target)
        printf(" %s", n->link_target->name);
    if (n->kind == FS_DIR)
        printf(" %llu", (unsigned long long)fs_recursive_size(n));
#else
    unsigned i;
    for (i = 0; i < depth; i++)
        fputs("  ", stdout);
    printf("%c %-16s mode=%04o size=%llu",
           n->kind == FS_DIR ? 'd' : (n->kind == FS_LINK ? 'l' : '-'),
           n->name, n->mode, (unsigned long long)n->size);
    if (n->kind == FS_LINK && n->link_target)
        printf(" -> %s", n->link_target->name);
    if (n->kind == FS_DIR)
        printf(" [recursive=%llu]", (unsigned long long)fs_recursive_size(n));
#endif
    putchar('\n');

    for (c = n->first_child; c != NULL; c = c->next_sibling)
        fs_print(c, depth + 1);
}

static void fs_free(FsNode *n)
{
    FsNode *c = n->first_child;

    while (c) {
        FsNode *next = c->next_sibling;
        fs_free(c);
        c = next;
    }
    free(n);
}

int main(void)
{
    FsNode *root = fs_new("root", FS_DIR, 0);
    FsNode *usr  = fs_attach(root, fs_new("usr", FS_DIR, 0));
    FsNode *bin  = fs_attach(usr,  fs_new("bin", FS_DIR, 0));
    FsNode *lib  = fs_attach(usr,  fs_new("lib", FS_DIR, 0));
    FsNode *etc  = fs_attach(root, fs_new("etc", FS_DIR, 0));
    FsNode *ln;
    FsStats st;
    char path[256];
    unsigned i;

    for (i = 0; i < 3; i++) {
        char nm[32];
        snprintf(nm, sizeof(nm), "tool%u", i);
        fs_attach(bin, fs_new(nm, FS_FILE, 4096ull * (i + 1)));
        snprintf(nm, sizeof(nm), "libz%u.so", i);
        fs_attach(lib, fs_new(nm, FS_FILE, 131072ull + i * 777ull));
    }
    fs_attach(etc, fs_new("hosts", FS_FILE, 220));
    fs_attach(etc, fs_new("passwd", FS_FILE, 1810));

    ln = fs_attach(etc, fs_new("tool0.link", FS_LINK, 12));
    ln->link_target = fs_find(root, "tool0");

    fs_print(root, 0);

    memset(&st, 0, sizeof(st));
    fs_collect(root, 0, &st);
    BENCH_OUTPUT(
        printf("files=%u dirs=%u links=%u bytes=%llu depth=%u\n",
               st.n_files, st.n_dirs, st.n_links,
               (unsigned long long)st.total_bytes, st.max_depth),
        printf("%u %u %u %llu %u\n", st.n_files, st.n_dirs, st.n_links,
               (unsigned long long)st.total_bytes, st.max_depth));

    if (ln->link_target) {
        fs_path(ln->link_target, path, sizeof(path));
        BENCH_OUTPUT(printf("link resolves to %s\n", path), printf("%s\n", path));
        BENCH_OUTPUT(printf("slot check: %d\n", ln->self_slot && *ln->self_slot == ln),
                     printf("%d\n", ln->self_slot && *ln->self_slot == ln));
    }

    fs_free(root);
    return 0;
}
