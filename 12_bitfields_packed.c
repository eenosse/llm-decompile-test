/*
 * 12_bitfields_packed.c
 * Target feature: bit-fields of assorted widths and base types, packed
 * (attribute) structs with no padding, and a union that overlays a raw byte
 * array on a bit-field record - a classic on-disk / on-wire descriptor.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

/* Naturally aligned bit-field record. */
typedef struct PageEntry {
    uint64_t present    : 1;
    uint64_t writable   : 1;
    uint64_t user       : 1;
    uint64_t write_thru : 1;
    uint64_t no_cache   : 1;
    uint64_t accessed   : 1;
    uint64_t dirty      : 1;
    uint64_t huge       : 1;
    uint64_t global     : 1;
    uint64_t avail_lo   : 3;
    uint64_t frame      : 40;
    uint64_t avail_hi   : 11;
    uint64_t no_exec    : 1;
} PageEntry;

/* Signed bit-fields and a mix of base types. */
typedef struct SensorSample {
    unsigned int  channel   : 4;
    signed int    temp_c    : 12;   /* signed, -2048..2047 */
    unsigned int  humidity  : 7;
    signed int    trend     : 3;    /* -4..3 */
    unsigned int  valid     : 1;
    unsigned int  timestamp : 32;
    unsigned char quality   : 5;
} SensorSample;

/* Packed: byte-exact wire layout, unaligned members. */
#pragma pack(push, 1)
typedef struct WireHeader {
    uint8_t  version;
    uint32_t stream_id;      /* unaligned at offset 1 */
    uint16_t seq;
    uint8_t  flags;
    uint64_t nanos;          /* unaligned at offset 8 */
} WireHeader;

typedef struct WireRecord {
    WireHeader hdr;
    int16_t    values[5];
    uint8_t    checksum;
} WireRecord;
#pragma pack(pop)

/* Same bits seen two ways. */
typedef union DescriptorView {
    struct {
        uint32_t opcode : 6;
        uint32_t dst    : 5;
        uint32_t src1   : 5;
        uint32_t src2   : 5;
        uint32_t imm    : 10;
        uint32_t hi;
    } fields;
    uint64_t raw;
    uint8_t  bytes[8];
    uint16_t words[4];
} DescriptorView;

static PageEntry pte_make(uint64_t phys, int writable, int user, int huge)
{
    PageEntry e;
    memset(&e, 0, sizeof(e));
    e.present  = 1;
    e.writable = (uint64_t)(writable != 0);
    e.user     = (uint64_t)(user != 0);
    e.huge     = (uint64_t)(huge != 0);
    e.frame    = (phys >> 12) & 0xffffffffffull;
    e.no_exec  = user ? 1 : 0;
    e.avail_lo = 5;
    return e;
}

static void pte_dump(const char *tag, const PageEntry *e)
{
    uint64_t raw;
    BENCH_VERBOSE_ARG(tag);
    memcpy(&raw, e, sizeof(raw));
    BENCH_OUTPUT(
        printf("%s raw=%016llx present=%llu w=%llu u=%llu huge=%llu nx=%llu frame=%llx phys=%llx\n",
               tag, (unsigned long long)raw, (unsigned long long)e->present,
               (unsigned long long)e->writable, (unsigned long long)e->user,
               (unsigned long long)e->huge, (unsigned long long)e->no_exec,
               (unsigned long long)e->frame,
               (unsigned long long)(e->frame << 12)),
        printf("%016llx %llu %llu %llu %llu %llu %llx %llx\n",
               (unsigned long long)raw, (unsigned long long)e->present,
               (unsigned long long)e->writable, (unsigned long long)e->user,
               (unsigned long long)e->huge, (unsigned long long)e->no_exec,
               (unsigned long long)e->frame,
               (unsigned long long)(e->frame << 12)));
}

static void sample_fill(SensorSample *s, unsigned ch, int temp, unsigned hum, int trend, uint32_t ts)
{
    s->channel   = ch & 0xf;
    s->temp_c    = temp;
    s->humidity  = hum & 0x7f;
    s->trend     = trend;
    s->valid     = 1;
    s->timestamp = ts;
    s->quality   = (unsigned char)((hum + (unsigned)ch) & 0x1f);
}

