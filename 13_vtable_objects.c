/*
 * 13_vtable_objects.c
 * Target feature: hand-rolled C++-style objects - a struct of function
 * pointers (vtable) shared by instances, "inheritance" by embedding the base
 * struct as the first member, and upcasts/downcasts done with casts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef struct Shape Shape;

typedef struct ShapeVTable {
    const char *type_name;
    double (*area)(const Shape *);
    double (*perimeter)(const Shape *);
    void   (*scale)(Shape *, double);
    void   (*describe)(const Shape *, char *, size_t);
    void   (*destroy)(Shape *);
} ShapeVTable;

/* Base class. */
struct Shape {
    const ShapeVTable *vt;
    uint32_t           id;
    char               tag[12];
};

/* Derived: base embedded first. */
typedef struct Circle {
    Shape  base;
    double radius;
} Circle;

typedef struct Rect {
    Shape  base;
    double width;
    double height;
} Rect;

typedef struct Polygon {
    Shape    base;
    uint32_t n;
    struct { double x, y; } pts[8];
} Polygon;

/* Second-level derivation. */
typedef struct RoundedRect {
    Rect   rect;      /* which itself starts with Shape */
    double corner_r;
} RoundedRect;

static double circle_area(const Shape *s)      { return 3.14159265358979 * ((const Circle *)s)->radius * ((const Circle *)s)->radius; }
static double circle_perim(const Shape *s)     { return 2.0 * 3.14159265358979 * ((const Circle *)s)->radius; }
static void   circle_scale(Shape *s, double k) { ((Circle *)s)->radius *= k; }
static void   circle_describe(const Shape *s, char *buf, size_t n)
{
    snprintf(buf, n, "circle r=%.2f", ((const Circle *)s)->radius);
}

static double rect_area(const Shape *s)
{
    const Rect *r = (const Rect *)s;
    return r->width * r->height;
}
static double rect_perim(const Shape *s)
{
    const Rect *r = (const Rect *)s;
    return 2.0 * (r->width + r->height);
}
static void rect_scale(Shape *s, double k)
{
    Rect *r = (Rect *)s;
    r->width  *= k;
    r->height *= k;
}
static void rect_describe(const Shape *s, char *buf, size_t n)
{
    const Rect *r = (const Rect *)s;
    snprintf(buf, n, "rect %.2fx%.2f", r->width, r->height);
}

static double poly_area(const Shape *s)
{
    const Polygon *p = (const Polygon *)s;
    double acc = 0.0;
    uint32_t i;

    for (i = 0; i < p->n; i++) {
        uint32_t j = (i + 1) % p->n;
        acc += p->pts[i].x * p->pts[j].y - p->pts[j].x * p->pts[i].y;
    }
    return acc < 0 ? -acc / 2.0 : acc / 2.0;
}
static double poly_perim(const Shape *s)
{
    const Polygon *p = (const Polygon *)s;
    double acc = 0.0;
    uint32_t i;

    for (i = 0; i < p->n; i++) {
        uint32_t j = (i + 1) % p->n;
        double dx = p->pts[j].x - p->pts[i].x;
        double dy = p->pts[j].y - p->pts[i].y;
        double d2 = dx * dx + dy * dy;
        double r = d2;
        int k;
        for (k = 0; k < 24 && r > 0.0; k++)
            r = 0.5 * (r + d2 / r);
        acc += r;
    }
    return acc;
}
static void poly_scale(Shape *s, double k)
{
    Polygon *p = (Polygon *)s;
    uint32_t i;
    for (i = 0; i < p->n; i++) {
        p->pts[i].x *= k;
        p->pts[i].y *= k;
    }
}
static void poly_describe(const Shape *s, char *buf, size_t n)
{
    snprintf(buf, n, "polygon n=%u", ((const Polygon *)s)->n);
}

static double rrect_area(const Shape *s)
{
    const RoundedRect *rr = (const RoundedRect *)s;
    double r = rr->corner_r;
    return rect_area(s) - (4.0 - 3.14159265358979) * r * r;
}
static void rrect_describe(const Shape *s, char *buf, size_t n)
{
    const RoundedRect *rr = (const RoundedRect *)s;
    snprintf(buf, n, "rrect %.2fx%.2f r=%.2f",
             rr->rect.width, rr->rect.height, rr->corner_r);
}

static void generic_destroy(Shape *s) { free(s); }

static const ShapeVTable CIRCLE_VT = {
    "Circle", circle_area, circle_perim, circle_scale, circle_describe, generic_destroy
};
static const ShapeVTable RECT_VT = {
    "Rect", rect_area, rect_perim, rect_scale, rect_describe, generic_destroy
};
static const ShapeVTable POLY_VT = {
    "Polygon", poly_area, poly_perim, poly_scale, poly_describe, generic_destroy
};
static const ShapeVTable RRECT_VT = {
    "RoundedRect", rrect_area, rect_perim, rect_scale, rrect_describe, generic_destroy
};

