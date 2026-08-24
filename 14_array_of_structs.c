/*
 * 14_array_of_structs.c
 * Target feature: array-of-structs vs struct-of-arrays over the same data,
 * large struct passed/returned by value (memcpy or hidden-pointer ABI), and
 * qsort with a comparator over user structs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

#define N_PARTICLES 12

/* Array-of-structs layout. */
typedef struct Particle {
    float    pos[3];
    float    vel[3];
    float    mass;
    float    charge;
    uint32_t id;
    uint16_t flags;
    uint8_t  team;
    uint8_t  alive;
} Particle;

/* Struct-of-arrays layout of the very same fields. */
typedef struct ParticleSoa {
    float    pos_x[N_PARTICLES];
    float    pos_y[N_PARTICLES];
    float    pos_z[N_PARTICLES];
    float    vel_x[N_PARTICLES];
    float    vel_y[N_PARTICLES];
    float    vel_z[N_PARTICLES];
    float    mass[N_PARTICLES];
    uint32_t id[N_PARTICLES];
    uint8_t  team[N_PARTICLES];
    uint32_t count;
} ParticleSoa;

/* Big aggregate returned by value. */
typedef struct SystemStats {
    double   kinetic_energy;
    double   center_of_mass[3];
    double   momentum[3];
    float    min_mass;
    float    max_mass;
    uint32_t alive;
    uint32_t per_team[4];
    char     summary[48];
} SystemStats;

static void aos_init(Particle *arr, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        Particle *p = &arr[i];
        p->id      = (uint32_t)(500 + i);
        p->pos[0]  = (float)i * 1.5f;
        p->pos[1]  = (float)(i % 4) * 2.0f;
        p->pos[2]  = (float)(i * i % 7);
        p->vel[0]  = 0.5f - (float)(i % 3);
        p->vel[1]  = (float)(i % 5) * 0.25f;
        p->vel[2]  = -0.75f + (float)(i % 2);
        p->mass    = 1.0f + (float)(i % 6) * 0.5f;
        p->charge  = (i % 2) ? 1.0f : -1.0f;
        p->flags   = (uint16_t)(0x100u | i);
        p->team    = (uint8_t)(i % 4);
        p->alive   = (uint8_t)((i % 7) != 6);
    }
}

static void aos_to_soa(const Particle *arr, size_t n, ParticleSoa *soa)
{
    size_t i;

    memset(soa, 0, sizeof(*soa));
    for (i = 0; i < n && i < N_PARTICLES; i++) {
        soa->pos_x[i] = arr[i].pos[0];
        soa->pos_y[i] = arr[i].pos[1];
        soa->pos_z[i] = arr[i].pos[2];
        soa->vel_x[i] = arr[i].vel[0];
        soa->vel_y[i] = arr[i].vel[1];
        soa->vel_z[i] = arr[i].vel[2];
        soa->mass[i]  = arr[i].mass;
        soa->id[i]    = arr[i].id;
        soa->team[i]  = arr[i].team;
        soa->count++;
    }
}

static void soa_step(ParticleSoa *soa, float dt)
{
    uint32_t i;

    for (i = 0; i < soa->count; i++) {
        soa->pos_x[i] += soa->vel_x[i] * dt;
        soa->pos_y[i] += soa->vel_y[i] * dt;
        soa->pos_z[i] += soa->vel_z[i] * dt;
        soa->vel_y[i] -= 9.81f * dt / soa->mass[i];
    }
}

/* returns a 100+ byte struct by value */
static SystemStats compute_stats(const Particle *arr, size_t n)
{
    SystemStats st;
    double total_mass = 0.0;
    size_t i;

    memset(&st, 0, sizeof(st));
    st.min_mass = 1e30f;
    st.max_mass = -1e30f;

    for (i = 0; i < n; i++) {
        const Particle *p = &arr[i];
        double v2 = (double)p->vel[0] * p->vel[0] +
                    (double)p->vel[1] * p->vel[1] +
                    (double)p->vel[2] * p->vel[2];
        if (!p->alive)
            continue;
        st.alive++;
        st.per_team[p->team & 3]++;
        st.kinetic_energy += 0.5 * (double)p->mass * v2;
        total_mass += p->mass;
        st.center_of_mass[0] += (double)p->mass * p->pos[0];
        st.center_of_mass[1] += (double)p->mass * p->pos[1];
        st.center_of_mass[2] += (double)p->mass * p->pos[2];
        st.momentum[0] += (double)p->mass * p->vel[0];
        st.momentum[1] += (double)p->mass * p->vel[1];
        st.momentum[2] += (double)p->mass * p->vel[2];
        if (p->mass < st.min_mass) st.min_mass = p->mass;
        if (p->mass > st.max_mass) st.max_mass = p->mass;
    }
    if (total_mass > 0.0) {
        st.center_of_mass[0] /= total_mass;
        st.center_of_mass[1] /= total_mass;
        st.center_of_mass[2] /= total_mass;
    }
    snprintf(st.summary, sizeof(st.summary), "alive=%u mass=%.2f ke=%.2f",
             st.alive, total_mass, st.kinetic_energy);
    return st;
}

