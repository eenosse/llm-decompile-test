#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t value;
    uint8_t category;
    int16_t score;
} Cell;

typedef struct {
    char name[16];
    Cell cells[8][8];
    int32_t data[3][8][8];
    float kernel[3][3];
    uint8_t row_masks[8];
} State;

typedef struct {
    uint16_t dimensions[4];
    double values[2][3][4][5];
} Volume;

typedef struct {
    char text[6][32];
    int32_t lengths[6];
} TextTable;

_Static_assert(sizeof(Cell) == 4, "unexpected Cell layout");
_Static_assert(offsetof(State, cells) == 16, "unexpected State layout");
_Static_assert(offsetof(State, data) == 272, "unexpected State layout");
_Static_assert(offsetof(State, kernel) == 1040, "unexpected State layout");
_Static_assert(offsetof(State, row_masks) == 1076, "unexpected State layout");
_Static_assert(sizeof(State) == 1084, "unexpected State size");
_Static_assert(offsetof(Volume, values) == 8, "unexpected Volume layout");
_Static_assert(offsetof(TextTable, lengths) == 192,
               "unexpected TextTable layout");

static _Alignas(32) State global_state;
static _Alignas(32) Volume global_volume;
static _Alignas(32) TextTable global_text;

static void initialize_state(State *state, const char *name)
{
    int layer;
    int row;
    int column;

    memset(state, 0, sizeof(*state));
    strncpy(state->name, name, 15);

    for (row = 0; row < 8; ++row) {
        for (column = 0; column < 8; ++column) {
            Cell *cell = &state->cells[row][column];

            if ((row + column) % 5 == 0)
                cell->value = (uint8_t)((row * column) % 6 + 1);
            else
                cell->value = 0;

            if (cell->value != 0)
                cell->category = (uint8_t)(row <= 3 ? 1 : 2);
            else
                cell->category = 0;

            cell->score =
                (int16_t)(10 * (int)cell->value + row - column);

            if (cell->value != 0)
                state->row_masks[row] =
                    (uint8_t)(state->row_masks[row] | (1U << column));
        }
    }

    for (layer = 0; layer < 3; ++layer) {
        for (row = 0; row < 8; ++row) {
            for (column = 0; column < 8; ++column) {
                state->data[layer][row][column] =
                    (layer + 1) * (column * column - row);
            }
        }
    }

    for (row = 0; row < 3; ++row) {
        for (column = 0; column < 3; ++column) {
            state->kernel[row][column] =
                (row == 1 && column == 1) ? 1.0f : 0.125f;
        }
    }
}

static int32_t sum_rows(const int32_t matrix[][8], int32_t row_count)
{
    int32_t sum = 0;
    int32_t row;
    int32_t column;

    for (row = 0; row < row_count; ++row) {
        for (column = 0; column < 8; ++column)
            sum += matrix[row][column];
    }

    return sum;
}

static void apply_kernel(const State *state, int32_t layer,
                         float output[8][8])
{
    int32_t row;
    int32_t column;

    for (row = 0; row < 8; ++row) {
        for (column = 0; column < 8; ++column) {
            float sum = 0.0f;
            int32_t row_offset;
            int32_t column_offset;

            for (row_offset = -1; row_offset <= 1; ++row_offset) {
                for (column_offset = -1;
                     column_offset <= 1;
                     ++column_offset) {
                    int32_t source_row = row + row_offset;
                    int32_t source_column = column + column_offset;

                    if (source_row >= 0 && source_row <= 7 &&
                        source_column >= 0 && source_column <= 7) {
                        sum += state->kernel[row_offset + 1]
                                            [column_offset + 1] *
                               (float)state->data[layer][source_row]
                                                        [source_column];
                    }
                }
            }

            output[row][column] = sum;
        }
    }
}

static void initialize_volume(Volume *volume)
{
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;

    volume->dimensions[0] = 2;
    volume->dimensions[1] = 3;
    volume->dimensions[2] = 4;
    volume->dimensions[3] = 5;

    for (a = 0; a < 2; ++a) {
        for (b = 0; b < 3; ++b) {
            for (c = 0; c < 4; ++c) {
                for (d = 0; d < 5; ++d) {
                    volume->values[a][b][c][d] =
                        (100 * a + 20 * b + 5 * c + d) / 10.0;
                }
            }
        }
    }
}

static double selected_volume_sum(const Volume *volume)
{
    double sum = 0.0;
    int32_t i;

    for (i = 0; i < 2; ++i)
        sum += volume->values[i][i][i][i];

    for (i = 0; i < 3; ++i)
        sum += volume->values[1][i][i + 1][i + 2];

    return sum;
}

static void initialize_text_table(TextTable *table)
{
    static const char suffix[] = "neutral-text";
    int32_t i;

    for (i = 0; i < 6; ++i) {
        snprintf(table->text[i], sizeof(table->text[i]),
                 "n%02d-%d-%s", i, 3 * i, suffix);
        table->lengths[i] = (int32_t)strlen(table->text[i]);
    }
}

int main(void)
{
    float filtered[8][8];
    double last_value;
    double volume_sum;
    int32_t row;
    int32_t column;
    int32_t layer;
    int32_t i;

    initialize_state(&global_state, "state");
    initialize_volume(&global_volume);
    initialize_text_table(&global_text);

    puts(global_state.name);

    for (row = 0; row < 8; ++row) {
        for (column = 0; column < 8; ++column) {
            Cell *cell = &global_state.cells[row][column];
            printf("%c%d ",
                   cell->category != 0 ? 'P' : '.',
                   (int)cell->value);
        }
        printf("%02X\n", (unsigned int)global_state.row_masks[row]);
    }

    for (layer = 0; layer < 3; ++layer)
        printf("%d:%d\n", layer, sum_rows(global_state.data[layer], 8));

    apply_kernel(&global_state, 1, filtered);
    printf("%.1f %.1f %.1f %.1f\n",
           (double)filtered[0][0],
           (double)filtered[0][7],
           (double)filtered[7][0],
           (double)filtered[7][7]);

    last_value = global_volume.values[1][2][3][4];
    volume_sum = selected_volume_sum(&global_volume);

    printf("%u %u %u %u %.1f %.1f\n",
           (unsigned int)global_volume.dimensions[0],
           (unsigned int)global_volume.dimensions[1],
           (unsigned int)global_volume.dimensions[2],
           (unsigned int)global_volume.dimensions[3],
           volume_sum,
           last_value);

    for (i = 0; i < 6; ++i)
        printf("%d:%d:%s\n", i, global_text.lengths[i],
               global_text.text[i]);

    return 0;
}