#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAX_VERTICES = 12,
    MAX_ROUTES = 48,
    INFINITY_VALUE = 1000000
};

typedef struct {
    char code[4];
    char name[16];
    double coordinate_a;
    double coordinate_b;
    uint32_t value;
} Vertex;

typedef struct Edge {
    uint16_t destination;
    uint16_t metric;
    uint32_t cost;
    struct Edge *next;
} Edge;

typedef struct {
    uint16_t origin;
    uint16_t destination;
    uint16_t metric;
    uint32_t cost;
} Route;

typedef struct {
    Vertex vertices[MAX_VERTICES];
    Edge *adjacency[MAX_VERTICES];
    uint16_t matrix[MAX_VERTICES][MAX_VERTICES];
    Route routes[MAX_ROUTES];
    uint16_t vertex_count;
    uint16_t route_count;
} Database;

typedef struct {
    int32_t distance[MAX_VERTICES];
    int16_t predecessor[MAX_VERTICES];
    uint8_t visited[MAX_VERTICES];
    int16_t queue[24];
    int32_t front;
    int32_t rear;
    int32_t processed;
} SearchState;

_Static_assert(sizeof(Vertex) == 48, "unexpected Vertex layout");
_Static_assert(sizeof(Edge) == 16, "unexpected Edge layout");
_Static_assert(sizeof(Route) == 12, "unexpected Route layout");
_Static_assert(offsetof(Database, adjacency) == 0x240, "unexpected database layout");
_Static_assert(offsetof(Database, matrix) == 0x2a0, "unexpected database layout");
_Static_assert(offsetof(Database, routes) == 0x3c0, "unexpected database layout");
_Static_assert(offsetof(Database, vertex_count) == 0x600, "unexpected database layout");
_Static_assert(sizeof(Database) == 0x608, "unexpected database size");
_Static_assert(offsetof(SearchState, predecessor) == 0x30, "unexpected search layout");
_Static_assert(offsetof(SearchState, visited) == 0x48, "unexpected search layout");
_Static_assert(offsetof(SearchState, queue) == 0x54, "unexpected search layout");
_Static_assert(offsetof(SearchState, front) == 0x84, "unexpected search layout");
_Static_assert(offsetof(SearchState, rear) == 0x88, "unexpected search layout");
_Static_assert(offsetof(SearchState, processed) == 0x8c, "unexpected search layout");
_Static_assert(sizeof(SearchState) == 0x90, "unexpected search size");

static Database database;

static uint16_t add_vertex(const char *code, const char *name,
                           double coordinate_a, double coordinate_b,
                           uint32_t value)
{
    uint16_t index = database.vertex_count;
    Vertex *vertex = &database.vertices[index];

    memset(vertex, 0, sizeof(*vertex));
    strncpy(vertex->code, code, 3);
    strncpy(vertex->name, name, 15);
    vertex->coordinate_a = coordinate_a;
    vertex->coordinate_b = coordinate_b;
    vertex->value = value;

    database.vertex_count = (uint16_t)(index + 1);
    return index;
}

static void add_edge(uint16_t origin, uint16_t destination,
                     uint16_t metric, uint32_t cost)
{
    Edge *edge = calloc(1, sizeof(*edge));

    if (edge == NULL)
        exit(1);

    edge->destination = destination;
    edge->metric = metric;
    edge->cost = cost;
    edge->next = database.adjacency[origin];
    database.adjacency[origin] = edge;

    database.matrix[origin][destination] = metric;

    if (database.route_count <= 47) {
        Route *route = &database.routes[database.route_count++];

        route->origin = origin;
        route->destination = destination;
        route->metric = metric;
        route->cost = cost;
    }
}

static void add_bidirectional_edge(uint16_t first, uint16_t second,
                                   uint16_t metric, uint32_t cost)
{
    add_edge(first, second, metric, cost);
    add_edge(second, first, (uint16_t)(metric + 10), cost + 25);
}

static void initialize_search(SearchState *state, uint16_t count)
{
    uint16_t i;

    memset(state, 0, sizeof(*state));

    for (i = 0; i < count; ++i)
        state->distance[i] = INFINITY_VALUE;

    memset(state->predecessor, 0xff,
           (size_t)count * sizeof(state->predecessor[0]));
}

static void breadth_first_search(SearchState *state, uint16_t start)
{
    initialize_search(state, database.vertex_count);

    state->visited[start] = 1;
    state->distance[start] = 0;
    state->queue[state->rear++] = (int16_t)start;

    while (state->front < state->rear) {
        uint16_t current = (uint16_t)state->queue[state->front++];
        Edge *edge = database.adjacency[current];

        ++state->processed;

        while (edge != NULL) {
            uint16_t next = edge->destination;

            if (!state->visited[next]) {
                state->visited[next] = 1;
                state->predecessor[next] = (int16_t)current;
                state->distance[next] = state->distance[current] + 1;
                state->queue[state->rear++] = (int16_t)next;
            }

            edge = edge->next;
        }
    }
}

static void depth_first_visit(uint16_t current, SearchState *state, int depth)
{
    uint16_t next;

    (void)depth;
    state->visited[current] = 1;
    ++state->processed;
    printf("%s -> ", database.vertices[current].code);

    for (next = 0; next < database.vertex_count; ++next) {
        if (database.matrix[current][next] != 0 && !state->visited[next]) {
            state->predecessor[next] = (int16_t)current;
            depth_first_visit(next, state, depth + 1);
        }
    }
}

