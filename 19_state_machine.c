/*
 * 19_state_machine.c
 * Target feature: enum-heavy code plus a 2-D table of transition structs
 * containing function pointers, and a context struct with a nested union of
 * per-state payloads.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef enum ConnState {
    ST_CLOSED = 0,
    ST_LISTEN,
    ST_SYN_RECV,
    ST_ESTABLISHED,
    ST_CLOSING,
    ST_ERROR,
    ST_COUNT
} ConnState;

typedef enum ConnEvent {
    EV_OPEN = 0,
    EV_SYN,
    EV_ACK,
    EV_DATA,
    EV_FIN,
    EV_TIMEOUT,
    EV_RESET,
    EV_COUNT
} ConnEvent;

typedef struct ClosedInfo {
    uint32_t reason;
    char     detail[24];
} ClosedInfo;

typedef struct HandshakeInfo {
    uint32_t local_seq;
    uint32_t remote_seq;
    uint16_t window;
    uint8_t  retries;
} HandshakeInfo;

typedef struct StreamInfo {
    uint64_t bytes_in;
    uint64_t bytes_out;
    uint32_t last_ack;
    uint16_t mss;
    uint8_t  nodelay;
} StreamInfo;

typedef struct Connection Connection;

typedef struct Transition {
    ConnState next;
    void    (*action)(Connection *, ConnEvent);
    const char *label;
} Transition;

struct Connection {
    uint32_t  id;
    char      peer[24];
    ConnState state;
    ConnState prev_state;
    uint32_t  transitions;
    uint32_t  rejected;
    uint32_t  event_counts[EV_COUNT];
    union {
        ClosedInfo    closed;
        HandshakeInfo handshake;
        StreamInfo    stream;
    } info;
};

#ifndef RESULT_ONLY
static const char *state_name(ConnState s)
{
    static const char *names[ST_COUNT] = {
        "CLOSED", "LISTEN", "SYN_RECV", "ESTABLISHED", "CLOSING", "ERROR"
    };
    return (s < ST_COUNT) ? names[s] : "?";
}

static const char *event_name(ConnEvent e)
{
    static const char *names[EV_COUNT] = {
        "OPEN", "SYN", "ACK", "DATA", "FIN", "TIMEOUT", "RESET"
    };
    return (e < EV_COUNT) ? names[e] : "?";
}
#endif

static void act_listen(Connection *c, ConnEvent e)
{
    (void)e;
    memset(&c->info, 0, sizeof(c->info));
    c->info.handshake.window = 8192;
}

static void act_syn(Connection *c, ConnEvent e)
{
    (void)e;
    c->info.handshake.local_seq  = 0x1000 + c->id * 7;
    c->info.handshake.remote_seq = 0x9000 + c->id * 13;
    c->info.handshake.retries    = 0;
}

static void act_establish(Connection *c, ConnEvent e)
{
    uint32_t seq = c->info.handshake.local_seq;
    (void)e;
    memset(&c->info, 0, sizeof(c->info));
    c->info.stream.last_ack = seq + 1;
    c->info.stream.mss      = 1460;
    c->info.stream.nodelay  = 1;
}

static void act_data(Connection *c, ConnEvent e)
{
    (void)e;
    c->info.stream.bytes_in  += 512 + c->transitions * 8;
    c->info.stream.bytes_out += 128 + c->transitions * 3;
    c->info.stream.last_ack  += (uint32_t)c->info.stream.bytes_in & 0xffu;
}

static void act_retry(Connection *c, ConnEvent e)
{
    (void)e;
    c->info.handshake.retries++;
}

static void act_close(Connection *c, ConnEvent e)
{
    uint64_t total = c->info.stream.bytes_in + c->info.stream.bytes_out;
    memset(&c->info, 0, sizeof(c->info));
    c->info.closed.reason = (uint32_t)e;
    snprintf(c->info.closed.detail, sizeof(c->info.closed.detail),
             "graceful %llu B", (unsigned long long)total);
}

static void act_reset(Connection *c, ConnEvent e)
{
    memset(&c->info, 0, sizeof(c->info));
    c->info.closed.reason = 0x8000u | (uint32_t)e;
    strncpy(c->info.closed.detail, "peer reset", sizeof(c->info.closed.detail) - 1);
}

/* 2-D table of structs indexed [state][event] */
static const Transition TABLE[ST_COUNT][EV_COUNT] = {
/* CLOSED      */ {
    { ST_LISTEN,      act_listen,    "open->listen" },
    { ST_ERROR,       act_reset,     "syn on closed" },
    { ST_ERROR,       act_reset,     "ack on closed" },
    { ST_ERROR,       act_reset,     "data on closed" },
    { ST_CLOSED,      NULL,          "fin ignored" },
    { ST_CLOSED,      NULL,          "timeout ignored" },
    { ST_CLOSED,      NULL,          "reset ignored" }
},
/* LISTEN      */ {
    { ST_LISTEN,      NULL,          "already listening" },
    { ST_SYN_RECV,    act_syn,       "syn received" },
    { ST_ERROR,       act_reset,     "unexpected ack" },
    { ST_ERROR,       act_reset,     "data before handshake" },
    { ST_CLOSED,      act_close,     "fin -> close" },
    { ST_LISTEN,      NULL,          "listen timeout" },
    { ST_CLOSED,      act_reset,     "reset" }
},
/* SYN_RECV    */ {
    { ST_ERROR,       act_reset,     "open while handshaking" },
    { ST_SYN_RECV,    act_retry,     "duplicate syn" },
    { ST_ESTABLISHED, act_establish, "handshake complete" },
    { ST_ERROR,       act_reset,     "data too early" },
    { ST_CLOSING,     act_close,     "fin during handshake" },
    { ST_SYN_RECV,    act_retry,     "retransmit" },
    { ST_CLOSED,      act_reset,     "reset" }
},
/* ESTABLISHED */ {
    { ST_ESTABLISHED, NULL,          "open ignored" },
    { ST_ESTABLISHED, NULL,          "stray syn" },
    { ST_ESTABLISHED, act_data,      "ack" },
    { ST_ESTABLISHED, act_data,      "data" },
    { ST_CLOSING,     act_close,     "fin -> closing" },
    { ST_CLOSING,     act_close,     "idle timeout" },
    { ST_CLOSED,      act_reset,     "reset" }
},
/* CLOSING     */ {
    { ST_ERROR,       act_reset,     "open while closing" },
    { ST_ERROR,       act_reset,     "syn while closing" },
    { ST_CLOSED,      NULL,          "final ack" },
    { ST_CLOSING,     NULL,          "late data" },
    { ST_CLOSED,      NULL,          "fin ack" },
    { ST_CLOSED,      NULL,          "close timeout" },
    { ST_CLOSED,      act_reset,     "reset" }
},
/* ERROR       */ {
    { ST_CLOSED,      act_close,     "reopen" },
    { ST_ERROR,       NULL,          "stuck" },
    { ST_ERROR,       NULL,          "stuck" },
    { ST_ERROR,       NULL,          "stuck" },
    { ST_CLOSED,      act_close,     "cleanup" },
    { ST_CLOSED,      act_close,     "cleanup" },
    { ST_CLOSED,      act_reset,     "cleanup" }
}
};

