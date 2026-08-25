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

typedef struct {
    void *handle;
    char name[24];
    Vec3 position;
    Vec3 velocity;
    Vec3 scale;
    char label[16];
    Color color;
    Color adjusted_color;
    float weight;
    uint32_t mode;
    int16_t index;
    bool enabled;
    double value;
} Record;

int main(void)
{
    Record records[4];
    Vec3 total = { 0.0f, 0.0f, 0.0f };
    char text[24];

    for (size_t i = 0; i < 4; ++i) {
        Record *record = &records[i];

        snprintf(text, sizeof text, "obj_%zu", i);
        memset(record, 0, sizeof *record);

        record->handle = (void *)(uintptr_t)(1000 + i);
        strncpy(record->name, text, sizeof record->name - 1);

        record->position.x = (float)i;
        record->position.y = (float)(2 * i);
        record->position.z = (float)(i * i);

        record->scale.x = 1.0f;
        record->scale.y = 1.0f;
        record->scale.z = 1.0f;

        strncpy(record->label, text, sizeof record->label - 1);

        record->weight = (float)i + 0.5f;
        record->mode = 5;
        record->index = (int16_t)i;
        record->enabled = true;
        record->value = 1000.0 + (double)i * 0.25;

        record->color.r = (uint8_t)(i * 60);
        record->color.g = (uint8_t)(255 - i * 40);
        record->color.b = (uint8_t)(i * i * 17);
        record->color.a = 255;

        record->adjusted_color.r =
            (uint8_t)((record->color.r * 127u + 32640u) / 255u);
        record->adjusted_color.g =
            (uint8_t)((record->color.g * 127u + 32640u) / 255u);
        record->adjusted_color.b =
            (uint8_t)((record->color.b * 127u + 32640u) / 255u);
        record->adjusted_color.a = 255;

        {
            float factor = 1.0f + (float)i * 0.1f;
            record->scale.x *= factor;
            record->scale.y *= factor;
            record->scale.z *= factor;
        }

        total.x += record->position.x;
        total.y += record->position.y;
        total.z += record->position.z;
    }

    total.x *= 0.25f;
    total.y *= 0.25f;
    total.z *= 0.25f;

    for (size_t i = 0; i < 4; ++i) {
        Record *record = &records[i];
        float derived =
            record->weight *
            (record->scale.x * record->scale.x +
             record->scale.y * record->scale.y +
             record->scale.z * record->scale.z);

        printf("%p %s %hd %d %.2f\n",
               record->handle,
               record->name,
               record->index,
               record->enabled,
               record->value);

        printf("%.2f %.2f %.2f %.2f %.2f %.2f\n",
               record->position.x,
               record->position.y,
               record->position.z,
               record->scale.x,
               record->scale.y,
               record->scale.z);

        printf("%s %u %u %u %u %u %.2f %.2f\n",
               record->label,
               (unsigned)record->color.r,
               (unsigned)record->color.g,
               (unsigned)record->color.b,
               (unsigned)record->color.a,
               record->mode,
               record->weight,
               derived);
    }

    printf("%.2f %.2f %.2f\n", total.x, total.y, total.z);
    return 0;
}