#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

union word32 {
    uint32_t value;
    uint16_t half[2];
    uint8_t byte[4];
};

union status32 {
    uint32_t value;
    struct {
        unsigned carry : 1;
        unsigned zero : 1;
        unsigned sign : 1;
        unsigned overflow : 1;
        unsigned reserved : 28;
    } bit;
};

struct vm_state {
    union word32 reg[8];
    union status32 status;
    uint32_t pc;
    uint32_t sp;
    char name[10];
};

union float32 {
    float value;
    uint32_t bits;
};

union float64 {
    double value;
    uint64_t bits;
};

_Static_assert(sizeof(union word32) == 4, "unexpected word layout");
_Static_assert(sizeof(union status32) == 4, "unexpected status layout");
_Static_assert(offsetof(struct vm_state, status) == 32, "unexpected status offset");
_Static_assert(offsetof(struct vm_state, pc) == 36, "unexpected pc offset");
_Static_assert(offsetof(struct vm_state, sp) == 40, "unexpected sp offset");
_Static_assert(offsetof(struct vm_state, name) == 44, "unexpected name offset");
_Static_assert(sizeof(struct vm_state) == 56, "unexpected state size");

static void print_word(const char *name, uint32_t value)
{
    printf("%s: %04x %04x | %02x %02x %02x %02x\n",
           name,
           (unsigned)(uint16_t)value,
           (unsigned)(uint16_t)(value >> 16),
           (unsigned)(uint8_t)value,
           (unsigned)(uint8_t)(value >> 8),
           (unsigned)(uint8_t)(value >> 16),
           (unsigned)(uint8_t)(value >> 24));
}

int main(void)
{
    union float32 f = { .value = -118.625f };
    union float32 g = { .value = f.value * -2.0f };
    union float64 pi = { .value = 3.141592653589793 };

    printf("float %g %08x %u %02x %06x %02x %02x %02x %02x\n",
           (double)f.value,
           (unsigned)f.bits,
           (unsigned)(f.bits >> 31),
           (unsigned)((f.bits >> 23) & 0xffu),
           (unsigned)(f.bits & 0x7fffffu),
           (unsigned)(uint8_t)(f.bits >> 24),
           (unsigned)(uint8_t)(f.bits >> 16),
           (unsigned)(uint8_t)(f.bits >> 8),
           (unsigned)(uint8_t)f.bits);

    printf("g %g %08x\n", (double)g.value, (unsigned)g.bits);

    printf("pi: %g %016lx %08x %08x\n",
           pi.value,
           (unsigned long)pi.bits,
           (unsigned)(uint32_t)pi.bits,
           (unsigned)(uint32_t)(pi.bits >> 32));

    struct vm_state vm = {
        .reg = {
            { .value = UINT32_C(0x11111111) },
            { .value = UINT32_C(0x22222222) },
            { .value = UINT32_C(0x33333333) },
            { .value = UINT32_C(0x44444444) },
            { .value = UINT32_C(0x55555555) },
            { .value = UINT32_C(0x66666666) },
            { .value = UINT32_C(0x77777777) },
            { .value = UINT32_C(0x88888888) }
        },
        .status = { .value = 0 },
        .pc = UINT32_C(0x00400500),
        .sp = UINT32_C(0x7ffff000),
        .name = "vm-core-0"
    };

    print_word("r0", vm.reg[0].value);
    print_word("r3", vm.reg[3].value);

    vm.reg[0].value += vm.reg[7].value;

    {
        uint16_t temporary = vm.reg[3].half[0];
        vm.reg[3].half[0] = vm.reg[3].half[1];
        vm.reg[3].half[1] = temporary;
    }

    {
        uint32_t value = vm.reg[3].value;
        vm.status.bit.carry = value >> 31;
        value <<= 1;
        vm.status.bit.zero = value == 0;
        vm.status.bit.sign = value >> 31;
        vm.reg[3].value = value;
    }

    vm.pc += 12;

    print_word("r0'", vm.reg[0].value);
    print_word("r3'", vm.reg[3].value);

    printf("vm %08x %u%u%u%u %08x %08x %s\n",
           (unsigned)vm.status.value,
           (unsigned)vm.status.bit.carry,
           (unsigned)vm.status.bit.zero,
           (unsigned)vm.status.bit.sign,
           (unsigned)vm.status.bit.overflow,
           (unsigned)vm.pc,
           (unsigned)vm.sp,
           vm.name);

    for (unsigned i = 0; i < 8; ++i)
        vm.reg[i].byte[1] = (uint8_t)(0xa0u + i);

    for (unsigned i = 0; i < 8; ++i)
        printf("%08x ", (unsigned)vm.reg[i].value);

    putc('\n', stdout);
    return 0;
}