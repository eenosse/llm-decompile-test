#include <float.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { ITEM_CAPACITY = 12 };

typedef struct {
    float position[3];
    float velocity[3];
    float mass;
    float charge;
    uint32_t identifier;
    uint16_t flags;
    uint8_t category;
    uint8_t active;
} Item;

typedef struct {
    float position[3][ITEM_CAPACITY];
    float velocity[3][ITEM_CAPACITY];
    float mass[ITEM_CAPACITY];
    uint32_t identifier[ITEM_CAPACITY];
    uint8_t category[ITEM_CAPACITY];
    uint32_t count;
} ItemArrays;

typedef struct {
    double energy;
    double center[3];
    double momentum[3];
    float minimum_mass;
    float maximum_mass;
    uint32_t active_count;
    uint32_t category_count[4];
    char summary[48];
} Statistics;

static void initialize_items(Item *items, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        Item *item = &items[i];

        item->identifier = (uint32_t)(i + 500U);
        item->position[0] = (float)i * 1.25f;

        {
            float value = (float)(i & 3U);
            item->position[1] = value + value;
        }

        item->position[2] = (float)((i * i) % 7U);
        item->velocity[0] = 1.0f - (float)(i % 3U);
        item->velocity[1] = (float)(i % 5U) * 0.25f;
        item->velocity[2] = (float)(i & 1U) - 0.5f;
        item->mass = 1.0f + 1.0f * (float)(i % 6U);
        item->charge = (i & 1U) ? 1.0f : -1.0f;
        item->flags = (uint16_t)(i | 0x0100U);
        item->category = (uint8_t)(i & 3U);
        item->active = (uint8_t)((i % 7U) != 6U);
    }
}

static void pack_items(const Item *items, size_t count, ItemArrays *arrays)
{
    memset(arrays, 0, sizeof(*arrays));

    for (size_t i = 0; i < count && i < ITEM_CAPACITY; ++i) {
        arrays->position[0][i] = items[i].position[0];
        arrays->position[1][i] = items[i].position[1];
        arrays->position[2][i] = items[i].position[2];

        arrays->velocity[0][i] = items[i].velocity[0];
        arrays->velocity[1][i] = items[i].velocity[1];
        arrays->velocity[2][i] = items[i].velocity[2];

        arrays->mass[i] = items[i].mass;
        arrays->identifier[i] = items[i].identifier;
        arrays->category[i] = items[i].category;
        ++arrays->count;
    }
}

static void advance_items(ItemArrays *arrays, float time_step)
{
    for (uint32_t i = 0; i < arrays->count; ++i) {
        arrays->position[0][i] += arrays->velocity[0][i] * time_step;
        arrays->position[1][i] += arrays->velocity[1][i] * time_step;
        arrays->position[2][i] += arrays->velocity[2][i] * time_step;
        arrays->velocity[1][i] -= 9.81f * time_step / arrays->mass[i];
    }
}

static Statistics compute_statistics(const Item *items, size_t count)
{
    Statistics result;
    double total_mass = 0.0;

    memset(&result, 0, sizeof(result));
    result.minimum_mass = FLT_MAX;
    result.maximum_mass = -FLT_MAX;

    for (size_t i = 0; i < count; ++i) {
        const Item *item = &items[i];
        double speed_squared =
            (double)item->velocity[0] * (double)item->velocity[0] +
            (double)item->velocity[1] * (double)item->velocity[1] +
            (double)item->velocity[2] * (double)item->velocity[2];

        if (!item->active)
            continue;

        ++result.active_count;
        ++result.category_count[item->category & 3U];

        result.energy += 0.5 * (double)item->mass * speed_squared;
        total_mass += (double)item->mass;

        result.center[0] += (double)item->mass * (double)item->position[0];
        result.center[1] += (double)item->mass * (double)item->position[1];
        result.center[2] += (double)item->mass * (double)item->position[2];

        result.momentum[0] +=
            (double)item->mass * (double)item->velocity[0];
        result.momentum[1] +=
            (double)item->mass * (double)item->velocity[1];
        result.momentum[2] +=
            (double)item->mass * (double)item->velocity[2];

        if (item->mass < result.minimum_mass)
            result.minimum_mass = item->mass;
        if (item->mass > result.maximum_mass)
            result.maximum_mass = item->mass;
    }

    if (total_mass > 0.0) {
        result.center[0] /= total_mass;
        result.center[1] /= total_mass;
        result.center[2] /= total_mass;
    }

    snprintf(result.summary, sizeof(result.summary),
             "active=%u mass=%.3f E=%.3f",
             result.active_count, total_mass, result.energy);

    return result;
}

