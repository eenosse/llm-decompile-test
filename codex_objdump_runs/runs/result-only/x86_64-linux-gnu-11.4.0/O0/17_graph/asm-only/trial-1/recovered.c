#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAX_VERTICES = 12,
    MAX_EDGES = 48,
    INFINITY_DISTANCE = 1000000
};

typedef struct Vertex {
    char code[4];
    char name[16];
    double coordinate_x;
    double coordinate_y;
    int32_t value;
} Vertex;

typedef struct EdgeNode {
    uint16_t destination;
    uint16_t weight;
    int32_t value;
    struct EdgeNode *next;
} EdgeNode;

typedef struct EdgeRecord {
    uint16_t source;
    uint16_t destination;
    uint16_t weight;
    uint16_t padding;
    int32_t value;
} EdgeRecord;

typedef struct Graph {
    Vertex vertices[MAX_VERTICES];
    EdgeNode *adjacency[MAX_VERTICES];
    uint16_t matrix[MAX_VERTICES][MAX_VERTICES];
    EdgeRecord edges[MAX_EDGES];
    uint16_t vertex_count;
    uint16_t edge_count;
} Graph;

typedef struct SearchState {
    int32_t distance[MAX_VERTICES];
    int16_t predecessor[MAX_VERTICES];
    uint8_t visited[MAX_VERTICES];
    uint16_t queue[24];
    int32_t queue_head;
    int32_t queue_tail;
    int32_t visit_count;
} SearchState;

_Static_assert(sizeof(Vertex) == 48, "unexpected Vertex layout");
_Static_assert(sizeof(EdgeNode) == 16, "unexpected EdgeNode layout");
_Static_assert(sizeof(EdgeRecord) == 12, "unexpected EdgeRecord layout");
_Static_assert(offsetof(Graph, adjacency) == 0x240, "unexpected Graph layout");
_Static_assert(offsetof(Graph, matrix) == 0x2a0, "unexpected Graph layout");
_Static_assert(offsetof(Graph, edges) == 0x3c0, "unexpected Graph layout");
_Static_assert(offsetof(Graph, vertex_count) == 0x600, "unexpected Graph layout");
_Static_assert(sizeof(Graph) == 0x608, "unexpected Graph size");
_Static_assert(sizeof(SearchState) == 0x90, "unexpected SearchState size");

static Graph graph_data;

static uint16_t add_vertex(Graph *graph, const char *code, const char *name,
                           double x, double y, int32_t value)
{
    uint16_t index = graph->vertex_count;
    Vertex *vertex = &graph->vertices[index];

    memset(vertex, 0, sizeof(*vertex));
    strncpy(vertex->code, code, 3);
    strncpy(vertex->name, name, 15);
    vertex->coordinate_x = x;
    vertex->coordinate_y = y;
    vertex->value = value;

    graph->vertex_count = (uint16_t)(index + 1);
    return index;
}

static void add_edge(Graph *graph, uint16_t source, uint16_t destination,
                     uint16_t weight, int32_t value)
{
    EdgeNode *node = calloc(1, sizeof(*node));

    if (node == NULL)
        exit(1);

    node->destination = destination;
    node->weight = weight;
    node->value = value;
    node->next = graph->adjacency[source];
    graph->adjacency[source] = node;

    graph->matrix[source][destination] = weight;

    if (graph->edge_count < MAX_EDGES) {
        EdgeRecord *record = &graph->edges[graph->edge_count++];

        record->source = source;
        record->destination = destination;
        record->weight = weight;
        record->value = value;
    }
}

static void add_bidirectional_edge(Graph *graph, uint16_t first,
                                   uint16_t second, uint16_t weight,
                                   int32_t value)
{
    add_edge(graph, first, second, weight, value);
    add_edge(graph, second, first, (uint16_t)(weight + 10), value + 25);
}

static void initialize_search(SearchState *state, uint16_t count)
{
    uint16_t i;

    memset(state, 0, sizeof(*state));
    for (i = 0; i < count; ++i) {
        state->distance[i] = INFINITY_DISTANCE;
        state->predecessor[i] = -1;
    }
}

