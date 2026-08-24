/*
 * 20_combined_stress.c
 * Target feature: everything at once - a tiny inventory/VM system that mixes
 * nested structs, a recursive container tree, a tagged union item payload, a
 * flexible-array script chunk, a 2-D grid, an intrusive doubly linked list,
 * a binary index tree and function pointers. Intended as the hardest case.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include "result_only/output_mode.h"

#define GRID_W 6
#define GRID_H 4

/* ---------- primitives ---------- */

typedef struct Dim3 {
    uint16_t w, h, d;
} Dim3;

typedef struct Money {
    int64_t  units;
    uint16_t scale;
    char     currency[4];
} Money;

/* ---------- tagged union payload ---------- */

typedef enum ItemKind {
    IT_WEAPON = 0,
    IT_POTION,
    IT_KEY,
    IT_SCRIPT,
    IT_BUNDLE
} ItemKind;

typedef struct Item Item;
typedef struct Container Container;

typedef struct WeaponData {
    uint16_t damage_min;
    uint16_t damage_max;
    uint8_t  crit_pct;
    char     element[8];
} WeaponData;

typedef struct PotionData {
    int16_t  hp_delta;
    int16_t  mp_delta;
    uint16_t duration_s;
    uint8_t  stack;
} PotionData;

typedef struct KeyData {
    uint32_t door_id;
    uint8_t  fingerprint[8];
} KeyData;

/* flexible array of opcodes */
typedef struct ScriptChunk {
    uint16_t n_ops;
    uint16_t max_stack;
    uint8_t  ops[];
} ScriptChunk;

typedef struct ScriptData {
    ScriptChunk *chunk;
    uint32_t     run_count;
} ScriptData;

typedef struct BundleData {
    Container *contents;      /* recursion through another type */
    uint16_t   discount_pct;
} BundleData;

/* ---------- intrusive list link ---------- */

typedef struct Link {
    struct Link *next;
    struct Link *prev;
} Link;

/* ---------- the item ---------- */

struct Item {
    uint32_t id;
    char     name[20];
    Dim3     size;
    Money    price;
    ItemKind kind;
    Link     slot_link;                /* intrusive: offset != 0 */
    union {
        WeaponData weapon;
        PotionData potion;
        KeyData    key;
        ScriptData script;
        BundleData bundle;
    } payload;
    uint32_t (*value_fn)(const Item *); /* per-kind valuation */
};

/* ---------- recursive container tree ---------- */

struct Container {
    char       label[16];
    Dim3       capacity;
    Link       items;                  /* sentinel head of Item::slot_link */
    uint32_t   item_count;
    Container *parent;
    Container *first_child;
    Container *next_sibling;
    uint8_t    grid[GRID_H][GRID_W];   /* occupancy map */
};

/* ---------- binary index over items ---------- */

typedef struct IndexNode {
    uint32_t          key;             /* item id */
    Item             *item;
    struct IndexNode *left;
    struct IndexNode *right;
    uint32_t          depth;
} IndexNode;

typedef struct World {
    Container *root;
    IndexNode *index;
    uint32_t   next_id;
    uint32_t   items_created;
    uint64_t   total_value;
} World;

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

static void link_init(Link *l) { l->next = l; l->prev = l; }

static void link_add_tail(Link *head, Link *n)
{
    n->prev = head->prev;
    n->next = head;
    head->prev->next = n;
    head->prev = n;
}

static void link_del(Link *n)
{
    n->prev->next = n->next;
    n->next->prev = n->prev;
    link_init(n);
}

/* ---------- valuation function pointers ---------- */

static uint32_t value_weapon(const Item *it)
{
    return (uint32_t)((it->payload.weapon.damage_min + it->payload.weapon.damage_max) * 5u
                      + it->payload.weapon.crit_pct * 3u);
}

static uint32_t value_potion(const Item *it)
{
    int32_t mag = it->payload.potion.hp_delta + it->payload.potion.mp_delta;
    if (mag < 0)
        mag = -mag;
    return (uint32_t)(mag * 2 + it->payload.potion.duration_s / 10) * it->payload.potion.stack;
}

static uint32_t value_key(const Item *it)
{
    uint32_t acc = it->payload.key.door_id;
    int i;
    for (i = 0; i < 8; i++)
        acc = acc * 3u + it->payload.key.fingerprint[i];
    return acc % 5000u;
}

static uint32_t value_script(const Item *it)
{
    const ScriptChunk *c = it->payload.script.chunk;
    return c ? (uint32_t)(c->n_ops * 11u + c->max_stack * 7u + it->payload.script.run_count) : 0u;
}

static uint32_t container_value(const Container *c);

static uint32_t value_bundle(const Item *it)
{
    uint32_t inner = it->payload.bundle.contents
                   ? container_value(it->payload.bundle.contents) : 0u;
    return inner * (100u - it->payload.bundle.discount_pct) / 100u;
}

