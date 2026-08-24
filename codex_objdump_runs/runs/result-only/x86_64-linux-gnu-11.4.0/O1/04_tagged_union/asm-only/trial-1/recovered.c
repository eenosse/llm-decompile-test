#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    RECORD_SIZE = 120,
    PAYLOAD_SIZE = 104
};

struct payload_1 {
    char text[24];
    uint8_t bytes[16];
    uint32_t flags;
};

struct payload_2 {
    int32_t number;
    float values[6];
    uint16_t code;
};

struct payload_3 {
    int32_t number;
    uint16_t length;
    char text[98];
};

struct payload_4 {
    int32_t number;
    uint32_t length;
    uint8_t bytes[96];
};

struct payload_5 {
    int32_t number;
    char text[100];
};

union payload {
    uint64_t value;
    struct payload_1 kind_1;
    struct payload_2 kind_2;
    struct payload_3 kind_3;
    struct payload_4 kind_4;
    struct payload_5 kind_5;
    uint8_t raw[PAYLOAD_SIZE];
};

struct record {
    uint32_t type;
    uint32_t sequence;
    uint64_t timestamp;
    union payload payload;
};

_Static_assert(sizeof(float) == 4, "32-bit float required");
_Static_assert(sizeof(struct record) == RECORD_SIZE, "unexpected record layout");
_Static_assert(offsetof(struct record, payload) == 16, "unexpected payload offset");
_Static_assert(offsetof(struct payload_1, bytes) == 24, "unexpected byte-array offset");
_Static_assert(offsetof(struct payload_1, flags) == 40, "unexpected flags offset");
_Static_assert(offsetof(struct payload_3, text) == 6, "unexpected text offset");
_Static_assert(offsetof(struct payload_4, bytes) == 8, "unexpected byte-array offset");
_Static_assert(offsetof(struct payload_5, text) == 4, "unexpected text offset");

static uint32_t next_sequence;

/*
 * The referenced read-only bytes are absent from the supplied evidence.
 * Zero is the conservative XOR-identity fallback.
 */
static const uint8_t byte_key[8] = {
    0, 0, 0, 0, 0, 0, 0, 0
};

/* The exact referenced floating-point constant is likewise unavailable. */
static const float scale_factor = 1.0f;

static void initialize_record(struct record *record, uint32_t type)
{
    uint32_t sequence;

    memset(&record->payload, 0, sizeof(record->payload));
    record->type = type;

    sequence = next_sequence++;
    record->sequence = sequence;
    record->timestamp = UINT64_C(1600000000) + UINT64_C(37) * sequence;
}

static uint32_t record_hash(const struct record *record)
{
    uint32_t hash = record->type * UINT32_C(0x9e3779b1)
                  + record->sequence;

    switch (record->type) {
    case 0: {
        uint64_t value = record->payload.value;
        return hash ^ (uint32_t)value ^ (uint32_t)(value >> 32);
    }

    case 1:
        for (size_t i = 0; i < 16; ++i)
            hash = hash * UINT32_C(31)
                 + record->payload.kind_1.bytes[i];

        return hash ^ record->payload.kind_1.flags;

    case 2: {
        int64_t first;
        int64_t second;

        hash ^= (uint32_t)record->payload.kind_2.number;
        first = (int64_t)(scale_factor *
                          record->payload.kind_2.values[4]);
        second = (int64_t)(scale_factor *
                           record->payload.kind_2.values[5]);

        hash += (uint32_t)first;
        hash += (uint32_t)second;
        hash += record->payload.kind_2.code;
        return hash;
    }

    case 3:
        hash ^= (uint32_t)record->payload.kind_3.number;

        for (uint16_t i = 0; i < record->payload.kind_3.length; ++i)
            hash = hash * UINT32_C(131)
                 + (uint8_t)record->payload.kind_3.text[i];

        return hash;

    case 4:
        hash ^= (uint32_t)record->payload.kind_4.number;

        for (uint32_t i = 0; i < record->payload.kind_4.length; ++i)
            hash = hash * UINT32_C(17)
                 + record->payload.kind_4.bytes[i];

        return hash;

    case 5: {
        const uint8_t *text;

        hash ^= (uint32_t)record->payload.kind_5.number;
        text = (const uint8_t *)record->payload.kind_5.text;

        while (*text != 0) {
            hash = hash * UINT32_C(7) + *text;
            ++text;
        }

        return hash;
    }

    default:
        return hash;
    }
}

