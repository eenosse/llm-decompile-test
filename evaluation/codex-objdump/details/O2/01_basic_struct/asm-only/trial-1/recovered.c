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
} Color;

typedef union {
    Color channels;
    float packed;
} ColorValue;

typedef struct {
    uint64_t id;
    char name[24];
    Vec3 position;
    Vec3 velocity;
    Vec3 scale;
    char label[16];
    ColorValue primary;
    Color secondary;
    float weight;
    int32_t count;
    int16_t category;
    bool enabled;
    double score;
} Record;

int main(void)
{
    Record records[4];
    Vec3 sum = { 0.0f, 0.0f, 0.0f };

    for (int i = 0; i < 4; ++i) {
        char text[24];
        Record *record = &records[i];
        float factor;

        snprintf(text, sizeof text, "item_%d", i);
        memset(record, 0, sizeof *record);

        record->id = 1000u + (uint64_t)i;
        strncpy(record->name, text, sizeof record->name - 1);

        record->position.x = (float)i;
        record->position.y = (float)(2 * i);
        record->position.z = (float)(i * i);

        record->scale.x = 1.0f;
        record->scale.y = 1.0f;
        record->scale.z = 1.0f;

        strncpy(record->label, text, sizeof record->label - 1);

        record->category = (int16_t)i;
        record->enabled = true;
        record->score = (double)i * 1.25 + 10.0;

        record->primary.channels.r = (uint8_t)(60 * i);
        record->primary.channels.g = (uint8_t)(255 - 40 * i);
        record->primary.channels.b = (uint8_t)(17 * i * i);
        record->primary.channels.a = UINT8_MAX;

        record->secondary.r = (uint8_t)(
            ((uint32_t)record->primary.channels.r * 127u + 255u * 128u) /
            255u);
        record->secondary.g = (uint8_t)(
            ((uint32_t)record->primary.channels.g * 127u + 255u * 128u) /
            255u);
        record->secondary.b = (uint8_t)(
            ((uint32_t)record->primary.channels.b * 127u + 255u * 128u) /
            255u);
        record->secondary.a = UINT8_MAX;

        record->weight = (float)i + 0.5f;
        record->count = 5;

        factor = 1.0f + 0.25f * (float)i;
        record->scale.x *= factor;
        record->scale.y *= factor;
        record->scale.z *= factor;

        sum.x += record->position.x;
        sum.y += record->position.y;
        sum.z += record->position.z;
    }

    sum.x *= 0.25f;
    sum.y *= 0.25f;
    sum.z *= 0.25f;

    for (int i = 0; i < 4; ++i) {
        const Record *record = &records[i];
        float scaled_packed =
            (record->scale.x * record->scale.x +
             record->scale.y * record->scale.y +
             record->scale.z * record->scale.z) *
            record->primary.packed;

        printf("%lu %s %d %d %g\n",
               (unsigned long)record->id,
               record->name,
               (int)record->category,
               (int)record->enabled,
               record->score);

        printf("%g %g %g %g %g %g\n",
               (double)record->position.x,
               (double)record->position.y,
               (double)record->position.z,
               (double)record->scale.x,
               (double)record->scale.y,
               (double)record->scale.z);

        printf("%s %u %u %u %u %g %g %d\n",
               record->label,
               (unsigned)record->primary.channels.r,
               (unsigned)record->primary.channels.g,
               (unsigned)record->primary.channels.b,
               (unsigned)record->primary.channels.a,
               (double)record->primary.packed,
               (double)scaled_packed,
               record->count);
    }

    printf("avg %g %g %g\n",
           (double)sum.x,
           (double)sum.y,
           (double)sum.z);

    return 0;
}