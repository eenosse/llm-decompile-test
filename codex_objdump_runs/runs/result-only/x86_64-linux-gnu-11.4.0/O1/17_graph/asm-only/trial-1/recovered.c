#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAX_LOCATIONS = 12,
    MAX_CONNECTIONS = 48,
    INFINITE_DISTANCE = 1000000
};

typedef struct {
    char code[4];
    char name[16];
    double coordinate_x;
    double coordinate_y;
    int32_t population;
} Location;

typedef struct Connection {
    uint16_t destination;
    uint16_t weight;
    uint32_t metric;
    struct Connection *next;
} Connection;

typedef struct {
    uint16_t source;
    uint16_t destination;
    uint16_t weight;
    uint16_t reserved;
    uint32_t metric;
} ConnectionRecord;

typedef struct {
    Location locations[MAX_LOCATIONS];
    Connection *adjacency[MAX_LOCATIONS];
    uint16_t matrix[MAX_LOCATIONS][MAX_LOCATIONS];
    ConnectionRecord records[MAX_CONNECTIONS];
    uint16_t location_count;
    uint16_t connection_count;
    uint32_t reserved;
} Network;

typedef struct {
    int32_t distance[MAX_LOCATIONS];
    int16_t predecessor[MAX_LOCATIONS];
    uint8_t visited[MAX_LOCATIONS];
    uint16_t queue[MAX_LOCATIONS];
    uint8_t reserved[24];
    int32_t queue_head;
    int32_t queue_tail;
    int32_t processed;
} SearchState;

_Static_assert(sizeof(Location) == 48, "unexpected Location layout");
_Static_assert(sizeof(Connection) == 16, "unexpected Connection layout");
_Static_assert(sizeof(ConnectionRecord) == 12, "unexpected record layout");
_Static_assert(offsetof(Network, adjacency) == 0x240, "unexpected adjacency offset");
_Static_assert(offsetof(Network, matrix) == 0x2a0, "unexpected matrix offset");
_Static_assert(offsetof(Network, records) == 0x3c0, "unexpected record offset");
_Static_assert(offsetof(Network, location_count) == 0x600, "unexpected count offset");
_Static_assert(sizeof(Network) == 0x608, "unexpected Network layout");
_Static_assert(offsetof(SearchState, queue_head) == 0x84, "unexpected state layout");
_Static_assert(offsetof(SearchState, processed) == 0x8c, "unexpected state layout");
_Static_assert(sizeof(SearchState) == 0x90, "unexpected SearchState size");

static Network network;

static void initialize_state(SearchState *state, uint16_t count)
{
    uint16_t i;

    memset(state, 0, sizeof(*state));
    for (i = 0; i < count; ++i) {
        state->distance[i] = INFINITE_DISTANCE;
        state->predecessor[i] = -1;
    }
}

static uint16_t add_location(Network *net, const char *code,
                             const char *name, double coordinate_x,
                             double coordinate_y, int32_t population)
{
    uint16_t index = net->location_count;
    Location *location = &net->locations[index];

    memset(location, 0, sizeof(*location));
    strncpy(location->code, code, 3);
    strncpy(location->name, name, 15);
    location->coordinate_x = coordinate_x;
    location->coordinate_y = coordinate_y;
    location->population = population;

    net->location_count = (uint16_t)(index + 1);
    return index;
}

static void add_directed_connection(Network *net, uint16_t source,
                                    uint16_t destination, uint16_t weight,
                                    uint32_t metric)
{
    Connection *connection = calloc(1, sizeof(*connection));

    if (connection == NULL)
        exit(1);

    connection->destination = destination;
    connection->weight = weight;
    connection->metric = metric;
    connection->next = net->adjacency[source];
    net->adjacency[source] = connection;

    net->matrix[source][destination] = weight;

    if (net->connection_count < MAX_CONNECTIONS) {
        ConnectionRecord *record =
            &net->records[net->connection_count++];

        record->source = source;
        record->destination = destination;
        record->weight = weight;
        record->metric = metric;
    }
}

