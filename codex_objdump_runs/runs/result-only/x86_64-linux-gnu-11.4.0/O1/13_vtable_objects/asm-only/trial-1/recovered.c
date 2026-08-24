#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Shape Shape;
typedef struct ShapeVTable ShapeVTable;

struct ShapeVTable {
    const char *kind;
    double (*area)(Shape *);
    double (*perimeter)(Shape *);
    void (*scale)(Shape *, double);
    void (*describe)(Shape *, char *, size_t);
    void (*destroy)(Shape *);
};

struct Shape {
    const ShapeVTable *vtable;
    uint32_t id;
    char name[12];
};

typedef struct {
    Shape base;
    double radius;
} Circle;

typedef struct {
    Shape base;
    double width;
    double height;
} Rectangle;

typedef struct {
    double x;
    double y;
} Point;

typedef struct {
    Shape base;
    uint32_t count;
    uint32_t padding;
    Point points[8];
} Polygon;

typedef struct {
    Shape base;
    double width;
    double height;
    double radius;
} CutoutRectangle;

_Static_assert(sizeof(Shape) == 24, "unexpected Shape layout");
_Static_assert(offsetof(Shape, name) == 12, "unexpected Shape layout");
_Static_assert(offsetof(Circle, radius) == 24, "unexpected Circle layout");
_Static_assert(offsetof(Rectangle, width) == 24, "unexpected Rectangle layout");
_Static_assert(offsetof(Polygon, count) == 24, "unexpected Polygon layout");
_Static_assert(offsetof(Polygon, points) == 32, "unexpected Polygon layout");
_Static_assert(sizeof(Polygon) == 160, "unexpected Polygon size");
_Static_assert(offsetof(CutoutRectangle, radius) == 40,
               "unexpected CutoutRectangle layout");

static uint32_t next_id;

static const char name_pool[16] = {
    'C', '1', '\0',
    'P', '3', '\0', '\0',
    'P', '4', '\0', '\0', '\0',
    'C', 'R', '1', '\0'
};

static double circle_area(Shape *shape)
{
    Circle *circle = (Circle *)shape;
    return 3.14159265358979323846 * circle->radius * circle->radius;
}

static double circle_perimeter(Shape *shape)
{
    Circle *circle = (Circle *)shape;
    return 6.28318530717958647692 * circle->radius;
}

static void circle_scale(Shape *shape, double factor)
{
    Circle *circle = (Circle *)shape;
    circle->radius *= factor;
}

static double rectangle_area(Shape *shape)
{
    Rectangle *rectangle = (Rectangle *)shape;
    return rectangle->width * rectangle->height;
}

static double rectangle_perimeter(Shape *shape)
{
    Rectangle *rectangle = (Rectangle *)shape;
    return 2.0 * (rectangle->width + rectangle->height);
}

static void rectangle_scale(Shape *shape, double factor)
{
    Rectangle *rectangle = (Rectangle *)shape;
    rectangle->width *= factor;
    rectangle->height *= factor;
}

static double polygon_area(Shape *shape)
{
    Polygon *polygon = (Polygon *)shape;
    uint32_t count = polygon->count;
    double sum = 0.0;

    if (count != 0) {
        uint32_t i;

        for (i = 0; i < count; ++i) {
            uint32_t next = (i + 1U) % count;
            sum += polygon->points[i].x * polygon->points[next].y
                 - polygon->points[next].x * polygon->points[i].y;
        }
    }

    if (sum < 0.0)
        sum = -sum;

    return sum * 0.5;
}

static double polygon_perimeter(Shape *shape)
{
    Polygon *polygon = (Polygon *)shape;
    uint32_t count = polygon->count;
    double total = 0.0;

    if (count != 0) {
        uint32_t i;

        for (i = 0; i < count; ++i) {
            uint32_t next = (i + 1U) % count;
            double dx = polygon->points[next].x - polygon->points[i].x;
            double dy = polygon->points[next].y - polygon->points[i].y;
            double square = dx * dx + dy * dy;
            double root = square;

            if (square > 0.0) {
                unsigned iteration;

                for (iteration = 0; iteration < 24; ++iteration)
                    root = (root + square / root) * 0.5;
            }

            total += root;
        }
    }

    return total;
}

static void polygon_scale(Shape *shape, double factor)
{
    Polygon *polygon = (Polygon *)shape;
    uint32_t i;

    for (i = 0; i < polygon->count; ++i) {
        polygon->points[i].x *= factor;
        polygon->points[i].y *= factor;
    }
}

static double cutout_area(Shape *shape)
{
    CutoutRectangle *cutout = (CutoutRectangle *)shape;
    return cutout->width * cutout->height
         - 3.14159265358979323846 * cutout->radius * cutout->radius;
}

static void circle_describe(Shape *shape, char *buffer, size_t size)
{
    Circle *circle = (Circle *)shape;
    snprintf(buffer, size, "Circle(%.2f)", circle->radius);
}

static void rectangle_describe(Shape *shape, char *buffer, size_t size)
{
    Rectangle *rectangle = (Rectangle *)shape;
    snprintf(buffer, size, "R(%.2f,%.2f)",
             rectangle->width, rectangle->height);
}

static void polygon_describe(Shape *shape, char *buffer, size_t size)
{
    Polygon *polygon = (Polygon *)shape;
    snprintf(buffer, size, "Polygon(%u)", polygon->count);
}

