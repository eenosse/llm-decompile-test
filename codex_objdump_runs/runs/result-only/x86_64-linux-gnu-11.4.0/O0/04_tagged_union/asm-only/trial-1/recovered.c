#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    TYPE_REFERENCE = 0,
    TYPE_NAMED = 1,
    TYPE_VECTOR = 2,
    TYPE_MESSAGE = 3,
    TYPE_BYTES = 4,
    TYPE_TEXT = 5
};

typedef struct {
    uint32_t type;
    uint32_t id;
    uint64_t timestamp;

    union {
        uint64_t reference;

        struct {
            char name[24];
            uint8_t digest[16];
            uint32_t flags;
        } named;

        struct {
            int32_t value;
            float x;
            float y;
            float z;
            float adjusted_x;
            float adjusted_y;
            float adjusted_z;
            uint16_t code;
        } vector;

        struct {
            int32_t channel;
            uint16_t length;
            char text[96];
        } message;

        struct {
            int32_t seed;
            uint32_t length;
            uint8_t data[64];
        } bytes;

        struct {
            int32_t code;
            char text[40];
        } text;
    } payload;
} record_t;

_Static_assert(sizeof(record_t) == 120, "unexpected record layout");

static uint32_t next_id;

static void initialize_record(record_t *record, uint32_t type)
{
    memset(record, 0, sizeof(*record));
    record->type = type;
    record->id = next_id++;
    record->timestamp = UINT64_C(1600000000) + (uint64_t)record->id * UINT64_C(37);
}

static void initialize_reference(record_t *record, uint64_t reference)
{
    initialize_record(record, TYPE_REFERENCE);
    record->payload.reference = reference;
}

static void initialize_named(record_t *record, const char *name, uint32_t flags)
{
    int i;
    int length;

    initialize_record(record, TYPE_NAMED);
    strncpy(record->payload.named.name, name,
            sizeof(record->payload.named.name) - 1);

    length = (int)strlen(name);
    for (i = 0; i <= 15; ++i) {
        record->payload.named.digest[i] =
            (uint8_t)((i * 31) ^ (uint8_t)name[i % length]);
    }

    record->payload.named.flags = flags;
}

static void initialize_vector(record_t *record, int32_t value,
                              float dx, float dy, float dz)
{
    initialize_record(record, TYPE_VECTOR);

    record->payload.vector.value = value;
    record->payload.vector.x = (float)value;
    record->payload.vector.y = (float)value * 2.0f;
    record->payload.vector.z = 0.0f;

    record->payload.vector.adjusted_x = record->payload.vector.x + dx;
    record->payload.vector.adjusted_y = record->payload.vector.y + dy;
    record->payload.vector.adjusted_z = record->payload.vector.z + dz;
    record->payload.vector.code = (uint16_t)(value * 5 + 120);
}

static void initialize_message(record_t *record, int32_t channel,
                               const char *text)
{
    initialize_record(record, TYPE_MESSAGE);

    record->payload.message.channel = channel;
    strncpy(record->payload.message.text, text,
            sizeof(record->payload.message.text) - 1);
    record->payload.message.length =
        (uint16_t)strlen(record->payload.message.text);
}

static void initialize_bytes(record_t *record, int32_t seed, uint32_t length)
{
    uint32_t i;

    initialize_record(record, TYPE_BYTES);

    if (length > 64)
        length = 64;

    record->payload.bytes.seed = seed;
    record->payload.bytes.length = length;

    for (i = 0; i < length; ++i) {
        record->payload.bytes.data[i] =
            (uint8_t)(seed * 13 + (int32_t)i * 5);
    }
}

static void initialize_text(record_t *record, int32_t code, const char *text)
{
    initialize_record(record, TYPE_TEXT);

    record->payload.text.code = code;
    strncpy(record->payload.text.text, text,
            sizeof(record->payload.text.text) - 1);
}

