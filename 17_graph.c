/*
 * 17_graph.c
 * Target feature: three coexisting representations of one graph - adjacency
 * list (array of pointers to edge nodes), adjacency matrix (2-D array), and
 * a flat edge array - plus BFS/DFS/Dijkstra over user structs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

#define MAX_V 12
#define INF   1000000

typedef struct Airport {
    char     code[4];
    char     city[16];
    double   lat;
    double   lon;
    uint32_t annual_pax;
} Airport;

typedef struct EdgeNode {
    uint16_t         to;
    uint16_t         minutes;
    uint32_t         price;
    struct EdgeNode *next;
} EdgeNode;

typedef struct Edge {
    uint16_t from;
    uint16_t to;
    uint16_t minutes;
    uint32_t price;
} Edge;

typedef struct Graph {
    Airport   vertices[MAX_V];
    EdgeNode *adj[MAX_V];                /* array of chain heads */
    uint16_t  matrix[MAX_V][MAX_V];      /* 0 = no edge, else minutes */
    Edge      edges[48];
    uint16_t  n_vertices;
    uint16_t  n_edges;
} Graph;

typedef struct SearchState {
    int      dist[MAX_V];
    int16_t  prev[MAX_V];
    uint8_t  visited[MAX_V];
    uint16_t queue[MAX_V * 2];
    int      qhead;
    int      qtail;
    uint32_t expansions;
} SearchState;

static uint16_t graph_add_vertex(Graph *g, const char *code, const char *city,
                                 double lat, double lon, uint32_t pax)
{
    Airport *a = &g->vertices[g->n_vertices];
    memset(a, 0, sizeof(*a));
    strncpy(a->code, code, sizeof(a->code) - 1);
    strncpy(a->city, city, sizeof(a->city) - 1);
    a->lat = lat;
    a->lon = lon;
    a->annual_pax = pax;
    return g->n_vertices++;
}

static void graph_add_edge(Graph *g, uint16_t from, uint16_t to,
                           uint16_t minutes, uint32_t price)
{
    EdgeNode *e = (EdgeNode *)calloc(1, sizeof(EdgeNode));
    if (!e)
        exit(1);
    e->to = to;
    e->minutes = minutes;
    e->price = price;
    e->next = g->adj[from];
    g->adj[from] = e;

    g->matrix[from][to] = minutes;
    if (g->n_edges < (uint16_t)(sizeof(g->edges) / sizeof(g->edges[0]))) {
        Edge *rec = &g->edges[g->n_edges++];
        rec->from = from;
        rec->to = to;
        rec->minutes = minutes;
        rec->price = price;
    }
}

static void graph_add_bidir(Graph *g, uint16_t a, uint16_t b, uint16_t m, uint32_t p)
{
    graph_add_edge(g, a, b, m, p);
    graph_add_edge(g, b, a, (uint16_t)(m + 10), p + 25);
}

static void state_reset(SearchState *s, uint16_t n)
{
    uint16_t i;
    memset(s, 0, sizeof(*s));
    for (i = 0; i < n; i++) {
        s->dist[i] = INF;
        s->prev[i] = -1;
    }
}

static void bfs(Graph *g, uint16_t src, SearchState *s)
{
    state_reset(s, g->n_vertices);
    s->dist[src] = 0;
    s->visited[src] = 1;
    s->queue[s->qtail++] = src;

    while (s->qhead < s->qtail) {
        uint16_t v = s->queue[s->qhead++];
        EdgeNode *e;
        s->expansions++;
        for (e = g->adj[v]; e; e = e->next) {
            if (!s->visited[e->to]) {
                s->visited[e->to] = 1;
                s->dist[e->to] = s->dist[v] + 1;
                s->prev[e->to] = (int16_t)v;
                s->queue[s->qtail++] = e->to;
            }
        }
    }
}

static void dfs_matrix(Graph *g, uint16_t v, SearchState *s, int depth)
{
    uint16_t w;

    s->visited[v] = 1;
    s->expansions++;
    BENCH_OUTPUT(printf("%*s%s (%s)\n", depth * 2, "", g->vertices[v].code,
                       g->vertices[v].city),
                 printf("%u %s\n", depth, g->vertices[v].code));
    for (w = 0; w < g->n_vertices; w++)
        if (g->matrix[v][w] && !s->visited[w]) {
            s->prev[w] = (int16_t)v;
            dfs_matrix(g, w, s, depth + 1);
        }
}

