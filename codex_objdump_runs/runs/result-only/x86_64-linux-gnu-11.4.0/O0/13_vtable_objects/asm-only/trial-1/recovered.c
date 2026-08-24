#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Object Object;
typedef struct VTable VTable;

struct VTable {
    const char *type_name;
    double (*primary_measure)(const Object *);
    double (*secondary_measure)(const Object *);
    void (*scale)(Object *, double);
    void (*describe)(const Object *, char *, size_t);
    void (*destroy)(Object *);
};

struct Object {
    const VTable *vtable;
    uint32_t id;
    char name[12];
};

typedef struct {
    Object base;
    double radius;
} RoundObject;

typedef struct {
    Object base;
    double width;
    double height;
} BoxObject;

typedef struct {
    double x;
    double y;
} Point;

typedef struct {
    Object base;
    uint32_t count;
    uint32_t padding;
    Point points[8];
} PolygonObject;

typedef struct {
    Object base;
    double width;
    double height;
    double extra;
} ExtendedBoxObject;

_Static_assert(sizeof(Object) == 24, "unexpected Object layout");
_Static_assert(offsetof(RoundObject, radius) == 24, "unexpected layout");
_Static_assert(offsetof(BoxObject, width) == 24, "unexpected layout");
_Static_assert(offsetof(BoxObject, height) == 32, "unexpected layout");
_Static_assert(offsetof(PolygonObject, count) == 24, "unexpected layout");
_Static_assert(offsetof(PolygonObject, points) == 32, "unexpected layout");
_Static_assert(sizeof(PolygonObject) == 160, "unexpected layout");
_Static_assert(offsetof(ExtendedBoxObject, extra) == 40, "unexpected layout");

static uint32_t next_id;

static double round_primary(const Object *object)
{
    const RoundObject *round_object = (const RoundObject *)object;
    double intermediate = round_object->radius * 3.14159265358979323846;
    return round_object->radius * intermediate;
}

static double round_secondary(const Object *object)
{
    const RoundObject *round_object = (const RoundObject *)object;
    return 6.28318530717958647692 * round_object->radius;
}

static void round_scale(Object *object, double factor)
{
    RoundObject *round_object = (RoundObject *)object;
    round_object->radius *= factor;
}

static void round_describe(const Object *object, char *buffer, size_t size)
{
    const RoundObject *round_object = (const RoundObject *)object;
    snprintf(buffer, size, "Circle(%.2f)", round_object->radius);
}

static double box_primary(const Object *object)
{
    const BoxObject *box = (const BoxObject *)object;
    return box->width * box->height;
}

static double box_secondary(const Object *object)
{
    const BoxObject *box = (const BoxObject *)object;
    return (box->width + box->height) * 2.0;
}

static void box_scale(Object *object, double factor)
{
    BoxObject *box = (BoxObject *)object;
    box->width *= factor;
    box->height *= factor;
}

static void box_describe(const Object *object, char *buffer, size_t size)
{
    const BoxObject *box = (const BoxObject *)object;
    snprintf(buffer, size, "Box(%.2f,%.2f)", box->width, box->height);
}

static double polygon_primary(const Object *object)
{
    const PolygonObject *polygon = (const PolygonObject *)object;
    double sum = 0.0;
    uint32_t i;

    for (i = 0; i < polygon->count; ++i) {
        uint32_t next = (i + 1U) % polygon->count;
        sum += polygon->points[i].x * polygon->points[next].y
             - polygon->points[next].x * polygon->points[i].y;
    }

    if (sum < 0.0)
        sum = -sum;

    return sum / 2.0;
}

static double polygon_secondary(const Object *object)
{
    const PolygonObject *polygon = (const PolygonObject *)object;
    double result = 0.0;
    uint32_t i;

    for (i = 0; i < polygon->count; ++i) {
        uint32_t next = (i + 1U) % polygon->count;
        double dx = polygon->points[next].x - polygon->points[i].x;
        double dy = polygon->points[next].y - polygon->points[i].y;
        double squared_distance = dx * dx + dy * dy;
        double estimate = squared_distance;
        int iteration = 0;

        while (iteration <= 23 && estimate > 0.0) {
            estimate = 0.5 * (squared_distance / estimate + estimate);
            ++iteration;
        }

        result += estimate;
    }

    return result;
}

static void polygon_scale(Object *object, double factor)
{
    PolygonObject *polygon = (PolygonObject *)object;
    uint32_t i;

    for (i = 0; i < polygon->count; ++i) {
        polygon->points[i].x *= factor;
        polygon->points[i].y *= factor;
    }
}

static void polygon_describe(const Object *object, char *buffer, size_t size)
{
    const PolygonObject *polygon = (const PolygonObject *)object;
    snprintf(buffer, size, "Polygon(%u)", polygon->count);
}

static double extended_primary(const Object *object)
{
    const ExtendedBoxObject *extended = (const ExtendedBoxObject *)object;
    double base_value = box_primary(object);
    return base_value - 0.5 * extended->extra * extended->extra;
}

