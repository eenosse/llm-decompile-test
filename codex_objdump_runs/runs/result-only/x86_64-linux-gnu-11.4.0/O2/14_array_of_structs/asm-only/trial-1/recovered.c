#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum { ITEM_COUNT = 12 };

typedef struct {
    float x;
    float y;
    float z;
    float vx;
    float vy;
    float vz;
    float mass;
    float charge;
    uint32_t id;
    uint16_t tag;
    uint8_t category;
    uint8_t active;
} Item;

typedef struct {
    double energy;
    double center_x;
    double center_y;
    double center_z;
    double momentum_x;
    double momentum_y;
    double momentum_z;
    float min_mass;
    float max_mass;
    uint32_t count;
    uint32_t categories[4];
    char description[48];
} Summary;

typedef struct {
    float x[ITEM_COUNT];
    float y[ITEM_COUNT];
    float z[ITEM_COUNT];
    float vx[ITEM_COUNT];
    float vy[ITEM_COUNT];
    float vz[ITEM_COUNT];
    float mass[ITEM_COUNT];
    uint32_t id[ITEM_COUNT];
    uint8_t category[ITEM_COUNT];
    uint32_t count;
} ItemArrays;

_Static_assert(sizeof(Item) == 40, "unexpected Item layout");
_Static_assert(offsetof(Item, id) == 32, "unexpected Item layout");
_Static_assert(offsetof(Item, tag) == 36, "unexpected Item layout");
_Static_assert(offsetof(Item, category) == 38, "unexpected Item layout");
_Static_assert(offsetof(Item, active) == 39, "unexpected Item layout");

_Static_assert(sizeof(Summary) == 136, "unexpected Summary layout");
_Static_assert(offsetof(Summary, min_mass) == 56, "unexpected Summary layout");
_Static_assert(offsetof(Summary, count) == 64, "unexpected Summary layout");
_Static_assert(offsetof(Summary, description) == 84, "unexpected Summary layout");

_Static_assert(sizeof(ItemArrays) == 400, "unexpected ItemArrays layout");

static void print_items(const Item items[ITEM_COUNT])
{
    size_t i;

    for (i = 0; i < ITEM_COUNT; ++i) {
        const Item *item = &items[i];

        printf("%u m=%g q=%g p=%g,%g,%g c=%u t=%u a=%u\n",
               (unsigned)item->id,
               (double)item->mass,
               (double)item->charge,
               (double)item->x,
               (double)item->y,
               (double)item->z,
               (unsigned)item->category,
               (unsigned)item->tag,
               (unsigned)item->active);
    }
}

static void print_summary(const Summary *summary)
{
    printf("E=%g C=(%g,%g,%g) P=(%g,%g,%g)\n",
           summary->energy,
           summary->center_x,
           summary->center_y,
           summary->center_z,
           summary->momentum_x,
           summary->momentum_y,
           summary->momentum_z);

    printf("[%g,%g] n=%u %u,%u,%u,%u\n",
           (double)summary->min_mass,
           (double)summary->max_mass,
           (unsigned)summary->count,
           (unsigned)summary->categories[0],
           (unsigned)summary->categories[1],
           (unsigned)summary->categories[2],
           (unsigned)summary->categories[3]);
}

static int compare_items(const void *left_pointer, const void *right_pointer)
{
    const Item *left = left_pointer;
    const Item *right = right_pointer;

    if (right->mass > left->mass)
        return 1;
    if (left->mass > right->mass)
        return -1;
    if (left->id < right->id)
        return -1;
    return left->id > right->id;
}

