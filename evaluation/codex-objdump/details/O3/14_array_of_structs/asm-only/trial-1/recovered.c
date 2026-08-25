#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum { PARTICLE_COUNT = 12, GROUP_COUNT = 4, UPDATE_COUNT = 3 };

typedef struct {
    float position[3];
    float velocity[3];
    float mass;
    float value;
    uint32_t id;
    uint16_t tag;
    uint8_t group;
    uint8_t active;
} Particle;

typedef struct {
    double kinetic_energy;
    double center[3];
    double momentum[3];
    float minimum_mass;
    float maximum_mass;
    uint32_t active_count;
    uint32_t group_count[GROUP_COUNT];
    char text[48];
} Summary;

typedef struct {
    float position[3][PARTICLE_COUNT];
    float velocity[3][PARTICLE_COUNT];
    float mass[PARTICLE_COUNT];
    uint32_t id[PARTICLE_COUNT];
    uint8_t group[PARTICLE_COUNT];
    uint8_t padding[4];
} ParticleArrays;

_Static_assert(sizeof(Particle) == 40, "unexpected Particle layout");
_Static_assert(offsetof(Particle, mass) == 24, "unexpected Particle layout");
_Static_assert(offsetof(Particle, value) == 28, "unexpected Particle layout");
_Static_assert(offsetof(Particle, id) == 32, "unexpected Particle layout");
_Static_assert(offsetof(Particle, tag) == 36, "unexpected Particle layout");
_Static_assert(offsetof(Particle, group) == 38, "unexpected Particle layout");
_Static_assert(offsetof(Particle, active) == 39, "unexpected Particle layout");
_Static_assert(sizeof(Summary) == 136, "unexpected Summary layout");
_Static_assert(sizeof(ParticleArrays) == 400, "unexpected ParticleArrays layout");

/*
 * The supplied disassembly omits the bytes containing the initial floating
 * values and scalar constants. Neutral finite substitutions preserve the
 * recovered control flow and aggregate relationships.
 */
static const float update_interval = 0.0f;
static const float vertical_impulse = 0.0f;
static const double conversion_factor = 1.0;

static void initialize_particles(Particle particles[PARTICLE_COUNT])
{
    uint32_t i;

    for (i = 0; i < PARTICLE_COUNT; ++i) {
        particles[i] = (Particle){
            .position = { 0.0f, 0.0f, 0.0f },
            .velocity = { 0.0f, 0.0f, 0.0f },
            .mass = 1.0f,
            .value = 0.0f,
            .id = 500u + i,
            .tag = (uint16_t)(0x100u + i),
            .group = (uint8_t)(i & 3u),
            .active = (uint8_t)(i != 6u)
        };
    }
}

static void print_particles(const Particle particles[PARTICLE_COUNT])
{
    size_t i;

    for (i = 0; i < PARTICLE_COUNT; ++i) {
        const Particle *p = &particles[i];

        printf("%u %u %u %u %g %g %g %g %g\n",
               p->id,
               (unsigned)p->group,
               (unsigned)p->tag,
               (unsigned)p->active,
               (double)p->mass,
               (double)p->value,
               (double)p->position[0],
               (double)p->position[1],
               (double)p->position[2]);
    }
}

static Summary summarize(const Particle particles[PARTICLE_COUNT],
                         double *total_mass)
{
    Summary result = { 0 };
    double mass_sum = 0.0;
    size_t i;

    result.minimum_mass = INFINITY;
    result.maximum_mass = -INFINITY;

    for (i = 0; i < PARTICLE_COUNT; ++i) {
        const Particle *p = &particles[i];
        double mass;
        double vx;
        double vy;
        double vz;

        if (!p->active)
            continue;

        mass = (double)p->mass;
        vx = (double)p->velocity[0];
        vy = (double)p->velocity[1];
        vz = (double)p->velocity[2];

        ++result.active_count;
        ++result.group_count[p->group & 3u];

        mass_sum += mass;
        result.kinetic_energy +=
            ((vx * vx + vy * vy) + vz * vz) * (0.5 * mass);

        result.center[0] += (double)p->position[0] * mass;
        result.center[1] += (double)p->position[1] * mass;
        result.center[2] += (double)p->position[2] * mass;

        result.momentum[0] += vx * mass;
        result.momentum[1] += vy * mass;
        result.momentum[2] += vz * mass;

        if (p->mass < result.minimum_mass)
            result.minimum_mass = p->mass;
        if (p->mass > result.maximum_mass)
            result.maximum_mass = p->mass;
    }

    if (mass_sum > 0.0) {
        result.center[0] /= mass_sum;
        result.center[1] /= mass_sum;
        result.center[2] /= mass_sum;
    }

    *total_mass = mass_sum;
    return result;
}