static void add_bidirectional_connection(Network *net, uint16_t first,
                                         uint16_t second, uint16_t weight,
                                         uint32_t metric)
{
    add_directed_connection(net, first, second, weight, metric);
    add_directed_connection(net, second, first,
                            (uint16_t)(weight + 10), metric + 25);
}

static void depth_first_search(Network *net, uint16_t current,
                               SearchState *state, int depth)
{
    uint16_t neighbor;

    state->visited[current] = 1;
    ++state->processed;
    printf("%*s%s\n", depth, "", net->locations[current].code);

    for (neighbor = 0; neighbor < net->location_count; ++neighbor) {
        if (net->matrix[current][neighbor] != 0 &&
            state->visited[neighbor] == 0) {
            state->predecessor[neighbor] = (int16_t)current;
            depth_first_search(net, neighbor, state, depth + 1);
        }
    }
}

static void breadth_first_search(Network *net, uint16_t start,
                                 SearchState *state)
{
    initialize_state(state, net->location_count);

    state->distance[start] = 0;
    state->visited[start] = 1;
    state->queue[state->queue_tail++] = start;

    while (state->queue_head < state->queue_tail) {
        uint16_t current = state->queue[state->queue_head++];
        Connection *connection;

        ++state->processed;

        for (connection = net->adjacency[current];
             connection != NULL;
             connection = connection->next) {
            uint16_t destination = connection->destination;

            if (state->visited[destination] == 0) {
                state->visited[destination] = 1;
                state->distance[destination] =
                    state->distance[current] + 1;
                state->predecessor[destination] = (int16_t)current;
                state->queue[state->queue_tail++] = destination;
            }
        }
    }
}

static void shortest_paths(Network *net, uint16_t start, SearchState *state)
{
    uint16_t iteration;

    initialize_state(state, net->location_count);
    state->distance[start] = 0;

    for (iteration = 0; iteration < net->location_count; ++iteration) {
        int selected = -1;
        uint16_t i;
        Connection *connection;

        for (i = 0; i < net->location_count; ++i) {
            if (state->visited[i] == 0 &&
                (selected < 0 ||
                 state->distance[i] < state->distance[selected])) {
                selected = i;
            }
        }

        if (selected < 0 ||
            state->distance[selected] == INFINITE_DISTANCE)
            break;

        state->visited[selected] = 1;
        ++state->processed;

        for (connection = net->adjacency[selected];
             connection != NULL;
             connection = connection->next) {
            uint16_t destination = connection->destination;
            int32_t candidate =
                state->distance[selected] + connection->weight;

            if (candidate < state->distance[destination]) {
                state->distance[destination] = candidate;
                state->predecessor[destination] = (int16_t)selected;
            }
        }
    }
}

static void print_network(const Network *net)
{
    uint16_t i;
    uint16_t j;

    printf("Locations: %u, connections: %u\n",
           (unsigned)net->location_count,
           (unsigned)net->connection_count);

    for (i = 0; i < net->location_count; ++i) {
        const Location *location = &net->locations[i];
        const Connection *connection;

        printf("%s %s %.2f %.2f %d\n",
               location->code, location->name,
               location->coordinate_x, location->coordinate_y,
               location->population);

        for (connection = net->adjacency[i];
             connection != NULL;
             connection = connection->next) {
            printf("  %s %u %u\n",
                   net->locations[connection->destination].code,
                   (unsigned)connection->weight,
                   (unsigned)connection->metric);
        }
        putchar('\n');
    }

    for (i = 0; i < net->location_count; ++i) {
        printf("%s ", net->locations[i].code);
        for (j = 0; j < net->location_count; ++j) {
            if (net->matrix[i][j] != 0)
                printf("%u ", (unsigned)net->matrix[i][j]);
            else
                printf("- ");
        }
        putchar('\n');
    }
}

static void print_search_result(const Network *net,
                                const SearchState *state)
{
    uint16_t i;

    for (i = 0; i < net->location_count; ++i) {
        const char *predecessor = "-";
        int32_t distance = state->distance[i];

        if (state->predecessor[i] >= 0)
            predecessor =
                net->locations[(uint16_t)state->predecessor[i]].code;

        if (distance == INFINITE_DISTANCE)
            distance = -1;

        printf("%s %d %s\n",
               net->locations[i].code, distance, predecessor);
    }

    printf("%d\n", state->processed);
}

