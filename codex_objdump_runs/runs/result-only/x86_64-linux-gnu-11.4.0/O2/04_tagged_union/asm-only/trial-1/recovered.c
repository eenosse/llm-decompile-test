#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum {
    PAYLOAD_SIZE = 104,
    RECORD_COUNT = 6
};

typedef union {
    uint8_t raw[PAYLOAD_SIZE];

    struct {
        uint64_t value;
    } kind0;

    struct {
        char name[24];
        uint8_t block[16];
        uint32_t check;
    } kind1;

    struct {
        uint32_t code;
        float values[6];
        uint16_t tail;
    } kind2;

    struct {
        uint32_t code;
        uint16_t length;
        char text[98];
    } kind3;

    struct {
        uint32_t code;
        uint32_t count;
        uint8_t data[96];
    } kind4;

    struct {
        int32_t code;
        char text[100];
    } kind5;
} Payload;

typedef struct {
    uint32_t kind;
    uint32_t sequence;
    uint64_t identifier;
    Payload payload;
} Record;

_Static_assert(sizeof(Payload) == 104, "unexpected payload layout");
_Static_assert(sizeof(Record) == 120, "unexpected record layout");
_Static_assert(offsetof(Record, payload) == 16, "unexpected record layout");
_Static_assert(offsetof(Payload, kind1.block) == 24, "unexpected kind1 layout");
_Static_assert(offsetof(Payload, kind1.check) == 40, "unexpected kind1 layout");
_Static_assert(offsetof(Payload, kind2.tail) == 28, "unexpected kind2 layout");
_Static_assert(offsetof(Payload, kind3.text) == 6, "unexpected kind3 layout");
_Static_assert(offsetof(Payload, kind4.data) == 8, "unexpected kind4 layout");

static uint32_t next_sequence;

static void initialize_record(Record *record, uint32_t kind)
{
    uint32_t sequence = next_sequence++;

    memset(&record->payload, 0, sizeof(record->payload));
    record->kind = kind;
    record->sequence = sequence;
    record->identifier =
        UINT64_C(0x5f5e1000) + UINT64_C(41) * sequence;
}

static uint32_t record_checksum(const Record *record)
{
    uint32_t value =
        record->kind * UINT32_C(0x9e3779b1) + record->sequence;

    switch (record->kind) {
    case 0: {
        uint64_t item = record->payload.kind0.value;
        value ^= (uint32_t)item;
        value ^= (uint32_t)(item >> 32);
        break;
    }

    case 1:
        for (size_t i = 0; i < 16; ++i)
            value = value * UINT32_C(31) +
                    record->payload.kind1.block[i];
        value ^= record->payload.kind1.check;
        break;

    case 2: {
        int64_t first =
            (int64_t)(record->payload.kind2.values[4] * 1000.0f);
        int64_t second =
            (int64_t)(record->payload.kind2.values[5] * 1000.0f);

        value ^= record->payload.kind2.code;
        value += (uint32_t)first;
        value += (uint32_t)second;
        value += record->payload.kind2.tail;
        break;
    }

    case 3:
        value ^= record->payload.kind3.code;
        for (uint16_t i = 0; i < record->payload.kind3.length; ++i)
            value = value * UINT32_C(131) +
                    (uint8_t)record->payload.kind3.text[i];
        break;

    case 4:
        value ^= record->payload.kind4.code;
        for (uint32_t i = 0; i < record->payload.kind4.count; ++i)
            value = value * UINT32_C(17) +
                    record->payload.kind4.data[i];
        break;

    case 5:
        value ^= (uint32_t)record->payload.kind5.code;
        for (size_t i = 0;
             record->payload.kind5.text[i] != '\0';
             ++i) {
            value = value * UINT32_C(7) +
                    (uint8_t)record->payload.kind5.text[i];
        }
        break;

    default:
        break;
    }

    return value;
}

int main(void)
{
    static const uint8_t key[8] = {
        'o', 'p', 'e', 'r', 'a', 't', 'o', 'r'
    };

    Record records[RECORD_COUNT];

    initialize_record(&records[0], 0);
    records[0].payload.kind0.value = UINT64_C(0xdeadbeefcafe1234);

    initialize_record(&records[1], 1);
    memcpy(records[1].payload.kind1.name, "operator", 9);
    for (uint32_t i = 0; i < 16; ++i) {
        records[1].payload.kind1.block[i] =
            (uint8_t)(key[i & 7] ^ (uint8_t)(i * 31u));
    }
    records[1].payload.kind1.check = UINT32_C(0x00030201);

    initialize_record(&records[2], 2);
    records[2].payload.kind2.code = 42;
    records[2].payload.kind2.values[0] = 42.0f;
    records[2].payload.kind2.values[1] = 84.0f;
    records[2].payload.kind2.values[2] = 0.0f;
    records[2].payload.kind2.values[3] = 43.5f;
    records[2].payload.kind2.values[4] = 81.75f;
    records[2].payload.kind2.values[5] = 0.75f;
    records[2].payload.kind2.tail = 330;

    initialize_record(&records[3], 3);
    records[3].payload.kind3.code = 7;
    memcpy(records[3].payload.kind3.text,
           "short message under test", 25);
    records[3].payload.kind3.length =
        (uint16_t)strlen(records[3].payload.kind3.text);

    initialize_record(&records[4], 4);
    records[4].payload.kind4.code = 99;
    records[4].payload.kind4.count = 24;
    for (uint32_t i = 0; i < 24; ++i)
        records[4].payload.kind4.data[i] = (uint8_t)(7u + 5u * i);

    initialize_record(&records[5], 5);
    records[5].payload.kind5.code = -11;
    memcpy(records[5].payload.kind5.text,
           "negative code mode", 19);

    uint32_t total = 0;

    for (size_t i = 0; i < RECORD_COUNT; ++i) {
        const Record *record = &records[i];
        uint32_t checksum = record_checksum(record);

        printf("%u %lu %u %u\n",
               record->sequence,
               (unsigned long)record->identifier,
               checksum,
               record->kind);

        switch (record->kind) {
        case 0:
            printf("0x%llx\n",
                   (unsigned long long)record->payload.kind0.value);
            break;

        case 1:
            printf("%s %x %02x\n",
                   record->payload.kind1.name,
                   record->payload.kind1.check,
                   record->payload.kind1.block[0]);
            break;

        case 2:
            printf("%u %.2f %.2f %.2f %.2f %.2f %.2f %u\n",
                   record->payload.kind2.code,
                   (double)record->payload.kind2.values[0],
                   (double)record->payload.kind2.values[1],
                   (double)record->payload.kind2.values[2],
                   (double)record->payload.kind2.values[3],
                   (double)record->payload.kind2.values[4],
                   (double)record->payload.kind2.values[5],
                   (unsigned)record->payload.kind2.tail);
            break;

        case 3:
            printf("%u %u %s\n",
                   record->payload.kind3.code,
                   (unsigned)record->payload.kind3.length,
                   record->payload.kind3.text);
            break;

        case 4:
            printf("%u %u %02x %02x\n",
                   record->payload.kind4.code,
                   record->payload.kind4.count,
                   record->payload.kind4.data[0],
                   record->payload.kind4.data[1]);
            break;

        case 5:
            printf("%d %s\n",
                   record->payload.kind5.code,
                   record->payload.kind5.text);
            break;

        default:
            break;
        }

        total += record_checksum(record);
    }

    printf("T=%u\n", total);
    return 0;
}