static void conn_init(Connection *c, uint32_t id, const char *peer)
{
    memset(c, 0, sizeof(*c));
    c->id = id;
    strncpy(c->peer, peer, sizeof(c->peer) - 1);
    c->state = ST_CLOSED;
    c->prev_state = ST_CLOSED;
}

static void conn_feed(Connection *c, ConnEvent e)
{
    const Transition *tr = &TABLE[c->state][e];

    c->event_counts[e]++;
    if (tr->next == c->state && tr->action == NULL) {
        c->rejected++;
        BENCH_OUTPUT(
            printf("  %-12s + %-8s = (no-op: %s)\n", state_name(c->state),
                   event_name(e), tr->label),
            printf("%d %d %d 0\n", (int)c->state, (int)e, (int)c->state));
        return;
    }
    if (tr->action)
        tr->action(c, e);
    c->prev_state = c->state;
    c->state = tr->next;
    c->transitions++;
    BENCH_OUTPUT(
        printf("  %-12s + %-8s -> %-12s [%s]\n",
               state_name(c->prev_state), event_name(e), state_name(c->state), tr->label),
        printf("%d %d %d 1\n", (int)c->prev_state, (int)e, (int)c->state));
}

static void conn_dump(const Connection *c)
{
    unsigned i;

    BENCH_OUTPUT(
        printf("conn #%u peer=%s state=%s prev=%s transitions=%u rejected=%u\n",
               c->id, c->peer, state_name(c->state), state_name(c->prev_state),
               c->transitions, c->rejected),
        printf("%u|%s|%d %d %u %u\n", c->id, c->peer, (int)c->state,
               (int)c->prev_state, c->transitions, c->rejected));
    switch (c->state) {
    case ST_CLOSED:
    case ST_ERROR:
        BENCH_OUTPUT(
            printf("  closed: reason=0x%x '%s'\n", c->info.closed.reason,
                   c->info.closed.detail),
            printf("%x|%s\n", c->info.closed.reason, c->info.closed.detail));
        break;
    case ST_LISTEN:
    case ST_SYN_RECV:
        BENCH_OUTPUT(
            printf("  handshake: lseq=%08x rseq=%08x win=%u retries=%u\n",
                   c->info.handshake.local_seq, c->info.handshake.remote_seq,
                   c->info.handshake.window, c->info.handshake.retries),
            printf("%08x %08x %u %u\n", c->info.handshake.local_seq,
                   c->info.handshake.remote_seq, c->info.handshake.window,
                   c->info.handshake.retries));
        break;
    case ST_ESTABLISHED:
    case ST_CLOSING:
        BENCH_OUTPUT(
            printf("  stream: in=%llu out=%llu ack=%08x mss=%u nodelay=%u\n",
                   (unsigned long long)c->info.stream.bytes_in,
                   (unsigned long long)c->info.stream.bytes_out,
                   c->info.stream.last_ack, c->info.stream.mss,
                   c->info.stream.nodelay),
            printf("%llu %llu %08x %u %u\n",
                   (unsigned long long)c->info.stream.bytes_in,
                   (unsigned long long)c->info.stream.bytes_out,
                   c->info.stream.last_ack, c->info.stream.mss,
                   c->info.stream.nodelay));
        break;
    default:
        break;
    }
    BENCH_VERBOSE(printf("  events:"));
    for (i = 0; i < EV_COUNT; i++)
        BENCH_OUTPUT(printf(" %s=%u", event_name((ConnEvent)i), c->event_counts[i]),
                     printf(" %u", c->event_counts[i]));
    putchar('\n');
}

