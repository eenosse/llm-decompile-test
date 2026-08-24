#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct record {
    uint64_t id;
    char name[24];
    float position[3];
    float rotation[3];
    float scale[3];
    char tag[16];
    uint8_t color[4];
    uint8_t muted_color[4];
    float mass;
    uint32_t count;
    int16_t category;
    uint8_t active;
    double weight;
};

int main(void)
{
    struct record records[4];
    char buffer[24];
    float position_sum[3] = { 0.0f, 0.0f, 0.0f };

    for (size_t i = 0; i < 4; ++i) {
        struct record *record = &records[i];
        float value = (float)i;
        uint32_t squared = (uint32_t)i * (uint32_t)i;
        uint8_t red = (uint8_t)((uint32_t)i * 60u);
        uint8_t green = (uint8_t)(255u - (uint32_t)i * 40u);
        uint8_t blue = (uint8_t)(squared * 17u);

        snprintf(buffer, sizeof buffer, "item%zu", i);
        memset(record, 0, sizeof *record);

        record->id = (uint64_t)i + 1000u;
        strncpy(record->name, buffer, sizeof record->name - 1);

        record->position[0] = value;
        record->position[1] = (float)(i * 2u);
        record->position[2] = (float)(i * i);

        record->scale[0] = 1.0f;
        record->scale[1] = 1.0f;
        record->scale[2] = 1.0f;

        record->category = (int16_t)i;
        record->active = 1;
        record->weight = (double)(i & 3u) * 0.25 + 1.0;

        strncpy(record->tag, buffer, sizeof record->tag - 1);

        record->color[0] = red;
        record->color[1] = green;
        record->color[2] = blue;
        record->color[3] = 255;

        record->muted_color[0] =
            (uint8_t)(((uint32_t)red * 127u + 128u * 255u) / 255u);
        record->muted_color[1] =
            (uint8_t)(((uint32_t)green * 127u + 128u * 255u) / 255u);
        record->muted_color[2] =
            (uint8_t)(((uint32_t)blue * 127u + 128u * 255u) / 255u);
        record->muted_color[3] = 255;

        record->mass = value + 0.5f;
        record->count = 5;

        {
            float factor = value * 0.1f + 1.0f;
            record->scale[0] *= factor;
            record->scale[1] *= factor;
            record->scale[2] *= factor;
        }

        position_sum[0] += record->position[0];
        position_sum[1] += record->position[1];
        position_sum[2] += record->position[2];
    }

    position_sum[0] *= 0.25f;
    position_sum[1] *= 0.25f;
    position_sum[2] *= 0.25f;

    for (size_t i = 0; i < 4; ++i) {
        const struct record *record = &records[i];
        float energy =
            record->mass *
            (record->scale[0] * record->scale[0] +
             record->scale[1] * record->scale[1] +
             record->scale[2] * record->scale[2]);

        printf("id=%" PRIu64 " %s %d %u %g\n",
               record->id,
               record->name,
               (int)record->category,
               (unsigned)record->active,
               record->weight);

        printf("pos=%g,%g,%g scale=%g,%g,%g\n",
               (double)record->position[0],
               (double)record->position[1],
               (double)record->position[2],
               (double)record->scale[0],
               (double)record->scale[1],
               (double)record->scale[2]);

        printf("tag=%s rgba=%u,%u,%u,%u n=%u m=%g e=%g\n",
               record->tag,
               (unsigned)record->color[0],
               (unsigned)record->color[1],
               (unsigned)record->color[2],
               (unsigned)record->color[3],
               record->count,
               (double)record->mass,
               (double)energy);
    }

    printf("avg=%g,%g,%g\n",
           (double)position_sum[0],
           (double)position_sum[1],
           (double)position_sum[2]);

    return 0;
}