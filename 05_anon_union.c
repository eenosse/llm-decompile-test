/*
 * 05_anon_union.c
 * Target feature: anonymous unions and anonymous structs nested inside a
 * named struct, plus a union used purely for type punning (float/uint32,
 * double/uint64) and an overlapped register-file view.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef union FloatBits {
    float    f;
    uint32_t u;
    struct {
        uint32_t mantissa : 23;
        uint32_t exponent : 8;
        uint32_t sign     : 1;
    } parts;
    uint8_t bytes[4];
} FloatBits;

typedef union DoubleBits {
    double   d;
    uint64_t u;
    uint32_t halves[2];
} DoubleBits;

/* A 4-byte register that can be read as 32/16/8 bit pieces. */
typedef union Reg32 {
    uint32_t dword;
    struct { uint16_t lo16, hi16; };            /* anonymous struct */
    struct { uint8_t b0, b1, b2, b3; };         /* anonymous struct */
} Reg32;

typedef struct CpuState {
    Reg32 gpr[8];
    union {                                      /* anonymous union */
        uint32_t flags;
        struct {
            uint32_t carry    : 1;
            uint32_t zero     : 1;
            uint32_t sign     : 1;
            uint32_t overflow : 1;
            uint32_t reserved : 28;
        };
    };
    struct {                                     /* anonymous struct */
        uint32_t pc;
        uint32_t sp;
    };
    char tag[12];
} CpuState;

static void reg_show(const char *name, Reg32 r)
{
    BENCH_VERBOSE_ARG(name);
    BENCH_OUTPUT(
        printf("%s = %08x  lo16=%04x hi16=%04x  bytes=%02x %02x %02x %02x\n",
               name, r.dword, r.lo16, r.hi16, r.b0, r.b1, r.b2, r.b3),
        printf("%08x %04x %04x %02x %02x %02x %02x\n", r.dword,
               r.lo16, r.hi16, r.b0, r.b1, r.b2, r.b3));
}

static void cpu_reset(CpuState *st, const char *tag)
{
    unsigned i;
    memset(st, 0, sizeof(*st));
    for (i = 0; i < 8; i++)
        st->gpr[i].dword = 0x11111111u * (i + 1);
    st->pc = 0x00400500u;
    st->sp = 0x7ffff000u;
    strncpy(st->tag, tag, sizeof(st->tag) - 1);
}

static void cpu_add(CpuState *st, unsigned dst, unsigned src)
{
    uint64_t wide = (uint64_t)st->gpr[dst].dword + (uint64_t)st->gpr[src].dword;
    uint32_t res  = (uint32_t)wide;

    st->carry    = (wide >> 32) & 1u;
    st->zero     = (res == 0);
    st->sign     = (res >> 31) & 1u;
    st->overflow = 0;
    st->gpr[dst].dword = res;
    st->pc += 4;
}

static void cpu_swap_halves(CpuState *st, unsigned r)
{
    uint16_t t = st->gpr[r].lo16;
    st->gpr[r].lo16 = st->gpr[r].hi16;
    st->gpr[r].hi16 = t;
    st->pc += 4;
}

int main(void)
{
    FloatBits fb;
    DoubleBits db;
    CpuState cpu;
    unsigned i;

    fb.f = -118.625f;
    BENCH_OUTPUT(
        printf("float %.4f -> u=%08x sign=%u exp=%u mant=%06x bytes=%02x%02x%02x%02x\n",
               fb.f, fb.u, fb.parts.sign, fb.parts.exponent, fb.parts.mantissa,
               fb.bytes[3], fb.bytes[2], fb.bytes[1], fb.bytes[0]),
        printf("%.4f %08x %u %u %06x %02x %02x %02x %02x\n", fb.f, fb.u,
               fb.parts.sign, fb.parts.exponent, fb.parts.mantissa,
               fb.bytes[3], fb.bytes[2], fb.bytes[1], fb.bytes[0]));

    fb.parts.sign = 0;
    fb.parts.exponent += 1;
    BENCH_OUTPUT(printf("patched -> %.4f (u=%08x)\n", fb.f, fb.u),
                 printf("%.4f %08x\n", fb.f, fb.u));

    db.d = 3.14159265358979;
    BENCH_OUTPUT(
        printf("double %.14f -> u=%016llx halves=%08x/%08x\n", db.d,
               (unsigned long long)db.u, db.halves[0], db.halves[1]),
        printf("%.14f %016llx %08x %08x\n", db.d,
               (unsigned long long)db.u, db.halves[0], db.halves[1]));

    cpu_reset(&cpu, "vm-core-0");
    reg_show("r0", cpu.gpr[0]);
    reg_show("r3", cpu.gpr[3]);

    cpu_add(&cpu, 0, 7);
    cpu_swap_halves(&cpu, 3);
    cpu_add(&cpu, 3, 3);

    reg_show("r0'", cpu.gpr[0]);
    reg_show("r3'", cpu.gpr[3]);
    BENCH_OUTPUT(
        printf("flags=%08x c=%u z=%u s=%u o=%u pc=%08x sp=%08x tag='%s'\n",
               cpu.flags, cpu.carry, cpu.zero, cpu.sign, cpu.overflow,
               cpu.pc, cpu.sp, cpu.tag),
        printf("%08x %u %u %u %u %08x %08x %s\n", cpu.flags, cpu.carry,
               cpu.zero, cpu.sign, cpu.overflow, cpu.pc, cpu.sp, cpu.tag));

    for (i = 0; i < 8; i++)
        cpu.gpr[i].b1 = (uint8_t)(0xA0 + i);
    BENCH_VERBOSE(printf("after byte writes: "));
    for (i = 0; i < 8; i++)
        printf("%08x ", cpu.gpr[i].dword);
    putchar('\n');
    return 0;
}