static void dijkstra(SearchState *state, uint16_t start)
{
    uint16_t completed = 0;

    initialize_search(state, database.vertex_count);
    state->distance[start] = 0;

    while (completed < database.vertex_count) {
        int selected = -1;
        uint16_t i;
        Edge *edge;

        for (i = 0; i < database.vertex_count; ++i) {
            if (!state->visited[i] &&
                (selected < 0 ||
                 state->distance[i] < state->distance[selected])) {
                selected = i;
            }
        }

        if (selected < 0 ||
            state->distance[selected] == INFINITY_VALUE) {
            break;
        }

        state->visited[selected] = 1;
        ++completed;

        edge = database.adjacency[selected];
        while (edge != NULL) {
            uint16_t next = edge->destination;
            int32_t candidate =
                state->distance[selected] + (int32_t)edge->cost;

            if (candidate < state->distance[next]) {
                state->distance[next] = candidate;
                state->predecessor[next] = (int16_t)selected;
            }

            edge = edge->next;
        }
    }

    state->processed = completed;
}

static void print_vertices_and_graph(void)
{
    uint16_t i;

    printf("Vertices: %hu, routes: %hu\n",
           database.vertex_count, database.route_count);

    for (i = 0; i < database.vertex_count; ++i) {
        const Vertex *vertex = &database.vertices[i];
        Edge *edge;

        printf("%s (%s): %.2f, %.2f, %u\n",
               vertex->code, vertex->name,
               vertex->coordinate_a, vertex->coordinate_b,
               vertex->value);

        edge = database.adjacency[i];
        while (edge != NULL) {
            printf("  %s (%hu, %u)",
                   database.vertices[edge->destination].code,
                   edge->metric, edge->cost);
            edge = edge->next;
        }
        putchar('\n');
    }

    for (i = 0; i < database.vertex_count; ++i) {
        uint16_t j;

        printf("%s: ", database.vertices[i].code);
        for (j = 0; j < database.vertex_count; ++j)
            printf(database.matrix[i][j] != 0 ? "1 " : "0 ");
        putchar('\n');
    }
}

static void print_breadth_first_results(const SearchState *state)
{
    uint16_t i;

    for (i = 0; i < database.vertex_count; ++i) {
        int16_t predecessor = state->predecessor[i];
        const char *predecessor_code = "-";
        int distance = state->distance[i];

        if (predecessor >= 0)
            predecessor_code = database.vertices[predecessor].code;

        if (distance == INFINITY_VALUE)
            distance = -1;

        printf("%s: distance=%d, predecessor=%s\n",
               database.vertices[i].code,
               distance,
               predecessor_code);
    }

    printf("Processed: %d\n", state->processed);
}

static void print_shortest_paths(const SearchState *state)
{
    uint16_t target;

    for (target = 0; target < database.vertex_count; ++target) {
        uint16_t path[12];
        int path_length = 0;
        int16_t current = (int16_t)target;
        int i;

        printf("%s: ", database.vertices[target].code);

        while (current >= 0 && path_length <= 11) {
            path[path_length++] = (uint16_t)current;
            current = state->predecessor[(uint16_t)current];
        }

        for (i = path_length - 1; i >= 0; --i) {
            printf("%s%s",
                   database.vertices[path[i]].code,
                   i == 0 ? "" : " -> ");
        }

        printf(" %d\n", state->distance[target]);
    }
}

static void print_minimum_route(void)
{
    uint32_t minimum = UINT32_MAX;
    int selected = -1;
    uint16_t i;

    for (i = 0; i < database.route_count; ++i) {
        if (database.routes[i].cost < minimum) {
            minimum = database.routes[i].cost;
            selected = i;
        }
    }

    if (selected >= 0) {
        const Route *route = &database.routes[selected];

        printf("Minimum route: %s -> %s, cost=%u, metric=%hu\n",
               database.vertices[route->origin].code,
               database.vertices[route->destination].code,
               route->cost,
               route->metric);
    }
}

static void release_edges(void)
{
    uint16_t i;

    for (i = 0; i < database.vertex_count; ++i) {
        Edge *edge = database.adjacency[i];

        while (edge != NULL) {
            Edge *next = edge->next;
            free(edge);
            edge = next;
        }

        database.adjacency[i] = NULL;
    }
}

int main(void)
{
    SearchState state;
    uint16_t v0, v1, v2, v3, v4;
    uint16_t v5, v6, v7, v8, v9;

    memset(&database, 0, sizeof(database));

    v0 = add_vertex("V00", "Location 00", 0.0, 0.0, 29000000u);
    v1 = add_vertex("V01", "Location 01", 0.0, 0.0, 41000000u);
    v2 = add_vertex("V02", "Location 02", 0.0, 0.0, 71000000u);
    v3 = add_vertex("V03", "Location 03", 0.0, 0.0, 42000000u);
    v4 = add_vertex("V04", "Location 04", 0.0, 0.0, 80000000u);
    v5 = add_vertex("V05", "Location 05", 0.0, 0.0, 76000000u);
    v6 = add_vertex("V06", "Location 06", 0.0, 0.0, 89000000u);
    v7 = add_vertex("V07", "Location 07", 0.0, 0.0, 68000000u);
    v8 = add_vertex("V08", "Location 08", 0.0, 0.0, 57000000u);
    v9 = add_vertex("V09", "Location 09", 0.0, 0.0, 62000000u);

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

    print_vertices_and_graph();

    breadth_first_search(&state, v0);
    print_breadth_first_results(&state);

    initialize_search(&state, database.vertex_count);
    depth_first_visit(v0, &state, 1);
    putchar('\n');

    dijkstra(&state, v0);
    print_shortest_paths(&state);

    print_minimum_route();
    release_edges();

    return 0;
}