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

struct string_entry {
    uint32_t hash;
    uint32_t length;
    char text[];
};

struct string_table {
    uint32_t count;
    uint32_t capacity;
    struct string_entry *entries[8];
};

static uint32_t crc32_bytes(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    uint32_t crc = UINT32_C(0xffffffff);

    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^
                  (UINT32_C(0xedb88320) & (uint32_t)-(int32_t)(crc & 1));
    }

    return ~crc;
}

static uint32_t fnv1a_bytes(const char *text, size_t length)
{
    uint32_t hash = UINT32_C(0x811c9dc5);

    for (size_t i = 0; i < length; ++i) {
        hash ^= (uint8_t)text[i];
        hash *= UINT32_C(0x01000193);
    }

    return hash;
}

static struct matrix *matrix_create(uint32_t rows, uint32_t columns)
{
    uint64_t cells = (uint64_t)rows * columns;
    size_t size = (size_t)(UINT64_C(8) + cells * UINT64_C(8));
    struct matrix *matrix = calloc(1, size);

    if (matrix == NULL)
        exit(1);

    matrix->rows = rows;
    matrix->columns = columns;
    return matrix;
}

static struct matrix *matrix_multiply(const struct matrix *left,
                                      const struct matrix *right)
{
    struct matrix *result = matrix_create(left->rows, right->columns);

    for (uint32_t row = 0; row < left->rows; ++row) {
        for (uint32_t column = 0; column < right->columns; ++column) {
            double sum = 0.0;

            for (uint32_t k = 0; k < left->columns; ++k) {
                sum += left->values[(uint64_t)row * left->columns + k] *
                       right->values[(uint64_t)k * right->columns + column];
            }

            result->values[(uint64_t)row * result->columns + column] = sum;
        }
    }

    return result;
}

static struct string_entry *string_entry_create(const char *text)
{
    size_t length = strlen(text);
    struct string_entry *entry = malloc(length + 9);

    if (entry == NULL)
        exit(1);

    memcpy(entry->text, text, length + 1);
    entry->length = (uint32_t)length;
    entry->hash = fnv1a_bytes(text, length);
    return entry;
}

static void string_table_insert(struct string_table *table, const char *text)
{
    for (uint32_t i = 0; i < table->count; ++i) {
        if (strcmp(table->entries[i]->text, text) == 0)
            return;
    }

    if (table->count < table->capacity) {
        struct string_entry *entry = string_entry_create(text);
        table->entries[table->count++] = entry;
    }
}

int main(void)
{
    const char *words[6] = {
        "alpha",
        "beta",
        "gamma",
        "beta",
        "delta",
        "alpha"
    };
    uint8_t initial_payload[48];
    uint8_t value = 3;

    for (size_t i = 0; i < sizeof initial_payload; ++i) {
        initial_payload[i] = value;
        value = (uint8_t)(value + 7);
    }

    struct packet *packet = malloc(sizeof *packet);
    if (packet == NULL)
        exit(1);

    packet->magic = UINT16_C(0xc0de);
    packet->version = 3;
    packet->type = 17;
    packet->length = 48;
    memcpy(packet->payload, initial_payload, sizeof packet->payload);
    packet->checksum = crc32_bytes(packet->payload, packet->length);

    printf("t=%u n=%u c=%08x %d %u %u\n",
           (unsigned int)packet->type,
           (unsigned int)packet->length,
           packet->checksum,
           crc32_bytes(packet->payload, packet->length) == packet->checksum,
           (unsigned int)packet->payload[0],
           (unsigned int)packet->payload[47]);

    struct matrix *left = matrix_create(3, 4);
    struct matrix *right = matrix_create(4, 2);

    for (uint32_t row = 0; row < left->rows; ++row) {
        for (uint32_t column = 0; column < left->columns; ++column) {
            left->values[(uint64_t)row * left->columns + column] =
                (double)(uint64_t)(row + 1) *
                (double)(uint64_t)(column + 2);
        }
    }

    for (uint32_t row = 0; row < right->rows; ++row) {
        for (uint32_t column = 0; column < right->columns; ++column) {
            right->values[(uint64_t)row * right->columns + column] =
                (double)((uint64_t)row * 2) - (double)(uint64_t)column;
        }
    }

    struct matrix *product = matrix_multiply(left, right);

    printf("%u %u\n", product->rows, product->columns);
    for (uint32_t row = 0; row < product->rows; ++row) {
        for (uint32_t column = 0; column < product->columns; ++column)
            printf("%6.2f ",
                   product->values[(uint64_t)row * product->columns + column]);
        putc('\n', stdout);
    }

    struct string_table *table = calloc(1, sizeof *table);
    if (table == NULL)
        exit(1);

    table->capacity = 8;

    for (size_t i = 0; i < 6; ++i)
        string_table_insert(table, words[i]);

    printf("%u %u\n", table->count, table->capacity);
    for (uint32_t i = 0; i < table->count; ++i) {
        struct string_entry *entry = table->entries[i];
        printf("%u %08x %u %s\n",
               i, entry->hash, entry->length, entry->text);
    }

    for (uint32_t i = 0; i < table->count; ++i)
        free(table->entries[i]);

    free(table);
    free(product);
    free(right);
    free(left);
    free(packet);
    return 0;
}