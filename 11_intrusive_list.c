/*
 * 11_intrusive_list.c
 * Target feature: Linux-kernel style intrusive containers - a bare link
 * struct embedded at a non-zero offset inside several *different* owner
 * types, recovered with offsetof/container_of. The same list node type is
 * reached from unrelated structs, which stresses type propagation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef struct HList {
    struct HList *next;
    struct HList *prev;
} HList;

#define HLIST_INIT(h)   do { (h)->next = (h); (h)->prev = (h); } while (0)
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#define hlist_for_each(pos, head) \
    for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

typedef enum TaskState {
    TASK_NEW = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DONE
} TaskState;

/* Owner type A: link is in the middle of the struct. */
typedef struct Task {
    uint32_t   pid;
    char       name[16];
    TaskState  state;
    HList      run_link;     /* offset != 0 */
    HList      all_link;     /* a second, independent list membership */
    uint32_t   priority;
    uint64_t   cpu_ticks;
} Task;

/* Owner type B: completely different layout, same link type. */
typedef struct Timer {
    uint64_t deadline;
    HList    queue_link;
    void   (*callback)(struct Timer *);
    char     label[12];
    uint32_t fire_count;
} Timer;

/* Owner type C: link is the last member. */
typedef struct Buffer {
    uint8_t  data[32];
    uint32_t used;
    uint32_t generation;
    HList    pool_link;
} Buffer;

typedef struct Scheduler {
    HList    runqueue;
    HList    all_tasks;
    HList    timers;
    HList    free_buffers;
    uint64_t now;
    uint32_t switches;
} Scheduler;

static void hlist_add_tail(HList *head, HList *node)
{
    node->prev = head->prev;
    node->next = head;
    head->prev->next = node;
    head->prev = node;
}

static void hlist_del(HList *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node;
    node->prev = node;
}

static int hlist_empty(const HList *head) { return head->next == head; }

static size_t hlist_len(const HList *head)
{
    const HList *p;
    size_t n = 0;
    for (p = head->next; p != head; p = p->next)
        n++;
    return n;
}

static void timer_fired(Timer *t)
{
    t->fire_count++;
    BENCH_OUTPUT(
        printf("    timer '%s' fired (deadline=%llu, count=%u)\n", t->label,
               (unsigned long long)t->deadline, t->fire_count),
        printf("%s %llu %u\n", t->label,
               (unsigned long long)t->deadline, t->fire_count));
}

static void sched_init(Scheduler *s)
{
    HLIST_INIT(&s->runqueue);
    HLIST_INIT(&s->all_tasks);
    HLIST_INIT(&s->timers);
    HLIST_INIT(&s->free_buffers);
    s->now = 1000;
    s->switches = 0;
}

static Task *task_create(Scheduler *s, uint32_t pid, const char *name, uint32_t prio)
{
    Task *t = (Task *)calloc(1, sizeof(Task));
    if (!t)
        exit(1);
    t->pid = pid;
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->priority = prio;
    t->state = TASK_READY;
    HLIST_INIT(&t->run_link);
    HLIST_INIT(&t->all_link);
    hlist_add_tail(&s->all_tasks, &t->all_link);
    hlist_add_tail(&s->runqueue, &t->run_link);
    return t;
}

static Task *sched_pick(Scheduler *s)
{
    HList *p;
    Task *best = NULL;

    hlist_for_each(p, &s->runqueue) {
        Task *t = container_of(p, Task, run_link);
        if (!best || t->priority > best->priority)
            best = t;
    }
    return best;
}

