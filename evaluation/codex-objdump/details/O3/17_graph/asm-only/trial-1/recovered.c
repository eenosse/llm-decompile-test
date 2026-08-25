#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAX_VERTICES = 12,
    MAX_RECORDED_EDGES = 48,
    INFINITY_DISTANCE = 1000000
};

typedef struct {
    char code[4];
    char name[16];
    double coordinate_a;
    double coordinate_b;
    uint32_t quantity;
} Vertex;

typedef struct Edge {
    uint16_t destination;
    uint16_t weight;
    uint32_t metric;
    struct Edge *next;
} Edge;

typedef struct {
    uint16_t source;
    uint16_t destination;
    uint16_t weight;
    uint32_t metric;
} EdgeRecord;

typedef struct {
    Vertex vertices[MAX_VERTICES];
    Edge *adjacency[MAX_VERTICES];
    uint16_t matrix[MAX_VERTICES][MAX_VERTICES];
    EdgeRecord records[MAX_RECORDED_EDGES];
    uint16_t vertex_count;
    uint16_t edge_count;
} Graph;

typedef struct {
    int32_t distance[MAX_VERTICES];
    int16_t predecessor[MAX_VERTICES];
    uint8_t visited[MAX_VERTICES];
    uint16_t queue[24];
    int32_t queue_front;
    int32_t queue_rear;
    int32_t visited_count;
} SearchState;

_Static_assert(sizeof(Vertex) == 48, "unexpected Vertex layout");
_Static_assert(sizeof(Edge) == 16, "unexpected Edge layout");
_Static_assert(sizeof(EdgeRecord) == 12, "unexpected EdgeRecord layout");
_Static_assert(offsetof(Graph, adjacency) == 576, "unexpected Graph layout");
_Static_assert(offsetof(Graph, matrix) == 672, "unexpected Graph layout");
_Static_assert(offsetof(Graph, records) == 960, "unexpected Graph layout");
_Static_assert(offsetof(Graph, vertex_count) == 1536, "unexpected Graph layout");
_Static_assert(sizeof(Graph) == 1544, "unexpected Graph size");
_Static_assert(sizeof(SearchState) == 144, "unexpected SearchState size");

static Graph graph_data;

static uint16_t add_vertex(const char *code, const char *name,
                           uint32_t quantity, double coordinate_a,
                           double coordinate_b)
{
    uint16_t index = graph_data.vertex_count;
    Vertex *vertex = &graph_data.vertices[index];

    memset(vertex, 0, sizeof(*vertex));
    strncpy(vertex->code, code, 3);
    strncpy(vertex->name, name, 15);
    vertex->coordinate_a = coordinate_a;
    vertex->coordinate_b = coordinate_b;
    vertex->quantity = quantity;
    graph_data.vertex_count = (uint16_t)(index + 1);

    return index;
}

static void add_directed_edge(uint16_t source, uint16_t destination,
                              uint16_t weight, uint32_t metric)
{
    Edge *edge = calloc(1, sizeof(*edge));

    if (edge == NULL)
        exit(1);

    edge->destination = destination;
    edge->weight = weight;
    edge->metric = metric;
    edge->next = graph_data.adjacency[source];
    graph_data.adjacency[source] = edge;
    graph_data.matrix[source][destination] = weight;

    if (graph_data.edge_count < MAX_RECORDED_EDGES) {
        EdgeRecord *record = &graph_data.records[graph_data.edge_count++];

        record->source = source;
        record->destination = destination;
        record->weight = weight;
        record->metric = metric;
    }
}

static void add_bidirectional_edge(uint16_t first, uint16_t second,
                                   uint16_t weight, uint32_t metric)
{
    add_directed_edge(first, second, weight, metric);
    add_directed_edge(second, first, (uint16_t)(weight + 10),
                      metric + 25);
}

static void initialize_search(SearchState *state, uint16_t vertex_count)
{
    uint16_t i;

    memset(state, 0, sizeof(*state));

    for (i = 0; i < vertex_count; ++i) {
        state->distance[i] = INFINITY_DISTANCE;
        state->predecessor[i] = -1;
    }
}

