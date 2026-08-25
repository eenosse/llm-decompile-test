#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    RECORD_COUNT = 6,
    PAYLOAD_SIZE = 104
};

typedef struct {
    uint64_t value;
    unsigned char unused[96];
} payload_0;

typedef struct {
    char name[16];
    uint32_t field_16;
    uint16_t field_20;
    uint8_t field_22;
    uint8_t padding_23;
    unsigned char field_24[16];
    unsigned char fixed_data[16];
    uint32_t checksum;
    unsigned char unused[44];
} payload_1;

typedef struct {
    int32_t identifier;
    float measurements[6];
    uint16_t trailing_value;
    unsigned char unused[74];
} payload_2;

typedef struct {
    int32_t identifier;
    uint16_t length;
    char text[98];
} payload_3;

typedef struct {
    int32_t value;
    char text[100];
} payload_4;

typedef struct {
    int32_t status;
    uint32_t length;
    unsigned char data[96];
} payload_5;

typedef union {
    unsigned char bytes[PAYLOAD_SIZE];
    payload_0 type_0;
    payload_1 type_1;
    payload_2 type_2;
    payload_3 type_3;
    payload_4 type_4;
    payload_5 type_5;
} payload;

typedef struct {
    uint32_t type;
    uint32_t sequence;
    uint64_t timestamp;
    payload data;
} record;

_Static_assert(sizeof(payload) == 104, "payload layout");
_Static_assert(sizeof(record) == 120, "record layout");
_Static_assert(offsetof(record, data) == 16, "record payload offset");
_Static_assert(offsetof(payload_1, field_24) == 24, "type 1 layout");
_Static_assert(offsetof(payload_1, fixed_data) == 40, "type 1 layout");
_Static_assert(offsetof(payload_1, checksum) == 56, "type 1 layout");
_Static_assert(offsetof(payload_2, trailing_value) == 28, "type 2 layout");
_Static_assert(offsetof(payload_3, text) == 6, "type 3 layout");
_Static_assert(offsetof(payload_5, data) == 8, "type 5 layout");

static uint32_t next_sequence;

/*
 * The supplied disassembly references these read-only objects without
 * including their bytes. Zero-filled objects conservatively represent those
 * unrecovered literals; the explicitly visible literal fragments are applied
 * separately below.
 */
static const unsigned char literal_type1_24[16];
static const unsigned char literal_type2_first[16];
static const unsigned char literal_type2_last[8];
static const unsigned char literal_type3_text[16];
static const unsigned char literal_type4_first[8];
static const unsigned char literal_type4_middle[16];
static const unsigned char literal_type4_last[8];
static const unsigned char literal_type5_first[16];
static const float hash_scale;

static uint32_t load_u32(const void *address)
{
    uint32_t value;
    memcpy(&value, address, sizeof(value));
    return value;
}

static void initialize_record(record *item, uint32_t type)
{
    uint32_t sequence = next_sequence++;

    item->type = type;
    item->sequence = sequence;
    item->timestamp = UINT64_C(1600000000) + UINT64_C(37) * sequence;
    memset(&item->data, 0, sizeof(item->data));
}

static uint32_t record_hash(const record *item)
{
    uint32_t hash =
        item->type * UINT32_C(0x9e3779b1) + item->sequence;

    switch (item->type) {
    case 0: {
        uint64_t value = item->data.type_0.value;
        hash ^= (uint32_t)value;
        hash ^= (uint32_t)(value >> 32);
        break;
    }

    case 1:
        for (size_t i = 0; i < 16; ++i)
            hash = hash * UINT32_C(31) +
                   item->data.type_1.fixed_data[i];
        hash ^= item->data.type_1.checksum;
        break;

    case 2: {
        float first =
            item->data.type_2.measurements[3] * hash_scale;
        float second =
            item->data.type_2.measurements[4] * hash_scale;
        int64_t first_integer = (int64_t)first;
        int64_t second_integer = (int64_t)second;

        hash += (uint32_t)first_integer;
        hash += (uint32_t)second_integer;
        hash += item->data.type_2.trailing_value;
        break;
    }

    case 3:
        for (uint16_t i = 0; i < item->data.type_3.length; ++i)
            hash = hash * UINT32_C(131) +
                   (unsigned char)item->data.type_3.text[i];
        break;

    case 4: {
        const unsigned char *text =
            (const unsigned char *)item->data.type_4.text;

        hash ^= (uint32_t)item->data.type_4.value;
        while (*text != 0)
            hash = hash * UINT32_C(7) + *text++;
        break;
    }

    case 5:
        hash ^= (uint32_t)item->data.type_5.status;
        for (uint32_t i = 0; i < item->data.type_5.length; ++i)
            hash = hash * UINT32_C(17) +
                   item->data.type_5.data[i];
        break;

    default:
        break;
    }

    return hash;
}

