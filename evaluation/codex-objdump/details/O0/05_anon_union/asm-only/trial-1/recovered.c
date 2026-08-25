#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef RECOVERED_F32_BITS
#define RECOVERED_F32_BITS UINT32_C(0)
#endif

#ifndef RECOVERED_F64_BITS
#define RECOVERED_F64_BITS UINT64_C(0)
#endif

#ifndef RECOVERED_MACHINE_NAME
#define RECOVERED_MACHINE_NAME "processor"
#endif

union word32 {
    uint32_t value;
    uint16_t half[2];
    uint8_t byte[4];
};

union status_word {
    uint32_t value;
    struct {
        unsigned int carry : 1;
        unsigned int zero : 1;
        unsigned int negative : 1;
        unsigned int overflow : 1;
        unsigned int reserved : 28;
    } bit;
};

struct machine_state {
    union word32 reg[8];
    union status_word status;
    uint32_t pc;
    uint32_t sp;
    char name[12];
};

union float32_view {
    float value;
    uint32_t bits;
    uint8_t byte[4];
    struct {
        unsigned int fraction : 23;
        unsigned int exponent : 8;
        unsigned int sign : 1;
    } part;
};

union float64_view {
    double value;
    uint64_t bits;
    uint32_t word[2];
};

_Static_assert(sizeof(union word32) == 4, "unexpected word layout");
_Static_assert(sizeof(union status_word) == 4, "unexpected status layout");
_Static_assert(offsetof(struct machine_state, status) == 32,
               "unexpected status offset");
_Static_assert(offsetof(struct machine_state, pc) == 36,
               "unexpected pc offset");
_Static_assert(offsetof(struct machine_state, sp) == 40,
               "unexpected sp offset");
_Static_assert(offsetof(struct machine_state, name) == 44,
               "unexpected name offset");
_Static_assert(sizeof(struct machine_state) == 56,
               "unexpected machine-state size");

static void print_word(const char *unused, union word32 word)
{
    (void)unused;

    printf("%08x = %04x:%04x = %02x:%02x:%02x:%02x\n",
           word.value,
           word.half[0],
           word.half[1],
           word.byte[0],
           word.byte[1],
           word.byte[2],
           word.byte[3]);
}

static void initialize_state(struct machine_state *state, const char *name)
{
    unsigned int i;

    memset(state, 0, sizeof(*state));

    for (i = 0; i <= 7; ++i)
        state->reg[i].value = (i + 1U) * UINT32_C(0x11111111);

    state->pc = UINT32_C(0x00400500);
    state->sp = UINT32_C(0x7ffff000);
    strncpy(state->name, name, 11);
}

static void add_registers(struct machine_state *state,
                          unsigned int destination,
                          unsigned int source)
{
    uint64_t sum =
        (uint64_t)state->reg[destination].value +
        (uint64_t)state->reg[source].value;
    uint32_t result = (uint32_t)sum;

    state->status.bit.carry = (unsigned int)((sum >> 32) & 1U);
    state->status.bit.zero = result == 0;
    state->status.bit.negative = (result >> 31) & 1U;
    state->status.bit.overflow = 0;

    state->reg[destination].value = result;
    state->pc += 4U;
}

static void swap_register_halves(struct machine_state *state,
                                 unsigned int index)
{
    uint16_t temporary = state->reg[index].half[0];

    state->reg[index].half[0] = state->reg[index].half[1];
    state->reg[index].half[1] = temporary;
    state->pc += 4U;
}

int main(void)
{
    union float32_view single = { .bits = RECOVERED_F32_BITS };
    union float64_view wide = { .bits = RECOVERED_F64_BITS };
    struct machine_state state;
    unsigned int i;

    printf("%f %08x %u %02x %06x %02x:%02x:%02x:%02x\n",
           (double)single.value,
           single.bits,
           single.part.sign,
           single.part.exponent,
           single.part.fraction,
           single.byte[3],
           single.byte[2],
           single.byte[1],
           single.byte[0]);

    single.part.sign = 0;
    ++single.part.exponent;

    printf("%f = %08x\n", (double)single.value, single.bits);

    printf("%f = %016lx = %08x:%08x\n",
           wide.value,
           (unsigned long)wide.bits,
           wide.word[0],
           wide.word[1]);

    initialize_state(&state, RECOVERED_MACHINE_NAME);

    print_word("r0", state.reg[0]);
    print_word("r3", state.reg[3]);

    add_registers(&state, 0, 7);
    swap_register_halves(&state, 3);
    add_registers(&state, 3, 3);

    print_word("r0+", state.reg[0]);
    print_word("r3sw+", state.reg[3]);

    printf("%02x %u:%u:%u:%u %08x %08x %s\n",
           state.status.value,
           state.status.bit.carry,
           state.status.bit.zero,
           state.status.bit.negative,
           state.status.bit.overflow,
           state.pc,
           state.sp,
           state.name);

    for (i = 0; i <= 7; ++i)
        state.reg[i].byte[1] = (uint8_t)(i - 0x60U);

    for (i = 0; i <= 7; ++i)
        printf("%08x ", state.reg[i].value);

    putchar('\n');
    return 0;
}