static void extended_describe(const Object *object, char *buffer, size_t size)
{
    const ExtendedBoxObject *extended = (const ExtendedBoxObject *)object;
    snprintf(buffer, size, "Ext(%.2f,%.2f,%.2f)",
             extended->width, extended->height, extended->extra);
}

static void object_destroy(Object *object)
{
    free(object);
}

static const VTable round_vtable = {
    "round",
    round_primary,
    round_secondary,
    round_scale,
    round_describe,
    object_destroy
};

static const VTable box_vtable = {
    "box",
    box_primary,
    box_secondary,
    box_scale,
    box_describe,
    object_destroy
};

static const VTable polygon_vtable = {
    "polygon",
    polygon_primary,
    polygon_secondary,
    polygon_scale,
    polygon_describe,
    object_destroy
};

static const VTable extended_vtable = {
    "extended",
    extended_primary,
    box_secondary,
    box_scale,
    extended_describe,
    object_destroy
};

static Object *initialize_object(Object *object, const VTable *vtable,
                                 const char *name)
{
    object->vtable = vtable;
    object->id = next_id++;
    memset(object->name, 0, sizeof(object->name));
    strncpy(object->name, name, sizeof(object->name) - 1U);
    return object;
}

static RoundObject *new_round(double radius, const char *name)
{
    RoundObject *object = calloc(1, sizeof(*object));

    if (object == NULL)
        exit(1);

    object->radius = radius;
    initialize_object(&object->base, &round_vtable, name);
    return object;
}

static BoxObject *new_box(double width, double height, const char *name)
{
    BoxObject *object = calloc(1, sizeof(*object));

    if (object == NULL)
        exit(1);

    object->width = width;
    object->height = height;
    initialize_object(&object->base, &box_vtable, name);
    return object;
}

static ExtendedBoxObject *new_extended(double width, double height,
                                       double extra, const char *name)
{
    ExtendedBoxObject *object = calloc(1, sizeof(*object));

    if (object == NULL)
        exit(1);

    object->width = width;
    object->height = height;
    object->extra = extra;
    initialize_object(&object->base, &extended_vtable, name);
    return object;
}

static PolygonObject *new_polygon(const double *coordinates, uint32_t count,
                                  const char *name)
{
    PolygonObject *object = calloc(1, sizeof(*object));
    uint32_t i;

    if (object == NULL)
        exit(1);

    if (count > 8U)
        count = 8U;

    object->count = count;

    for (i = 0; i < count; ++i) {
        object->points[i].x = coordinates[i * 2U];
        object->points[i].y = coordinates[i * 2U + 1U];
    }

    initialize_object(&object->base, &polygon_vtable, name);
    return object;
}

static void report_objects(Object *const *objects, size_t count)
{
    double total = 0.0;
    size_t i;

    for (i = 0; i < count; ++i) {
        Object *object = objects[i];
        char description[64];
        size_t description_length;
        double secondary;
        double primary;

        object->vtable->describe(object, description, sizeof(description));
        description_length = strlen(description);
        secondary = object->vtable->secondary_measure(object);
        primary = object->vtable->primary_measure(object);

        printf("%u %s %.2f %.2f %zu\n",
               object->id, object->name, primary, secondary,
               description_length);

        total += object->vtable->primary_measure(object);
    }

    printf("%.2f\n", total);
}

int main(void)
{
    static const double first_polygon_points[8] = {
        0.0, 0.0,
        3.0, 0.0,
        0.0, 4.0,
        0.0, 0.0
    };
    static const double second_polygon_points[8] = {
        0.0, 0.0,
        2.0, 0.0,
        2.0, 2.0,
        0.0, 2.0
    };

    Object *objects[5];
    size_t i;

    objects[0] = (Object *)new_round(2.0, "c1");
    objects[1] = (Object *)new_box(3.0, 4.0, "r1");
    objects[2] = (Object *)new_polygon(first_polygon_points, 3U, "tri");
    objects[3] = (Object *)new_polygon(second_polygon_points, 4U, "quad");
    objects[4] = (Object *)new_extended(5.0, 4.0, 2.0, "t1");

    report_objects(objects, 5U);

    for (i = 0; i <= 4U; ++i)
        objects[i]->vtable->scale(objects[i], 1.5);

    report_objects(objects, 5U);

    for (i = 0; i <= 4U; ++i) {
        if (objects[i]->vtable == &extended_vtable) {
            ExtendedBoxObject *extended = (ExtendedBoxObject *)objects[i];
            printf("%u %.2f %.2f %.2f\n",
                   extended->base.id,
                   extended->width,
                   extended->height,
                   extended->extra);
        } else if (objects[i]->vtable == &polygon_vtable) {
            PolygonObject *polygon = (PolygonObject *)objects[i];
            uint32_t last = polygon->count - 1U;

            printf("%s %.2f %.2f %.2f %.2f\n",
                   polygon->base.name,
                   polygon->points[0].x,
                   polygon->points[0].y,
                   polygon->points[last].x,
                   polygon->points[last].y);
        }
    }

    for (i = 0; i <= 4U; ++i)
        objects[i]->vtable->destroy(objects[i]);

    return 0;
}