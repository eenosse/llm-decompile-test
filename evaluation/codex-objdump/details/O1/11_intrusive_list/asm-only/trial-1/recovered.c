#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ListNode {
    struct ListNode *next;
    struct ListNode *prev;
} ListNode;

typedef struct Job {
    int32_t id;
    char name[16];
    int32_t state;
    ListNode active_link;
    ListNode all_link;
    uint32_t priority;
    uint32_t padding;
    uint64_t runtime;
} Job;

typedef struct Timer Timer;
typedef void (*TimerCallback)(Timer *);

struct Timer {
    uint64_t deadline;
    ListNode link;
    TimerCallback callback;
    char name[12];
    uint32_t count;
};

typedef struct Resource {
    unsigned char storage[32];
    uint32_t capacity;
    uint32_t index;
    ListNode link;
} Resource;

typedef struct Scheduler {
    ListNode active_jobs;
    ListNode all_jobs;
    ListNode timers;
    ListNode resources;
    uint64_t clock;
    uint32_t dispatch_count;
    uint32_t padding;
} Scheduler;

_Static_assert(sizeof(ListNode) == 16, "unexpected ListNode layout");
_Static_assert(offsetof(Job, state) == 0x14, "unexpected Job layout");
_Static_assert(offsetof(Job, active_link) == 0x18, "unexpected Job layout");
_Static_assert(offsetof(Job, all_link) == 0x28, "unexpected Job layout");
_Static_assert(offsetof(Job, priority) == 0x38, "unexpected Job layout");
_Static_assert(offsetof(Job, runtime) == 0x40, "unexpected Job layout");
_Static_assert(sizeof(Job) == 0x48, "unexpected Job size");
_Static_assert(offsetof(Timer, callback) == 0x18, "unexpected Timer layout");
_Static_assert(offsetof(Timer, name) == 0x20, "unexpected Timer layout");
_Static_assert(offsetof(Timer, count) == 0x2c, "unexpected Timer layout");
_Static_assert(sizeof(Timer) == 0x30, "unexpected Timer size");
_Static_assert(offsetof(Resource, capacity) == 0x20, "unexpected Resource layout");
_Static_assert(offsetof(Resource, index) == 0x24, "unexpected Resource layout");
_Static_assert(offsetof(Resource, link) == 0x28, "unexpected Resource layout");
_Static_assert(sizeof(Resource) == 0x38, "unexpected Resource size");

#define CONTAINER_OF(pointer, type, member) \
    ((type *)((char *)(pointer) - offsetof(type, member)))

static void list_init(ListNode *list)
{
    list->next = list;
    list->prev = list;
}

static void list_insert_tail(ListNode *list, ListNode *node)
{
    node->prev = list->prev;
    node->next = list;
    list->prev->next = node;
    list->prev = node;
}

static void list_remove(ListNode *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node;
    node->prev = node;
}

static size_t list_count(const ListNode *list)
{
    size_t count = 0;
    const ListNode *node;

    for (node = list->next; node != list; node = node->next)
        ++count;

    return count;
}

static void timer_callback(Timer *timer)
{
    uint32_t count = ++timer->count;
    printf("T%s %lu %u\n",
           timer->name,
           (unsigned long)timer->deadline,
           count);
}

static Job *create_job(Scheduler *scheduler, int32_t id,
                       const char *name, uint32_t priority)
{
    Job *job = calloc(1, sizeof(*job));

    if (job == NULL)
        exit(1);

    job->id = id;
    strncpy(job->name, name, 15);
    job->state = 1;
    job->priority = priority;

    list_insert_tail(&scheduler->all_jobs, &job->all_link);
    list_insert_tail(&scheduler->active_jobs, &job->active_link);

    return job;
}

static Job *highest_priority_job(Scheduler *scheduler)
{
    ListNode *node = scheduler->active_jobs.next;
    Job *selected;

    if (node == &scheduler->active_jobs)
        return NULL;

    selected = CONTAINER_OF(node, Job, active_link);
    node = node->next;

    while (node != &scheduler->active_jobs) {
        Job *candidate = CONTAINER_OF(node, Job, active_link);

        if (candidate->priority > selected->priority)
            selected = candidate;

        node = node->next;
    }

    return selected;
}

