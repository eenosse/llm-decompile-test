#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct packet {
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint32_t length;
    uint32_t checksum;
    uint8_t payload[48];
};

struct matrix {
    uint32_t rows;
    uint32_t columns;
    double values[];
};

struct text_entry {
    uint32_t hash;
    uint32_t length;
    char text[];
};

struct text_set {
    uint32_t count;
    uint32_t capacity;
    struct text_entry *entries[8];
};

static void allocation_failure(void)
{
    exit(1);
}

static uint32_t crc32_bytes(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_MAX;

    while (length-- != 0) {
        unsigned bit;

        crc ^= *data++;
        for (bit = 0; bit < 8; ++bit) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }

    return ~crc;
}

static struct matrix *matrix_create(uint32_t rows, uint32_t columns)
{
    size_t elements = (size_t)rows * (size_t)columns;
    struct matrix *matrix =
        calloc(1, sizeof(*matrix) + elements * sizeof(matrix->values[0]));

    if (matrix == NULL)
        allocation_failure();

    matrix->rows = rows;
    matrix->columns = columns;
    return matrix;
}

static struct matrix *matrix_multiply(const struct matrix *left,
                                      const struct matrix *right)
{
    struct matrix *result = matrix_create(left->rows, right->columns);
    uint32_t row;
    uint32_t column;

    for (row = 0; row < left->rows; ++row) {
        for (column = 0; column < right->columns; ++column) {
            double sum = 0.0;
            uint32_t inner;

            for (inner = 0; inner < left->columns; ++inner) {
                sum += left->values[(size_t)row * left->columns + inner] *
                       right->values[(size_t)inner * right->columns + column];
            }

            result->values[(size_t)row * result->columns + column] = sum;
        }
    }

    return result;
}

static uint32_t fnv1a(const char *text, size_t length)
{
    uint32_t hash = UINT32_C(0x811c9dc5);
    size_t i;

    for (i = 0; i < length; ++i) {
        hash ^= (unsigned char)text[i];
        hash *= UINT32_C(0x01000193);
    }

    return hash;
}

static void text_set_insert(struct text_set *set, const char *text)
{
    uint32_t i;
    size_t length;
    struct text_entry *entry;

    for (i = 0; i < set->count; ++i) {
        if (strcmp(set->entries[i]->text, text) == 0)
            return;
    }

    if (set->count == set->capacity)
        return;

    length = strlen(text);
    entry = malloc(sizeof(*entry) + length + 1);
    if (entry == NULL)
        allocation_failure();

    memcpy(entry->text, text, length + 1);
    entry->length = (uint32_t)length;
    entry->hash = fnv1a(text, length);
    set->entries[set->count++] = entry;
}

int main(void)
{
    static const uint8_t initial_payload[48] = {
         0,  1,  2,  3,  4,  5,  6,  7,
         8,  9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39,
        40, 41, 42, 43, 44, 45, 46, 47
    };
    const char *words[6] = {
        "alpha", "beta", "gamma", "beta", "delta", "alpha"
    };
    struct packet *packet;
    struct matrix *left;
    struct matrix *right;
    struct matrix *product;
    struct text_set *set;
    uint32_t row;
    uint32_t column;
    uint32_t computed_checksum;
    size_t i;

    packet = malloc(sizeof(*packet));
    if (packet == NULL)
        allocation_failure();

    packet->magic = UINT16_C(0xc0de);
    packet->version = 3;
    packet->type = UINT8_C(0x11);
    packet->length = 48;
    memcpy(packet->payload, initial_payload, sizeof(initial_payload));
    packet->checksum = crc32_bytes(packet->payload, packet->length);

    computed_checksum = crc32_bytes(packet->payload, packet->length);
    printf("P %u %u %08x %d %02x %02x\n",
           (unsigned)packet->type,
           packet->length,
           packet->checksum,
           computed_checksum == packet->checksum,
           (unsigned)packet->payload[0],
           (unsigned)packet->payload[packet->length - 1]);

    left = matrix_create(3, 4);
    right = matrix_create(4, 2);

    for (row = 0; row < left->rows; ++row) {
        for (column = 0; column < left->columns; ++column) {
            left->values[(size_t)row * left->columns + column] =
                (double)(row + 1u) * (double)(column + 2u);
        }
    }

    for (row = 0; row < right->rows; ++row) {
        for (column = 0; column < right->columns; ++column) {
            right->values[(size_t)row * right->columns + column] =
                (double)(row * 2u) - (double)column;
        }
    }

    product = matrix_multiply(left, right);

    printf("%u %u\n", product->rows, product->columns);
    for (row = 0; row < product->rows; ++row) {
        for (column = 0; column < product->columns; ++column) {
            printf("%8.2f ",
                   product->values[(size_t)row * product->columns + column]);
        }
        putc('\n', stdout);
    }

    set = calloc(1, sizeof(*set));
    if (set == NULL)
        allocation_failure();

    set->capacity = 8;
    for (i = 0; i < 6; ++i)
        text_set_insert(set, words[i]);

    printf("%u %u\n", set->count, set->capacity);
    for (i = 0; i < set->count; ++i) {
        const struct text_entry *entry = set->entries[i];

        printf("[%u] %08x %u %s\n",
               (unsigned)i,
               entry->hash,
               entry->length,
               entry->text);
    }

    for (i = 0; i < set->count; ++i)
        free(set->entries[i]);

    free(set);
    free(product);
    free(right);
    free(left);
    free(packet);
    return 0;
}