int main(void)
{
    record records[RECORD_COUNT];
    uint32_t total = 0;

    initialize_record(&records[0], 0);
    records[0].data.type_0.value = UINT64_C(0xdeadbeefcafe1234);

    initialize_record(&records[1], 1);
    memcpy(records[1].data.type_1.name, "operator", 8);
    memcpy(records[1].data.type_1.field_24,
           literal_type1_24, sizeof(literal_type1_24));
    records[1].data.type_1.fixed_data[0] = 1;
    records[1].data.type_1.fixed_data[1] = 2;
    records[1].data.type_1.fixed_data[2] = 3;

    initialize_record(&records[2], 2);
    records[2].data.type_2.identifier = 42;
    memcpy(&records[2].data.type_2.measurements[0],
           literal_type2_first, sizeof(literal_type2_first));
    memcpy(&records[2].data.type_2.measurements[4],
           literal_type2_last, sizeof(literal_type2_last));
    records[2].data.type_2.trailing_value = 330;

    initialize_record(&records[3], 3);
    records[3].data.type_3.identifier = 7;
    memcpy(records[3].data.type_3.text,
           literal_type3_text, sizeof(literal_type3_text));
    memcpy(records[3].data.type_3.text + 16, "der test", 8);
    records[3].data.type_3.length =
        (uint16_t)strlen(records[3].data.type_3.text);

    initialize_record(&records[4], 4);
    memcpy(records[4].data.bytes,
           literal_type4_first, sizeof(literal_type4_first));
    memcpy(records[4].data.bytes + 8,
           literal_type4_middle, sizeof(literal_type4_middle));
    memcpy(records[4].data.bytes + 24,
           literal_type4_last, sizeof(literal_type4_last));

    initialize_record(&records[5], 5);
    records[5].data.type_5.status = -11;
    memcpy(records[5].data.bytes + 4,
           literal_type5_first, sizeof(literal_type5_first));
    records[5].data.type_5.data[12] = 'd';
    records[5].data.type_5.data[13] = 'e';

    for (size_t i = 0; i < RECORD_COUNT; ++i) {
        const record *item = &records[i];
        uint32_t hash = record_hash(item);

        printf("%u %" PRIu64 " %u %u ",
               item->sequence, item->timestamp, hash, item->type);

        switch (item->type) {
        case 0:
            printf("%" PRIx64 "\n", item->data.type_0.value);
            break;

        case 1:
            printf("%s %u %u\n",
                   item->data.type_1.name,
                   load_u32(item->data.type_1.fixed_data),
                   item->data.type_1.field_24[0]);
            break;

        case 2:
            printf("%d %g %g %g %g %g %g %u\n",
                   item->data.type_2.identifier,
                   (double)item->data.type_2.measurements[0],
                   (double)item->data.type_2.measurements[1],
                   (double)item->data.type_2.measurements[2],
                   (double)item->data.type_2.measurements[3],
                   (double)item->data.type_2.measurements[4],
                   (double)item->data.type_2.measurements[5],
                   item->data.type_2.trailing_value);
            break;

        case 3:
            printf("%d %u %s\n",
                   item->data.type_3.identifier,
                   item->data.type_3.length,
                   item->data.type_3.text);
            break;

        case 4:
            printf("%d %s\n",
                   item->data.type_4.value,
                   item->data.type_4.text);
            break;

        case 5:
            printf("%d %u %u %u\n",
                   item->data.type_5.status,
                   item->data.type_5.length,
                   item->data.type_5.data[0],
                   item->data.type_5.data[1]);
            break;

        default:
            break;
        }

        total += record_hash(item);
    }

    printf("sum=%d\n", (int32_t)total);
    return 0;
}