/* takes a big struct by value, mutates the copy */
static SystemStats stats_rescale(SystemStats st, double k)
{
    int i;
    st.kinetic_energy *= k;
    for (i = 0; i < 3; i++) {
        st.center_of_mass[i] *= k;
        st.momentum[i] *= k;
    }
    snprintf(st.summary, sizeof(st.summary), "scaled x%.2f ke=%.2f", k, st.kinetic_energy);
    return st;
}

static int cmp_by_mass_then_id(const void *a, const void *b)
{
    const Particle *pa = (const Particle *)a;
    const Particle *pb = (const Particle *)b;

    if (pa->mass < pb->mass) return 1;
    if (pa->mass > pb->mass) return -1;
    return (pa->id < pb->id) ? -1 : (pa->id > pb->id);
}

static void aos_dump(const Particle *arr, size_t n, const char *label)
{
    size_t i;
    BENCH_VERBOSE_ARG(label);
    BENCH_VERBOSE(printf("%s\n", label));
    for (i = 0; i < n; i++)
        BENCH_OUTPUT(
            printf("  id=%u team=%u m=%.2f q=%+.0f pos=(%6.2f %6.2f %6.2f) flags=%04x %s\n",
                   arr[i].id, arr[i].team, arr[i].mass, arr[i].charge,
                   arr[i].pos[0], arr[i].pos[1], arr[i].pos[2], arr[i].flags,
                   arr[i].alive ? "" : "(dead)"),
            printf("%u %u %.2f %.0f %.2f %.2f %.2f %04x %u\n",
                   arr[i].id, arr[i].team, arr[i].mass, arr[i].charge,
                   arr[i].pos[0], arr[i].pos[1], arr[i].pos[2], arr[i].flags,
                   arr[i].alive));
}

static void stats_dump(const SystemStats *st, const char *label)
{
    BENCH_VERBOSE_ARG(label);
    BENCH_OUTPUT(
        printf("%s: ke=%.3f com=(%.3f %.3f %.3f) p=(%.3f %.3f %.3f)\n",
               label, st->kinetic_energy, st->center_of_mass[0],
               st->center_of_mass[1], st->center_of_mass[2], st->momentum[0],
               st->momentum[1], st->momentum[2]),
        printf("%.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
               st->kinetic_energy, st->center_of_mass[0],
               st->center_of_mass[1], st->center_of_mass[2], st->momentum[0],
               st->momentum[1], st->momentum[2]));
    BENCH_OUTPUT(
        printf("   mass=[%.2f..%.2f] alive=%u teams=%u/%u/%u/%u '%s'\n",
               st->min_mass, st->max_mass, st->alive, st->per_team[0],
               st->per_team[1], st->per_team[2], st->per_team[3], st->summary),
        printf("%.2f %.2f %u %u %u %u %u\n", st->min_mass, st->max_mass,
               st->alive, st->per_team[0], st->per_team[1], st->per_team[2],
               st->per_team[3]));
}

int main(void)
{
    Particle aos[N_PARTICLES];
    Particle copy[N_PARTICLES];
    ParticleSoa soa;
    SystemStats st, scaled;
    int step;

    aos_init(aos, N_PARTICLES);
    aos_dump(aos, N_PARTICLES, "AoS initial:");

    st = compute_stats(aos, N_PARTICLES);
    stats_dump(&st, "stats");

    scaled = stats_rescale(st, 2.5);
    stats_dump(&scaled, "rescaled");

    aos_to_soa(aos, N_PARTICLES, &soa);
    for (step = 0; step < 3; step++)
        soa_step(&soa, 0.1f);
    BENCH_OUTPUT(printf("SoA after 3 steps (count=%u):\n", soa.count),
                 printf("%u\n", soa.count));
    {
        uint32_t i;
        for (i = 0; i < soa.count; i += 3)
            BENCH_OUTPUT(
                printf("  id=%u pos=(%6.3f %6.3f %6.3f) vel_y=%6.3f team=%u\n",
                       soa.id[i], soa.pos_x[i], soa.pos_y[i], soa.pos_z[i],
                       soa.vel_y[i], soa.team[i]),
                printf("%u %.3f %.3f %.3f %.3f %u\n", soa.id[i],
                       soa.pos_x[i], soa.pos_y[i], soa.pos_z[i],
                       soa.vel_y[i], soa.team[i]));
    }

    memcpy(copy, aos, sizeof(copy));            /* whole-array struct copy */
    qsort(copy, N_PARTICLES, sizeof(Particle), cmp_by_mass_then_id);
    aos_dump(copy, N_PARTICLES, "sorted by mass desc:");

    return 0;
}