static void breadth_first_search(uint16_t start, SearchState *state)
{
    state->visited[start] = 1;
    state->distance[start] = 0;
    state->queue[state->queue_rear++] = start;

    while (state->queue_front < state->queue_rear) {
        uint16_t current = state->queue[state->queue_front++];
        Edge *edge = graph_data.adjacency[current];

        ++state->visited_count;

        while (edge != NULL) {
            uint16_t destination = edge->destination;

            if (!state->visited[destination]) {
                state->visited[destination] = 1;
                state->predecessor[destination] = (int16_t)current;
                state->distance[destination] =
                    state->distance[current] + 1;
                state->queue[state->queue_rear++] = destination;
            }

            edge = edge->next;
        }
    }
}

static void depth_first_search(uint16_t current, SearchState *state,
                               int depth)
{
    uint16_t destination;

    state->visited[current] = 1;
    ++state->visited_count;
    printf("%*s\n", depth, graph_data.vertices[current].code);

    for (destination = 0;
         destination < graph_data.vertex_count;
         ++destination) {
        if (graph_data.matrix[current][destination] != 0 &&
            !state->visited[destination]) {
            state->predecessor[destination] = (int16_t)current;
            depth_first_search(destination, state, depth + 1);
        }
    }
}

static void shortest_paths(uint16_t start, SearchState *state)
{
    uint16_t iteration;

    state->distance[start] = 0;

    for (iteration = 0; iteration < graph_data.vertex_count; ++iteration) {
        int selected = -1;
        uint16_t i;
        Edge *edge;

        for (i = 0; i < graph_data.vertex_count; ++i) {
            if (!state->visited[i] &&
                (selected < 0 ||
                 state->distance[i] < state->distance[selected])) {
                selected = i;
            }
        }

        if (selected < 0 ||
            state->distance[selected] == INFINITY_DISTANCE)
            break;

        state->visited[selected] = 1;
        ++state->visited_count;

        edge = graph_data.adjacency[selected];
        while (edge != NULL) {
            uint16_t destination = edge->destination;
            int32_t candidate =
                state->distance[selected] + edge->weight;

            if (candidate < state->distance[destination]) {
                state->distance[destination] = candidate;
                state->predecessor[destination] =
                    (int16_t)selected;
            }

            edge = edge->next;
        }
    }
}

static void print_graph(void)
{
    uint16_t i;

    printf("%u %u\n", (unsigned)graph_data.vertex_count,
           (unsigned)graph_data.edge_count);

    for (i = 0; i < graph_data.vertex_count; ++i) {
        const Vertex *vertex = &graph_data.vertices[i];
        Edge *edge;

        printf("%s %s %.2f %.2f %u",
               vertex->code, vertex->name,
               vertex->coordinate_a, vertex->coordinate_b,
               vertex->quantity);

        for (edge = graph_data.adjacency[i];
             edge != NULL;
             edge = edge->next) {
            printf(" %s %u %u",
                   graph_data.vertices[edge->destination].code,
                   (unsigned)edge->weight, edge->metric);
        }

        putchar('\n');
    }

    for (i = 0; i < graph_data.vertex_count; ++i) {
        uint16_t j;

        printf("%s", graph_data.vertices[i].code);
        for (j = 0; j < graph_data.vertex_count; ++j) {
            if (graph_data.matrix[i][j] != 0)
                printf("%u ", (unsigned)graph_data.matrix[i][j]);
            else
                printf("0 ");
        }
        putchar('\n');
    }
}

static void print_breadth_first_result(const SearchState *state)
{
    uint16_t i;

    for (i = 0; i < graph_data.vertex_count; ++i) {
        const char *predecessor = "-";
        int32_t distance = state->distance[i];

        if (state->predecessor[i] >= 0)
            predecessor =
                graph_data.vertices[state->predecessor[i]].code;

        if (distance == INFINITY_DISTANCE)
            distance = -1;

        printf("%s %d %s\n", graph_data.vertices[i].code,
               distance, predecessor);
    }

    printf("%u\n", (unsigned)state->visited_count);
}

