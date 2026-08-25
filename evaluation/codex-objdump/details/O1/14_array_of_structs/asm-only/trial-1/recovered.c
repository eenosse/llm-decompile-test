#include <float.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ITEM_COUNT = 12 };

typedef struct {
    float x;
    float y;
    float z;
} Vector3;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    float mass;
    float charge;
    uint32_t id;
    uint16_t flags;
    uint8_t category;
    bool active;
} Item;

typedef struct {
    double energy;
    double weighted_position[3];
    double weighted_velocity[3];
    float minimum_mass;
    float maximum_mass;
    uint32_t active_count;
    uint32_t category_counts[4];
    char summary[48];
} Statistics;

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

_Static_assert(sizeof(Vector3) == 12, "unexpected Vector3 layout");
_Static_assert(sizeof(Item) == 40, "unexpected Item layout");
_Static_assert(offsetof(Item, mass) == 24, "unexpected Item layout");
_Static_assert(offsetof(Item, id) == 32, "unexpected Item layout");
_Static_assert(sizeof(Statistics) == 136, "unexpected Statistics layout");
_Static_assert(offsetof(Statistics, summary) == 84,
               "unexpected Statistics layout");
_Static_assert(sizeof(ItemArrays) == 400, "unexpected ItemArrays layout");

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
    if (left->id > right->id)
        return 1;
    return 0;
}

static void print_items(const Item *items, size_t count,
                        const char *unused_heading)
{
    size_t i;

    (void)unused_heading;
    for (i = 0; i < count; ++i) {
        const Item *item = &items[i];

        printf("%u %.3f %.3f %.3f %.3f %.3f %u %hu %u\n",
               item->id,
               (double)item->mass,
               (double)item->charge,
               (double)item->position.x,
               (double)item->position.y,
               (double)item->position.z,
               (unsigned int)item->category,
               (unsigned int)item->flags,
               (unsigned int)item->active);
    }
}

static void print_statistics(const Statistics *statistics,
                             const char *unused_heading)
{
    (void)unused_heading;

    printf("%.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
           statistics->energy,
           statistics->weighted_position[0],
           statistics->weighted_position[1],
           statistics->weighted_position[2],
           statistics->weighted_velocity[0],
           statistics->weighted_velocity[1],
           statistics->weighted_velocity[2]);

    printf("%.3f %.3f %u %u %u %u %u\n",
           (double)statistics->minimum_mass,
           (double)statistics->maximum_mass,
           statistics->active_count,
           statistics->category_counts[0],
           statistics->category_counts[1],
           statistics->category_counts[2],
           statistics->category_counts[3]);
}

static Statistics calculate_statistics(const Item *items, size_t count)
{
    Statistics result = { 0 };
    double total_mass = 0.0;
    size_t i;

    result.minimum_mass = FLT_MAX;
    result.maximum_mass = -FLT_MAX;

    for (i = 0; i < count; ++i) {
        const Item *item = &items[i];
        double mass;
        double vx;
        double vy;
        double vz;

        if (!item->active)
            continue;

        ++result.active_count;
        ++result.category_counts[item->category & 3u];

        mass = (double)item->mass;
        vx = (double)item->velocity.x;
        vy = (double)item->velocity.y;
        vz = (double)item->velocity.z;

        result.energy +=
            (vx * vx + vy * vy + vz * vz) * (mass * 0.5);
        total_mass += mass;

        result.weighted_position[0] +=
            (double)item->position.x * mass;
        result.weighted_position[1] +=
            (double)item->position.y * mass;
        result.weighted_position[2] +=
            (double)item->position.z * mass;

        result.weighted_velocity[0] += vx * mass;
        result.weighted_velocity[1] += vy * mass;
        result.weighted_velocity[2] += vz * mass;

        if (item->mass < result.minimum_mass)
            result.minimum_mass = item->mass;
        if (item->mass > result.maximum_mass)
            result.maximum_mass = item->mass;
    }

    if (total_mass > 0.0) {
        result.weighted_position[0] /= total_mass;
        result.weighted_position[1] /= total_mass;
        result.weighted_position[2] /= total_mass;
    }

    snprintf(result.summary, sizeof(result.summary),
             "active=%u mass=%.3f E=%.3f",
             result.active_count, total_mass, result.energy);

    return result;
}

