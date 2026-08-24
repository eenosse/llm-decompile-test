#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Object Object;
typedef struct Vtable Vtable;

struct Vtable {
    const void *reserved;
    double (*measure_a)(const void *);
    double (*measure_b)(const void *);
    void (*scale)(void *, double);
    int (*describe)(const void *, char *, size_t);
    void (*destroy)(void *);
};

struct Object {
    const Vtable *vtable;
    uint32_t id;
    char name[12];
};

typedef struct {
    Object base;
    double radius;
} Circle;

typedef struct {
    Object base;
    double width;
    double height;
} Rectangle;

typedef struct {
    Rectangle rectangle;
    double corner_radius;
} RoundedRectangle;

typedef struct {
    double x;
    double y;
} Point;

typedef struct {
    Object base;
    uint32_t count;
    uint32_t padding;
    Point points[8];
} Polygon;

_Static_assert(sizeof(Object) == 24, "unexpected Object layout");
_Static_assert(sizeof(Circle) == 32, "unexpected Circle layout");
_Static_assert(sizeof(Rectangle) == 40, "unexpected Rectangle layout");
_Static_assert(sizeof(RoundedRectangle) == 48,
               "unexpected RoundedRectangle layout");
_Static_assert(offsetof(Polygon, points) == 32, "unexpected Polygon layout");
_Static_assert(sizeof(Polygon) == 160, "unexpected Polygon layout");

static uint32_t next_id;

static const double pi_value = 3.14159265358979323846;
static const double two_pi_value = 6.28318530717958647692;
static const double half_value = 0.5;
static const double rounded_corner_loss =
    4.0 - 3.14159265358979323846;

static double circle_measure_a(const void *pointer)
{
    const Circle *object = pointer;
    return pi_value * object->radius * object->radius;
}

static double circle_measure_b(const void *pointer)
{
    const Circle *object = pointer;
    return two_pi_value * object->radius;
}

static void circle_scale(void *pointer, double factor)
{
    Circle *object = pointer;
    object->radius *= factor;
}

static double rectangle_measure_a(const void *pointer)
{
    const Rectangle *object = pointer;
    return object->width * object->height;
}

static double rectangle_measure_b(const void *pointer)
{
    const Rectangle *object = pointer;
    return 2.0 * (object->width + object->height);
}

static void rectangle_scale(void *pointer, double factor)
{
    Rectangle *object = pointer;
    object->width *= factor;
    object->height *= factor;
}

static double polygon_measure_a(const void *pointer)
{
    const Polygon *object = pointer;
    uint32_t count = object->count;
    double sum = 0.0;
    uint32_t i;

    if (count == 0)
        return 0.0;

    for (i = 0; i < count; ++i) {
        uint32_t next = (i + 1U) % count;
        sum += object->points[i].x * object->points[next].y
             - object->points[i].y * object->points[next].x;
    }

    if (sum < 0.0)
        sum = -sum;

    return sum * half_value;
}

static double approximate_square_root(double value)
{
    double estimate = value;
    unsigned iteration;

    for (iteration = 0; iteration < 24; ++iteration) {
        if (!(estimate > 0.0))
            break;
        estimate = (estimate + value / estimate) * half_value;
    }

    return estimate;
}

static double polygon_measure_b(const void *pointer)
{
    const Polygon *object = pointer;
    uint32_t count = object->count;
    double total = 0.0;
    uint32_t i;

    if (count == 0)
        return 0.0;

    for (i = 0; i < count; ++i) {
        uint32_t next = (i + 1U) % count;
        double dx = object->points[next].x - object->points[i].x;
        double dy = object->points[next].y - object->points[i].y;
        total += approximate_square_root(dx * dx + dy * dy);
    }

    return total;
}

static void polygon_scale(void *pointer, double factor)
{
    Polygon *object = pointer;
    uint32_t i;

    for (i = 0; i < object->count; ++i) {
        object->points[i].x *= factor;
        object->points[i].y *= factor;
    }
}

static double rounded_rectangle_measure_a(const void *pointer)
{
    const RoundedRectangle *object = pointer;
    double radius = object->corner_radius;

    return object->rectangle.width * object->rectangle.height
         - rounded_corner_loss * radius * radius;
}

static int circle_describe(const void *pointer, char *buffer, size_t size)
{
    const Circle *object = pointer;
    return snprintf(buffer, size, "Circle r=%.2f", object->radius);
}

static int rectangle_describe(const void *pointer, char *buffer, size_t size)
{
    const Rectangle *object = pointer;
    return snprintf(buffer, size, "Rect %.2fx%.2f",
                    object->width, object->height);
}

static int polygon_describe(const void *pointer, char *buffer, size_t size)
{
    const Polygon *object = pointer;
    return snprintf(buffer, size, "Polygon n=%u", object->count);
}

static int rounded_rectangle_describe(const void *pointer,
                                      char *buffer,
                                      size_t size)
{
    const RoundedRectangle *object = pointer;
    return snprintf(buffer, size, "RRect %.2f %.2f %.2f",
                    object->rectangle.width,
                    object->rectangle.height,
                    object->corner_radius);
}