int main(void)
{
    static const ConnEvent script_a[] = {
        EV_OPEN, EV_SYN, EV_ACK, EV_DATA, EV_DATA, EV_ACK, EV_FIN, EV_ACK
    };
    static const ConnEvent script_b[] = {
        EV_OPEN, EV_SYN, EV_SYN, EV_TIMEOUT, EV_DATA, EV_OPEN, EV_RESET, EV_OPEN
    };
    Connection a, b;
    unsigned i;

    conn_init(&a, 1, "10.0.0.7:443");
    conn_init(&b, 2, "192.168.1.9:80");

    BENCH_VERBOSE(printf("session A:\n"));
    for (i = 0; i < sizeof(script_a) / sizeof(script_a[0]); i++)
        conn_feed(&a, script_a[i]);
    conn_dump(&a);

    BENCH_VERBOSE(printf("\nsession B:\n"));
    for (i = 0; i < sizeof(script_b) / sizeof(script_b[0]); i++)
        conn_feed(&b, script_b[i]);
    conn_dump(&b);

    {
        unsigned s, e, with_action = 0;
        for (s = 0; s < ST_COUNT; s++)
            for (e = 0; e < EV_COUNT; e++)
                if (TABLE[s][e].action)
                    with_action++;
        BENCH_OUTPUT(
            printf("\ntable: %zu cells, actions=%u\n",
                   (size_t)(ST_COUNT * EV_COUNT), with_action),
            printf("%zu %u\n", (size_t)(ST_COUNT * EV_COUNT), with_action));
    }
    return 0;
}
