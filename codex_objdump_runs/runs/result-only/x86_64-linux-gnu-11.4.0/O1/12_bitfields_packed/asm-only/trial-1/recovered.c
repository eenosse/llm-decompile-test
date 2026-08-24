#include <stdint.h>
#include <stdio.h>

typedef union {
    uint64_t raw;
    struct {
        unsigned long flag0 : 1;
        unsigned long flag1 : 1;
        unsigned long flag2 : 1;
        unsigned long reserved0 : 4;
        unsigned long flag7 : 1;
        unsigned long reserved1 : 1;
        unsigned long mode : 3;
        unsigned long payload : 40;
        unsigned long reserved2 : 11;
        unsigned long flag63 : 1;
    } bits;
} encoded_word;

typedef struct {
    unsigned short field0 : 4;
    signed short field1 : 12;

    unsigned short field2 : 7;
    signed short field3 : 3;
    unsigned short flag : 1;
    unsigned short reserved0 : 5;

    uint32_t timestamp;

    unsigned int field4 : 5;
    unsigned int reserved1 : 27;
} record;

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint32_t identifier;
    uint16_t count;
    uint8_t flags;
    uint64_t nonce;
    uint16_t values[5];
    uint8_t checksum;
} packet;

typedef union {
    uint64_t whole;
    struct {
        uint16_t part0;
        uint16_t part1;
        uint16_t part2;
        uint16_t part3;
    } parts;
} split_word;

typedef struct {
    unsigned int field0 : 6;
    unsigned int field1 : 4;
    unsigned int field2 : 2;
    unsigned int field3 : 5;
    unsigned int field4 : 10;
    unsigned int reserved : 5;
} compact_fields;

static const char example_one[12] = "example one";
static const char example_two[12] = "example two";

static void print_encoded_word(const char *unused, const encoded_word *word)
{
    unsigned long shifted;

    (void)unused;
    shifted = (word->bits.payload << 12) & 0xffffffffffUL;

    printf("raw: %016lx %u %u %u %u %u %010lx %010lx\n",
           (unsigned long)word->raw,
           (unsigned)word->bits.flag0,
           (unsigned)word->bits.flag1,
           (unsigned)word->bits.flag2,
           (unsigned)word->bits.flag7,
           (unsigned)word->bits.flag63,
           (unsigned long)word->bits.payload,
           shifted);
}

int main(void)
{
    encoded_word first = { .raw = 0 };
    encoded_word second = { .raw = 0 };
    record records[4];
    packet message;
    split_word split;
    compact_fields compact;
    const unsigned char *bytes;
    unsigned int i;
    uint8_t checksum;

    first.bits.flag0 = 1;
    first.bits.flag1 = 1;
    first.bits.payload = 0x123456UL;
    first.bits.mode = 5;

    second.bits.flag0 = 1;
    second.bits.flag1 = 1;
    second.bits.flag2 = 1;
    second.bits.flag7 = 1;
    second.bits.payload = 0x7fff88200UL;
    second.bits.flag63 = 1;
    second.bits.mode = 5;

    print_encoded_word(example_one, &first);
    print_encoded_word(example_two, &second);

    records[0].field0 = 1;
    records[0].field1 = -253;
    records[0].field2 = 66;
    records[0].field3 = -2;
    records[0].flag = 1;
    records[0].timestamp = 0x6553f100U;
    records[0].field4 = 3;

    records[1].field0 = 3;
    records[1].field1 = 412;
    records[1].field2 = 41;
    records[1].field3 = 3;
    records[1].flag = 1;
    records[1].timestamp = 0x6553f13cU;
    records[1].field4 = 12;

    records[2].field0 = 7;
    records[2].field1 = 0;
    records[2].field2 = 99;
    records[2].field3 = 0;
    records[2].flag = 1;
    records[2].timestamp = 0x6553f178U;
    records[2].field4 = 10;

    records[3].field0 = 15;
    records[3].field1 = -2048;
    records[3].field2 = 127;
    records[3].field3 = -4;
    records[3].flag = 1;
    records[3].timestamp = 0x6553f1b4U;
    records[3].field4 = 14;

    for (i = 0; i < 4; ++i) {
        printf("%u %d %u %d %u %u %u\n",
               (unsigned)records[i].field0,
               (int)records[i].field1,
               (unsigned)records[i].field2,
               (int)records[i].field3,
               (unsigned)records[i].flag,
               records[i].timestamp,
               (unsigned)records[i].field4);
    }

    message.checksum = 0;
    message.type = 2;
    message.identifier = 0xaabbccddU;
    message.count = 7;
    message.flags = 0x81;
    message.nonce = UINT64_C(0x17979d0c2e715807);
    message.values[0] = 0x019c;
    message.values[1] = 0x0029;
    message.values[2] = 0x0003;
    message.values[3] = 0x000c;
    message.values[4] = 0x012c;

    bytes = (const unsigned char *)&message;
    checksum = 0;
    for (i = 0; i < sizeof(message) - 1; ++i)
        checksum = (uint8_t)(checksum * 31U + bytes[i]);
    message.checksum = checksum;

    printf("%u %08x %u %02x %016lx %u\n",
           (unsigned)message.type,
           message.identifier,
           (unsigned)message.count,
           (unsigned)message.flags,
           (unsigned long)message.nonce,
           (unsigned)message.checksum);

    bytes = (const unsigned char *)&message;
    for (i = 0; i < sizeof(message); ++i)
        printf("%02x ", (unsigned)bytes[i]);
    putc('\n', stdout);

    split.whole = UINT64_C(0xfeedface4cb11a6b);
    printf("%016lx: %04x %04x %04x %04x\n",
           (unsigned long)split.whole,
           (unsigned)split.parts.part0,
           (unsigned)split.parts.part1,
           (unsigned)split.parts.part2,
           (unsigned)split.parts.part3);

    compact.field0 = 0x2b;
    compact.field1 = 9;
    compact.field2 = 3;
    compact.field3 = 0x11;
    compact.field4 = 0x265;

    printf("%u %u %u %u %u\n",
           compact.field0,
           compact.field1,
           compact.field2,
           compact.field3,
           compact.field4);

    return 0;
}