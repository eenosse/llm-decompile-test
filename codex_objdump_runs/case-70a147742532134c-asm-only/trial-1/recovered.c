#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t number;
    char name[24];
    float position[3];
    unsigned char reserved[12];
    float vector[3];
    char label[16];
    uint8_t color[4];
    uint8_t light_color[4];
    float weight;
    int32_t count;
    int16_t index;
    uint8_t enabled;
    unsigned char padding;
    double value;
} Record;

_Static_assert(sizeof(Record) == 112, "unexpected Record layout");
_Static_assert(offsetof(Record, position) == 0x20, "unexpected layout");
_Static_assert(offsetof(Record, vector) == 0x38, "unexpected layout");
_Static_assert(offsetof(Record, label) == 0x44, "unexpected layout");
_Static_assert(offsetof(Record, color) == 0x54, "unexpected layout");
_Static_assert(offsetof(Record, weight) == 0x5c, "unexpected layout");
_Static_assert(offsetof(Record, value) == 0x68, "unexpected layout");

int main(void)
{
    Record records[4];
    char text[24];
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_z = 0.0f;

    for (size_t i = 0; i < 4; ++i) {
        Record *record = &records[i];
        unsigned square = (unsigned)(i * i);
        float factor;

        snprintf(text, sizeof(text), "obj_%zu", i);
        memset(record, 0, sizeof(*record));

        record->number = (uint64_t)i + UINT64_C(1000);
        strncpy(record->name, text, sizeof(record->name) - 1);

        record->position[0] = (float)i;
        record->position[1] = (float)(2 * i);
        record->position[2] = (float)square;

        record->vector[0] = 1.0f;
        record->vector[1] = 1.0f;
        record->vector[2] = 1.0f;

        strncpy(record->label, text, sizeof(record->label) - 1);

        record->color[0] = (uint8_t)(60U * (unsigned)i);
        record->color[1] = (uint8_t)(255U - 40U * (unsigned)i);
        record->color[2] = (uint8_t)(17U * square);
        record->color[3] = UINT8_C(255);

        record->weight = (float)i + 0.5f;
        record->count = 5;
        record->index = (int16_t)i;
        record->enabled = 1;
        record->value = 1.5 + (double)i * 0.25;

        sum_x += record->position[0];
        sum_y += record->position[1];
        sum_z += record->position[2];

        factor = 1.0f + (float)i * 0.1f;
        record->vector[0] *= factor;
        record->vector[1] *= factor;
        record->vector[2] *= factor;
        record->weight *= factor;

        for (size_t channel = 0; channel < 3; ++channel)
            record->light_color[channel] =
                (uint8_t)(128U + 127U * record->color[channel] / 255U);
        record->light_color[3] = UINT8_C(255);
    }

    sum_x *= 0.25f;
    sum_y *= 0.25f;
    sum_z *= 0.25f;

    for (size_t i = 0; i < 4; ++i) {
        Record *record = &records[i];
        float magnitude;
        float x2 = record->vector[0] * record->vector[0];
        float y2 = record->vector[1] * record->vector[1];
        float z2 = record->vector[2] * record->vector[2];

        printf("%" PRIu64 " %s %hd %d %.2f\n",
               record->number,
               record->name,
               record->index,
               record->enabled,
               record->value);

        printf("%.2f %.2f %.2f %.2f %.2f %.2f\n",
               (double)record->position[0],
               (double)record->position[1],
               (double)record->position[2],
               (double)record->vector[0],
               (double)record->vector[1],
               (double)record->vector[2]);

        magnitude = (x2 + y2 + z2) * record->weight;

        printf("%s %u %u %u %u %.2f %.2f %d\n",
               record->label,
               (unsigned)record->color[0],
               (unsigned)record->color[1],
               (unsigned)record->color[2],
               (unsigned)record->color[3],
               (double)record->weight,
               (double)magnitude,
               record->count);
    }

    printf("%.2f %.2f %.2f\n",
           (double)sum_x,
           (double)sum_y,
           (double)sum_z);

    return 0;
}