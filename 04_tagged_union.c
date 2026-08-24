/*
 * 04_tagged_union.c
 * Target feature: discriminated (tagged) unions - an enum tag selecting
 * between union arms of very different shapes, including a nested struct arm
 * and a fixed-array arm. Dispatch happens through a switch on the tag.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef enum MsgType {
    MSG_PING    = 0,
    MSG_LOGIN   = 1,
    MSG_MOVE    = 2,
    MSG_CHAT    = 3,
    MSG_BLOB    = 4,
    MSG_ERROR   = 5
} MsgType;

typedef struct LoginBody {
    char     user[24];
    uint8_t  pwhash[16];
    uint32_t client_ver;
} LoginBody;

typedef struct MoveBody {
    int32_t entity;
    struct {
        float x;
        float y;
        float z;
    } from;
    struct {
        float x;
        float y;
        float z;
    } to;
    uint16_t duration_ms;
} MoveBody;

typedef struct ChatBody {
    uint32_t channel;
    uint16_t length;
    char     text[96];
} ChatBody;

typedef struct BlobBody {
    uint32_t id;
    uint32_t nbytes;
    uint8_t  bytes[64];
} BlobBody;

typedef struct ErrorBody {
    int32_t  code;
    char     reason[40];
} ErrorBody;

typedef struct Message {
    MsgType  type;
    uint32_t seq;
    uint64_t timestamp;
    union {
        uint64_t  ping_nonce;
        LoginBody login;
        MoveBody  move;
        ChatBody  chat;
        BlobBody  blob;
        ErrorBody error;
    } body;
} Message;

static uint32_t g_seq = 1;

static void msg_init(Message *m, MsgType t)
{
    memset(m, 0, sizeof(*m));
    m->type      = t;
    m->seq       = g_seq++;
    m->timestamp = 1600000000ull + (uint64_t)m->seq * 37ull;
}

static void make_ping(Message *m, uint64_t nonce)
{
    msg_init(m, MSG_PING);
    m->body.ping_nonce = nonce;
}

static void make_login(Message *m, const char *user, uint32_t ver)
{
    int i;
    msg_init(m, MSG_LOGIN);
    strncpy(m->body.login.user, user, sizeof(m->body.login.user) - 1);
    for (i = 0; i < 16; i++)
        m->body.login.pwhash[i] = (uint8_t)(user[i % (int)strlen(user)] ^ (i * 31));
    m->body.login.client_ver = ver;
}

static void make_move(Message *m, int32_t ent, float dx, float dy, float dz)
{
    msg_init(m, MSG_MOVE);
    m->body.move.entity      = ent;
    m->body.move.from.x      = (float)ent;
    m->body.move.from.y      = (float)ent * 2.0f;
    m->body.move.from.z      = 0.0f;
    m->body.move.to.x        = m->body.move.from.x + dx;
    m->body.move.to.y        = m->body.move.from.y + dy;
    m->body.move.to.z        = m->body.move.from.z + dz;
    m->body.move.duration_ms = (uint16_t)(120 + ent * 5);
}

static void make_chat(Message *m, uint32_t chan, const char *text)
{
    msg_init(m, MSG_CHAT);
    m->body.chat.channel = chan;
    strncpy(m->body.chat.text, text, sizeof(m->body.chat.text) - 1);
    m->body.chat.length  = (uint16_t)strlen(m->body.chat.text);
}

static void make_blob(Message *m, uint32_t id, uint32_t n)
{
    uint32_t i;
    msg_init(m, MSG_BLOB);
    if (n > sizeof(m->body.blob.bytes))
        n = (uint32_t)sizeof(m->body.blob.bytes);
    m->body.blob.id     = id;
    m->body.blob.nbytes = n;
    for (i = 0; i < n; i++)
        m->body.blob.bytes[i] = (uint8_t)(id * 13 + i * 5);
}

static void make_error(Message *m, int32_t code, const char *why)
{
    msg_init(m, MSG_ERROR);
    m->body.error.code = code;
    strncpy(m->body.error.reason, why, sizeof(m->body.error.reason) - 1);
}

/* Tag-driven checksum: each arm reads a different member set. */
static uint32_t msg_checksum(const Message *m)
{
    uint32_t sum = (uint32_t)m->type * 2654435761u + m->seq;
    uint32_t i;

    switch (m->type) {
    case MSG_PING:
        sum ^= (uint32_t)(m->body.ping_nonce >> 32) ^ (uint32_t)m->body.ping_nonce;
        break;
    case MSG_LOGIN:
        for (i = 0; i < 16; i++)
            sum = sum * 31u + m->body.login.pwhash[i];
        sum ^= m->body.login.client_ver;
        break;
    case MSG_MOVE:
        sum ^= (uint32_t)m->body.move.entity;
        sum += (uint32_t)(m->body.move.to.x * 100.0f);
        sum += (uint32_t)(m->body.move.to.y * 100.0f);
        sum += m->body.move.duration_ms;
        break;
    case MSG_CHAT:
        sum ^= m->body.chat.channel;
        for (i = 0; i < m->body.chat.length; i++)
            sum = sum * 131u + (uint8_t)m->body.chat.text[i];
        break;
    case MSG_BLOB:
        sum ^= m->body.blob.id;
        for (i = 0; i < m->body.blob.nbytes; i++)
            sum = sum * 17u + m->body.blob.bytes[i];
        break;
    case MSG_ERROR:
        sum ^= (uint32_t)m->body.error.code;
        for (i = 0; m->body.error.reason[i]; i++)
            sum = sum * 7u + (uint8_t)m->body.error.reason[i];
        break;
    }
    return sum;
}

