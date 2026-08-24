#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t data[12];
} Record;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint32_t identifier;
    uint16_t sequence;
    uint8_t flags;
    uint64_t timestamp;
    int16_t value;
    uint16_t code;
    int16_t adjustment;
    uint16_t combined;
    uint16_t scaled_kind;
    uint8_t checksum;
} Packet;

_Static_assert(sizeof(Record) == 12, "unexpected Record size");
_Static_assert(sizeof(Packet) == 27, "unexpected Packet size");

static uint16_t load_u16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void store_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void store_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint32_t load_u32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static unsigned record_kind(const Record *record)
{
    return record->data[0] & 0x0fU;
}

static int record_value(const Record *record)
{
    unsigned value = load_u16(record->data) >> 4;

    if (value & 0x800U)
        value |= ~0xfffU;

    return (int)value;
}

static unsigned record_code(const Record *record)
{
    return record->data[2] & 0x7fU;
}

static int record_adjustment(const Record *record)
{
    unsigned value = (load_u16(record->data + 2) >> 7) & 7U;

    if (value & 4U)
        value |= ~7U;

    return (int)value;
}

static unsigned record_active(const Record *record)
{
    return (record->data[3] >> 2) & 1U;
}

static unsigned record_combined(const Record *record)
{
    return record->data[8] & 0x1fU;
}

static uint32_t record_timestamp(const Record *record)
{
    return load_u32(record->data + 4);
}

static uint64_t make_descriptor(uintptr_t address, int writable,
                                int user_accessible, int large_page)
{
    uint64_t descriptor = 0;
    uint64_t page_number;

    descriptor |= UINT64_C(1);
    descriptor |= (uint64_t)(writable != 0) << 1;
    descriptor |= (uint64_t)(user_accessible != 0) << 2;
    descriptor |= (uint64_t)(large_page != 0) << 7;
    descriptor |= UINT64_C(0x0a00);

    page_number = ((uint64_t)address >> 12) & UINT64_C(0xffffffffff);
    descriptor |= page_number << 12;

    descriptor |= (uint64_t)(user_accessible != 0) << 63;
    return descriptor;
}

static void print_descriptor(const char *unused_label,
                             const uint64_t *descriptor_pointer)
{
    uint64_t raw = *descriptor_pointer;
    uint64_t page_number =
        (raw >> 12) & UINT64_C(0xffffffffff);
    uint64_t physical_address =
        (page_number << 12) & UINT64_C(0xffffffffff);

    (void)unused_label;

    printf("%016" PRIx64 " %u %u %u %u %u %010" PRIx64
           " %010" PRIx64 "\n",
           raw,
           (unsigned)(raw & 1U),
           (unsigned)((raw >> 1) & 1U),
           (unsigned)((raw >> 2) & 1U),
           (unsigned)((raw >> 7) & 1U),
           (unsigned)((raw >> 63) & 1U),
           page_number,
           physical_address);
}

static void initialize_record(Record *record, int kind, int value,
                              int code, int adjustment,
                              uint32_t timestamp)
{
    uint16_t word;

    word = load_u16(record->data);
    word = (uint16_t)((word & 0xfff0U) | ((unsigned)kind & 0x0fU));
    store_u16(record->data, word);

    word = load_u16(record->data);
    word = (uint16_t)((word & 0x000fU)
                     | (((uint16_t)value & 0x0fffU) << 4));
    store_u16(record->data, word);

    record->data[2] =
        (uint8_t)((record->data[2] & 0x80U) | ((unsigned)code & 0x7fU));

    word = load_u16(record->data + 2);
    word = (uint16_t)((word & 0xfc7fU)
                     | (((unsigned)adjustment & 7U) << 7));
    store_u16(record->data + 2, word);

    record->data[3] |= 0x04U;
    store_u32(record->data + 4, timestamp);

    record->data[8] =
        (uint8_t)((record->data[8] & 0xe0U)
                  | ((unsigned)(kind + code) & 0x1fU));
}

