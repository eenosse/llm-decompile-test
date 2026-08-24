/*
 * 06_multidim_array.c
 * Target feature: multi-dimensional arrays - 2-D/3-D arrays inside structs,
 * arrays of structs of arrays, pointer-to-array parameters (int (*)[N]),
 * and VLA-style manual striding.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

#define BOARD_W 8
#define BOARD_H 8
#define LAYERS  3

typedef struct Cell {
    uint8_t piece;
    uint8_t owner;
    int16_t weight;
} Cell;

typedef struct Board {
    char  name[16];
    Cell  grid[BOARD_H][BOARD_W];
    int   heat[LAYERS][BOARD_H][BOARD_W];
    float kernel[3][3];
    uint8_t occupancy[BOARD_H];    /* one bit per column */
} Board;

typedef struct Tensor4 {
    uint16_t dim[4];
    double   values[2][3][4][5];
} Tensor4;

typedef struct StringGrid {
    char rows[6][32];
    int  lengths[6];
} StringGrid;

static void board_init(Board *b, const char *name)
{
    int y, x, l;

    memset(b, 0, sizeof(*b));
    strncpy(b->name, name, sizeof(b->name) - 1);

    for (y = 0; y < BOARD_H; y++)
        for (x = 0; x < BOARD_W; x++) {
            Cell *c = &b->grid[y][x];
            c->piece  = (uint8_t)(((x + y) % 5 == 0) ? (1 + (x * y) % 6) : 0);
            c->owner  = (uint8_t)(c->piece ? (y < 4 ? 1 : 2) : 0);
            c->weight = (int16_t)(c->piece * 10 - (x - y));
            if (c->piece)
                b->occupancy[y] |= (uint8_t)(1u << x);
        }

    for (l = 0; l < LAYERS; l++)
        for (y = 0; y < BOARD_H; y++)
            for (x = 0; x < BOARD_W; x++)
                b->heat[l][y][x] = (l + 1) * (x * x - y);

    for (y = 0; y < 3; y++)
        for (x = 0; x < 3; x++)
            b->kernel[y][x] = (y == 1 && x == 1) ? 0.5f : 0.0625f;
}

/* pointer-to-array parameter: forces the decompiler to see int[8] stride */
static int row_sum(int (*rows)[BOARD_W], int nrows)
{
    int total = 0;
    int y, x;

    for (y = 0; y < nrows; y++)
        for (x = 0; x < BOARD_W; x++)
            total += rows[y][x];
    return total;
}

static void board_convolve(const Board *src, int layer, float out[BOARD_H][BOARD_W])
{
    int y, x, ky, kx;

    for (y = 0; y < BOARD_H; y++)
        for (x = 0; x < BOARD_W; x++) {
            float acc = 0.0f;
            for (ky = -1; ky <= 1; ky++)
                for (kx = -1; kx <= 1; kx++) {
                    int sy = y + ky, sx = x + kx;
                    if (sy < 0 || sy >= BOARD_H || sx < 0 || sx >= BOARD_W)
                        continue;
                    acc += src->kernel[ky + 1][kx + 1] * (float)src->heat[layer][sy][sx];
                }
            out[y][x] = acc;
        }
}

static void tensor_fill(Tensor4 *t)
{
    int a, b, c, d;

    t->dim[0] = 2; t->dim[1] = 3; t->dim[2] = 4; t->dim[3] = 5;
    for (a = 0; a < 2; a++)
        for (b = 0; b < 3; b++)
            for (c = 0; c < 4; c++)
                for (d = 0; d < 5; d++)
                    t->values[a][b][c][d] = (double)(a * 100 + b * 20 + c * 5 + d) / 7.0;
}

static double tensor_trace(const Tensor4 *t)
{
    double acc = 0.0;
    int i;

    for (i = 0; i < 2; i++)
        acc += t->values[i][i][i][i];
    for (i = 0; i < 3; i++)
        acc += t->values[1][i][i + 1][i + 2];
    return acc;
}

static void grid_fill(StringGrid *g)
{
    int i;
    for (i = 0; i < 6; i++) {
        snprintf(g->rows[i], sizeof(g->rows[i]), "row-%d:%.*s", i, i * 3, "############");
        g->lengths[i] = (int)strlen(g->rows[i]);
    }
}

int main(void)
{
    static Board board;
    static Tensor4 tensor;
    static StringGrid grid;
    float conv[BOARD_H][BOARD_W];
    int y, x, l;

    board_init(&board, "arena");
    tensor_fill(&tensor);
    grid_fill(&grid);

    BENCH_OUTPUT(printf("board '%s'\n", board.name), printf("%s\n", board.name));
    for (y = 0; y < BOARD_H; y++) {
        for (x = 0; x < BOARD_W; x++)
            printf("%c%d ", board.grid[y][x].owner ? 'P' : '.', board.grid[y][x].piece);
        BENCH_OUTPUT(printf("| occ=%02x\n", board.occupancy[y]),
                     printf("%02x\n", board.occupancy[y]));
    }

    for (l = 0; l < LAYERS; l++)
        BENCH_OUTPUT(printf("heat layer %d sum = %d\n", l,
                           row_sum(board.heat[l], BOARD_H)),
                     printf("%d %d\n", l, row_sum(board.heat[l], BOARD_H)));

    board_convolve(&board, 1, conv);
    BENCH_OUTPUT(
        printf("conv corners: %.3f %.3f %.3f %.3f\n", conv[0][0],
               conv[0][BOARD_W - 1], conv[BOARD_H - 1][0],
               conv[BOARD_H - 1][BOARD_W - 1]),
        printf("%.3f %.3f %.3f %.3f\n", conv[0][0], conv[0][BOARD_W - 1],
               conv[BOARD_H - 1][0], conv[BOARD_H - 1][BOARD_W - 1]));

    BENCH_OUTPUT(
        printf("tensor dims %u,%u,%u,%u trace=%.5f edge=%.5f\n",
               tensor.dim[0], tensor.dim[1], tensor.dim[2], tensor.dim[3],
               tensor_trace(&tensor), tensor.values[1][2][3][4]),
        printf("%u %u %u %u %.5f %.5f\n", tensor.dim[0], tensor.dim[1],
               tensor.dim[2], tensor.dim[3], tensor_trace(&tensor),
               tensor.values[1][2][3][4]));

    for (y = 0; y < 6; y++)
        BENCH_OUTPUT(printf("grid[%d] len=%2d '%s'\n", y, grid.lengths[y], grid.rows[y]),
                     printf("%d %d %s\n", y, grid.lengths[y], grid.rows[y]));

    return 0;
}