static void print_shortest_path_result(const SearchState *state)
{
    uint16_t destination;

    for (destination = 0;
         destination < graph_data.vertex_count;
         ++destination) {
        int16_t path[MAX_VERTICES];
        unsigned length = 0;
        int16_t current = (int16_t)destination;

        while (length < MAX_VERTICES && current >= 0) {
            path[length++] = current;
            current = state->predecessor[current];
        }

        printf("%s ", graph_data.vertices[destination].code);

        while (length != 0) {
            --length;
            printf("%s%s",
                   graph_data.vertices[path[length]].code,
                   length == 0 ? "" : " ");
        }

        printf("%d\n", state->distance[destination]);
    }
}

static void print_minimum_metric_edge(void)
{
    uint16_t i;
    int selected = -1;
    uint32_t minimum = UINT32_MAX;

    for (i = 0; i < graph_data.edge_count; ++i) {
        if (graph_data.records[i].metric < minimum) {
            minimum = graph_data.records[i].metric;
            selected = i;
        }
    }

    if (selected >= 0) {
        const EdgeRecord *record = &graph_data.records[selected];

        printf("%s %s %u %u\n",
               graph_data.vertices[record->source].code,
               graph_data.vertices[record->destination].code,
               record->metric, (unsigned)record->weight);
    }
}

static void free_graph(void)
{
    uint16_t i;

    for (i = 0; i < graph_data.vertex_count; ++i) {
        Edge *edge = graph_data.adjacency[i];

        while (edge != NULL) {
            Edge *next = edge->next;
            free(edge);
            edge = next;
        }

        graph_data.adjacency[i] = NULL;
    }
}

int main(void)
{
    SearchState state;
    uint16_t v0, v1, v2, v3, v4;
    uint16_t v5, v6, v7, v8, v9;

    memset(&graph_data, 0, sizeof(graph_data));

    v0 = add_vertex("A00", "Alpha", 29000000, 0.0, 0.0);
    v1 = add_vertex("A01", "Location 1", 41000000, 0.0, 0.0);
    v2 = add_vertex("A02", "Location 2", 71000000, 0.0, 0.0);
    v3 = add_vertex("A03", "Delta", 42000000, 0.0, 0.0);
    v4 = add_vertex("A04", "Region", 80000000, 0.0, 0.0);
    v5 = add_vertex("A05", "Point", 76000000, 0.0, 0.0);
    v6 = add_vertex("A06", "Node6", 89000000, 0.0, 0.0);
    v7 = add_vertex("A07", "Location7", 68000000, 0.0, 0.0);
    v8 = add_vertex("A08", "Destination8", 57000000, 0.0, 0.0);
    v9 = add_vertex("A09", "Terminal", 62000000, 0.0, 0.0);

    add_bidirectional_edge(v0, v1, 125, 90);
    add_bidirectional_edge(v0, v2, 105, 180);
    add_bidirectional_edge(v1, v7, 110, 140);
    add_bidirectional_edge(v2, v3, 230, 320);
    add_bidirectional_edge(v2, v6, 505, 640);
    add_bidirectional_edge(v7, v6, 445, 590);
    add_bidirectional_edge(v6, v4, 430, 700);
    add_bidirectional_edge(v4, v5, 80, 120);
    add_bidirectional_edge(v3, v8, 585, 880);
    add_bidirectional_edge(v8, v9, 330, 260);
    add_bidirectional_edge(v4, v9, 425, 540);

    print_graph();

    initialize_search(&state, graph_data.vertex_count);
    breadth_first_search(v0, &state);
    print_breadth_first_result(&state);

    initialize_search(&state, graph_data.vertex_count);
    depth_first_search(v0, &state, 1);

    initialize_search(&state, graph_data.vertex_count);
    shortest_paths(v0, &state);
    print_shortest_path_result(&state);

    print_minimum_metric_edge();
    free_graph();

    return 0;
}