static void process_timers(Scheduler *scheduler)
{
    ListNode *node = scheduler->timers.next;

    while (node != &scheduler->timers) {
        Timer *timer = CONTAINER_OF(node, Timer, link);
        ListNode *next = node->next;

        if (timer->deadline <= scheduler->clock) {
            list_remove(&timer->link);

            if (timer->callback != NULL)
                timer->callback(timer);

            timer->deadline = scheduler->clock + 40;
            list_insert_tail(&scheduler->timers, &timer->link);
        }

        node = next;
    }
}

int main(void)
{
    Scheduler scheduler;
    Timer timers[2];
    Resource resources[3];
    uint32_t iteration;

    list_init(&scheduler.active_jobs);
    list_init(&scheduler.all_jobs);
    list_init(&scheduler.timers);
    list_init(&scheduler.resources);
    scheduler.clock = 1000;
    scheduler.dispatch_count = 0;
    scheduler.padding = 0;

    create_job(&scheduler, 1, "init", 1);
    create_job(&scheduler, 7, "worker", 5);
    create_job(&scheduler, 9, "logger", 3);
    create_job(&scheduler, 12, "db", 9);

    memset(timers, 0, sizeof(timers));

    timers[0].deadline = 1030;
    timers[0].callback = timer_callback;
    snprintf(timers[0].name, sizeof(timers[0].name), "tmr%d", 0);
    list_insert_tail(&scheduler.timers, &timers[0].link);

    timers[1].deadline = 1055;
    timers[1].callback = timer_callback;
    snprintf(timers[1].name, sizeof(timers[1].name), "tmr%d", 1);
    list_insert_tail(&scheduler.timers, &timers[1].link);

    for (uint32_t i = 1; i <= 3; ++i) {
        Resource *resource = &resources[i - 1];

        memset(resource, 0, sizeof(*resource));
        resource->index = i;
        resource->capacity = i * 8;
        memset(resource, (int)(i + 63), i * 8);
        list_insert_tail(&scheduler.resources, &resource->link);
    }

    for (iteration = 0; iteration < 8; ++iteration) {
        Job *job = highest_priority_job(&scheduler);
        uint64_t slice;

        if (job == NULL)
            break;

        slice = (uint64_t)((iteration % 3) * 5 + 15);
        job->state = 2;
        job->runtime += slice;
        scheduler.clock += slice;
        ++scheduler.dispatch_count;

        printf("D %d %s %u %lu %lu\n",
               job->id,
               job->name,
               job->priority,
               (unsigned long)job->runtime,
               (unsigned long)scheduler.clock);

        process_timers(&scheduler);

        if (job->runtime > 59) {
            job->state = 4;
            list_remove(&job->active_link);
            printf("%d %zu\n",
                   job->id,
                   list_count(&scheduler.active_jobs));
        } else {
            job->state = 1;
            list_remove(&job->active_link);
            list_insert_tail(&scheduler.active_jobs, &job->active_link);
        }
    }

    printf("%zu\n", list_count(&scheduler.all_jobs));

    for (ListNode *node = scheduler.all_jobs.next;
         node != &scheduler.all_jobs;
         node = node->next) {
        Job *job = CONTAINER_OF(node, Job, all_link);

        printf("A%d %s %d %u %lu\n",
               job->id,
               job->name,
               job->state,
               job->priority,
               (unsigned long)job->runtime);
    }

    printf("%zu\n", list_count(&scheduler.resources));

    for (ListNode *node = scheduler.resources.next;
         node != &scheduler.resources;
         node = node->next) {
        Resource *resource = CONTAINER_OF(node, Resource, link);

        printf("R %u %u %c\n",
               resource->index,
               resource->capacity,
               resource->storage[0]);
    }

    printf("S %u %zu %zu\n",
           scheduler.dispatch_count,
           list_count(&scheduler.active_jobs),
           list_count(&scheduler.resources));

    while (scheduler.all_jobs.next != &scheduler.all_jobs) {
        Job *job = CONTAINER_OF(scheduler.all_jobs.next, Job, all_link);

        list_remove(&job->all_link);

        if (job->active_link.next != &job->active_link)
            list_remove(&job->active_link);

        free(job);
    }

    return 0;
}