static Statistics scale_statistics(Statistics value, double factor)
{
    value.energy *= factor;

    for (int i = 0; i < 3; ++i) {
        value.center[i] *= factor;
        value.momentum[i] *= factor;
    }

    snprintf(value.summary, sizeof(value.summary),
             "scaled %.2f: E=%.3f", factor, value.energy);

    return value;
}

static int compare_items(const void *left_pointer, const void *right_pointer)
{
    const Item *left = left_pointer;
    const Item *right = right_pointer;

    if (right->mass > left->mass)
        return 1;
    if (left->mass > right->mass)
        return -1;

    if (left->identifier < right->identifier)
        return -1;
    return left->identifier > right->identifier;
}

static void print_items(const Item *items, size_t count, const char *unused_label)
{
    (void)unused_label;

    for (size_t i = 0; i < count; ++i) {
        printf("%u %u %.3f %.3f %.3f %.3f %.3f %u %u\n",
               items[i].identifier,
               (unsigned)items[i].category,
               (double)items[i].mass,
               (double)items[i].charge,
               (double)items[i].position[0],
               (double)items[i].position[1],
               (double)items[i].position[2],
               (unsigned)items[i].flags,
               (unsigned)items[i].active);
    }
}

static void print_statistics(const Statistics *value, const char *unused_label)
{
    (void)unused_label;

    printf("%.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
           value->energy,
           value->center[0],
           value->center[1],
           value->center[2],
           value->momentum[0],
           value->momentum[1],
           value->momentum[2]);

    printf("%.3f %.3f %u %u %u %u %u\n",
           (double)value->minimum_mass,
           (double)value->maximum_mass,
           value->active_count,
           value->category_count[0],
           value->category_count[1],
           value->category_count[2],
           value->category_count[3]);
}

int main(void)
{
    Item items[ITEM_CAPACITY];
    Item sorted_items[ITEM_CAPACITY];
    ItemArrays arrays;

    initialize_items(items, ITEM_CAPACITY);
    print_items(items, ITEM_CAPACITY, "initial");

    Statistics statistics = compute_statistics(items, ITEM_CAPACITY);
    print_statistics(&statistics, "base");

    Statistics scaled = scale_statistics(statistics, 2.0);
    print_statistics(&scaled, "scaled");

    pack_items(items, ITEM_CAPACITY, &arrays);

    for (int step = 0; step <= 2; ++step)
        advance_items(&arrays, 0.016f);

    printf("%u\n", arrays.count);

    for (uint32_t i = 0; i < arrays.count; i += 3U) {
        printf("%u %.3f %.3f %.3f %.3f %u\n",
               arrays.identifier[i],
               (double)arrays.position[0][i],
               (double)arrays.position[1][i],
               (double)arrays.position[2][i],
               (double)arrays.velocity[1][i],
               (unsigned)arrays.category[i]);
    }

    memcpy(sorted_items, items, sizeof(sorted_items));
    qsort(sorted_items, ITEM_CAPACITY, sizeof(sorted_items[0]), compare_items);
    print_items(sorted_items, ITEM_CAPACITY, "sorted");

    return 0;
}