static void cutout_describe(Shape *shape, char *buffer, size_t size)
{
    CutoutRectangle *cutout = (CutoutRectangle *)shape;
    snprintf(buffer, size, "Cutout(%.2f,%.2f,%.2f)",
             cutout->width, cutout->height, cutout->radius);
}

static void shape_destroy(Shape *shape)
{
    free(shape);
}

static const ShapeVTable circle_vtable = {
    "circle",
    circle_area,
    circle_perimeter,
    circle_scale,
    circle_describe,
    shape_destroy
};

static const ShapeVTable rectangle_vtable = {
    "rectangle",
    rectangle_area,
    rectangle_perimeter,
    rectangle_scale,
    rectangle_describe,
    shape_destroy
};

static const ShapeVTable polygon_vtable = {
    "polygon",
    polygon_area,
    polygon_perimeter,
    polygon_scale,
    polygon_describe,
    shape_destroy
};

static const ShapeVTable cutout_vtable = {
    "cutout",
    cutout_area,
    rectangle_perimeter,
    rectangle_scale,
    cutout_describe,
    shape_destroy
};

static Shape *shape_initialize(Shape *shape, const ShapeVTable *vtable,
                               const char *name)
{
    shape->vtable = vtable;
    shape->id = next_id++;
    memset(shape->name, 0, sizeof(shape->name));
    strncpy(shape->name, name, 11);
    return shape;
}

static Circle *new_circle(double radius, const char *name)
{
    Circle *circle = calloc(1, sizeof(*circle));

    if (circle == NULL)
        exit(1);

    circle->radius = radius;
    shape_initialize(&circle->base, &circle_vtable, name);
    return circle;
}

static Rectangle *new_rectangle(double width, double height, const char *name)
{
    Rectangle *rectangle = calloc(1, sizeof(*rectangle));

    if (rectangle == NULL)
        exit(1);

    rectangle->width = width;
    rectangle->height = height;
    shape_initialize(&rectangle->base, &rectangle_vtable, name);
    return rectangle;
}

static Polygon *new_polygon(const Point *points, uint32_t count,
                            const char *name)
{
    Polygon *polygon = calloc(1, sizeof(*polygon));
    uint32_t stored_count;
    uint32_t i;

    if (polygon == NULL)
        exit(1);

    stored_count = count <= 8U ? count : 8U;
    polygon->count = stored_count;

    for (i = 0; i < stored_count; ++i)
        polygon->points[i] = points[i];

    shape_initialize(&polygon->base, &polygon_vtable, name);
    return polygon;
}

static CutoutRectangle *new_cutout(double width, double height, double radius,
                                   const char *name)
{
    CutoutRectangle *cutout = calloc(1, sizeof(*cutout));

    if (cutout == NULL)
        exit(1);

    cutout->width = width;
    cutout->height = height;
    cutout->radius = radius;
    shape_initialize(&cutout->base, &cutout_vtable, name);
    return cutout;
}

static void print_shapes(Shape *const *shapes, size_t count)
{
    double total = 0.0;
    size_t i;

    for (i = 0; i < count; ++i) {
        Shape *shape = shapes[i];
        char description[64];
        size_t description_length;
        double area;
        double perimeter;

        shape->vtable->describe(shape, description, sizeof(description));
        description_length = strlen(description);
        area = shape->vtable->area(shape);
        perimeter = shape->vtable->perimeter(shape);

        printf("%u %s %.2f %.2f %zu\n",
               shape->id, shape->name, perimeter, area,
               description_length);

        total += shape->vtable->area(shape);
    }

    printf("%.2f\n", total);
}

int main(void)
{
    static const Point triangle_points[3] = {
        { 0.0, 0.0 },
        { 3.0, 0.0 },
        { 0.0, 4.0 }
    };
    static const Point quadrilateral_points[4] = {
        { 0.0, 0.0 },
        { 4.0, 0.0 },
        { 4.0, 3.0 },
        { 0.0, 3.0 }
    };

    Shape *shapes[5];
    size_t i;
    const double scale_factor = 1.5;

    shapes[0] = &new_circle(2.0, name_pool)->base;
    shapes[1] = &new_rectangle(3.0, 4.0, name_pool + 13)->base;
    shapes[2] = &new_polygon(triangle_points, 3, name_pool + 3)->base;
    shapes[3] = &new_polygon(quadrilateral_points, 4, name_pool + 7)->base;
    shapes[4] = &new_cutout(10.0, 4.0, 1.0, name_pool + 12)->base;

    print_shapes(shapes, 5);

    for (i = 0; i < 5; ++i)
        shapes[i]->vtable->scale(shapes[i], scale_factor);

    print_shapes(shapes, 5);

    for (i = 0; i < 5; ++i) {
        Shape *shape = shapes[i];

        if (shape->vtable == &cutout_vtable) {
            CutoutRectangle *cutout = (CutoutRectangle *)shape;
            printf("%u %.2f %.2f %.2f\n",
                   shape->id, cutout->width, cutout->height, cutout->radius);
        } else if (shape->vtable == &polygon_vtable) {
            Polygon *polygon = (Polygon *)shape;
            Point first = polygon->points[0];
            Point last = polygon->points[polygon->count - 1U];

            printf("%s %.2f %.2f %.2f %.2f\n",
                   shape->name, first.x, first.y, last.x, last.y);
        }
    }

    for (i = 0; i < 5; ++i)
        shapes[i]->vtable->destroy(shapes[i]);

    return 0;
}