static void dijkstra(Graph *g, uint16_t src, SearchState *s)
{
    uint16_t iter;

    state_reset(s, g->n_vertices);
    s->dist[src] = 0;

    for (iter = 0; iter < g->n_vertices; iter++) {
        int best = -1;
        uint16_t v;
        EdgeNode *e;

        for (v = 0; v < g->n_vertices; v++)
            if (!s->visited[v] && (best < 0 || s->dist[v] < s->dist[best]))
                best = v;
        if (best < 0 || s->dist[best] == INF)
            break;
        s->visited[best] = 1;
        s->expansions++;
        for (e = g->adj[best]; e; e = e->next) {
            int nd = s->dist[best] + e->minutes;
            if (nd < s->dist[e->to]) {
                s->dist[e->to] = nd;
                s->prev[e->to] = (int16_t)best;
            }
        }
    }
}

static void print_path(Graph *g, SearchState *s, uint16_t dst)
{
    uint16_t stack[MAX_V];
    int top = 0;
    int16_t v = (int16_t)dst;

    while (v >= 0 && top < MAX_V) {
        stack[top++] = (uint16_t)v;
        v = s->prev[v];
    }
    while (top-- > 0)
        BENCH_OUTPUT(printf("%s%s", g->vertices[stack[top]].code,
                           top ? " -> " : ""),
                     printf("%s%s", g->vertices[stack[top]].code,
                            top ? " " : ""));
    BENCH_OUTPUT(printf("  (cost=%d)\n", s->dist[dst]),
                 printf(" %d\n", s->dist[dst]));
}

static void print_matrix(const Graph *g)
{
    uint16_t i, j;

#ifndef RESULT_ONLY
    printf("      ");
    for (j = 0; j < g->n_vertices; j++)
        printf("%4s", g->vertices[j].code);
    putchar('\n');
#endif
    for (i = 0; i < g->n_vertices; i++) {
        BENCH_OUTPUT(printf("%4s: ", g->vertices[i].code),
                     printf("%s", g->vertices[i].code));
        for (j = 0; j < g->n_vertices; j++)
            if (g->matrix[i][j])
                BENCH_OUTPUT(printf("%4u", g->matrix[i][j]),
                             printf(" %u", g->matrix[i][j]));
            else
                BENCH_OUTPUT(printf("   ."), printf(" 0"));
        putchar('\n');
    }
}

static void graph_free(Graph *g)
{
    uint16_t i;
    for (i = 0; i < g->n_vertices; i++) {
        EdgeNode *e = g->adj[i];
        while (e) {
            EdgeNode *n = e->next;
            free(e);
            e = n;
        }
        g->adj[i] = NULL;
    }
}