static void print_shortest_paths(const Network *net,
                                 const SearchState *state)
{
    uint16_t destination;

    for (destination = 0;
         destination < net->location_count;
         ++destination) {
        int16_t path[MAX_LOCATIONS];
        int16_t current = (int16_t)destination;
        int count = 0;
        int i;

        printf("%s ", net->locations[destination].code);

        while (count < MAX_LOCATIONS && current >= 0) {
            path[count++] = current;
            current = state->predecessor[(uint16_t)current];
        }

        for (i = count - 1; i >= 0; --i) {
            printf("%s%s",
                   net->locations[(uint16_t)path[i]].code,
                   i == 0 ? "" : "->");
        }

        printf(" %d\n", state->distance[destination]);
    }
}

static void print_minimum_metric(const Network *net)
{
    uint16_t i;
    int selected = -1;
    uint32_t minimum = UINT32_MAX;

    for (i = 0; i < net->connection_count; ++i) {
        if (net->records[i].metric < minimum) {
            selected = i;
            minimum = net->records[i].metric;
        }
    }

    if (selected >= 0) {
        const ConnectionRecord *record = &net->records[selected];

        printf("%s %s %u %u\n",
               net->locations[record->source].code,
               net->locations[record->destination].code,
               (unsigned)record->metric,
               (unsigned)record->weight);
    }
}

static void free_connections(Network *net)
{
    uint16_t i;

    for (i = 0; i < net->location_count; ++i) {
        Connection *connection = net->adjacency[i];

        while (connection != NULL) {
            Connection *next = connection->next;
            free(connection);
            connection = next;
        }

        net->adjacency[i] = NULL;
    }
}

int main(void)
{
    SearchState state;
    uint16_t a;
    uint16_t b;
    uint16_t c;
    uint16_t d;
    uint16_t e;
    uint16_t f;
    uint16_t g;
    uint16_t h;
    uint16_t i;
    uint16_t j;

    memset(&network, 0, sizeof(network));

    a = add_location(&network, "A00", "SiteA",
                     0.0, 0.0, 29000000);
    b = add_location(&network, "B01", "SettlementB",
                     0.0, 0.0, 41000000);
    c = add_location(&network, "C02", "LocationC",
                     0.0, 0.0, 71000000);
    d = add_location(&network, "D03", "SiteD",
                     0.0, 0.0, 42000000);
    e = add_location(&network, "E04", "PlaceE",
                     0.0, 0.0, 80000000);
    f = add_location(&network, "F05", "SiteF",
                     0.0, 0.0, 76000000);
    g = add_location(&network, "G06", "SiteG",
                     0.0, 0.0, 89000000);
    h = add_location(&network, "H07", "LocationH",
                     0.0, 0.0, 68000000);
    i = add_location(&network, "I08", "MunicipalityI",
                     0.0, 0.0, 57000000);
    j = add_location(&network, "J09", "RegionJ",
                     0.0, 0.0, 62000000);

    add_bidirectional_connection(&network, a, b, 125, 90);
    add_bidirectional_connection(&network, a, c, 105, 180);
    add_bidirectional_connection(&network, b, h, 110, 140);
    add_bidirectional_connection(&network, c, d, 230, 320);
    add_bidirectional_connection(&network, c, g, 505, 640);
    add_bidirectional_connection(&network, h, g, 445, 590);
    add_bidirectional_connection(&network, g, e, 430, 700);
    add_bidirectional_connection(&network, e, f, 80, 120);
    add_bidirectional_connection(&network, d, i, 585, 880);
    add_bidirectional_connection(&network, i, j, 330, 260);
    add_bidirectional_connection(&network, e, j, 425, 540);

    print_network(&network);

    breadth_first_search(&network, a, &state);
    print_search_result(&network, &state);

    initialize_state(&state, network.location_count);
    depth_first_search(&network, a, &state, 1);
    printf("%d\n", state.processed);

    shortest_paths(&network, a, &state);
    print_shortest_paths(&network, &state);

    print_minimum_metric(&network);
    free_connections(&network);

    return 0;
}