static void print_summary(const Summary *summary)
{
    printf("%g %g %g %g %g %g %g\n",
           summary->kinetic_energy,
           summary->center[0],
           summary->center[1],
           summary->center[2],
           summary->momentum[0],
           summary->momentum[1],
           summary->momentum[2]);

    printf("%g %g %u %u %u %u %u\n",
           (double)summary->minimum_mass,
           (double)summary->maximum_mass,
           summary->active_count,
           summary->group_count[0],
           summary->group_count[1],
           summary->group_count[2],
           summary->group_count[3]);
}

static Summary converted_summary(Summary summary)
{
    summary.kinetic_energy *= conversion_factor;
    summary.center[0] *= conversion_factor;
    summary.center[1] *= conversion_factor;
    summary.center[2] *= conversion_factor;
    summary.momentum[0] *= conversion_factor;
    summary.momentum[1] *= conversion_factor;
    summary.momentum[2] *= conversion_factor;

    snprintf(summary.text, sizeof summary.text, "%g %g",
             conversion_factor, summary.kinetic_energy);
    return summary;
}

static void transpose_particles(const Particle particles[PARTICLE_COUNT],
                                ParticleArrays *arrays)
{
    size_t i;
    size_t axis;

    *arrays = (ParticleArrays){ 0 };

    for (i = 0; i < PARTICLE_COUNT; ++i) {
        for (axis = 0; axis < 3; ++axis) {
            arrays->position[axis][i] = particles[i].position[axis];
            arrays->velocity[axis][i] = particles[i].velocity[axis];
        }

        arrays->mass[i] = particles[i].mass;
        arrays->id[i] = particles[i].id;
        arrays->group[i] = particles[i].group;
    }
}

static void update_particles(ParticleArrays *arrays)
{
    unsigned step;
    size_t i;

    for (step = 0; step < UPDATE_COUNT; ++step) {
        for (i = 0; i < PARTICLE_COUNT; ++i) {
            arrays->position[0][i] +=
                arrays->velocity[0][i] * update_interval;
            arrays->position[1][i] +=
                arrays->velocity[1][i] * update_interval;
            arrays->position[2][i] +=
                arrays->velocity[2][i] * update_interval;

            arrays->velocity[1][i] -=
                vertical_impulse / arrays->mass[i];
        }
    }
}

static void print_particle_samples(const ParticleArrays *arrays)
{
    size_t i;

    printf("%d\n", PARTICLE_COUNT);

    for (i = 0; i < PARTICLE_COUNT; i += 3) {
        printf("%u %u %g %g %g %g\n",
               arrays->id[i],
               (unsigned)arrays->group[i],
               (double)arrays->position[0][i],
               (double)arrays->position[1][i],
               (double)arrays->position[2][i],
               (double)arrays->velocity[1][i]);
    }
}

static int compare_particles(const void *left, const void *right)
{
    const Particle *a = left;
    const Particle *b = right;

    if (b->mass > a->mass)
        return 1;
    if (a->mass > b->mass)
        return -1;

    if (a->id < b->id)
        return -1;
    if (a->id > b->id)
        return 1;
    return 0;
}

int main(void)
{
    Particle particles[PARTICLE_COUNT];
    Particle sorted[PARTICLE_COUNT];
    ParticleArrays arrays;
    Summary summary;
    Summary converted;
    double total_mass;
    size_t i;

    initialize_particles(particles);
    print_particles(particles);

    summary = summarize(particles, &total_mass);
    snprintf(summary.text, sizeof summary.text, "%u %g %g",
             summary.active_count, total_mass, summary.kinetic_energy);
    print_summary(&summary);

    converted = converted_summary(summary);
    print_summary(&converted);

    transpose_particles(particles, &arrays);
    update_particles(&arrays);
    print_particle_samples(&arrays);

    for (i = 0; i < PARTICLE_COUNT; ++i)
        sorted[i] = particles[i];

    qsort(sorted, PARTICLE_COUNT, sizeof sorted[0], compare_particles);
    print_particles(sorted);

    return 0;
}