/* ---------- construction ---------- */

static Container *container_new(const char *label, uint16_t w, uint16_t h, uint16_t d)
{
    Container *c = (Container *)calloc(1, sizeof(Container));
    if (!c)
        exit(1);
    strncpy(c->label, label, sizeof(c->label) - 1);
    c->capacity.w = w;
    c->capacity.h = h;
    c->capacity.d = d;
    link_init(&c->items);
    return c;
}

static Container *container_attach(Container *parent, Container *child)
{
    Container **slot = &parent->first_child;
    while (*slot)
        slot = &(*slot)->next_sibling;
    *slot = child;
    child->parent = parent;
    return child;
}

static Item *item_new(World *w, const char *name, ItemKind kind, int64_t price_units)
{
    Item *it = (Item *)calloc(1, sizeof(Item));
    if (!it)
        exit(1);
    it->id = w->next_id++;
    strncpy(it->name, name, sizeof(it->name) - 1);
    it->kind = kind;
    it->price.units = price_units;
    it->price.scale = 100;
    memcpy(it->price.currency, "USD", 4);
    it->size.w = (uint16_t)(1 + (it->id % 3));
    it->size.h = (uint16_t)(1 + (it->id % 2));
    it->size.d = 1;
    link_init(&it->slot_link);
    switch (kind) {
    case IT_WEAPON: it->value_fn = value_weapon; break;
    case IT_POTION: it->value_fn = value_potion; break;
    case IT_KEY:    it->value_fn = value_key;    break;
    case IT_SCRIPT: it->value_fn = value_script; break;
    case IT_BUNDLE: it->value_fn = value_bundle; break;
    }
    w->items_created++;
    return it;
}

static void container_place(Container *c, Item *it)
{
    uint16_t y, x;

    link_add_tail(&c->items, &it->slot_link);
    c->item_count++;
    for (y = 0; y < GRID_H; y++)
        for (x = 0; x < GRID_W; x++)
            if (c->grid[y][x] == 0) {
                c->grid[y][x] = (uint8_t)(it->id & 0xff);
                return;
            }
}

/* uses link_del: unlink from one container's list, relink into another */
static void container_move(Container *from, Container *to, Item *it)
{
    uint16_t y, x;

    link_del(&it->slot_link);
    from->item_count--;
    for (y = 0; y < GRID_H; y++)
        for (x = 0; x < GRID_W; x++)
            if (from->grid[y][x] == (uint8_t)(it->id & 0xff))
                from->grid[y][x] = 0;
    container_place(to, it);
}

static ScriptChunk *chunk_new(const uint8_t *ops, uint16_t n, uint16_t max_stack)
{
    ScriptChunk *c = (ScriptChunk *)malloc(sizeof(ScriptChunk) + n);
    if (!c)
        exit(1);
    c->n_ops = n;
    c->max_stack = max_stack;
    memcpy(c->ops, ops, n);
    return c;
}

/* ---------- index tree ---------- */

static IndexNode *index_insert(IndexNode *node, Item *it, uint32_t depth)
{
    if (!node) {
        IndexNode *n = (IndexNode *)calloc(1, sizeof(IndexNode));
        if (!n)
            exit(1);
        n->key = it->id;
        n->item = it;
        n->depth = depth;
        return n;
    }
    if (it->id < node->key)
        node->left = index_insert(node->left, it, depth + 1);
    else
        node->right = index_insert(node->right, it, depth + 1);
    return node;
}

static Item *index_find(IndexNode *n, uint32_t id)
{
    while (n) {
        if (n->key == id)
            return n->item;
        n = (id < n->key) ? n->left : n->right;
    }
    return NULL;
}

static void index_walk(const IndexNode *n)
{
    if (!n)
        return;
    index_walk(n->left);
    BENCH_OUTPUT(
        printf("    id=%-3u depth=%u %-18s kind=%d value=%u\n",
               n->key, n->depth, n->item->name, (int)n->item->kind,
               n->item->value_fn ? n->item->value_fn(n->item) : 0u),
        printf("%u %u|%s|%d %u\n", n->key, n->depth, n->item->name,
               (int)n->item->kind,
               n->item->value_fn ? n->item->value_fn(n->item) : 0u));
    index_walk(n->right);
}

static void index_free(IndexNode *n)
{
    if (!n)
        return;
    index_free(n->left);
    index_free(n->right);
    free(n);
}

/* ---------- traversal ---------- */

static uint32_t container_value(const Container *c)
{
    uint32_t total = 0;
    const Link *p;
    const Container *kid;

    for (p = c->items.next; p != &c->items; p = p->next) {
        const Item *it = container_of((Link *)p, Item, slot_link);
        if (it->value_fn)
            total += it->value_fn(it);
    }
    for (kid = c->first_child; kid; kid = kid->next_sibling)
        total += container_value(kid);
    return total;
}

