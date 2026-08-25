#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef union {
    uint64_t raw;
    struct {
        uint64_t flag0 : 1;
        uint64_t flag1 : 1;
        uint64_t flag2 : 1;
        uint64_t reserved0 : 4;
        uint64_t flag7 : 1;
        uint64_t reserved1 : 4;
        uint64_t value : 40;
        uint64_t reserved2 : 11;
        uint64_t flag63 : 1;
    } bits;
} bit_word;

struct sample {
    unsigned int field0 : 4;
    signed int field1 : 12;
    unsigned int field2 : 7;
    signed int field3 : 3;
    unsigned int field4 : 1;
    uint32_t timestamp;
    unsigned int field5 : 5;
};

struct __attribute__((packed)) packet {
    uint8_t type;
    uint32_t identifier;
    uint16_t sequence;
    uint8_t flags;
    uint64_t token;
    uint16_t readings[5];
    uint8_t checksum;
};

union split_word {
    unsigned long long whole;
    uint16_t part[4];
};

_Static_assert(sizeof(bit_word) == 8, "unexpected bit-word layout");
_Static_assert(sizeof(struct sample) == 12, "unexpected sample layout");
_Static_assert(offsetof(struct packet, checksum) == 26,
               "unexpected packet layout");
_Static_assert(sizeof(struct packet) == 27, "unexpected packet size");

static void show_word(const bit_word *word)
{
    const uint64_t mask40 = UINT64_C(0xffffffffff);
    uint64_t value = word->bits.value;
    uint64_t shifted = (value << 12) & mask40;

    printf("0x%016" PRIx64 " %u %u %u %u %u 0x%010" PRIx64
           " 0x%010" PRIx64 "\n",
           word->raw,
           (unsigned)word->bits.flag0,
           (unsigned)word->bits.flag1,
           (unsigned)word->bits.flag2,
           (unsigned)word->bits.flag7,
           (unsigned)word->bits.flag63,
           value,
           shifted);
}

int main(void)
{
    bit_word words[2] = {
        { .raw = UINT64_C(0x0000000123456a03) },
        { .raw = UINT64_C(0x80007fff88200a87) }
    };
    struct sample samples[4];
    struct packet message;
    union split_word split;
    uint32_t hash;
    size_t i;
    const uint8_t *bytes;

    show_word(&words[0]);
    show_word(&words[1]);

    samples[0].field0 = 1;
    samples[0].field1 = -253;
    samples[0].field2 = 66;
    samples[0].field3 = -2;
    samples[0].field4 = 1;
    samples[0].timestamp = UINT32_C(0x6553f100);
    samples[0].field5 = 3;

    samples[1].field0 = 3;
    samples[1].field1 = 412;
    samples[1].field2 = 41;
    samples[1].field3 = 3;
    samples[1].field4 = 1;
    samples[1].timestamp = UINT32_C(0x6553f13c);
    samples[1].field5 = 12;

    samples[2].field0 = 7;
    samples[2].field1 = 0;
    samples[2].field2 = 99;
    samples[2].field3 = 0;
    samples[2].field4 = 1;
    samples[2].timestamp = UINT32_C(0x6553f178);
    samples[2].field5 = 10;

    samples[3].field0 = 15;
    samples[3].field1 = -2048;
    samples[3].field2 = 127;
    samples[3].field3 = -4;
    samples[3].field4 = 1;
    samples[3].timestamp = UINT32_C(0x6553f1b4);
    samples[3].field5 = 14;

    for (i = 0; i < 4; ++i) {
        printf("%u %d %u %d %u %u %u\n",
               samples[i].field0,
               samples[i].field1,
               samples[i].field2,
               samples[i].field3,
               samples[i].field4,
               samples[i].field5,
               samples[i].timestamp);
    }

    message.type = 2;
    message.identifier = UINT32_C(0xaabbccdd);
    message.sequence = 7;
    message.flags = UINT8_C(0x81);
    message.token = UINT64_C(0x17979d0c2e715807);
    message.readings[0] = 412;
    message.readings[1] = 41;
    message.readings[2] = 3;
    message.readings[3] = 12;
    message.readings[4] = 300;

    hash = 0;
    bytes = (const uint8_t *)&message;
    for (i = 0; i < offsetof(struct packet, checksum); ++i)
        hash = hash * UINT32_C(31) + bytes[i];
    message.checksum = (uint8_t)hash;

    printf("%u %08x %u %02x %016" PRIx64 " %u\n",
           (unsigned)message.type,
           message.identifier,
           (unsigned)message.sequence,
           (unsigned)message.flags,
           message.token,
           (unsigned)message.checksum);

    bytes = (const uint8_t *)&message;
    for (i = 0; i < sizeof message; ++i)
        printf("%02x ", (unsigned)bytes[i]);
    putc('\n', stdout);

    split.whole = 0xfeedface4cb11a6bULL;
    printf("%016llx %04x %04x %04x %04x\n",
           split.whole,
           (unsigned)split.part[0],
           (unsigned)split.part[1],
           (unsigned)split.part[2],
           (unsigned)split.part[3]);

    printf("%u %u %u %u %u\n", 43U, 9U, 3U, 17U, 613U);

    return 0;
}