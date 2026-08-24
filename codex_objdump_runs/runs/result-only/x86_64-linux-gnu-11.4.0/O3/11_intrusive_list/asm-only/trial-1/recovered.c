#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Link {
    struct Link *next;
    struct Link *previous;
} Link;

typedef struct Task {
    uint32_t id;
    char name[16];
    uint32_t state;
    Link active_link;
    Link all_link;
    uint32_t priority;
    uint32_t padding;
    uint64_t accumulated;
} Task;

typedef struct Timer {
    uint64_t due;
    Link link;
    void (*callback)(struct Timer *);
    char name[12];
    uint32_t count;
} Timer;

typedef union DeferredStamp {
    uint64_t value;
    struct {
        uint32_t amount;
        uint32_t tag;
    } fields;
} DeferredStamp;

typedef struct DeferredRecord {
    unsigned char payload[32];
    DeferredStamp stamp;
    Link link;
} DeferredRecord;

_Static_assert(sizeof(Link) == 16, "unexpected Link layout");
_Static_assert(offsetof(Task, active_link) == 24, "unexpected Task layout");
_Static_assert(offsetof(Task, all_link) == 40, "unexpected Task layout");
_Static_assert(offsetof(Task, priority) == 56, "unexpected Task layout");
_Static_assert(offsetof(Task, accumulated) == 64, "unexpected Task layout");
_Static_assert(sizeof(Task) == 72, "unexpected Task size");
_Static_assert(offsetof(Timer, link) == 8, "unexpected Timer layout");
_Static_assert(offsetof(Timer, callback) == 24, "unexpected Timer layout");
_Static_assert(offsetof(Timer, name) == 32, "unexpected Timer layout");
_Static_assert(offsetof(Timer, count) == 44, "unexpected Timer layout");
_Static_assert(sizeof(Timer) == 48, "unexpected Timer size");
_Static_assert(offsetof(DeferredRecord, stamp) == 32,
               "unexpected DeferredRecord layout");
_Static_assert(offsetof(DeferredRecord, link) == 40,
               "unexpected DeferredRecord layout");
_Static_assert(sizeof(DeferredRecord) == 56,
               "unexpected DeferredRecord size");

#define CONTAINER(pointer, type, member) \
    ((type *)((unsigned char *)(pointer) - offsetof(type, member)))

static void link_initialize(Link *link)
{
    link->next = link;
    link->previous = link;
}

static void link_append(Link *head, Link *link)
{
    link->next = head;
    link->previous = head->previous;
    head->previous->next = link;
    head->previous = link;
}

static void link_remove(Link *link)
{
    link->previous->next = link->next;
    link->next->previous = link->previous;
    link_initialize(link);
}

static size_t link_count(const Link *head)
{
    size_t count = 0;
    const Link *link;

    for (link = head->next; link != head; link = link->next)
        ++count;

    return count;
}

static Task *create_task(Link *active_tasks, Link *all_tasks,
                         uint32_t id, const char *name, uint32_t priority)
{
    Task *task = calloc(1, sizeof(*task));

    if (task == NULL)
        exit(1);

    task->id = id;
    strncpy(task->name, name, sizeof(task->name) - 1);
    task->state = 1;
    task->priority = priority;

    link_append(all_tasks, &task->all_link);
    link_append(active_tasks, &task->active_link);
    return task;
}

static void timer_callback(Timer *timer)
{
    ++timer->count;
    printf("T%s %" PRIu64 " %u\n",
           timer->name, timer->due, timer->count);
}

static void process_timers(Link *timers, uint64_t now)
{
    Link *link = timers->next;

    while (link != timers) {
        Timer *timer = CONTAINER(link, Timer, link);

        link = link->next;
        if (timer->due <= now) {
            link_remove(&timer->link);
            if (timer->callback != NULL)
                timer->callback(timer);
            timer->due = now + 40;
            link_append(timers, &timer->link);
        }
    }
}

