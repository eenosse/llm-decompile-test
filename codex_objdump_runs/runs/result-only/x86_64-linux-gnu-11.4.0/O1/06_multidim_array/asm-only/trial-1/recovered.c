#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct cell {
    uint8_t value;
    uint8_t category;
    uint16_t payload;
};

struct arena {
    char label[16];
    struct cell cells[8][8];
    int32_t matrices[3][8][8];
    float kernel[3][3];
    uint8_t masks[8];
};

struct tensor {
    uint16_t dimensions[4];
    double values[2][3][4][5];
};

_Static_assert(sizeof(struct cell) == 4, "unexpected cell layout");
_Static_assert(sizeof(struct arena) == 1084, "unexpected arena layout");
_Static_assert(sizeof(struct tensor) == 968, "unexpected tensor layout");

static char names[6][32];
static int32_t name_lengths[6];
static struct tensor tensor_data;
static struct arena data_area;

int main(void)
{
    static const char suffix[] = "placeholder0";
    static const float outer_weight = 0.0625f;
    int row;
    int column;
    int plane;

    memset(&data_area, 0, sizeof(data_area));
    memcpy(data_area.label, "arena", sizeof("arena"));

    for (row = 0; row < 8; ++row) {
        int cursor = row;
        int sequence = 0;

        for (column = 0; column < 8; ++column) {
            struct cell *cell = &data_area.cells[row][column];

            if ((row + column) % 5 == 0) {
                cell->value = (uint8_t)(sequence % 6 + 1);
                cell->category = (uint8_t)(row > 3 ? 2 : 1);
                cell->payload =
                    (uint16_t)(cursor + (int)cell->value * 10);
                --cursor;
                sequence += row;
            } else {
                cell->value = 0;
                cell->category = 0;
                cell->payload = (uint16_t)cursor;
            }

            data_area.masks[row] |= (uint8_t)(1u << column);
        }
    }

    for (plane = 0; plane < 3; ++plane) {
        for (row = 0; row < 8; ++row) {
            for (column = 0; column < 8; ++column) {
                data_area.matrices[plane][row][column] =
                    (plane + 1) * (column * column - row);
            }
        }
    }

    for (row = 0; row < 3; ++row) {
        for (column = 0; column < 3; ++column)
            data_area.kernel[row][column] = outer_weight;
    }
    data_area.kernel[1][1] = 0.5f;

    tensor_data.dimensions[0] = 2;
    tensor_data.dimensions[1] = 3;
    tensor_data.dimensions[2] = 4;
    tensor_data.dimensions[3] = 5;

    {
        int a;
        int b;
        int c;
        int d;
        int index = 0;

        for (a = 0; a < 2; ++a) {
            for (b = 0; b < 3; ++b) {
                for (c = 0; c < 4; ++c) {
                    for (d = 0; d < 5; ++d) {
                        tensor_data.values[a][b][c][d] =
                            (double)index / 10.0;
                        ++index;
                    }
                }
            }
        }
    }

    for (row = 0; row < 6; ++row) {
        snprintf(names[row], sizeof(names[row]),
                 "key%d:%d:%s", row, row * 3, suffix);
        name_lengths[row] = (int32_t)strlen(names[row]);
    }

    puts(data_area.label);

    for (row = 0; row < 8; ++row) {
        for (column = 0; column < 8; ++column) {
            const struct cell *cell = &data_area.cells[row][column];
            int marker = cell->category < 1 ? '.' : 'P';

            printf("%c%d ", marker, (unsigned)cell->value);
        }
        printf("%02x\n", (unsigned)data_area.masks[row]);
    }

    for (plane = 0; plane < 3; ++plane) {
        int32_t sum = 0;

        for (row = 0; row < 8; ++row) {
            for (column = 0; column < 8; ++column)
                sum += data_area.matrices[plane][row][column];
        }
        printf("%d:%d\n", plane, sum);
    }

    {
        float filtered[8][8];

        for (row = 0; row < 8; ++row) {
            for (column = 0; column < 8; ++column) {
                float sum = 0.0f;
                int kernel_row;
                int kernel_column;

                for (kernel_row = 0; kernel_row < 3; ++kernel_row) {
                    int source_row = row + kernel_row - 1;

                    for (kernel_column = 0;
                         kernel_column < 3;
                         ++kernel_column) {
                        int source_column =
                            column + kernel_column - 1;

                        if ((unsigned)source_row < 8u &&
                            (unsigned)source_column < 8u) {
                            sum +=
                                (float)data_area.matrices[1]
                                                          [source_row]
                                                          [source_column] *
                                data_area.kernel[kernel_row][kernel_column];
                        }
                    }
                }

                filtered[row][column] = sum;
            }
        }

        printf("%.2f %.2f %.2f %.2f\n",
               (double)filtered[0][0],
               (double)filtered[0][7],
               (double)filtered[7][0],
               (double)filtered[7][7]);
    }

    {
        double sum = 0.0;

        sum += tensor_data.values[0][0][0][0];
        sum += tensor_data.values[1][1][1][1];
        sum += tensor_data.values[1][0][1][2];
        sum += tensor_data.values[1][1][2][3];
        sum += tensor_data.values[1][2][3][4];

        printf("%u %u %u %u %.2f %.2f\n",
               (unsigned)tensor_data.dimensions[0],
               (unsigned)tensor_data.dimensions[1],
               (unsigned)tensor_data.dimensions[2],
               (unsigned)tensor_data.dimensions[3],
               sum,
               tensor_data.values[1][2][3][4]);
    }

    for (row = 0; row < 6; ++row)
        printf("[%d] %d %s\n", row, name_lengths[row], names[row]);

    return 0;
}