#include <stdint.h>
#include <stdio.h>

union float_image {
    uint32_t bits;
    float value;
};

union double_image {
    uint64_t bits;
    double value;
};

union status_image {
    uint32_t raw;
    struct {
        unsigned carry : 1;
        unsigned zero : 1;
        unsigned sign : 1;
        unsigned overflow : 1;
        unsigned reserved : 28;
    } bits;
};

struct machine_state {
    union status_image status;
    uint32_t value_a;
    uint32_t value_b;
    char name[10];
};

static void print_word(uint32_t value)
{
    printf("0x%08x = %04x %04x = %02x %02x %02x %02x\n",
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
    union float_image first_float = { .bits = UINT32_C(0xc2ed4000) };
    union float_image second_float = { .bits = UINT32_C(0x436d4000) };
    union double_image pi = { .bits = UINT64_C(0x400921fb54442d11) };

    uint32_t words[8] = {
        UINT32_C(0x11111111),
        UINT32_C(0x22222222),
        UINT32_C(0x33333333),
        UINT32_C(0x44444444),
        UINT32_C(0x55555555),
        UINT32_C(0x66666666),
        UINT32_C(0x77777777),
        UINT32_C(0x88888888)
    };

    struct machine_state state = {
        .status.raw = 0,
        .value_a = UINT32_C(0x11111111),
        .value_b = UINT32_C(0x22222222),
        .name = "vm-core-0"
    };

    uint32_t value;
    uint32_t result;
    unsigned char *bytes;

    printf("0x%08x = %u %02x %06x = %02x %02x %02x %02x %f\n",
           first_float.bits,
           first_float.bits >> 31,
           (first_float.bits >> 23) & UINT32_C(0xff),
           first_float.bits & UINT32_C(0x7fffff),
           first_float.bits >> 24,
           (first_float.bits >> 16) & UINT32_C(0xff),
           (first_float.bits >> 8) & UINT32_C(0xff),
           first_float.bits & UINT32_C(0xff),
           (double)first_float.value);

    printf("0x%08x %f\n",
           second_float.bits,
           (double)second_float.value);

    printf("0x%016lx = %08x %08x %f\n",
           (unsigned long)pi.bits,
           (uint32_t)pi.bits,
           (uint32_t)(pi.bits >> 32),
           pi.value);

    print_word(UINT32_C(0x11111111));
    print_word(words[3]);

    words[6] = words[7] + words[0];

    value = ((words[3] & UINT32_C(0xffff)) << 16) | (words[3] >> 16);
    result = value + value;
    words[3] = result;

    state.status.bits.carry = value >> 31;
    state.status.bits.zero = result == 0;
    state.status.bits.sign = result >> 31;
    state.value_a += 12;

    print_word(words[6]);
    print_word(words[3]);

    printf("0x%08x = %u%u%u%u %08x%08x %s\n",
           state.status.raw,
           state.status.bits.carry,
           state.status.bits.zero,
           state.status.bits.sign,
           state.status.bits.overflow,
           state.value_a,
           state.value_b,
           state.name);

    bytes = (unsigned char *)words;
    for (unsigned int i = 0; i < 8; ++i)
        bytes[i * 4 + 1] = (unsigned char)(0xa0 + i);

    for (unsigned int i = 0; i < 8; ++i)
        printf("0x%08x ", words[i]);

    putchar('\n');
    return 0;
}