static void process_deferred(Link *records, Link *timers, uint64_t now)
{
    Link *link = records->next;

    while (link != records) {
        DeferredRecord *record = CONTAINER(link, DeferredRecord, link);

        link = link->next;
        if (record->stamp.value <= now) {
            void (*callback)(void *) = NULL;

            link_remove(&record->link);
            memcpy(&callback, (unsigned char *)&record->link + sizeof(Link),
                   sizeof(callback));
            if (callback != NULL)
                callback(&record->stamp);

            record->stamp.value = now + 40;
            link_append(timers, &record->link);
        }
    }
}

int main(void)
{
    Link active_tasks;
    Link all_tasks;
    Link timers;
    Link deferred;
    Timer timer_objects[2];
    DeferredRecord records[3];
    uint64_t current = 1000;
    uint32_t processed = 0;
    uint32_t round;

    link_initialize(&active_tasks);
    link_initialize(&all_tasks);
    link_initialize(&timers);
    link_initialize(&deferred);

    create_task(&active_tasks, &all_tasks, 1, "init", 1);
    create_task(&active_tasks, &all_tasks, 7, "worker", 5);
    create_task(&active_tasks, &all_tasks, 9, "logger", 3);
    create_task(&active_tasks, &all_tasks, 12, "io", 9);

    memset(timer_objects, 0, sizeof(timer_objects));

    timer_objects[0].due = 1030;
    timer_objects[0].callback = timer_callback;
    snprintf(timer_objects[0].name, sizeof(timer_objects[0].name),
             "tmr%u", 0U);
    link_append(&timers, &timer_objects[0].link);

    timer_objects[1].due = 1055;
    timer_objects[1].callback = timer_callback;
    snprintf(timer_objects[1].name, sizeof(timer_objects[1].name),
             "tmr%u", 1U);
    link_append(&timers, &timer_objects[1].link);

    memset(records, 0, sizeof(records));
    for (uint32_t i = 1; i < 4; ++i) {
        memset(records[i - 1].payload, (int)('@' + i), i * 8U);
        records[i - 1].stamp.fields.amount = i * 8U;
        records[i - 1].stamp.fields.tag = i;
        link_append(&deferred, &records[i - 1].link);
    }

    for (round = 0; round < 8; ++round) {
        Link *link;
        Task *selected = NULL;
        uint64_t increment;

        for (link = active_tasks.next;
             link != &active_tasks;
             link = link->next) {
            Task *task = CONTAINER(link, Task, active_link);

            if (selected == NULL || selected->priority > task->priority)
                selected = task;
        }

        if (selected == NULL)
            break;

        selected->state = 2;
        increment = 15U + 5U * (round % 3U);
        selected->accumulated += increment;
        current += increment;
        ++processed;

        printf("R %u %s %u %" PRIu64 " %" PRIu64 "\n",
               selected->id,
               selected->name,
               selected->priority,
               selected->accumulated,
               current);

        process_deferred(&deferred, &timers, current);

        if (selected->accumulated > 59U) {
            selected->state = 4;
            link_remove(&selected->active_link);
            printf("%u %zu\n", selected->id, link_count(&active_tasks));
        } else {
            selected->state = 1;
            link_remove(&selected->active_link);
            link_append(&active_tasks, &selected->active_link);
        }
    }

    printf("%zu\n", link_count(&all_tasks));
    for (Link *link = all_tasks.next; link != &all_tasks; link = link->next) {
        Task *task = CONTAINER(link, Task, all_link);

        printf("J%u %s %u %u %" PRIu64 "\n",
               task->id,
               task->name,
               task->state,
               task->priority,
               task->accumulated);
    }

    printf("%zu\n", link_count(&deferred));
    for (Link *link = deferred.next; link != &deferred; link = link->next) {
        DeferredRecord *record = CONTAINER(link, DeferredRecord, link);

        printf("E %u %u %c\n",
               record->stamp.fields.tag,
               record->stamp.fields.amount,
               record->payload[0]);
    }

    printf("S %u %zu %zu\n",
           processed,
           link_count(&active_tasks),
           link_count(&timers));

    while (all_tasks.next != &all_tasks) {
        Task *task = CONTAINER(all_tasks.next, Task, all_link);

        link_remove(&task->all_link);
        if (task->active_link.next != &task->active_link)
            link_remove(&task->active_link);
        free(task);
    }

    return 0;
}