#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct cell {
    uint8_t value;
    uint8_t state;
    int16_t score;
};

struct arena_data {
    char name[16];
    struct cell cells[8][8];
    int32_t cube[3][8][8];
    float weights[3][3];
    uint8_t row_masks[8];
};

struct tensor_data {
    uint16_t dimensions[4];
    double values[2][3][4][5];
};

static char labels[6][32];
static int32_t label_lengths[6];
static struct tensor_data tensor;
static struct arena_data arena;

int main(void)
{
    memset(&arena, 0, sizeof arena);
    memcpy(arena.name, "arena", sizeof "arena");

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            struct cell *cell = &arena.cells[y][x];

            if ((x + y) % 5 == 0) {
                cell->state = (uint8_t)(1 + (y > 3));
                cell->value = (uint8_t)((x * y) % 6 + 1);
                cell->score = (int16_t)(y - x + 10 * cell->value);
                arena.row_masks[y] |= (uint8_t)(1U << x);
            } else {
                cell->value = 0;
                cell->state = 0;
                cell->score = (int16_t)(y - x);
            }
        }
    }

    for (int z = 0; z < 3; ++z) {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x)
                arena.cube[z][y][x] = (x * x - y) * (z + 1);
        }
    }

    arena.weights[0][0] = 0.0625f;
    arena.weights[0][1] = 0.0625f;
    arena.weights[0][2] = 0.0625f;
    arena.weights[1][0] = 0.0625f;
    arena.weights[1][1] = 0.5f;
    arena.weights[1][2] = 0.0625f;
    arena.weights[2][0] = 0.0625f;
    arena.weights[2][1] = 0.0625f;
    arena.weights[2][2] = 0.0625f;

    tensor.dimensions[0] = 2;
    tensor.dimensions[1] = 3;
    tensor.dimensions[2] = 4;
    tensor.dimensions[3] = 5;

    for (int a = 0; a < 2; ++a) {
        for (int b = 0; b < 3; ++b) {
            for (int c = 0; c < 4; ++c) {
                for (int d = 0; d < 5; ++d) {
                    tensor.values[a][b][c][d] =
                        (double)(a * 100 + b * 20 + c * 5 + d) / 100.0;
                }
            }
        }
    }

    for (int i = 0; i < 6; ++i) {
        snprintf(labels[i], sizeof labels[i],
                 "rec%d:%d:%s", i, i * 3, "static-token");
        label_lengths[i] = (int32_t)strlen(labels[i]);
    }

    puts(arena.name);

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            printf("%c%u ",
                   arena.cells[y][x].state ? 'P' : '.',
                   (unsigned)arena.cells[y][x].value);
        }
        printf("%02x\n", (unsigned)arena.row_masks[y]);
    }

    for (int z = 0; z < 3; ++z) {
        int32_t sum = 0;

        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                sum += arena.cube[z][y][x];

        printf("%d:%d\n", z, sum);
    }

    float filtered[8][8];

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            float sum = 0.0f;

            for (int ky = 0; ky < 3; ++ky) {
                int source_y = y + ky - 1;

                for (int kx = 0; kx < 3; ++kx) {
                    int source_x = x + kx - 1;

                    if (source_y >= 0 && source_y < 8 &&
                        source_x >= 0 && source_x < 8) {
                        sum += (float)arena.cube[1][source_y][source_x]
                             * arena.weights[ky][kx];
                    }
                }
            }

            filtered[y][x] = sum;
        }
    }

    printf("%.2f %.2f %.2f %.2f\n",
           (double)filtered[0][0],
           (double)filtered[0][7],
           (double)filtered[7][0],
           (double)filtered[7][7]);

    double selected_sum =
        tensor.values[0][0][0][0] +
        tensor.values[1][1][1][1] +
        tensor.values[1][0][1][2] +
        tensor.values[1][1][2][3] +
        tensor.values[1][2][3][4];

    printf("%u %u %u %u %.2f %.2f\n",
           (unsigned)tensor.dimensions[0],
           (unsigned)tensor.dimensions[1],
           (unsigned)tensor.dimensions[2],
           (unsigned)tensor.dimensions[3],
           selected_sum,
           tensor.values[1][2][3][4]);

    for (int i = 0; i < 6; ++i)
        printf("%d:%d:%s\n", i, label_lengths[i], labels[i]);

    return 0;
}