int main(void)
{
    static Graph g;
    SearchState st;
    uint16_t hn, sg, hk, nr, ld, pa, du, ty, sf, ny;
    uint16_t i;
    uint32_t cheapest = 0xffffffffu;
    int cheapest_idx = -1;

    memset(&g, 0, sizeof(g));
    hn = graph_add_vertex(&g, "HAN", "Hanoi",     21.03, 105.85, 29000000u);
    sg = graph_add_vertex(&g, "SGN", "Ho Chi Minh",10.82,106.63, 41000000u);
    hk = graph_add_vertex(&g, "HKG", "Hong Kong", 22.31, 113.91, 71000000u);
    nr = graph_add_vertex(&g, "NRT", "Tokyo",     35.77, 140.39, 42000000u);
    ld = graph_add_vertex(&g, "LHR", "London",    51.47, -0.45,  80000000u);
    pa = graph_add_vertex(&g, "CDG", "Paris",     49.01,  2.55,  76000000u);
    du = graph_add_vertex(&g, "DXB", "Dubai",     25.25, 55.36,  89000000u);
    ty = graph_add_vertex(&g, "SIN", "Singapore",  1.36, 103.99, 68000000u);
    sf = graph_add_vertex(&g, "SFO", "San Francisco",37.62,-122.38,57000000u);
    ny = graph_add_vertex(&g, "JFK", "New York",  40.64, -73.78, 62000000u);

    graph_add_bidir(&g, hn, sg, 125, 90);
    graph_add_bidir(&g, hn, hk, 105, 180);
    graph_add_bidir(&g, sg, ty, 110, 140);
    graph_add_bidir(&g, hk, nr, 230, 320);
    graph_add_bidir(&g, hk, du, 505, 640);
    graph_add_bidir(&g, ty, du, 445, 590);
    graph_add_bidir(&g, du, ld, 430, 700);
    graph_add_bidir(&g, ld, pa, 80, 120);
    graph_add_bidir(&g, nr, sf, 585, 880);
    graph_add_bidir(&g, sf, ny, 330, 260);
    graph_add_bidir(&g, ld, ny, 425, 540);

    BENCH_OUTPUT(printf("vertices=%u edges=%u\n", g.n_vertices, g.n_edges),
                 printf("%u %u\n", g.n_vertices, g.n_edges));
    for (i = 0; i < g.n_vertices; i++) {
        EdgeNode *e;
        BENCH_OUTPUT(
            printf("  %s %-14s (%6.2f,%7.2f) pax=%-9u ->", g.vertices[i].code,
                   g.vertices[i].city, g.vertices[i].lat, g.vertices[i].lon,
                   g.vertices[i].annual_pax),
            printf("%s|%s|%.2f|%.2f|%u", g.vertices[i].code,
                   g.vertices[i].city, g.vertices[i].lat, g.vertices[i].lon,
                   g.vertices[i].annual_pax));
        for (e = g.adj[i]; e; e = e->next)
            BENCH_OUTPUT(printf(" %s/%um/$%u", g.vertices[e->to].code,
                               e->minutes, e->price),
                         printf(" %s %u %u", g.vertices[e->to].code,
                                e->minutes, e->price));
        putchar('\n');
    }

    BENCH_VERBOSE(printf("\nadjacency matrix (minutes):\n"));
    print_matrix(&g);

    BENCH_VERBOSE(printf("\nBFS from HAN:\n"));
    bfs(&g, hn, &st);
    for (i = 0; i < g.n_vertices; i++)
        BENCH_OUTPUT(
            printf("  %s hops=%d prev=%s\n", g.vertices[i].code,
                   st.dist[i] == INF ? -1 : st.dist[i],
                   st.prev[i] >= 0 ? g.vertices[st.prev[i]].code : "-"),
            printf("%s %d %s\n", g.vertices[i].code,
                   st.dist[i] == INF ? -1 : st.dist[i],
                   st.prev[i] >= 0 ? g.vertices[st.prev[i]].code : "-"));
    BENCH_OUTPUT(printf("  expansions=%u\n", st.expansions),
                 printf("%u\n", st.expansions));

    BENCH_VERBOSE(printf("\nDFS over matrix from HAN:\n"));
    state_reset(&st, g.n_vertices);
    dfs_matrix(&g, hn, &st, 1);

    BENCH_VERBOSE(printf("\nDijkstra HAN -> all (minutes):\n"));
    dijkstra(&g, hn, &st);
    for (i = 0; i < g.n_vertices; i++) {
        BENCH_OUTPUT(printf("  -> %s: ", g.vertices[i].code),
                     printf("%s ", g.vertices[i].code));
        print_path(&g, &st, i);
    }

    for (i = 0; i < g.n_edges; i++)
        if (g.edges[i].price < cheapest) {
            cheapest = g.edges[i].price;
            cheapest_idx = i;
        }
    if (cheapest_idx >= 0)
        BENCH_OUTPUT(
            printf("\ncheapest leg: %s->%s $%u (%u min)\n",
                   g.vertices[g.edges[cheapest_idx].from].code,
                   g.vertices[g.edges[cheapest_idx].to].code,
                   g.edges[cheapest_idx].price,
                   g.edges[cheapest_idx].minutes),
            printf("%s %s %u %u\n",
                   g.vertices[g.edges[cheapest_idx].from].code,
                   g.vertices[g.edges[cheapest_idx].to].code,
                   g.edges[cheapest_idx].price,
                   g.edges[cheapest_idx].minutes));

    graph_free(&g);
    return 0;
}