static void msg_print(const Message *m)
{
    BENCH_OUTPUT(
        printf("[seq=%u ts=%llu chk=%08x] ", m->seq,
               (unsigned long long)m->timestamp, msg_checksum(m)),
        printf("%u %llu %08x %d ", m->seq,
               (unsigned long long)m->timestamp, msg_checksum(m), (int)m->type));
    switch (m->type) {
    case MSG_PING:
        BENCH_OUTPUT(printf("PING nonce=%016llx\n", (unsigned long long)m->body.ping_nonce),
                     printf("%016llx\n", (unsigned long long)m->body.ping_nonce));
        break;
    case MSG_LOGIN:
        BENCH_OUTPUT(
            printf("LOGIN user='%s' ver=%u hash0=%02x\n", m->body.login.user,
                   m->body.login.client_ver, m->body.login.pwhash[0]),
            printf("%s %u %02x\n", m->body.login.user,
                   m->body.login.client_ver, m->body.login.pwhash[0]));
        break;
    case MSG_MOVE:
        BENCH_OUTPUT(
            printf("MOVE ent=%d (%.1f,%.1f,%.1f)->(%.1f,%.1f,%.1f) %ums\n",
                   m->body.move.entity, m->body.move.from.x, m->body.move.from.y,
                   m->body.move.from.z, m->body.move.to.x, m->body.move.to.y,
                   m->body.move.to.z, m->body.move.duration_ms),
            printf("%d %.1f %.1f %.1f %.1f %.1f %.1f %u\n",
                   m->body.move.entity, m->body.move.from.x, m->body.move.from.y,
                   m->body.move.from.z, m->body.move.to.x, m->body.move.to.y,
                   m->body.move.to.z, m->body.move.duration_ms));
        break;
    case MSG_CHAT:
        BENCH_OUTPUT(
            printf("CHAT ch=%u len=%u '%s'\n", m->body.chat.channel,
                   m->body.chat.length, m->body.chat.text),
            printf("%u %u %s\n", m->body.chat.channel,
                   m->body.chat.length, m->body.chat.text));
        break;
    case MSG_BLOB:
        BENCH_OUTPUT(
            printf("BLOB id=%u n=%u [%02x %02x ...]\n", m->body.blob.id,
                   m->body.blob.nbytes, m->body.blob.bytes[0], m->body.blob.bytes[1]),
            printf("%u %u %02x %02x\n", m->body.blob.id,
                   m->body.blob.nbytes, m->body.blob.bytes[0], m->body.blob.bytes[1]));
        break;
    case MSG_ERROR:
        BENCH_OUTPUT(printf("ERROR %d '%s'\n", m->body.error.code, m->body.error.reason),
                     printf("%d %s\n", m->body.error.code, m->body.error.reason));
        break;
    }
}

int main(void)
{
    Message queue[6];
    uint32_t total = 0;
    unsigned i;

    make_ping(&queue[0], 0xdeadbeefcafe1234ull);
    make_login(&queue[1], "operator", 0x00030201u);
    make_move(&queue[2], 42, 1.5f, -2.25f, 0.75f);
    make_chat(&queue[3], 7, "type recovery under test");
    make_blob(&queue[4], 99, 24);
    make_error(&queue[5], -11, "unsupported opcode");

    for (i = 0; i < 6; i++) {
        msg_print(&queue[i]);
        total += msg_checksum(&queue[i]);
    }
    BENCH_OUTPUT(printf("total=%08x\n", total), printf("%08x\n", total));
    return 0;
}
