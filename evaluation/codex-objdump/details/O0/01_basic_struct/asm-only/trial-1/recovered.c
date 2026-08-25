#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    float x;
    float y;
    float z;
} Vec3;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} Rgba;

typedef struct {
    char name[16];
    Rgba color;
    Rgba blended;
    float weight;
    int32_t category;
} Component;

typedef struct {
    uint64_t identifier;
    char name[24];
    Vec3 position;
    Vec3 velocity;
    Vec3 scale;
    Component component;
    int16_t flags;
    bool active;
    double value;
} Object;

static Vec3 vec3_make(float x, float y, float z)
{
    Vec3 result = {x, y, z};
    return result;
}

static Vec3 vec3_add(Vec3 a, Vec3 b)
{
    return vec3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

static Vec3 vec3_scale(Vec3 value, float factor)
{
    return vec3_make(value.x * factor,
                     value.y * factor,
                     value.z * factor);
}

static float vec3_dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Rgba rgba_blend(Rgba a, Rgba b, uint8_t amount)
{
    unsigned inverse = 255U - amount;
    Rgba result;

    result.r = (uint8_t)(((unsigned)a.r * inverse +
                          (unsigned)b.r * amount) / 255U);
    result.g = (uint8_t)(((unsigned)a.g * inverse +
                          (unsigned)b.g * amount) / 255U);
    result.b = (uint8_t)(((unsigned)a.b * inverse +
                          (unsigned)b.b * amount) / 255U);
    result.a = (uint8_t)(((unsigned)a.a * inverse +
                          (unsigned)b.a * amount) / 255U);

    return result;
}

static void component_initialize(Component *component,
                                 const char *name,
                                 Rgba color,
                                 float weight)
{
    static const Rgba white = {255, 255, 255, 255};

    memset(component, 0, sizeof(*component));
    strncpy(component->name, name, sizeof(component->name) - 1);
    component->color = color;
    component->blended = rgba_blend(color, white, 128);
    component->weight = weight;
    component->category = 5;
}

static void object_initialize(Object *object,
                              uint64_t identifier,
                              const char *name,
                              Vec3 position)
{
    memset(object, 0, sizeof(*object));

    object->identifier = identifier;
    strncpy(object->name, name, sizeof(object->name) - 1);
    object->position = position;
    object->velocity = vec3_make(0.1f, 0.0f, 0.0f);
    object->scale = vec3_make(1.0f, 1.0f, 1.0f);
    object->flags = (int16_t)(identifier & 7U);
    object->active = true;
    object->value = 10.0 + 2.5 * (double)(identifier & 3U);
}

static float object_measure(const Object *object)
{
    return vec3_dot(object->scale, object->scale) *
           object->component.weight;
}

static void object_print(const Object *object)
{
    printf("%lu %s %d %d %.2f\n",
           (unsigned long)object->identifier,
           object->name,
           (int)object->flags,
           (int)object->active,
           object->value);

    printf("%.2f %.2f %.2f %.2f %.2f %.2f\n",
           (double)object->position.x,
           (double)object->position.y,
           (double)object->position.z,
           (double)object->scale.x,
           (double)object->scale.y,
           (double)object->scale.z);

    printf("%s %u %u %u %u %.2f %.2f %d\n",
           object->component.name,
           (unsigned)object->component.color.r,
           (unsigned)object->component.color.g,
           (unsigned)object->component.color.b,
           (unsigned)object->component.color.a,
           (double)object->component.weight,
           (double)object_measure(object),
           object->component.category);
}

int main(void)
{
    Object objects[4];
    Vec3 average = vec3_make(0.0f, 0.0f, 0.0f);

    for (uint32_t i = 0; i <= 3; ++i) {
        char name[24];
        Rgba color;
        Vec3 position;

        snprintf(name, sizeof(name), "node_%u", i);

        color.r = (uint8_t)(i * 60U);
        color.g = (uint8_t)~(i * 40U);
        color.b = (uint8_t)(17U * i * i);
        color.a = 255;

        position = vec3_make((float)i,
                             (float)(i * 2U),
                             (float)(i * i));

        object_initialize(&objects[i],
                          (uint64_t)i + 1000U,
                          name,
                          position);

        component_initialize(&objects[i].component,
                             name,
                             color,
                             (float)i + 0.5f);

        objects[i].scale =
            vec3_scale(objects[i].scale, 1.0f + 0.25f * (float)i);

        average = vec3_add(average, objects[i].position);
    }

    average = vec3_scale(average, 0.25f);

    for (uint32_t i = 0; i <= 3; ++i)
        object_print(&objects[i]);

    printf("%.2f %.2f %.2f\n",
           (double)average.x,
           (double)average.y,
           (double)average.z);

    return 0;
}