int main(void)
{
    struct record records[6];
    uint32_t total = 0;

    initialize_record(&records[0], 0);
    records[0].payload.value = UINT64_C(0xdeadbeefcafe1234);

    initialize_record(&records[1], 1);
    strcpy(records[1].payload.kind_1.text, "operator");

    for (int i = 0; i < 16; ++i) {
        records[1].payload.kind_1.bytes[i] =
            (uint8_t)(i * 31) ^ byte_key[i % 8];
    }

    records[1].payload.kind_1.flags = UINT32_C(0x00030201);

    initialize_record(&records[2], 2);
    records[2].payload.kind_2.number = 42;
    records[2].payload.kind_2.values[0] = 42.0f;
    records[2].payload.kind_2.values[1] = 84.0f;
    records[2].payload.kind_2.values[2] = 0.0f;
    records[2].payload.kind_2.values[3] = 43.5f;
    records[2].payload.kind_2.values[4] = 81.75f;
    records[2].payload.kind_2.values[5] = 0.75f;
    records[2].payload.kind_2.code = 330;

    initialize_record(&records[3], 3);
    records[3].payload.kind_3.number = 7;
    strcpy(records[3].payload.kind_3.text, "type recovery under test");
    records[3].payload.kind_3.length =
        (uint16_t)strlen(records[3].payload.kind_3.text);

    initialize_record(&records[4], 4);
    records[4].payload.kind_4.number = 99;
    records[4].payload.kind_4.length = 24;

    for (uint32_t i = 0; i < 24; ++i)
        records[4].payload.kind_4.bytes[i] = (uint8_t)(7 + 5 * i);

    initialize_record(&records[5], 5);
    records[5].payload.kind_5.number = -11;
    strcpy(records[5].payload.kind_5.text, "unsupported opcode");

    for (size_t i = 0; i < 6; ++i) {
        struct record *record = &records[i];
        uint32_t hash = record_hash(record);

        printf("%u %lu %08x %u\n",
               record->sequence,
               (unsigned long)record->timestamp,
               hash,
               record->type);

        switch (record->type) {
        case 0:
            printf("%016lx\n",
                   (unsigned long)record->payload.value);
            break;

        case 1:
            printf("%s %x %02x\n",
                   record->payload.kind_1.text,
                   record->payload.kind_1.flags,
                   (unsigned int)record->payload.kind_1.bytes[0]);
            break;

        case 2:
            printf("%d %.2f %.2f %.2f %.2f %.2f %.2f %u\n",
                   record->payload.kind_2.number,
                   (double)record->payload.kind_2.values[0],
                   (double)record->payload.kind_2.values[1],
                   (double)record->payload.kind_2.values[2],
                   (double)record->payload.kind_2.values[3],
                   (double)record->payload.kind_2.values[4],
                   (double)record->payload.kind_2.values[5],
                   (unsigned int)record->payload.kind_2.code);
            break;

        case 3:
            printf("%d %u %s\n",
                   record->payload.kind_3.number,
                   (unsigned int)record->payload.kind_3.length,
                   record->payload.kind_3.text);
            break;

        case 4:
            printf("%d %u %02x %02x\n",
                   record->payload.kind_4.number,
                   record->payload.kind_4.length,
                   (unsigned int)record->payload.kind_4.bytes[0],
                   (unsigned int)record->payload.kind_4.bytes[1]);
            break;

        case 5:
            printf("%d %s\n",
                   record->payload.kind_5.number,
                   record->payload.kind_5.text);
            break;

        default:
            break;
        }

        total += record_hash(record);
    }

    printf("%08x\n", total);
    return 0;
}