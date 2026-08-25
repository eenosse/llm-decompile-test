#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct blob {
    uint32_t magic;
    uint32_t length;
    uint32_t checksum;
    unsigned char payload[48];
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

static uint32_t crc32_bytes(const unsigned char *data, size_t length)
{
    uint32_t crc = UINT32_MAX;

    while (length-- != 0) {
        crc ^= *data++;

        for (unsigned int bit = 0; bit < 8; ++bit) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }

    return ~crc;
}

static struct matrix *matrix_create(uint32_t rows, uint32_t columns)
{
    size_t count = (size_t)rows * (size_t)columns;
    struct matrix *result = calloc(1, sizeof(*result) +
                                      count * sizeof(result->values[0]));

    if (result == NULL)
        exit(1);

    result->rows = rows;
    result->columns = columns;
    return result;
}

static uint32_t string_hash(const char *text, size_t length)
{
    uint32_t hash = UINT32_C(0x811c9dc5);

    for (size_t i = 0; i < length; ++i) {
        hash ^= (unsigned char)text[i];
        hash *= UINT32_C(0x01000193);
    }

    return hash;
}

int main(void)
{
    const char *names[5] = {
        "beta",
        "gamma",
        "beta",
        "delta",
        "alpha"
    };
    unsigned char generated[48];
    unsigned char value = 3;
    unsigned char *destination = generated;

    do {
        *destination++ = value;
        value = (unsigned char)(value + 7u);
    } while (value != 0x53u);

    struct blob *packet = malloc(sizeof(*packet));
    if (packet == NULL)
        exit(1);

    packet->magic = UINT32_C(0x1103c0de);
    packet->length = 48;
    memcpy(packet->payload, generated, sizeof(generated));

    packet->checksum = crc32_bytes(packet->payload, packet->length);
    uint32_t repeated_checksum =
        crc32_bytes(packet->payload, packet->length);

    printf("blob: %u %u %08x %d %u %u\n",
           packet->magic >> 24,
           packet->length,
           packet->checksum,
           repeated_checksum == packet->checksum,
           packet->payload[0],
           packet->payload[47]);

    struct matrix *left = matrix_create(3, 4);
    struct matrix *right = matrix_create(4, 2);

    for (uint32_t row = 0; row < left->rows; ++row) {
        for (uint32_t column = 0; column < left->columns; ++column) {
            left->values[(size_t)row * left->columns + column] =
                (double)(row + 1u) * (double)(column + 2u);
        }
    }

    for (uint32_t row = 0; row < right->rows; ++row) {
        for (uint32_t column = 0; column < right->columns; ++column) {
            right->values[(size_t)row * right->columns + column] =
                (double)(2u * row) - (double)column;
        }
    }

    struct matrix *product = matrix_create(left->rows, right->columns);

    for (uint32_t row = 0; row < left->rows; ++row) {
        for (uint32_t column = 0; column < right->columns; ++column) {
            double sum = 0.0;

            for (uint32_t inner = 0; inner < left->columns; ++inner) {
                sum += left->values[(size_t)row * left->columns + inner] *
                       right->values[(size_t)inner * right->columns + column];
            }

            product->values[(size_t)row * product->columns + column] = sum;
        }
    }

    printf("%ux%u\n", product->rows, product->columns);
    for (uint32_t row = 0; row < product->rows; ++row) {
        for (uint32_t column = 0; column < product->columns; ++column) {
            printf("%6.1f ",
                   product->values[(size_t)row * product->columns + column]);
        }
        putchar('\n');
    }

    struct string_table *table = calloc(1, sizeof(*table));
    if (table == NULL)
        exit(1);

    table->capacity = 8;

    const char *current = "alpha";
    const char **next = names;
    const char **end = names + 5;

    for (;;) {
        uint32_t index;

        for (index = 0; index < table->count; ++index) {
            if (strcmp(table->entries[index]->text, current) == 0)
                break;
        }

        if (index == table->count && table->count != table->capacity) {
            size_t length = strlen(current);
            struct string_entry *entry =
                malloc(sizeof(*entry) + length + 1);

            if (entry == NULL)
                exit(1);

            memcpy(entry->text, current, length + 1);
            entry->length = (uint32_t)length;
            entry->hash = string_hash(current, length);

            table->entries[table->count++] = entry;
        }

        if (next == end)
            break;

        current = *next++;
    }

    printf("%ux%u\n", table->count, table->capacity);

    for (uint32_t index = 0; index < table->count; ++index) {
        const struct string_entry *entry = table->entries[index];

        printf("[%u] %08x %u %s\n",
               index,
               entry->hash,
               entry->length,
               entry->text);
    }

    for (uint32_t index = 0; index < table->count; ++index)
        free(table->entries[index]);

    free(table);
    free(product);
    free(right);
    free(left);
    free(packet);

    return 0;
}