static void record_build(WireRecord *r, uint32_t stream, uint16_t seq, const SensorSample *s)
{
    const uint8_t *p = (const uint8_t *)r;
    uint8_t sum = 0;
    size_t i;

    memset(r, 0, sizeof(*r));
    r->hdr.version   = 2;
    r->hdr.stream_id = stream;
    r->hdr.seq       = seq;
    r->hdr.flags     = (uint8_t)(s->valid ? 0x81 : 0x01);
    r->hdr.nanos     = (uint64_t)s->timestamp * 1000000000ull + seq;
    r->values[0] = (int16_t)s->temp_c;
    r->values[1] = (int16_t)s->humidity;
    r->values[2] = (int16_t)s->trend;
    r->values[3] = (int16_t)s->quality;
    r->values[4] = (int16_t)(s->channel * 100);
    for (i = 0; i < sizeof(*r) - 1; i++)
        sum = (uint8_t)(sum * 31u + p[i]);
    r->checksum = sum;
}

int main(void)
{
    PageEntry pte_kernel, pte_user;
    SensorSample samples[4];
    WireRecord rec;
    DescriptorView dv;
    unsigned i;

    pte_kernel = pte_make(0x0000000123456000ull, 1, 0, 0);
    pte_user   = pte_make(0x00007fff88200000ull, 1, 1, 1);
    pte_dump("kernel pte:", &pte_kernel);
    pte_dump("user   pte:", &pte_user);

    sample_fill(&samples[0], 1, -253, 66, -2, 1700000000u);
    sample_fill(&samples[1], 3,  412, 41,  3, 1700000060u);
    sample_fill(&samples[2], 7,    0, 99,  0, 1700000120u);
    sample_fill(&samples[3], 15, -2048, 127, -4, 1700000180u);

    for (i = 0; i < 4; i++) {
        const SensorSample *s = &samples[i];
        BENCH_OUTPUT(
            printf("sample ch=%2u temp=%5d hum=%3u trend=%+d valid=%u q=%2u ts=%u\n",
                   s->channel, s->temp_c, s->humidity, s->trend, s->valid,
                   s->quality, s->timestamp),
            printf("%u %d %u %d %u %u %u\n", s->channel, s->temp_c,
                   s->humidity, s->trend, s->valid, s->quality, s->timestamp));
    }

    record_build(&rec, 0xAABBCCDDu, 7, &samples[1]);
    BENCH_OUTPUT(
        printf("wire hdr: ver=%u stream=%08x seq=%u flags=%02x nanos=%llu cksum=%02x\n",
               rec.hdr.version, rec.hdr.stream_id, rec.hdr.seq, rec.hdr.flags,
               (unsigned long long)rec.hdr.nanos, rec.checksum),
        printf("%u %08x %u %02x %llu %02x\n", rec.hdr.version,
               rec.hdr.stream_id, rec.hdr.seq, rec.hdr.flags,
               (unsigned long long)rec.hdr.nanos, rec.checksum));
    BENCH_VERBOSE(printf("wire bytes:"));
    for (i = 0; i < sizeof(rec); i++)
        printf(" %02x", ((const uint8_t *)&rec)[i]);
    putchar('\n');

    dv.raw = 0;
    dv.fields.opcode = 0x2b;
    dv.fields.dst    = 9;
    dv.fields.src1   = 3;
    dv.fields.src2   = 17;
    dv.fields.imm    = 613;
    dv.fields.hi     = 0xfeedface;
    BENCH_OUTPUT(
        printf("descriptor raw=%016llx words=%04x %04x %04x %04x\n",
               (unsigned long long)dv.raw, dv.words[0], dv.words[1],
               dv.words[2], dv.words[3]),
        printf("%016llx %04x %04x %04x %04x\n",
               (unsigned long long)dv.raw, dv.words[0], dv.words[1],
               dv.words[2], dv.words[3]));
    BENCH_OUTPUT(
        printf("decoded: op=%u dst=r%u src1=r%u src2=r%u imm=%u\n",
               dv.fields.opcode, dv.fields.dst, dv.fields.src1,
               dv.fields.src2, dv.fields.imm),
        printf("%u %u %u %u %u\n", dv.fields.opcode, dv.fields.dst,
               dv.fields.src1, dv.fields.src2, dv.fields.imm));

    return 0;
}
