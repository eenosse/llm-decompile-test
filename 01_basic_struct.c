/*
 * 01_basic_struct.c
 * Target feature: plain user-defined structs, nesting by value, mixed scalar
 * widths (padding/alignment holes), fixed char arrays, struct passed and
 * returned by value.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef struct Vec3f {
    float x;
    float y;
    float z;
} Vec3f;

typedef struct RgbaColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} RgbaColor;

typedef struct Material {
    char      name[16];
    RgbaColor diffuse;
    RgbaColor specular;
    float     shininess;
    uint32_t  flags;      /* bit 0: lit, bit 1: two-sided, bit 2: alpha blend */
} Material;

typedef struct Transform {
    Vec3f position;
    Vec3f rotation;
    Vec3f scale;
} Transform;

/* Deliberately padded: uint64 / char[24] / nested / int16 / uint8 / double */
typedef struct SceneObject {
    uint64_t  id;
    char      label[24];
    Transform xform;
    Material  material;
    int16_t   layer;
    uint8_t   visible;
    double    lod_bias;
} SceneObject;

static Vec3f vec3_make(float x, float y, float z)
{
    Vec3f v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

static Vec3f vec3_add(Vec3f a, Vec3f b)
{
    return vec3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

static Vec3f vec3_scale(Vec3f a, float k)
{
    return vec3_make(a.x * k, a.y * k, a.z * k);
}

static float vec3_dot(Vec3f a, Vec3f b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static RgbaColor color_blend(RgbaColor a, RgbaColor b, uint8_t t)
{
    RgbaColor out;
    out.r = (uint8_t)(((unsigned)a.r * (255u - t) + (unsigned)b.r * t) / 255u);
    out.g = (uint8_t)(((unsigned)a.g * (255u - t) + (unsigned)b.g * t) / 255u);
    out.b = (uint8_t)(((unsigned)a.b * (255u - t) + (unsigned)b.b * t) / 255u);
    out.a = (uint8_t)(((unsigned)a.a * (255u - t) + (unsigned)b.a * t) / 255u);
    return out;
}

static void material_init(Material *m, const char *name, RgbaColor diffuse, float shininess)
{
    memset(m, 0, sizeof(*m));
    strncpy(m->name, name, sizeof(m->name) - 1);
    m->diffuse   = diffuse;
    m->specular  = color_blend(diffuse, (RgbaColor){255, 255, 255, 255}, 128);
    m->shininess = shininess;
    m->flags     = 0x1u | 0x4u;
}

static void object_init(SceneObject *o, uint64_t id, const char *label, Vec3f pos)
{
    memset(o, 0, sizeof(*o));
    o->id = id;
    strncpy(o->label, label, sizeof(o->label) - 1);
    o->xform.position = pos;
    o->xform.rotation = vec3_make(0.0f, 0.0f, 0.0f);
    o->xform.scale    = vec3_make(1.0f, 1.0f, 1.0f);
    o->layer          = (int16_t)(id % 8);
    o->visible        = 1;
    o->lod_bias       = 0.5 + (double)(id % 4) * 0.25;
}

static float object_radius(const SceneObject *o)
{
    Vec3f s = o->xform.scale;
    return vec3_dot(s, s) * o->material.shininess;
}

static void object_dump(const SceneObject *o)
{
    BENCH_OUTPUT(
        printf("obj #%llu '%s' layer=%d visible=%u lod=%.2f\n",
               (unsigned long long)o->id, o->label, (int)o->layer,
               (unsigned)o->visible, o->lod_bias),
        printf("%llu %s %d %u %.2f\n", (unsigned long long)o->id, o->label,
               (int)o->layer, (unsigned)o->visible, o->lod_bias));
    BENCH_OUTPUT(
        printf("   pos=(%.2f %.2f %.2f) scale=(%.2f %.2f %.2f)\n",
               o->xform.position.x, o->xform.position.y, o->xform.position.z,
               o->xform.scale.x, o->xform.scale.y, o->xform.scale.z),
        printf("%.2f %.2f %.2f %.2f %.2f %.2f\n",
               o->xform.position.x, o->xform.position.y, o->xform.position.z,
               o->xform.scale.x, o->xform.scale.y, o->xform.scale.z));
    BENCH_OUTPUT(
        printf("   mat '%s' diffuse=%02x%02x%02x%02x shininess=%.1f flags=0x%x radius=%.3f\n",
               o->material.name, o->material.diffuse.r, o->material.diffuse.g,
               o->material.diffuse.b, o->material.diffuse.a,
               o->material.shininess, o->material.flags, object_radius(o)),
        printf("%s %02x %02x %02x %02x %.1f %x %.3f\n", o->material.name,
               o->material.diffuse.r, o->material.diffuse.g,
               o->material.diffuse.b, o->material.diffuse.a,
               o->material.shininess, o->material.flags, object_radius(o)));
}

int main(void)
{
    SceneObject scene[4];
    Vec3f centroid = vec3_make(0.0f, 0.0f, 0.0f);
    unsigned i;

    for (i = 0; i < 4; i++) {
        char label[24];
        RgbaColor c;

        snprintf(label, sizeof(label), "node_%u", i);
        c.r = (uint8_t)(i * 60);
        c.g = (uint8_t)(255 - i * 40);
        c.b = (uint8_t)(i * i * 17);
        c.a = 255;

        object_init(&scene[i], 1000 + i, label,
                    vec3_make((float)i, (float)(i * 2), (float)(i * i)));
        material_init(&scene[i].material, label, c, 8.0f + (float)i);
        scene[i].xform.scale = vec3_scale(scene[i].xform.scale, 1.0f + 0.1f * (float)i);
        centroid = vec3_add(centroid, scene[i].xform.position);
    }

    centroid = vec3_scale(centroid, 0.25f);

    for (i = 0; i < 4; i++)
        object_dump(&scene[i]);

    BENCH_OUTPUT(
        printf("centroid=(%.2f %.2f %.2f)\n", centroid.x, centroid.y, centroid.z),
        printf("%.2f %.2f %.2f\n", centroid.x, centroid.y, centroid.z));
    return 0;
}
