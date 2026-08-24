/*
 * 03_flexible_array.c
 * Target feature: flexible array members (C99 T x[]) of three different
 * element kinds - bytes, doubles and pointers - with the allocation size
 * computed from a runtime count. Also a "struct hack" trailing string.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef struct PacketHeader {
    uint16_t magic;
    uint8_t  version;
    uint8_t  opcode;
    uint32_t payload_len;
    uint32_t crc32;
} PacketHeader;

/* FAM of bytes */
typedef struct Packet {
    PacketHeader hdr;
    uint8_t      payload[];
} Packet;

/* FAM of doubles, 2-D indexed by hand */
typedef struct Matrix {
    uint32_t rows;
    uint32_t cols;
    double   data[];
} Matrix;

/* FAM of pointers to another FAM struct */
typedef struct InternedString {
    uint32_t hash;
    uint32_t len;
    char     text[];   /* NUL terminated, len bytes + 1 */
} InternedString;

typedef struct StringPool {
    uint32_t         count;
    uint32_t         capacity;
    InternedString  *entries[];
} StringPool;

static uint32_t crc32_lite(const uint8_t *p, size_t n)
{
    uint32_t c = 0xffffffffu;
    size_t i;
    int k;

    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xedb88320u & (uint32_t)(-(int32_t)(c & 1u)));
    }
    return ~c;
}

static Packet *packet_new(uint8_t opcode, const uint8_t *body, uint32_t n)
{
    Packet *p = (Packet *)malloc(sizeof(PacketHeader) + n);
    if (!p)
        exit(1);
    p->hdr.magic       = 0xC0DEu;
    p->hdr.version     = 3;
    p->hdr.opcode      = opcode;
    p->hdr.payload_len = n;
    memcpy(p->payload, body, n);
    p->hdr.crc32       = crc32_lite(p->payload, n);
    return p;
}

static int packet_verify(const Packet *p)
{
    return p->hdr.magic == 0xC0DEu &&
           p->hdr.crc32 == crc32_lite(p->payload, p->hdr.payload_len);
}

static Matrix *matrix_new(uint32_t rows, uint32_t cols)
{
    Matrix *m = (Matrix *)calloc(1, sizeof(Matrix) + (size_t)rows * cols * sizeof(double));
    if (!m)
        exit(1);
    m->rows = rows;
    m->cols = cols;
    return m;
}

static double *matrix_at(Matrix *m, uint32_t r, uint32_t c)
{
    return &m->data[(size_t)r * m->cols + c];
}

static Matrix *matrix_mul(const Matrix *a, const Matrix *b)
{
    Matrix *out = matrix_new(a->rows, b->cols);
    uint32_t i, j, k;

    for (i = 0; i < a->rows; i++)
        for (j = 0; j < b->cols; j++) {
            double acc = 0.0;
            for (k = 0; k < a->cols; k++)
                acc += a->data[(size_t)i * a->cols + k] * b->data[(size_t)k * b->cols + j];
            out->data[(size_t)i * out->cols + j] = acc;
        }
    return out;
}

static InternedString *intern_new(const char *s)
{
    size_t len = strlen(s);
    InternedString *is = (InternedString *)malloc(sizeof(InternedString) + len + 1);
    size_t i;
    uint32_t h = 2166136261u;

    if (!is)
        exit(1);
    memcpy(is->text, s, len + 1);
    is->len = (uint32_t)len;
    for (i = 0; i < len; i++) {
        h ^= (uint8_t)s[i];
        h *= 16777619u;
    }
    is->hash = h;
    return is;
}

static StringPool *pool_new(uint32_t capacity)
{
    StringPool *sp = (StringPool *)calloc(1, sizeof(StringPool) + capacity * sizeof(InternedString *));
    if (!sp)
        exit(1);
    sp->capacity = capacity;
    return sp;
}

static InternedString *pool_add(StringPool *sp, const char *s)
{
    uint32_t i;
    InternedString *is;

    for (i = 0; i < sp->count; i++)
        if (strcmp(sp->entries[i]->text, s) == 0)
            return sp->entries[i];
    if (sp->count == sp->capacity)
        return NULL;
    is = intern_new(s);
    sp->entries[sp->count++] = is;
    return is;
}

int main(void)
{
    const char *words[] = {"alpha", "beta", "gamma", "beta", "delta", "alpha"};
    uint8_t body[48];
    Packet *pkt;
    Matrix *a, *b, *c;
    StringPool *pool;
    uint32_t i, j;

    for (i = 0; i < sizeof(body); i++)
        body[i] = (uint8_t)(i * 7 + 3);

    pkt = packet_new(0x11, body, (uint32_t)sizeof(body));
    BENCH_OUTPUT(
        printf("packet op=%02x len=%u crc=%08x valid=%d first=%02x last=%02x\n",
               pkt->hdr.opcode, pkt->hdr.payload_len, pkt->hdr.crc32,
               packet_verify(pkt), pkt->payload[0], pkt->payload[sizeof(body) - 1]),
        printf("%02x %u %08x %d %02x %02x\n", pkt->hdr.opcode,
               pkt->hdr.payload_len, pkt->hdr.crc32, packet_verify(pkt),
               pkt->payload[0], pkt->payload[sizeof(body) - 1]));

    a = matrix_new(3, 4);
    b = matrix_new(4, 2);
    for (i = 0; i < a->rows; i++)
        for (j = 0; j < a->cols; j++)
            *matrix_at(a, i, j) = (double)(i + 1) * (double)(j + 2);
    for (i = 0; i < b->rows; i++)
        for (j = 0; j < b->cols; j++)
            *matrix_at(b, i, j) = (double)(i * 2) - (double)j;
    c = matrix_mul(a, b);

    BENCH_OUTPUT(printf("matrix %ux%u:\n", c->rows, c->cols),
                 printf("%u %u\n", c->rows, c->cols));
    for (i = 0; i < c->rows; i++) {
        for (j = 0; j < c->cols; j++)
            printf(" %8.2f", *matrix_at(c, i, j));
        putchar('\n');
    }

    pool = pool_new(8);
    for (i = 0; i < sizeof(words) / sizeof(words[0]); i++)
        pool_add(pool, words[i]);
    BENCH_OUTPUT(printf("pool count=%u/%u\n", pool->count, pool->capacity),
                 printf("%u %u\n", pool->count, pool->capacity));
    for (i = 0; i < pool->count; i++)
        BENCH_OUTPUT(
            printf("  [%u] hash=%08x len=%u '%s'\n", i,
                   pool->entries[i]->hash, pool->entries[i]->len,
                   pool->entries[i]->text),
            printf("%u %08x %u %s\n", i, pool->entries[i]->hash,
                   pool->entries[i]->len, pool->entries[i]->text));

    for (i = 0; i < pool->count; i++)
        free(pool->entries[i]);
    free(pool);
    free(c);
    free(b);
    free(a);
    free(pkt);
    return 0;
}
