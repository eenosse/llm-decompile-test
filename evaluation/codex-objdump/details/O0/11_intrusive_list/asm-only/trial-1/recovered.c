#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Link {
    struct Link *next;
    struct Link *prev;
} Link;

typedef struct Task {
    int id;
    char name[16];
    int state;
    Link active_link;
    Link all_link;
    unsigned int priority;
    unsigned long long elapsed;
} Task;

typedef struct Timer {
    unsigned long long due;
    Link link;
    void (*callback)(struct Timer *);
    char name[12];
    int count;
} Timer;

typedef struct Record {
    unsigned char data[32];
    unsigned int length;
    unsigned int id;
    Link link;
} Record;

typedef struct Scheduler {
    Link active;
    Link all_tasks;
    Link timers;
    Link records;
    unsigned long long clock;
    unsigned int rounds;
} Scheduler;

static void link_insert_tail(Link *head, Link *node)
{
    node->prev = head->prev;
    node->next = head;
    head->prev->next = node;
    head->prev = node;
}

static void link_remove(Link *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node;
    node->prev = node;
}

static int link_empty(const Link *head)
{
    return head->next == head;
}

static size_t link_count(const Link *head)
{
    size_t count = 0;
    const Link *node = head->next;

    while (node != head) {
        ++count;
        node = node->next;
    }

    return count;
}

static void timer_callback(Timer *timer)
{
    ++timer->count;
    printf("%s %llu %d\n", timer->name, timer->due, timer->count);
}

static void scheduler_init(Scheduler *scheduler)
{
    scheduler->active.next = &scheduler->active;
    scheduler->active.prev = &scheduler->active;

    scheduler->all_tasks.next = &scheduler->all_tasks;
    scheduler->all_tasks.prev = &scheduler->all_tasks;

    scheduler->timers.next = &scheduler->timers;
    scheduler->timers.prev = &scheduler->timers;

    scheduler->records.next = &scheduler->records;
    scheduler->records.prev = &scheduler->records;

    scheduler->clock = 1000;
    scheduler->rounds = 0;
}

static Task *task_create(Scheduler *scheduler, int id,
                         const char *name, unsigned int priority)
{
    Task *task = calloc(1, sizeof(*task));

    if (task == NULL)
        exit(1);

    task->id = id;
    strncpy(task->name, name, 15);
    task->priority = priority;
    task->state = 1;

    task->active_link.next = &task->active_link;
    task->active_link.prev = &task->active_link;
    task->all_link.next = &task->all_link;
    task->all_link.prev = &task->all_link;

    link_insert_tail(&scheduler->all_tasks, &task->all_link);
    link_insert_tail(&scheduler->active, &task->active_link);

    return task;
}

static Task *highest_priority_task(Link *head)
{
    Task *best = NULL;
    Link *node = head->next;

    while (node != head) {
        Task *task = (Task *)((char *)node - offsetof(Task, active_link));

        if (best == NULL || task->priority > best->priority)
            best = task;

        node = node->next;
    }

    return best;
}

static void scheduler_step(Scheduler *scheduler, unsigned long long amount)
{
    Task *task = highest_priority_task(&scheduler->active);
    Link *node;

    if (task == NULL)
        return;

    task->state = 2;
    task->elapsed += amount;
    scheduler->clock += amount;
    ++scheduler->rounds;

    printf("%d %s %u %llu %llu\n",
           task->id, task->name, task->priority,
           task->elapsed, scheduler->clock);

    node = scheduler->timers.next;
    while (node != &scheduler->timers) {
        Timer *timer =
            (Timer *)((char *)node - offsetof(Timer, link));
        Link *next = node->next;

        if (timer->due <= scheduler->clock) {
            link_remove(&timer->link);

            if (timer->callback != NULL)
                timer->callback(timer);

            timer->due = scheduler->clock + 40;
            link_insert_tail(&scheduler->timers, &timer->link);
        }

        node = next;
    }

    if (task->elapsed > 59) {
        task->state = 4;
        link_remove(&task->active_link);
        printf("%d %zu\n", task->id, link_count(&scheduler->active));
    } else {
        task->state = 1;
        link_remove(&task->active_link);
        link_insert_tail(&scheduler->active, &task->active_link);
    }
}

static void scheduler_dump(Scheduler *scheduler)
{
    Link *node;

    printf("%zu\n", link_count(&scheduler->all_tasks));

    node = scheduler->all_tasks.next;
    while (node != &scheduler->all_tasks) {
        Task *task =
            (Task *)((char *)node - offsetof(Task, all_link));

        printf("%d %s %d %u %llu\n",
               task->id, task->name, task->state,
               task->priority, task->elapsed);

        node = node->next;
    }

    printf("%zu\n", link_count(&scheduler->records));

    node = scheduler->records.next;
    while (node != &scheduler->records) {
        Record *record =
            (Record *)((char *)node - offsetof(Record, link));

        printf("%u %u '%c'\n",
               record->id, record->length, record->data[0]);

        node = node->next;
    }
}

int main(void)
{
    Scheduler scheduler;
    Timer timers[2];
    Record records[3];
    unsigned int i;

    scheduler_init(&scheduler);

    task_create(&scheduler, 1, "init", 1);
    task_create(&scheduler, 7, "worker", 5);
    task_create(&scheduler, 9, "logger", 3);
    task_create(&scheduler, 12, "io", 9);

    for (i = 0; i <= 1; ++i) {
        memset(&timers[i], 0, sizeof(timers[i]));
        timers[i].due = 1030 + i * 25;
        timers[i].callback = timer_callback;
        snprintf(timers[i].name, 12, "tmr%u", i);

        timers[i].link.next = &timers[i].link;
        timers[i].link.prev = &timers[i].link;
        link_insert_tail(&scheduler.timers, &timers[i].link);
    }

    for (i = 0; i <= 2; ++i) {
        memset(&records[i], 0, sizeof(records[i]));
        records[i].id = i + 1;
        records[i].length = (i + 1) * 8;
        memset(records[i].data, (int)(i + 0x40), records[i].length);

        records[i].link.next = &records[i].link;
        records[i].link.prev = &records[i].link;
        link_insert_tail(&scheduler.records, &records[i].link);
    }

    for (i = 0; i <= 7 && !link_empty(&scheduler.active); ++i)
        scheduler_step(&scheduler, (i % 3) * 5 + 15);

    scheduler_dump(&scheduler);

    printf("%u %zu %zu\n",
           scheduler.rounds,
           link_count(&scheduler.active),
           link_count(&scheduler.timers));

    while (!link_empty(&scheduler.all_tasks)) {
        Link *node = scheduler.all_tasks.next;
        Task *task =
            (Task *)((char *)node - offsetof(Task, all_link));

        link_remove(&task->all_link);

        if (task->active_link.next != &task->active_link)
            link_remove(&task->active_link);

        free(task);
    }

    return 0;
}