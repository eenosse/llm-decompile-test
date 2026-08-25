#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint32_t length;
    uint32_t checksum;
    uint8_t data[];
} Packet;

typedef struct {
    uint32_t rows;
    uint32_t columns;
    double values[];
} Matrix;

typedef struct {
    uint32_t hash;
    uint32_t length;
    char text[];
} StringEntry;

typedef struct {
    uint32_t count;
    uint32_t capacity;
    StringEntry *entries[];
} StringPool;

_Static_assert(sizeof(Packet) == 12, "unexpected Packet layout");
_Static_assert(sizeof(Matrix) == 8, "unexpected Matrix layout");
_Static_assert(sizeof(StringEntry) == 8, "unexpected StringEntry layout");
_Static_assert(sizeof(StringPool) == 8, "unexpected StringPool layout");
_Static_assert(sizeof(double) == 8, "unexpected double size");
_Static_assert(sizeof(void *) == 8, "unexpected pointer size");

static uint32_t checksum32(const uint8_t *data, size_t length)
{
    uint32_t value = UINT32_MAX;

    for (size_t i = 0; i < length; ++i) {
        value ^= data[i];
        for (uint32_t bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^
                    (UINT32_C(0xedb88320) & (uint32_t)-(int32_t)(value & 1u));
    }

    return ~value;
}

static Packet *packet_create(uint8_t type, const void *data, uint32_t length)
{
    Packet *packet = malloc((size_t)length + sizeof(*packet));

    if (packet == NULL)
        exit(1);

    packet->magic = UINT16_C(0xc0de);
    packet->version = 3;
    packet->type = type;
    packet->length = length;
    memcpy(packet->data, data, length);
    packet->checksum = checksum32(packet->data, length);

    return packet;
}

static int packet_valid(const Packet *packet)
{
    return packet->magic == UINT16_C(0xc0de) &&
           packet->checksum == checksum32(packet->data, packet->length);
}

static Matrix *matrix_create(uint32_t rows, uint32_t columns)
{
    size_t size = (((size_t)rows * columns) + 1u) * sizeof(double);
    Matrix *matrix = calloc(1, size);

    if (matrix == NULL)
        exit(1);

    matrix->rows = rows;
    matrix->columns = columns;
    return matrix;
}

static double *matrix_at(Matrix *matrix, uint32_t row, uint32_t column)
{
    return &matrix->values[(size_t)row * matrix->columns + column];
}

static Matrix *matrix_multiply(const Matrix *left, const Matrix *right)
{
    Matrix *result = matrix_create(left->rows, right->columns);

    for (uint32_t row = 0; row < left->rows; ++row) {
        for (uint32_t column = 0; column < right->columns; ++column) {
            double sum = 0.0;

            for (uint32_t k = 0; k < left->columns; ++k) {
                sum += left->values[(size_t)row * left->columns + k] *
                       right->values[(size_t)k * right->columns + column];
            }

            result->values[(size_t)row * result->columns + column] = sum;
        }
    }

    return result;
}

static StringEntry *string_entry_create(const char *text)
{
    size_t length = strlen(text);
    StringEntry *entry = malloc(length + sizeof(*entry) + 1u);
    uint32_t hash = UINT32_C(0x811c9dc5);

    if (entry == NULL)
        exit(1);

    memcpy(entry->text, text, length + 1u);
    entry->length = (uint32_t)length;

    for (size_t i = 0; i < length; ++i) {
        hash ^= (unsigned char)text[i];
        hash *= UINT32_C(0x01000193);
    }

    entry->hash = hash;
    return entry;
}

static StringPool *string_pool_create(uint32_t capacity)
{
    size_t size = ((size_t)capacity + 1u) * sizeof(void *);
    StringPool *pool = calloc(1, size);

    if (pool == NULL)
        exit(1);

    pool->capacity = capacity;
    return pool;
}

static StringEntry *string_pool_intern(StringPool *pool, const char *text)
{
    for (uint32_t i = 0; i < pool->count; ++i) {
        if (strcmp(pool->entries[i]->text, text) == 0)
            return pool->entries[i];
    }

    if (pool->count == pool->capacity)
        return NULL;

    StringEntry *entry = string_entry_create(text);
    pool->entries[pool->count++] = entry;
    return entry;
}

int main(void)
{
    static const char *const words[6] = {
        "alpha", "beta", "gamma", "beta", "delta", "alpha"
    };

    uint8_t payload[48];

    for (uint32_t i = 0; i < 48; ++i)
        payload[i] = (uint8_t)(i * 7u + 3u);

    Packet *packet = packet_create(17, payload, 48);

    printf("P %u %u %08x %d %02x %02x\n",
           (unsigned)packet->type,
           packet->length,
           packet->checksum,
           packet_valid(packet),
           (unsigned)packet->data[0],
           (unsigned)packet->data[47]);

    Matrix *left = matrix_create(3, 4);
    Matrix *right = matrix_create(4, 2);

    for (uint32_t row = 0; row < left->rows; ++row) {
        for (uint32_t column = 0; column < left->columns; ++column) {
            *matrix_at(left, row, column) =
                (double)(uint32_t)(row + 1u) *
                (double)(uint32_t)(column + 2u);
        }
    }

    for (uint32_t row = 0; row < right->rows; ++row) {
        for (uint32_t column = 0; column < right->columns; ++column) {
            *matrix_at(right, row, column) =
                (double)(uint32_t)(row + row) - (double)column;
        }
    }

    Matrix *product = matrix_multiply(left, right);

    printf("%ux%u\n", product->rows, product->columns);
    for (uint32_t row = 0; row < product->rows; ++row) {
        for (uint32_t column = 0; column < product->columns; ++column)
            printf("%8.2f ", *matrix_at(product, row, column));
        putchar('\n');
    }

    StringPool *pool = string_pool_create(8);

    for (uint32_t i = 0; i < 6; ++i)
        string_pool_intern(pool, words[i]);

    printf("%ux%u\n", pool->count, pool->capacity);
    for (uint32_t i = 0; i < pool->count; ++i) {
        StringEntry *entry = pool->entries[i];
        printf("[%u] %08x %u %s\n",
               i, entry->hash, entry->length, entry->text);
    }

    for (uint32_t i = 0; i < pool->count; ++i)
        free(pool->entries[i]);

    free(pool);
    free(product);
    free(right);
    free(left);
    free(packet);

    return 0;
}