static Statistics scale_statistics(Statistics value, double factor)
{
    size_t i;

    value.energy *= factor;
    for (i = 0; i < 3; ++i) {
        value.weighted_position[i] *= factor;
        value.weighted_velocity[i] *= factor;
    }

    snprintf(value.summary, sizeof(value.summary),
             "scaled by %.3f", factor);
    return value;
}

static ItemArrays convert_items(const Item *items, size_t count)
{
    ItemArrays result = { 0 };
    size_t i;

    for (i = 0; i < count; ++i) {
        result.x[i] = items[i].position.x;
        result.y[i] = items[i].position.y;
        result.z[i] = items[i].position.z;
        result.vx[i] = items[i].velocity.x;
        result.vy[i] = items[i].velocity.y;
        result.vz[i] = items[i].velocity.z;
        result.mass[i] = items[i].mass;
        result.id[i] = items[i].id;
        result.category[i] = items[i].category;
    }

    result.count = (uint32_t)count;
    return result;
}

static void advance_items(ItemArrays *items, uint32_t steps)
{
    const float time_step = 0.1f;
    const float gravity = 9.8f;
    uint32_t step;
    uint32_t i;

    for (step = 0; step < steps; ++step) {
        for (i = 0; i < items->count; ++i) {
            items->x[i] += time_step * items->vx[i];
            items->y[i] += time_step * items->vy[i];
            items->z[i] += time_step * items->vz[i];
            items->vy[i] -= gravity / items->mass[i];
        }
    }
}

int main(void)
{
    Item items[ITEM_COUNT];
    Item sorted_items[ITEM_COUNT];
    Statistics statistics;
    Statistics scaled_statistics_value;
    ItemArrays arrays;
    size_t i;

    for (i = 0; i < ITEM_COUNT; ++i) {
        items[i].position.x = (float)i * 1.25f;
        items[i].position.y = (float)(i % 4u) * 2.0f;
        items[i].position.z = (float)((i * i) % 7u);

        items[i].velocity.x = 0.5f - (float)(i % 3u);
        items[i].velocity.y = (float)(i % 5u) * 0.25f;
        items[i].velocity.z = (float)(i % 2u) - 0.5f;

        items[i].mass = 1.0f + (float)(i % 6u) * 0.5f;
        items[i].charge = (i & 1u) ? 1.0f : -1.0f;
        items[i].id = (uint32_t)i + 500u;
        items[i].flags = (uint16_t)(i | 0x100u);
        items[i].category = (uint8_t)(i & 3u);
        items[i].active = i % 7u != 6u;
    }

    print_items(items, ITEM_COUNT, "items");

    statistics = calculate_statistics(items, ITEM_COUNT);
    print_statistics(&statistics, "base");

    scaled_statistics_value = scale_statistics(statistics, 1.5);
    print_statistics(&scaled_statistics_value, "scaled");

    arrays = convert_items(items, ITEM_COUNT);
    advance_items(&arrays, 3u);

    printf("%u\n", arrays.count);
    for (i = 0; i < arrays.count; i += 3u) {
        printf("%u %.3f %.3f %.3f %.3f %u\n",
               arrays.id[i],
               (double)arrays.x[i],
               (double)arrays.y[i],
               (double)arrays.z[i],
               (double)arrays.vy[i],
               (unsigned int)arrays.category[i]);
    }

    memcpy(sorted_items, items, sizeof(sorted_items));
    qsort(sorted_items, ITEM_COUNT, sizeof(sorted_items[0]), compare_items);
    print_items(sorted_items, ITEM_COUNT, "sorted");

    return 0;
}