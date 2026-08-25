#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct object object;
typedef struct object_vtable object_vtable;

struct object_vtable {
    const char *type_name;
    double (*metric_a)(object *);
    double (*metric_b)(object *);
    void (*scale)(object *, double);
    int (*describe)(object *, char *, size_t);
    void (*destroy)(object *);
};

struct object {
    const object_vtable *vtable;
    uint32_t id;
    char name[12];
};

typedef struct {
    object base;
    double value;
} single_object;

typedef struct {
    object base;
    double first;
    double second;
} pair_object;

typedef struct {
    object base;
    uint32_t count;
    uint32_t padding;
    struct {
        double x;
        double y;
    } points[8];
} point_object;

typedef struct {
    object base;
    double first;
    double second;
    double third;
} triple_object;

static uint32_t next_id;

static double single_metric_a(object *base)
{
    single_object *self = (single_object *)base;
    return 3.14159265358979323846 * self->value * self->value;
}

static double single_metric_b(object *base)
{
    single_object *self = (single_object *)base;
    return 6.28318530717958647692 * self->value;
}

static void single_scale(object *base, double factor)
{
    single_object *self = (single_object *)base;
    self->value *= factor;
}

static double pair_metric_a(object *base)
{
    pair_object *self = (pair_object *)base;
    return self->first * self->second;
}

static double pair_metric_b(object *base)
{
    pair_object *self = (pair_object *)base;
    return 2.0 * (self->first + self->second);
}

static void pair_scale(object *base, double factor)
{
    pair_object *self = (pair_object *)base;
    self->first *= factor;
    self->second *= factor;
}

static double point_metric_a(object *base)
{
    point_object *self = (point_object *)base;
    uint32_t count = self->count;
    uint32_t i;
    double sum = 0.0;

    if (count == 0)
        return 0.0;

    for (i = 0; i < count; ++i) {
        uint32_t next = (i + 1U) % count;
        sum += self->points[i].x * self->points[next].y
             - self->points[next].x * self->points[i].y;
    }

    if (sum < 0.0)
        sum = -sum;

    return sum * 0.5;
}

static double point_metric_b(object *base)
{
    point_object *self = (point_object *)base;
    uint32_t count = self->count;
    uint32_t i;
    double total = 0.0;

    if (count == 0)
        return 0.0;

    for (i = 0; i < count; ++i) {
        uint32_t next = (i + 1U) % count;
        double dx = self->points[next].x - self->points[i].x;
        double dy = self->points[next].y - self->points[i].y;
        double square = dx * dx + dy * dy;
        double root = square;
        int iterations = 24;

        while (root > 0.0 && iterations != 0) {
            root = 0.5 * (root + square / root);
            --iterations;
        }

        total += root;
    }

    return total;
}

static void point_scale(object *base, double factor)
{
    point_object *self = (point_object *)base;
    uint32_t i;

    for (i = 0; i < self->count; ++i) {
        self->points[i].x *= factor;
        self->points[i].y *= factor;
    }
}

static double triple_metric_a(object *base)
{
    triple_object *self = (triple_object *)base;
    return self->first * self->second - 3.14 * self->third * self->third;
}

static void object_destroy(object *self)
{
    free(self);
}

static int single_describe(object *base, char *buffer, size_t size)
{
    single_object *self = (single_object *)base;
    return snprintf(buffer, size, "single %.2f", self->value);
}

static int pair_describe(object *base, char *buffer, size_t size)
{
    pair_object *self = (pair_object *)base;
    return snprintf(buffer, size, "pair %.2f %.2f",
                    self->first, self->second);
}

static int point_describe(object *base, char *buffer, size_t size)
{
    point_object *self = (point_object *)base;
    return snprintf(buffer, size, "points %u", self->count);
}

static int triple_describe(object *base, char *buffer, size_t size)
{
    triple_object *self = (triple_object *)base;
    return snprintf(buffer, size, "triple %.2f %.2f %.2f",
                    self->first, self->second, self->third);
}

static const object_vtable single_vtable = {
    "single",
    single_metric_a,
    single_metric_b,
    single_scale,
    single_describe,
    object_destroy
};