static void build_packet(Packet *packet, uint32_t identifier,
                         uint16_t sequence, const Record *record)
{
    uint8_t checksum = 0;
    size_t i;

    memset(packet, 0, sizeof(*packet));

    packet->type = 2;
    packet->identifier = identifier;
    packet->sequence = sequence;
    packet->flags = record_active(record) ? 0x81U : 0x01U;
    packet->timestamp =
        (uint64_t)record_timestamp(record) * UINT64_C(1000000000)
        + sequence;
    packet->value = (int16_t)record_value(record);
    packet->code = (uint16_t)record_code(record);
    packet->adjustment = (int16_t)record_adjustment(record);
    packet->combined = (uint16_t)record_combined(record);
    packet->scaled_kind = (uint16_t)(record_kind(record) * 100U);

    for (i = 0; i <= 25; ++i)
        checksum = (uint8_t)(checksum * 31U
                             + ((const uint8_t *)packet)[i]);

    packet->checksum = checksum;
}

int main(void)
{
    uint64_t first_descriptor;
    uint64_t second_descriptor;
    Record records[4] = {{{0}}};
    Packet packet;
    uint64_t packed = 0;
    uint16_t word;
    unsigned i;

    first_descriptor =
        make_descriptor((uintptr_t)UINT64_C(0x123456000), 1, 0, 0);
    second_descriptor =
        make_descriptor((uintptr_t)UINT64_C(0x7fff88200000), 1, 1, 1);

    print_descriptor("first entry", &first_descriptor);
    print_descriptor("second entry", &second_descriptor);

    initialize_record(&records[0], 1, -253, 66, -2, UINT32_C(0x6553f100));
    initialize_record(&records[1], 3, 412, 41, 3, UINT32_C(0x6553f13c));
    initialize_record(&records[2], 7, 0, 99, 0, UINT32_C(0x6553f178));
    initialize_record(&records[3], 15, -2048, 127, -4,
                      UINT32_C(0x6553f1b4));

    for (i = 0; i <= 3; ++i) {
        printf("%u %d %u %d %u %u %u\n",
               record_kind(&records[i]),
               record_value(&records[i]),
               record_code(&records[i]),
               record_adjustment(&records[i]),
               record_active(&records[i]),
               record_combined(&records[i]),
               record_timestamp(&records[i]));
    }

    build_packet(&packet, UINT32_C(0xaabbccdd), 7, &records[1]);

    printf("%u %08x %u %02x %" PRIu64 " %02x\n",
           (unsigned)packet.type,
           (unsigned)packet.identifier,
           (unsigned)packet.sequence,
           (unsigned)packet.flags,
           packet.timestamp,
           (unsigned)packet.checksum);

    for (i = 0; i <= 26; ++i)
        printf("%02x ", (unsigned)((const uint8_t *)&packet)[i]);
    putchar('\n');

    packed = (packed & ~UINT64_C(0x3f)) | UINT64_C(43);
    packed = (packed & ~UINT64_C(0x7c0)) | UINT64_C(0x240);
    packed = (packed & ~UINT64_C(0xf800)) | UINT64_C(0x1800);
    packed = (packed & ~UINT64_C(0x1f0000)) | UINT64_C(0x110000);
    packed = (packed & ~UINT64_C(0x7fe00000)) | UINT64_C(0x4ca00000);
    packed = (packed & UINT64_C(0x00000000ffffffff))
           | (UINT64_C(0xfeedface) << 32);

    printf("%016" PRIx64 " %04x %04x %04x %04x\n",
           packed,
           (unsigned)(packed & 0xffffU),
           (unsigned)((packed >> 16) & 0xffffU),
           (unsigned)((packed >> 32) & 0xffffU),
           (unsigned)((packed >> 48) & 0xffffU));

    word = (uint16_t)(packed >> 16);

    printf("%u %u %u %u %u\n",
           (unsigned)(packed & 0x3fU),
           (unsigned)((packed >> 6) & 0x1fU),
           (unsigned)((packed >> 11) & 0x1fU),
           (unsigned)(word & 0x1fU),
           (unsigned)((word >> 5) & 0x3ffU));

    return 0;
}