static void object_destroy(void *pointer)
{
    free(pointer);
}

static const Vtable rounded_rectangle_vtable = {
    NULL,
    rounded_rectangle_measure_a,
    rectangle_measure_b,
    rectangle_scale,
    rounded_rectangle_describe,
    object_destroy
};

static const Vtable polygon_vtable = {
    NULL,
    polygon_measure_a,
    polygon_measure_b,
    polygon_scale,
    polygon_describe,
    object_destroy
};

static const Vtable rectangle_vtable = {
    NULL,
    rectangle_measure_a,
    rectangle_measure_b,
    rectangle_scale,
    rectangle_describe,
    object_destroy
};

static const Vtable circle_vtable = {
    NULL,
    circle_measure_a,
    circle_measure_b,
    circle_scale,
    circle_describe,
    object_destroy
};

static void object_initialize(Object *object,
                              const Vtable *vtable,
                              const char *name)
{
    object->vtable = vtable;
    object->id = next_id++;
    memset(object->name, 0, sizeof(object->name));
    strncpy(object->name, name, sizeof(object->name) - 1U);
}

static Polygon *polygon_create(const Point *points,
                               uint32_t count,
                               const char *name)
{
    Polygon *object = calloc(1, sizeof(*object));

    if (object == NULL)
        exit(1);

    object->count = count;
    object->points[0] = points[0];
    object->points[1] = points[1];
    object->points[2] = points[2];

    if (count == 4)
        object->points[3] = points[3];

    object_initialize(&object->base, &polygon_vtable, name);
    return object;
}

static void report_objects(Object *const objects[5])
{
    double total = 0.0;
    size_t i;

    for (i = 0; i < 5; ++i) {
        Object *object = objects[i];
        const Vtable *vtable = object->vtable;
        char description[64];
        size_t description_length;
        double first;
        double second;

        vtable->describe(object, description, sizeof(description));
        description_length = strlen(description);
        second = vtable->measure_b(object);
        first = vtable->measure_a(object);

        printf("%u %s %.2f %.2f %zu\n",
               object->id,
               object->name,
               first,
               second,
               description_length);

        total += vtable->measure_a(object);
    }

    printf("%.2f\n", total);
}

int main(void)
{
    static const Point quadrilateral_points[4] = {
        { 0.0, 0.0 },
        { 4.0, 0.0 },
        { 4.0, 3.0 },
        { 0.0, 3.0 }
    };
    static const Point triangle_points[3] = {
        { 0.0, 0.0 },
        { 4.0, 0.0 },
        { 0.0, 3.0 }
    };
    static const char rounded_name[] = "rr1";
    static const double scale_factor = 1.5;

    Circle *circle;
    Rectangle *rectangle;
    Polygon *triangle;
    Polygon *quadrilateral;
    RoundedRectangle *rounded;
    Object *objects[5];
    size_t i;

    circle = calloc(1, sizeof(*circle));
    if (circle == NULL)
        exit(1);
    circle->radius = 2.5;
    object_initialize(&circle->base, &circle_vtable, "c1");
    objects[0] = &circle->base;

    rectangle = calloc(1, sizeof(*rectangle));
    if (rectangle == NULL)
        exit(1);
    rectangle->width = 3.0;
    rectangle->height = 4.0;
    object_initialize(&rectangle->base, &rectangle_vtable,
                      rounded_name + 1);
    objects[1] = &rectangle->base;

    triangle = polygon_create(triangle_points, 3, "tri");
    objects[2] = &triangle->base;

    quadrilateral = polygon_create(quadrilateral_points, 4, "quad");
    objects[3] = &quadrilateral->base;

    rounded = calloc(1, sizeof(*rounded));
    if (rounded == NULL)
        exit(1);
    rounded->rectangle.width = 6.0;
    rounded->rectangle.height = 4.0;
    rounded->corner_radius = 1.0;
    object_initialize(&rounded->rectangle.base,
                      &rounded_rectangle_vtable,
                      rounded_name);
    objects[4] = &rounded->rectangle.base;

    report_objects(objects);

    for (i = 0; i < 5; ++i)
        objects[i]->vtable->scale(objects[i], scale_factor);

    report_objects(objects);

    for (i = 0; i < 5; ++i) {
        Object *object = objects[i];

        if (object->vtable == &rounded_rectangle_vtable) {
            const RoundedRectangle *value = (const void *)object;
            printf("%u %.2f %.2f %.2f\n",
                   object->id,
                   value->rectangle.width,
                   value->rectangle.height,
                   value->corner_radius);
        } else if (object->vtable == &polygon_vtable) {
            const Polygon *value = (const void *)object;
            const Point *first = &value->points[0];
            const Point *last = &value->points[value->count - 1U];

            printf("%s %.2f %.2f %.2f %.2f\n",
                   object->name,
                   first->x,
                   first->y,
                   last->x,
                   last->y);
        }
    }

    for (i = 0; i < 5; ++i)
        objects[i]->vtable->destroy(objects[i]);

    return 0;
}