static uint32_t record_hash(const record_t *record)
{
    uint32_t hash = record->type * UINT32_C(0x9e3779b1) + record->id;
    uint32_t i;

    switch (record->type) {
    case TYPE_REFERENCE: {
        uint64_t value = record->payload.reference;
        hash ^= (uint32_t)(value ^ (value >> 32));
        break;
    }

    case TYPE_NAMED:
        for (i = 0; i <= 15; ++i)
            hash = hash * 31 + record->payload.named.digest[i];
        hash ^= record->payload.named.flags;
        break;

    case TYPE_VECTOR:
        hash ^= (uint32_t)record->payload.vector.value;
        hash += (uint32_t)(int64_t)
            (1000.0f * record->payload.vector.adjusted_x);
        hash += (uint32_t)(int64_t)
            (1000.0f * record->payload.vector.adjusted_y);
        hash += record->payload.vector.code;
        break;

    case TYPE_MESSAGE:
        hash ^= (uint32_t)record->payload.message.channel;
        for (i = 0; i < record->payload.message.length; ++i) {
            hash = hash * 131 +
                (uint8_t)record->payload.message.text[i];
        }
        break;

    case TYPE_BYTES:
        hash ^= (uint32_t)record->payload.bytes.seed;
        for (i = 0; i < record->payload.bytes.length; ++i)
            hash = hash * 17 + record->payload.bytes.data[i];
        break;

    case TYPE_TEXT:
        hash ^= (uint32_t)record->payload.text.code;
        for (i = 0; record->payload.text.text[i] != '\0'; ++i) {
            hash = hash * 7 +
                (uint8_t)record->payload.text.text[i];
        }
        break;

    default:
        break;
    }

    return hash;
}

static void print_record(const record_t *record)
{
    uint32_t hash = record_hash(record);

    printf("%u %llu %08x %u\n",
           record->id,
           (unsigned long long)record->timestamp,
           hash,
           record->type);

    switch (record->type) {
    case TYPE_REFERENCE:
        printf("%016llx\n",
               (unsigned long long)record->payload.reference);
        break;

    case TYPE_NAMED:
        printf("%s %08x %02x\n",
               record->payload.named.name,
               record->payload.named.flags,
               (unsigned)record->payload.named.digest[0]);
        break;

    case TYPE_VECTOR:
        printf("%d %.2f %.2f %.2f %.2f %.2f %.2f %u\n",
               record->payload.vector.value,
               (double)record->payload.vector.x,
               (double)record->payload.vector.y,
               (double)record->payload.vector.z,
               (double)record->payload.vector.adjusted_x,
               (double)record->payload.vector.adjusted_y,
               (double)record->payload.vector.adjusted_z,
               (unsigned)record->payload.vector.code);
        break;

    case TYPE_MESSAGE:
        printf("%d %u %s\n",
               record->payload.message.channel,
               (unsigned)record->payload.message.length,
               record->payload.message.text);
        break;

    case TYPE_BYTES:
        printf("%d %u %02x %02x\n",
               record->payload.bytes.seed,
               record->payload.bytes.length,
               (unsigned)record->payload.bytes.data[0],
               (unsigned)record->payload.bytes.data[1]);
        break;

    case TYPE_TEXT:
        printf("%d %s\n",
               record->payload.text.code,
               record->payload.text.text);
        break;

    default:
        break;
    }
}

int main(void)
{
    record_t records[6];
    uint32_t total = 0;
    uint32_t i;

    initialize_reference(&records[0], UINT64_C(0xdeadbeefcafe1234));
    initialize_named(&records[1], "alpha-42", UINT32_C(0x00030201));
    initialize_vector(&records[2], 42, 1.5f, -2.25f, 3.75f);
    initialize_message(&records[3], 7, "hello from variant three");
    initialize_bytes(&records[4], 99, 24);
    initialize_text(&records[5], -11, "negative code path");

    for (i = 0; i <= 5; ++i) {
        print_record(&records[i]);
        total += record_hash(&records[i]);
    }

    printf("%08x\n", total);
    return 0;
}