int main(void)
{
    Item items[ITEM_COUNT];
    Item sorted[ITEM_COUNT];
    Summary summary = { 0 };
    Summary printable;
    ItemArrays arrays = { 0 };
    double total_mass = 0.0;
    size_t i;
    unsigned step;

    for (i = 0; i < ITEM_COUNT; ++i) {
        Item *item = &items[i];

        item->x = (float)i * 1.25f;
        item->y = (float)(i & 3u) * 2.0f;
        item->z = (float)((i * i) % 7u);
        item->vx = 1.5f - (float)(i % 3u);
        item->vy = (float)(i % 5u) * 0.25f;
        item->vz = (float)(i & 1u) - 0.5f;
        item->mass = 1.0f + (float)(i % 6u) * 1.5f;
        item->charge = (i & 1u) ? 1.0f : -1.0f;
        item->id = (uint32_t)i + 500u;
        item->tag = (uint16_t)(i | 0x100u);
        item->category = (uint8_t)(i & 3u);
        item->active = (uint8_t)(i % 7u != 6u);
    }

    print_items(items);

    summary.min_mass = 1.0e30f;
    summary.max_mass = -1.0e30f;

    for (i = 0; i < ITEM_COUNT; ++i) {
        const Item *item = &items[i];
        double mass;
        double vx;
        double vy;
        double vz;

        if (!item->active)
            continue;

        mass = (double)item->mass;
        vx = (double)item->vx;
        vy = (double)item->vy;
        vz = (double)item->vz;

        ++summary.count;
        ++summary.categories[item->category & 3u];
        total_mass += mass;

        summary.energy +=
            (0.5 * mass) * (vx * vx + vy * vy + vz * vz);

        summary.center_x += (double)item->x * mass;
        summary.center_y += (double)item->y * mass;
        summary.center_z += (double)item->z * mass;

        summary.momentum_x += vx * mass;
        summary.momentum_y += vy * mass;
        summary.momentum_z += vz * mass;

        if (item->mass < summary.min_mass)
            summary.min_mass = item->mass;
        if (item->mass > summary.max_mass)
            summary.max_mass = item->mass;
    }

    if (total_mass > 0.0) {
        summary.center_x /= total_mass;
        summary.center_y /= total_mass;
        summary.center_z /= total_mass;
    }

    snprintf(summary.description, sizeof summary.description,
             "mass=%.3f energy=%.3f n=%u",
             total_mass, summary.energy, (unsigned)summary.count);

    printable = summary;
    print_summary(&printable);

    summary.energy *= 0.001;
    summary.center_x *= 0.001;
    summary.center_y *= 0.001;
    summary.center_z *= 0.001;
    summary.momentum_x *= 0.001;
    summary.momentum_y *= 0.001;
    summary.momentum_z *= 0.001;

    snprintf(summary.description, sizeof summary.description,
             "scaled m=%.3f e=%.3f",
             total_mass, summary.energy);

    printable = summary;
    print_summary(&printable);

    arrays.count = ITEM_COUNT;
    for (i = 0; i < ITEM_COUNT; ++i) {
        arrays.x[i] = items[i].x;
        arrays.y[i] = items[i].y;
        arrays.z[i] = items[i].z;
        arrays.vx[i] = items[i].vx;
        arrays.vy[i] = items[i].vy;
        arrays.vz[i] = items[i].vz;
        arrays.mass[i] = items[i].mass;
        arrays.id[i] = items[i].id;
        arrays.category[i] = items[i].category;
    }

    for (step = 0; step < 3; ++step) {
        for (i = 0; i < ITEM_COUNT; ++i) {
            arrays.x[i] += arrays.vx[i] * 0.1f;
            arrays.y[i] += arrays.vy[i] * 0.1f;
            arrays.z[i] += arrays.vz[i] * 0.1f;
            arrays.vx[i] -= 0.05f / arrays.mass[i];
        }
    }

    printf("n=%u\n", (unsigned)arrays.count);
    for (i = 0; i < ITEM_COUNT; ++i) {
        printf("%u %g,%g,%g v=%g c=%u\n",
               (unsigned)arrays.id[i],
               (double)arrays.x[i],
               (double)arrays.y[i],
               (double)arrays.z[i],
               (double)arrays.vx[i],
               (unsigned)arrays.category[i]);
    }

    for (i = 0; i < ITEM_COUNT; ++i)
        sorted[i] = items[i];

    qsort(sorted, ITEM_COUNT, sizeof sorted[0], compare_items);
    print_items(sorted);

    return 0;
}