static void breadth_first_search(Graph *graph, uint16_t start,
                                 SearchState *state)
{
    initialize_search(state, graph->vertex_count);

    state->distance[start] = 0;
    state->visited[start] = 1;
    state->visit_count++;
    state->queue[state->queue_tail++] = start;

    while (state->queue_head < state->queue_tail) {
        uint16_t current = state->queue[state->queue_head++];
        EdgeNode *edge;

        state->visit_count++;

        for (edge = graph->adjacency[current]; edge != NULL; edge = edge->next) {
            uint16_t next = edge->destination;

            if (!state->visited[next]) {
                state->visited[next] = 1;
                state->distance[next] = state->distance[current] + 1;
                state->predecessor[next] = (int16_t)current;
                state->queue[state->queue_tail++] = next;
            }
        }
    }
}

static void depth_first_visit(Graph *graph, uint16_t current,
                              SearchState *state, int32_t depth)
{
    uint16_t next;

    state->visited[current] = 1;
    state->visit_count++;
    printf("%d:%s\n", depth, graph->vertices[current].code);

    for (next = 0; next < graph->vertex_count; ++next) {
        if (graph->matrix[current][next] != 0 && !state->visited[next]) {
            state->predecessor[next] = (int16_t)current;
            depth_first_visit(graph, next, state, depth + 1);
        }
    }
}

static void shortest_paths(Graph *graph, uint16_t start, SearchState *state)
{
    uint16_t iteration;

    initialize_search(state, graph->vertex_count);
    state->distance[start] = 0;

    for (iteration = 0; iteration < graph->vertex_count; ++iteration) {
        int32_t selected = -1;
        uint16_t i;
        EdgeNode *edge;

        for (i = 0; i < graph->vertex_count; ++i) {
            if (!state->visited[i] &&
                (selected < 0 ||
                 state->distance[i] < state->distance[selected])) {
                selected = i;
            }
        }

        if (selected < 0 ||
            state->distance[selected] == INFINITY_DISTANCE) {
            break;
        }

        state->visited[selected] = 1;
        state->visit_count++;

        for (edge = graph->adjacency[selected];
             edge != NULL;
             edge = edge->next) {
            uint16_t next = edge->destination;
            int32_t candidate =
                state->distance[selected] + (int32_t)edge->weight;

            if (candidate < state->distance[next]) {
                state->distance[next] = candidate;
                state->predecessor[next] = (int16_t)selected;
            }
        }
    }
}

static void print_path(Graph *graph, SearchState *state, uint16_t destination)
{
    uint16_t path[MAX_VERTICES];
    int32_t length = 0;
    uint16_t current = destination;

    while ((int16_t)current >= 0 && length <= MAX_VERTICES - 1) {
        path[length++] = current;
        current = (uint16_t)state->predecessor[(int16_t)current];
    }

    while (length > 0) {
        --length;
        printf("%s%s",
               graph->vertices[path[length]].code,
               length != 0 ? " " : "");
    }

    printf(" %d\n", state->distance[destination]);
}

static void print_matrix(Graph *graph)
{
    uint16_t row;
    uint16_t column;

    for (row = 0; row < graph->vertex_count; ++row) {
        printf("%s", graph->vertices[row].code);

        for (column = 0; column < graph->vertex_count; ++column) {
            if (graph->matrix[row][column] != 0)
                printf(" %u", (unsigned)graph->matrix[row][column]);
            else
                printf(" -");
        }

        putchar('\n');
    }
}

static void free_edges(Graph *graph)
{
    uint16_t i;

    for (i = 0; i < graph->vertex_count; ++i) {
        EdgeNode *node = graph->adjacency[i];

        while (node != NULL) {
            EdgeNode *next = node->next;
            free(node);
            node = next;
        }

        graph->adjacency[i] = NULL;
    }
}