static void item_print(const Item *it, int depth)
{
    BENCH_OUTPUT(
        printf("%*s- #%-3u %-18s %ux%ux%u  $%.2f  ",
               depth * 2, "", it->id, it->name,
               it->size.w, it->size.h, it->size.d,
               (double)it->price.units / (double)it->price.scale),
        printf("%d %u|%s|%u %u %u %.2f %d ", depth, it->id, it->name,
               it->size.w, it->size.h, it->size.d,
               (double)it->price.units / (double)it->price.scale,
               (int)it->kind));
    switch (it->kind) {
    case IT_WEAPON:
        BENCH_OUTPUT(
            printf("weapon %u-%u crit=%u%% %s",
                   it->payload.weapon.damage_min, it->payload.weapon.damage_max,
                   it->payload.weapon.crit_pct, it->payload.weapon.element),
            printf("%u %u %u|%s", it->payload.weapon.damage_min,
                   it->payload.weapon.damage_max, it->payload.weapon.crit_pct,
                   it->payload.weapon.element));
        break;
    case IT_POTION:
        BENCH_OUTPUT(
            printf("potion hp%+d mp%+d %us x%u",
                   it->payload.potion.hp_delta, it->payload.potion.mp_delta,
                   it->payload.potion.duration_s, it->payload.potion.stack),
            printf("%d %d %u %u", it->payload.potion.hp_delta,
                   it->payload.potion.mp_delta, it->payload.potion.duration_s,
                   it->payload.potion.stack));
        break;
    case IT_KEY:
        BENCH_OUTPUT(
            printf("key door=%u fp=%02x%02x..", it->payload.key.door_id,
                   it->payload.key.fingerprint[0], it->payload.key.fingerprint[1]),
            printf("%u %02x %02x", it->payload.key.door_id,
                   it->payload.key.fingerprint[0], it->payload.key.fingerprint[1]));
        break;
    case IT_SCRIPT:
        BENCH_OUTPUT(
            printf("script ops=%u stack=%u runs=%u",
                   it->payload.script.chunk ? it->payload.script.chunk->n_ops : 0,
                   it->payload.script.chunk ? it->payload.script.chunk->max_stack : 0,
                   it->payload.script.run_count),
            printf("%u %u %u",
                   it->payload.script.chunk ? it->payload.script.chunk->n_ops : 0,
                   it->payload.script.chunk ? it->payload.script.chunk->max_stack : 0,
                   it->payload.script.run_count));
        break;
    case IT_BUNDLE:
        BENCH_OUTPUT(
            printf("bundle of '%s' -%u%%",
                   it->payload.bundle.contents ? it->payload.bundle.contents->label : "?",
                   it->payload.bundle.discount_pct),
            printf("%s|%u",
                   it->payload.bundle.contents ? it->payload.bundle.contents->label : "?",
                   it->payload.bundle.discount_pct));
        break;
    }
    BENCH_OUTPUT(printf("  [value=%u]\n", it->value_fn ? it->value_fn(it) : 0u),
                 printf(" %u\n", it->value_fn ? it->value_fn(it) : 0u));
}

static void container_print(const Container *c, int depth)
{
    const Link *p;
    const Container *kid;
    uint16_t y, x;

    BENCH_OUTPUT(
        printf("%*s[%s] cap=%ux%ux%u items=%u value=%u\n", depth * 2, "",
               c->label, c->capacity.w, c->capacity.h, c->capacity.d,
               c->item_count, container_value(c)),
        printf("%d|%s|%u %u %u %u %u\n", depth, c->label,
               c->capacity.w, c->capacity.h, c->capacity.d,
               c->item_count, container_value(c)));
    for (y = 0; y < GRID_H; y++) {
        BENCH_VERBOSE(printf("%*s  grid:", depth * 2, ""));
        for (x = 0; x < GRID_W; x++)
            printf(" %02x", c->grid[y][x]);
        putchar('\n');
    }
    for (p = c->items.next; p != &c->items; p = p->next)
        item_print(container_of((Link *)p, Item, slot_link), depth + 1);
    for (kid = c->first_child; kid; kid = kid->next_sibling)
        container_print(kid, depth + 1);
}

