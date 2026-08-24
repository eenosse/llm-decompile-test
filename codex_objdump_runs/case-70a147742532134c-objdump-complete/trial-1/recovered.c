#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct record {
    unsigned long long number;
    char name[24];
    float values[3];
    float unused_values[3];
    float factors[3];
    char short_name[16];
    uint8_t color[4];
    uint8_t secondary_color[4];
    float weight;
    uint32_t flags;
    int16_t index;
    uint8_t enabled;
    uint8_t reserved;
    double score;
};

int main(void)
{
    struct record records[4];
    char temporary_name[24];
    float totals[3] = { 0.0f, 0.0f, 0.0f };
    uint32_t state_a = 0x7f80u;
    uint32_t state_b = 0xfe01u;

    for (uint32_t i = 0; i < 4; ++i) {
        struct record *record = &records[i];
        uint32_t square = i * i;
        uint8_t component = (uint8_t)(17u * square);

        snprintf(temporary_name, sizeof temporary_name, "node_%u", i);
        memset(record, 0, sizeof *record);

        record->number = 1000u + i;
        strncpy(record->name, temporary_name, sizeof record->name - 1);

        record->values[0] = (float)i;
        record->values[1] = (float)(2u * i);
        record->values[2] = (float)square;

        record->factors[0] = 1.0f;
        record->factors[1] = 1.0f;
        record->factors[2] = 1.0f;

        strncpy(record->short_name, temporary_name,
                sizeof record->short_name - 1);

        record->index = (int16_t)i;
        record->enabled = 1;
        record->score = (double)i * 0.25 + 0.5;

        record->color[0] = (uint8_t)(-128u - state_a);
        record->color[1] = (uint8_t)(0u - state_b);
        record->color[2] = component;
        record->color[3] = 255;

        record->secondary_color[0] = (uint8_t)(state_a / 127u);
        record->secondary_color[1] = (uint8_t)(state_b / 127u);
        record->secondary_color[2] =
            (uint8_t)(((uint32_t)component * 127u + 32640u) / 127u);
        record->secondary_color[3] = 255;

        record->weight = 8.0f + record->values[0];
        record->flags = 5;

        totals[0] += record->values[0];
        totals[1] += record->values[1];
        totals[2] += record->values[2];

        {
            float factor = record->values[0] * 0.1f + 1.0f;
            record->factors[0] *= factor;
            record->factors[1] *= factor;
            record->factors[2] *= factor;
        }

        state_a += 0x1dc4u;
        state_b -= 0x13d8u;
    }

    totals[0] *= 0.25f;
    totals[1] *= 0.25f;
    totals[2] *= 0.25f;

    for (uint32_t i = 0; i < 4; ++i) {
        const struct record *record = &records[i];
        float magnitude =
            ((record->factors[0] * record->factors[0] +
              record->factors[1] * record->factors[1]) +
             record->factors[2] * record->factors[2]) *
            record->weight;

        printf("%llu %s %d %u %.2f\n",
               record->number,
               record->name,
               (int)record->index,
               (unsigned)record->enabled,
               record->score);

        printf("%.2f %.2f %.2f %.2f %.2f %.2f\n",
               record->values[0],
               record->values[1],
               record->values[2],
               record->factors[0],
               record->factors[1],
               record->factors[2]);

        printf("%s %02x %02x %02x %02x %.1f %x %.3f\n",
               record->short_name,
               (unsigned)record->color[0],
               (unsigned)record->color[1],
               (unsigned)record->color[2],
               (unsigned)record->color[3],
               record->weight,
               (unsigned)record->flags,
               magnitude);
    }

    printf("%.2f %.2f %.2f\n", totals[0], totals[1], totals[2]);
    return 0;
}