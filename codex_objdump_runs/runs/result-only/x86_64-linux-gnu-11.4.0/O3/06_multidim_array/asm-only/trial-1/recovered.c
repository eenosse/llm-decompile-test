#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t kind;
    uint8_t band;
    uint16_t value;
} Cell;

typedef struct {
    char name[16];
    Cell cells[8][8];
    int32_t planes[3][8][8];
    float kernel[9];
    uint8_t row_mask[8];
} Arena;

typedef struct {
    uint16_t tags[4];
    double values[2][3][4][5];
} Aggregate;

_Static_assert(sizeof(Cell) == 4, "unexpected Cell layout");
_Static_assert(offsetof(Arena, cells) == 16, "unexpected Arena layout");
_Static_assert(offsetof(Arena, planes) == 272, "unexpected Arena layout");
_Static_assert(offsetof(Arena, kernel) == 1040, "unexpected Arena layout");
_Static_assert(offsetof(Arena, row_mask) == 1076, "unexpected Arena layout");

static Arena arena;
static char labels[6][32];
static uint32_t label_lengths[6];
static Aggregate aggregate;

static const int32_t base_plane[8][8] = {
    { 0,  1,  2,  3,  4,  5,  6,  7},
    { 5,  0,  7,  8,  9, 10, 11, 12},
    {10, 11, 12, 13, 14, 15, 16, 17},
    {15, 16, 17, 18, 19, 20, 21, 22},
    {20, 21,  0, 23, 24, 25, 26, 27},
    {25, 26, 27, 28, 29, 30, 31, 32},
    {30, 31, 32, 33, 34, 35, 36, 37},
    {35, 36, 37, 38, 39, 40, 41, 42}
};

static void initialize_arena(void)
{
    unsigned row;
    unsigned column;
    unsigned plane;

    memset(&arena, 0, sizeof(arena));
    memcpy(arena.name, "arena", sizeof("arena"));

    for (row = 0; row < 8; ++row) {
        for (column = 0; column < 8; ++column) {
            Cell *cell = &arena.cells[row][column];

            if ((row + column) % 5 == 0) {
                cell->kind = (uint8_t)((column * row) % 6 + 1);
                cell->band = (uint8_t)(row > 3 ? 2 : 1);
                cell->value =
                    (uint16_t)(row + 10U * cell->kind - column);
                arena.row_mask[row] |= (uint8_t)(1U << column);
            } else {
                cell->kind = 0;
                cell->band = 0;
                cell->value = (uint16_t)(row - column);
            }
        }
    }

    for (plane = 0; plane < 3; ++plane) {
        for (row = 0; row < 8; ++row) {
            for (column = 0; column < 8; ++column) {
                arena.planes[plane][row][column] =
                    base_plane[row][column] * (int32_t)(plane + 1);
            }
        }
    }

    arena.kernel[0] = 0.0625f;
    arena.kernel[1] = 0.125f;
    arena.kernel[2] = 0.0625f;
    arena.kernel[3] = 0.125f;
    arena.kernel[4] = 0.25f;
    arena.kernel[5] = 0.125f;
    arena.kernel[6] = 0.0625f;
    arena.kernel[7] = 0.125f;
    arena.kernel[8] = 0.0625f;
}

static void initialize_aggregate(void)
{
    unsigned a;
    unsigned b;
    unsigned c;
    unsigned d;

    aggregate.tags[0] = 10;
    aggregate.tags[1] = 20;
    aggregate.tags[2] = 30;
    aggregate.tags[3] = 40;

    for (a = 0; a < 2; ++a) {
        for (b = 0; b < 3; ++b) {
            for (c = 0; c < 4; ++c) {
                for (d = 0; d < 5; ++d) {
                    aggregate.values[a][b][c][d] =
                        1000.0 * a + 100.0 * b +
                        10.0 * c + d + 0.5;
                }
            }
        }
    }
}

static void convolve_middle_plane(float output[8][8])
{
    int row;
    int column;
    int dy;
    int dx;

    for (row = 0; row < 8; ++row) {
        for (column = 0; column < 8; ++column) {
            float sum = 0.0f;

            for (dy = -1; dy <= 1; ++dy) {
                int source_row = row + dy;

                for (dx = -1; dx <= 1; ++dx) {
                    int source_column = column + dx;
                    unsigned weight =
                        (unsigned)((dy + 1) * 3 + dx + 1);

                    if (source_row >= 0 && source_row < 8 &&
                        source_column >= 0 && source_column < 8) {
                        float term =
                            (float)arena.planes[1][source_row][source_column];
                        term *= arena.kernel[weight];
                        sum += term;
                    }
                }
            }

            output[row][column] = sum;
        }
    }
}

int main(void)
{
    unsigned i;
    unsigned row;
    unsigned plane;
    float filtered[8][8];
    double selected_sum;

    initialize_arena();
    initialize_aggregate();

    for (i = 0; i < 6; ++i) {
        snprintf(labels[i], sizeof(labels[i]), "%u-%u-%s",
                 i, i * 3U, "payload-tag");
        label_lengths[i] = (uint32_t)strlen(labels[i]);
    }

    puts(arena.name);

    for (row = 0; row < 8; ++row) {
        for (i = 0; i < 8; ++i)
            printf("%c", arena.cells[row][i].band ? 'P' : '.');
        printf("%02x\n", (unsigned)arena.row_mask[row]);
    }

    for (plane = 0; plane < 3; ++plane) {
        uint32_t sum = (uint32_t)arena.planes[plane][0][1];

        sum += (uint32_t)arena.planes[plane][0][0];
        for (i = 2; i < 64; ++i)
            sum += (uint32_t)arena.planes[plane][i / 8][i % 8];

        printf("%u:%d\n", plane, (int32_t)sum);
    }

    convolve_middle_plane(filtered);
    printf("%.2f %.2f %.2f %.2f\n",
           (double)filtered[0][0],
           (double)filtered[0][7],
           (double)filtered[7][0],
           (double)filtered[7][7]);

    selected_sum = 0.0;
    selected_sum += aggregate.values[0][0][0][0];
    selected_sum += aggregate.values[1][1][1][1];
    selected_sum += aggregate.values[1][0][1][2];
    selected_sum += aggregate.values[1][1][2][3];
    selected_sum += aggregate.values[1][2][3][4];

    printf("%u %u %u %u %.2f %.2f\n",
           (unsigned)aggregate.tags[0],
           (unsigned)aggregate.tags[1],
           (unsigned)aggregate.tags[2],
           (unsigned)aggregate.tags[3],
           selected_sum,
           aggregate.values[1][2][3][4]);

    for (i = 0; i < 6; ++i)
        printf("%u:%u:%s\n", i, label_lengths[i], labels[i]);

    return 0;
}