static const object_vtable pair_vtable = {
    "pair",
    pair_metric_a,
    pair_metric_b,
    pair_scale,
    pair_describe,
    object_destroy
};

static const object_vtable point_vtable = {
    "points",
    point_metric_a,
    point_metric_b,
    point_scale,
    point_describe,
    object_destroy
};

static const object_vtable triple_vtable = {
    "triple",
    triple_metric_a,
    pair_metric_b,
    pair_scale,
    triple_describe,
    object_destroy
};

static object *initialize_object(object *self, const object_vtable *vtable,
                                 const char *name)
{
    self->vtable = vtable;
    self->id = next_id++;
    memset(self->name, 0, sizeof self->name);
    strncpy(self->name, name, 11);
    return self;
}

static point_object *make_point_object(const double (*points)[2],
                                       uint32_t count, const char *name)
{
    point_object *self;
    uint32_t i;

    self = calloc(1, sizeof *self);
    if (self == NULL)
        exit(1);

    self->count = count;
    for (i = 0; i < count; ++i) {
        self->points[i].x = points[i][0];
        self->points[i].y = points[i][1];
    }

    initialize_object(&self->base, &point_vtable, name);
    return self;
}

static void report_objects(object *const objects[5])
{
    double total = 0.0;
    size_t i;

    for (i = 0; i < 5; ++i) {
        object *current = objects[i];
        char description[64];
        size_t description_length;
        double metric_b;
        double metric_a;

        current->vtable->describe(current, description, sizeof description);
        description_length = strlen(description);
        metric_b = current->vtable->metric_b(current);
        metric_a = current->vtable->metric_a(current);

        printf("%u %s %.2f %.2f %zu\n",
               current->id, current->name,
               metric_a, metric_b, description_length);

        total += current->vtable->metric_a(current);
    }

    printf("%.2f\n", total);
}

int main(void)
{
    static const double triangle_points[3][2] = {
        {0.0, 0.0},
        {3.0, 0.0},
        {0.0, 4.0}
    };
    static const double quadrilateral_points[4][2] = {
        {0.0, 0.0},
        {4.0, 0.0},
        {4.0, 3.0},
        {0.0, 3.0}
    };

    single_object *single;
    pair_object *pair;
    point_object *triangle;
    point_object *quadrilateral;
    triple_object *triple;
    object *objects[5];
    size_t i;
    const double factor = 1.5;

    single = calloc(1, sizeof *single);
    if (single == NULL)
        exit(1);
    single->value = 2.0;
    initialize_object(&single->base, &single_vtable, "s");

    pair = calloc(1, sizeof *pair);
    if (pair == NULL)
        exit(1);
    pair->first = 3.0;
    pair->second = 4.0;
    initialize_object(&pair->base, &pair_vtable, "pair");

    triangle = make_point_object(triangle_points, 3, "tri");
    quadrilateral = make_point_object(quadrilateral_points, 4, "quad");

    triple = calloc(1, sizeof *triple);
    if (triple == NULL)
        exit(1);
    triple->first = 10.0;
    triple->second = 4.0;
    triple->third = 1.0;
    initialize_object(&triple->base, &triple_vtable, "triple");

    objects[0] = &single->base;
    objects[1] = &pair->base;
    objects[2] = &triangle->base;
    objects[3] = &quadrilateral->base;
    objects[4] = &triple->base;

    report_objects(objects);

    for (i = 0; i < 5; ++i)
        objects[i]->vtable->scale(objects[i], factor);

    report_objects(objects);

    for (i = 0; i < 5; ++i) {
        object *current = objects[i];

        if (current->vtable == &triple_vtable) {
            triple_object *value = (triple_object *)current;
            printf("%u %.2f %.2f %.2f\n",
                   value->base.id,
                   value->first, value->second, value->third);
        } else if (current->vtable == &point_vtable) {
            point_object *value = (point_object *)current;
            uint32_t last = value->count - 1U;

            printf("%u %s %.2f %.2f %.2f %.2f\n",
                   value->base.id, value->base.name,
                   value->points[0].x, value->points[0].y,
                   value->points[last].x, value->points[last].y);
        }
    }

    for (i = 0; i < 5; ++i)
        objects[i]->vtable->destroy(objects[i]);

    return 0;
}