int main(void)
{
    SearchState state;
    uint16_t vertex[10];
    uint16_t i;
    uint32_t minimum_value = UINT32_MAX;
    int32_t minimum_index = -1;

    static const char *const codes[10] = {
        "N00", "N01", "N02", "N03", "N04",
        "N05", "N06", "N07", "N08", "N09"
    };
    static const char *const names[10] = {
        "Node0", "Node1", "Node2", "Node3", "Node4",
        "Node5", "Node6", "Node7", "Node8", "Node9"
    };
    static const double coordinates[10][2] = {
        { 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 },
        { 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 },
        { 0.0, 0.0 }, { 0.0, 0.0 }
    };
    static const int32_t values[10] = {
        0x01ba8140, 0x02719c40, 0x043b5fc0, 0x0280de80,
        0x04c4b400, 0x0487ab00, 0x054e0840, 0x040d9900,
        0x0365c040, 0x03b20b80
    };

    memset(&graph_data, 0, sizeof(graph_data));

    for (i = 0; i < 10; ++i) {
        vertex[i] = add_vertex(&graph_data, codes[i], names[i],
                               coordinates[i][0], coordinates[i][1],
                               values[i]);
    }

    add_bidirectional_edge(&graph_data, vertex[0], vertex[1], 125, 90);
    add_bidirectional_edge(&graph_data, vertex[0], vertex[2], 105, 180);
    add_bidirectional_edge(&graph_data, vertex[1], vertex[7], 110, 140);
    add_bidirectional_edge(&graph_data, vertex[2], vertex[3], 230, 320);
    add_bidirectional_edge(&graph_data, vertex[2], vertex[6], 505, 640);
    add_bidirectional_edge(&graph_data, vertex[7], vertex[6], 445, 590);
    add_bidirectional_edge(&graph_data, vertex[6], vertex[4], 430, 700);
    add_bidirectional_edge(&graph_data, vertex[4], vertex[5], 80, 120);
    add_bidirectional_edge(&graph_data, vertex[3], vertex[8], 585, 880);
    add_bidirectional_edge(&graph_data, vertex[8], vertex[9], 330, 260);
    add_bidirectional_edge(&graph_data, vertex[4], vertex[9], 425, 540);

    printf("%u %u\n", (unsigned)graph_data.vertex_count,
           (unsigned)graph_data.edge_count);

    for (i = 0; i < graph_data.vertex_count; ++i) {
        EdgeNode *edge;

        printf("%s %s %f %f %d\n",
               graph_data.vertices[i].code,
               graph_data.vertices[i].name,
               graph_data.vertices[i].coordinate_x,
               graph_data.vertices[i].coordinate_y,
               graph_data.vertices[i].value);

        for (edge = graph_data.adjacency[i];
             edge != NULL;
             edge = edge->next) {
            printf("%s %u %d\n",
                   graph_data.vertices[edge->destination].code,
                   (unsigned)edge->weight,
                   edge->value);
        }

        putchar('\n');
    }

    print_matrix(&graph_data);

    breadth_first_search(&graph_data, vertex[0], &state);

    for (i = 0; i < graph_data.vertex_count; ++i) {
        const char *predecessor;

        if (state.predecessor[i] >= 0)
            predecessor =
                graph_data.vertices[(uint16_t)state.predecessor[i]].code;
        else
            predecessor = "-";

        printf("%s %d %s\n",
               graph_data.vertices[i].code,
               state.distance[i] == INFINITY_DISTANCE
                   ? -1
                   : state.distance[i],
               predecessor);
    }

    printf("%d\n", state.visit_count);

    initialize_search(&state, graph_data.vertex_count);
    depth_first_visit(&graph_data, vertex[0], &state, 1);

    shortest_paths(&graph_data, vertex[0], &state);

    for (i = 0; i < graph_data.vertex_count; ++i) {
        printf("%s\n", graph_data.vertices[i].code);
        print_path(&graph_data, &state, i);
    }

    for (i = 0; i < graph_data.edge_count; ++i) {
        uint32_t candidate = (uint32_t)graph_data.edges[i].value;

        if (minimum_value > candidate) {
            minimum_value = candidate;
            minimum_index = i;
        }
    }

    if (minimum_index >= 0) {
        EdgeRecord *record = &graph_data.edges[minimum_index];

        printf("%s %s %d %u\n",
               graph_data.vertices[record->source].code,
               graph_data.vertices[record->destination].code,
               record->value,
               (unsigned)record->weight);
    }

    free_edges(&graph_data);
    return 0;
}