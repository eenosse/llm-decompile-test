#include <stdint.h>
#include <stdio.h>

union bit_view {
    uint64_t raw;
    struct {
        unsigned long long bit0 : 1;
        unsigned long long bit1 : 1;
        unsigned long long bit2 : 1;
        unsigned long long reserved0 : 4;
        unsigned long long bit7 : 1;
        unsigned long long reserved1 : 4;
        unsigned long long field40 : 40;
        unsigned long long reserved2 : 11;
        unsigned long long bit63 : 1;
    } bits;
};

static void print_bit_view(const union bit_view *value)
{
    unsigned long long field = value->bits.field40;

    printf("raw=%016llx %u %u %u %u %u %010llx %010llx\n",
           (unsigned long long)value->raw,
           (unsigned)value->bits.bit0,
           (unsigned)value->bits.bit1,
           (unsigned)value->bits.bit2,
           (unsigned)value->bits.bit7,
           (unsigned)value->bits.bit63,
           field,
           (field << 12) & 0xffffffffffULL);
}

struct record {
    unsigned int field0 : 4;
    signed int field1 : 12;
    unsigned int field2 : 7;
    signed int field3 : 3;
    unsigned int field4 : 1;
    uint32_t value;
    unsigned int field5 : 5;
};

struct __attribute__((packed)) packet {
    uint8_t type;
    uint32_t code;
    uint16_t length;
    uint8_t flags;
    uint64_t identifier;
    double measurement;
    uint16_t amount;
    uint8_t checksum;
};

union word_view {
    unsigned long long whole;
    unsigned short words[4];
};

struct compact_fields {
    unsigned int field0 : 6;
    unsigned int field1 : 4;
    unsigned int field2 : 2;
    unsigned int field3 : 5;
    unsigned int field4 : 10;
};

int main(void)
{
    union bit_view views[2] = {
        { .raw = 0x0000000123456a03ULL },
        { .raw = 0x80007fff88200a87ULL }
    };

    print_bit_view(&views[0]);
    print_bit_view(&views[1]);

    struct record records[4] = {
        { 1, -253, 66, -2, 1, 1700000000U, 3 },
        { 3, 412, 41, 3, 1, 1700000060U, 12 },
        { 7, 0, 99, 0, 1, 1700000120U, 10 },
        { 15, -2048, 127, -4, 1, 1700000180U, 14 }
    };

    for (unsigned int i = 0; i < 4; ++i) {
        printf("%u %d %u %d %u %u %u\n",
               records[i].field0,
               records[i].field1,
               records[i].field2,
               records[i].field3,
               records[i].field4,
               records[i].field5,
               records[i].value);
    }

    struct packet packet = {
        .type = 2,
        .code = UINT32_C(0xaabbccdd),
        .length = 7,
        .flags = UINT8_C(0x81),
        .identifier = UINT64_C(0x17979d0c2e715807),
        .measurement = 3.141592653589793,
        .amount = 300,
        .checksum = 0
    };

    uint32_t accumulator = 0;
    const unsigned char *bytes = (const unsigned char *)&packet;

    for (unsigned int i = 0; i < sizeof packet - 1; ++i)
        accumulator = accumulator * 31U + bytes[i];

    packet.checksum = (uint8_t)accumulator;

    printf("%u %08x %u %02x %016lx %u\n",
           (unsigned)packet.type,
           packet.code,
           (unsigned)packet.length,
           (unsigned)packet.flags,
           (unsigned long)packet.identifier,
           (unsigned)packet.checksum);

    for (unsigned int i = 0; i < sizeof packet; ++i)
        printf("%02x ", ((const unsigned char *)&packet)[i]);
    putc('\n', stdout);

    union word_view words = {
        .whole = 0xfeedface4cb11a6bULL
    };

    printf("%016llx %04x %04x %04x %04x\n",
           words.whole,
           (unsigned)words.words[0],
           (unsigned)words.words[1],
           (unsigned)words.words[2],
           (unsigned)words.words[3]);

    struct compact_fields fields = {
        .field0 = 43,
        .field1 = 9,
        .field2 = 3,
        .field3 = 17,
        .field4 = 613
    };

    printf("%u %u %u %u %u\n",
           fields.field0,
           fields.field1,
           fields.field2,
           fields.field3,
           fields.field4);

    return 0;
}