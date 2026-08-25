#include <stdint.h>
#include <stdio.h>

typedef union {
    uint32_t word;
    uint16_t half[2];
    uint8_t byte[4];
} word32;

typedef struct {
    unsigned carry : 1;
    unsigned zero : 1;
    unsigned negative : 1;
    unsigned overflow : 1;
    unsigned reserved : 4;
} condition_codes;

typedef struct {
    word32 registers[8];
    condition_codes flags;
    uint32_t program_counter;
    uint32_t stack_pointer;
    char name[12];
} machine_state;

static __attribute__((noinline)) void print_word(uint32_t value)
{
    printf("%08x (%04x,%04x) [%02x,%02x,%02x,%02x]\n",
           value,
           value & UINT32_C(0xffff),
           value >> 16,
           value & UINT32_C(0xff),
           (value >> 8) & UINT32_C(0xff),
           (value >> 16) & UINT32_C(0xff),
           value >> 24);
}

int main(void)
{
    union {
        float value;
        uint32_t bits;
    } single = { .bits = UINT32_C(0xc2ed4000) };

    union {
        double value;
        uint64_t bits;
        uint32_t words[2];
    } wide = { .bits = UINT64_C(0x400921fb54442d11) };

    machine_state state = {
        .registers = {
            { .word = UINT32_C(0x11111111) },
            { .word = UINT32_C(0x22222222) },
            { .word = UINT32_C(0x33333333) },
            { .word = UINT32_C(0x44444444) },
            { .word = UINT32_C(0x55555555) },
            { .word = UINT32_C(0x66666666) },
            { .word = UINT32_C(0x77777777) },
            { .word = UINT32_C(0x88888888) }
        },
        .flags = { 0 },
        .program_counter = UINT32_C(0x00400500),
        .stack_pointer = UINT32_C(0x7ffff000),
        .name = "vm-core-0"
    };

    uint32_t bits = single.bits;

    printf("%f 0x%08x (%u,%02x,%06x) [%02x,%02x,%02x,%02x]\n",
           (double)single.value,
           bits,
           bits >> 31,
           (bits >> 23) & UINT32_C(0xff),
           bits & UINT32_C(0x7fffff),
           bits >> 24,
           (bits >> 16) & UINT32_C(0xff),
           (bits >> 8) & UINT32_C(0xff),
           bits & UINT32_C(0xff));

    single.value *= -2.0f;
    printf("%f 0x%08x\n", (double)single.value, single.bits);

    printf("%f 0x%016lx (%08x,%08x)\n",
           wide.value,
           (unsigned long)wide.bits,
           wide.words[0],
           wide.words[1]);

    print_word(state.registers[0].word);
    print_word(state.registers[3].word);

    {
        uint16_t temporary = state.registers[3].half[0];
        state.registers[3].half[0] = state.registers[3].half[1];
        state.registers[3].half[1] = temporary;
    }

    state.registers[0].word += state.registers[7].word;

    {
        uint32_t previous = state.registers[3].word;
        uint32_t result = previous << 1;

        state.registers[3].word = result;
        state.flags.carry = previous >> 31;
        state.flags.zero = result == 0;
        state.flags.negative = result >> 31;
        state.flags.overflow = 0;
    }

    state.program_counter += UINT32_C(12);

    print_word(state.registers[0].word);
    print_word(state.registers[3].word);

    printf("%08x %u %u %u %u %08x %08x %s\n",
           state.registers[6].word,
           state.flags.carry,
           state.flags.zero,
           state.flags.negative,
           state.flags.overflow,
           state.program_counter,
           state.stack_pointer,
           state.name);

    for (unsigned i = 0; i < 8; ++i)
        state.registers[i].byte[1] = (uint8_t)(UINT8_C(0xa0) + i);

    for (unsigned i = 0; i < 8; ++i)
        printf("%08x ", state.registers[i].word);

    putc('\n', stdout);
    return 0;
}