/* tiny stack VM over the FAM script chunk */
static int32_t script_run(Item *it)
{
    int32_t stack[16];
    int sp = 0;
    uint16_t pc;
    ScriptChunk *c = it->payload.script.chunk;

    if (!c)
        return 0;
    it->payload.script.run_count++;
    for (pc = 0; pc < c->n_ops; pc++) {
        uint8_t op = c->ops[pc];
        switch (op >> 4) {
        case 0: if (sp < 16) stack[sp++] = (int32_t)(op & 0xf); break;
        case 1: if (sp >= 2) { stack[sp - 2] += stack[sp - 1]; sp--; } break;
        case 2: if (sp >= 2) { stack[sp - 2] *= stack[sp - 1]; sp--; } break;
        case 3: if (sp >= 2) { stack[sp - 2] -= stack[sp - 1]; sp--; } break;
        case 4: if (sp >= 1) stack[sp - 1] = -stack[sp - 1]; break;
        default: break;
        }
    }
    return sp > 0 ? stack[sp - 1] : 0;
}

static void container_free(Container *c)
{
    Link *p = c->items.next;
    Container *kid;

    while (p != &c->items) {
        Link *next = p->next;
        Item *it = container_of(p, Item, slot_link);
        if (it->kind == IT_SCRIPT)
            free(it->payload.script.chunk);
        free(it);
        p = next;
    }
    kid = c->first_child;
    while (kid) {
        Container *nx = kid->next_sibling;
        container_free(kid);
        kid = nx;
    }
    free(c);
}

int main(void)
{
    static const uint8_t prog[] = {0x07, 0x03, 0x10, 0x05, 0x20, 0x02, 0x30, 0x40};
    World w;
    Container *root, *backpack, *chest, *pouch;
    Item *it;
    Item *sword;

    memset(&w, 0, sizeof(w));
    w.next_id = 1;

    root     = container_new("world",    16, 16, 16);
    backpack = container_attach(root, container_new("backpack", 6, 4, 2));
    chest    = container_attach(root, container_new("chest",    8, 6, 4));
    pouch    = container_attach(backpack, container_new("pouch", 2, 2, 1));
    w.root = root;

    sword = item_new(&w, "iron sword", IT_WEAPON, 12500);
    sword->payload.weapon.damage_min = 12;
    sword->payload.weapon.damage_max = 28;
    sword->payload.weapon.crit_pct   = 7;
    memcpy(sword->payload.weapon.element, "steel", 6);
    container_place(backpack, sword);
    w.index = index_insert(w.index, sword, 0);

    it = item_new(&w, "elixir", IT_POTION, 3200);
    it->payload.potion.hp_delta   = 120;
    it->payload.potion.mp_delta   = 45;
    it->payload.potion.duration_s = 300;
    it->payload.potion.stack      = 3;
    container_place(backpack, it);
    w.index = index_insert(w.index, it, 0);

    it = item_new(&w, "brass key", IT_KEY, 500);
    it->payload.key.door_id = 4711;
    {
        int i;
        for (i = 0; i < 8; i++)
            it->payload.key.fingerprint[i] = (uint8_t)(0x5a + i * 3);
    }
    container_place(pouch, it);
    w.index = index_insert(w.index, it, 0);

    it = item_new(&w, "rune tablet", IT_SCRIPT, 8800);
    it->payload.script.chunk = chunk_new(prog, (uint16_t)sizeof(prog), 8);
    container_place(chest, it);
    w.index = index_insert(w.index, it, 0);
    {
        int32_t result = script_run(it);
        BENCH_OUTPUT(printf("script result = %d (runs=%u)\n", result,
                           it->payload.script.run_count),
                     printf("%d %u\n", result, it->payload.script.run_count));
        result = script_run(it);
        BENCH_OUTPUT(printf("script result = %d (runs=%u)\n", result,
                           it->payload.script.run_count),
                     printf("%d %u\n", result, it->payload.script.run_count));
    }

    it = item_new(&w, "starter kit", IT_BUNDLE, 15000);
    it->payload.bundle.contents     = backpack;
    it->payload.bundle.discount_pct = 20;
    container_place(chest, it);
    w.index = index_insert(w.index, it, 0);

    BENCH_VERBOSE(printf("\ncontainer tree:\n"));
    container_print(root, 0);

    container_move(backpack, pouch, sword);
    BENCH_VERBOSE(printf("\nafter moving '%s' backpack -> pouch:\n", sword->name));
    container_print(root, 0);

    BENCH_VERBOSE(printf("\nindex in id order:\n"));
    index_walk(w.index);

    {
        Item *found = index_find(w.index, 3);
        BENCH_OUTPUT(
            printf("\nindex_find(3) -> %s (kind=%d)\n",
                   found ? found->name : "(none)", found ? (int)found->kind : -1),
            printf("%s|%d\n", found ? found->name : "", found ? (int)found->kind : -1));
    }

    w.total_value = container_value(root);
    BENCH_OUTPUT(
        printf("world value=%llu items=%u next_id=%u\n",
               (unsigned long long)w.total_value, w.items_created, w.next_id),
        printf("%llu %u %u\n", (unsigned long long)w.total_value,
               w.items_created, w.next_id));

    index_free(w.index);
    container_free(root);
    return 0;
}