static uint32_t g_next_id = 1;

static Shape *shape_init(Shape *s, const ShapeVTable *vt, const char *tag)
{
    s->vt = vt;
    s->id = g_next_id++;
    memset(s->tag, 0, sizeof(s->tag));
    strncpy(s->tag, tag, sizeof(s->tag) - 1);
    return s;
}

static Shape *make_circle(double r, const char *tag)
{
    Circle *c = (Circle *)calloc(1, sizeof(Circle));
    if (!c) exit(1);
    c->radius = r;
    return shape_init(&c->base, &CIRCLE_VT, tag);
}

static Shape *make_rect(double w, double h, const char *tag)
{
    Rect *r = (Rect *)calloc(1, sizeof(Rect));
    if (!r) exit(1);
    r->width = w;
    r->height = h;
    return shape_init(&r->base, &RECT_VT, tag);
}

static Shape *make_rrect(double w, double h, double cr, const char *tag)
{
    RoundedRect *rr = (RoundedRect *)calloc(1, sizeof(RoundedRect));
    if (!rr) exit(1);
    rr->rect.width  = w;
    rr->rect.height = h;
    rr->corner_r    = cr;
    return shape_init(&rr->rect.base, &RRECT_VT, tag);
}

static Shape *make_poly(const double *xy, uint32_t n, const char *tag)
{
    Polygon *p = (Polygon *)calloc(1, sizeof(Polygon));
    uint32_t i;
    if (!p) exit(1);
    if (n > 8) n = 8;
    p->n = n;
    for (i = 0; i < n; i++) {
        p->pts[i].x = xy[i * 2];
        p->pts[i].y = xy[i * 2 + 1];
    }
    return shape_init(&p->base, &POLY_VT, tag);
}

static void report(Shape **shapes, size_t n)
{
    char buf[64];
    size_t i;
    double total = 0.0;

    for (i = 0; i < n; i++) {
        Shape *s = shapes[i];
        s->vt->describe(s, buf, sizeof(buf));
        BENCH_OUTPUT(
            printf("  #%u %-12s %-11s area=%9.3f perim=%9.3f  %s\n",
                   s->id, s->tag, s->vt->type_name, s->vt->area(s),
                   s->vt->perimeter(s), buf),
            printf("%u|%s|%.3f|%.3f|%zu\n", s->id, s->tag,
                   s->vt->area(s), s->vt->perimeter(s), strlen(buf)));
        total += s->vt->area(s);
    }
    BENCH_OUTPUT(printf("  total area = %.3f\n", total), printf("%.3f\n", total));
}

int main(void)
{
    static const double tri[]  = {0.0, 0.0, 4.0, 0.0, 0.0, 3.0};
    static const double quad[] = {0.0, 0.0, 5.0, 0.0, 6.0, 4.0, 1.0, 4.0};
    Shape *shapes[5];
    size_t i;

    shapes[0] = make_circle(2.5, "c1");
    shapes[1] = make_rect(3.0, 4.0, "r1");
    shapes[2] = make_poly(tri, 3, "tri");
    shapes[3] = make_poly(quad, 4, "quad");
    shapes[4] = make_rrect(6.0, 4.0, 1.0, "rr1");

    BENCH_VERBOSE(printf("initial:\n"));
    report(shapes, 5);

    for (i = 0; i < 5; i++)
        shapes[i]->vt->scale(shapes[i], 1.5);
    BENCH_VERBOSE(printf("after scale(1.5):\n"));
    report(shapes, 5);

    /* downcast based on vtable identity */
    for (i = 0; i < 5; i++) {
        if (shapes[i]->vt == &RRECT_VT) {
            RoundedRect *rr = (RoundedRect *)shapes[i];
            BENCH_OUTPUT(
                printf("downcast rrect: %.2fx%.2f corner=%.2f base_id=%u\n",
                       rr->rect.width, rr->rect.height, rr->corner_r,
                       rr->rect.base.id),
                printf("%.2f %.2f %.2f %u\n", rr->rect.width,
                       rr->rect.height, rr->corner_r, rr->rect.base.id));
        } else if (shapes[i]->vt == &POLY_VT) {
            Polygon *p = (Polygon *)shapes[i];
            BENCH_OUTPUT(
                printf("downcast poly '%s': first=(%.2f,%.2f) last=(%.2f,%.2f)\n",
                       p->base.tag, p->pts[0].x, p->pts[0].y,
                       p->pts[p->n - 1].x, p->pts[p->n - 1].y),
                printf("%s %.2f %.2f %.2f %.2f\n", p->base.tag,
                       p->pts[0].x, p->pts[0].y, p->pts[p->n - 1].x,
                       p->pts[p->n - 1].y));
        }
    }

    for (i = 0; i < 5; i++)
        shapes[i]->vt->destroy(shapes[i]);

    return 0;
}