static void sched_run_slice(Scheduler *s, uint64_t ticks)
{
    Task *t = sched_pick(s);
    HList *p, *tmp;

    if (!t)
        return;
    t->state = TASK_RUNNING;
    t->cpu_ticks += ticks;
    s->now += ticks;
    s->switches++;
    BENCH_OUTPUT(
        printf("  run pid=%u '%s' prio=%u ticks=%llu now=%llu\n", t->pid,
               t->name, t->priority, (unsigned long long)t->cpu_ticks,
               (unsigned long long)s->now),
        printf("%u %s %u %llu %llu\n", t->pid, t->name, t->priority,
               (unsigned long long)t->cpu_ticks, (unsigned long long)s->now));

    for (p = s->timers.next; p != &s->timers; p = tmp) {
        Timer *tm = container_of(p, Timer, queue_link);
        tmp = p->next;
        if (tm->deadline <= s->now) {
            hlist_del(&tm->queue_link);
            if (tm->callback)
                tm->callback(tm);
            tm->deadline = s->now + 40;
            hlist_add_tail(&s->timers, &tm->queue_link);
        }
    }

    if (t->cpu_ticks >= 60) {
        t->state = TASK_DONE;
        hlist_del(&t->run_link);
        BENCH_OUTPUT(printf("    pid=%u finished, runqueue=%zu\n", t->pid,
                           hlist_len(&s->runqueue)),
                     printf("%u %zu\n", t->pid, hlist_len(&s->runqueue)));
    } else {
        t->state = TASK_READY;
        hlist_del(&t->run_link);
        hlist_add_tail(&s->runqueue, &t->run_link);
    }
}

static void dump_all(const Scheduler *s)
{
    const HList *p;

    BENCH_OUTPUT(printf("all tasks (%zu):\n", hlist_len(&s->all_tasks)),
                 printf("%zu\n", hlist_len(&s->all_tasks)));
    for (p = s->all_tasks.next; p != &s->all_tasks; p = p->next) {
        const Task *t = container_of(p, Task, all_link);
        BENCH_OUTPUT(
            printf("  pid=%-3u %-12s state=%d prio=%u cpu=%llu\n", t->pid,
                   t->name, (int)t->state, t->priority,
                   (unsigned long long)t->cpu_ticks),
            printf("%u %s %d %u %llu\n", t->pid, t->name, (int)t->state,
                   t->priority, (unsigned long long)t->cpu_ticks));
    }
    BENCH_OUTPUT(printf("free buffers (%zu):\n", hlist_len(&s->free_buffers)),
                 printf("%zu\n", hlist_len(&s->free_buffers)));
    for (p = s->free_buffers.next; p != &s->free_buffers; p = p->next) {
        const Buffer *b = container_of(p, Buffer, pool_link);
        BENCH_OUTPUT(printf("  gen=%u used=%u first=%02x\n", b->generation,
                           b->used, b->data[0]),
                     printf("%u %u %02x\n", b->generation, b->used,
                            b->data[0]));
    }
}

int main(void)
{
    Scheduler sched;
    Timer timers[2];
    Buffer bufs[3];
    unsigned i;
    HList *p;

    sched_init(&sched);

    task_create(&sched, 1, "init",     1);
    task_create(&sched, 7, "worker",   5);
    task_create(&sched, 9, "logger",   3);
    task_create(&sched, 12, "gc",      9);

    for (i = 0; i < 2; i++) {
        memset(&timers[i], 0, sizeof(timers[i]));
        timers[i].deadline = 1030 + i * 25;
        timers[i].callback = timer_fired;
        snprintf(timers[i].label, sizeof(timers[i].label), "tmr%u", i);
        HLIST_INIT(&timers[i].queue_link);
        hlist_add_tail(&sched.timers, &timers[i].queue_link);
    }

    for (i = 0; i < 3; i++) {
        memset(&bufs[i], 0, sizeof(bufs[i]));
        bufs[i].generation = i + 1;
        bufs[i].used = (i + 1) * 8;
        memset(bufs[i].data, (int)(0x40 + i), bufs[i].used);
        HLIST_INIT(&bufs[i].pool_link);
        hlist_add_tail(&sched.free_buffers, &bufs[i].pool_link);
    }

    for (i = 0; i < 8 && !hlist_empty(&sched.runqueue); i++)
        sched_run_slice(&sched, 15 + (i % 3) * 5);

    dump_all(&sched);
    BENCH_OUTPUT(
        printf("switches=%u runqueue=%zu timers=%zu\n", sched.switches,
               hlist_len(&sched.runqueue), hlist_len(&sched.timers)),
        printf("%u %zu %zu\n", sched.switches, hlist_len(&sched.runqueue),
               hlist_len(&sched.timers)));

    while (!hlist_empty(&sched.all_tasks)) {
        p = sched.all_tasks.next;
        {
            Task *t = container_of(p, Task, all_link);
            hlist_del(&t->all_link);
            if (t->run_link.next != &t->run_link)
                hlist_del(&t->run_link);
            free(t);
        }
    }
    return 0;
}
