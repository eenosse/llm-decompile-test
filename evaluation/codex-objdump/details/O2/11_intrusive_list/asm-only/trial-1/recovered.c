#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct list_node {
    struct list_node *next;
    struct list_node *prev;
};

struct scheduler {
    struct list_node active;
    struct list_node all_tasks;
    struct list_node timers;
    struct list_node buffers;
    unsigned long long clock;
    int dispatches;
};

struct task {
    int id;
    char name[16];
    int state;
    struct list_node active_link;
    struct list_node all_link;
    int priority;
    unsigned long long runtime;
};

struct timer_event;

typedef void (*timer_callback)(struct timer_event *);

struct timer_event {
    unsigned long long deadline;
    struct list_node link;
    timer_callback callback;
    char name[12];
    int firings;
};

struct buffer_record {
    unsigned char data[32];
    int length;
    int id;
    struct list_node link;
};

#define CONTAINER_OF(pointer, type, member) \
    ((type *)((char *)(pointer) - offsetof(type, member)))

static void list_init(struct list_node *head)
{
    head->next = head;
    head->prev = head;
}

static void list_insert_tail(struct list_node *head, struct list_node *node)
{
    node->next = head;
    node->prev = head->prev;
    head->prev->next = node;
    head->prev = node;
}

static void list_remove(struct list_node *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node;
    node->prev = node;
}

static size_t list_count(const struct list_node *head)
{
    const struct list_node *node;
    size_t count = 0;

    for (node = head->next; node != head; node = node->next)
        ++count;

    return count;
}

static void timer_callback_function(struct timer_event *event)
{
    ++event->firings;
    printf("%llu %s %d\n",
           event->deadline, event->name, event->firings);
}

static void add_task(struct scheduler *scheduler,
                     int id,
                     const char *name,
                     int priority)
{
    struct task *task = calloc(1, sizeof(*task));

    if (task == NULL)
        exit(1);

    task->id = id;
    strncpy(task->name, name, 15);
    task->state = 1;
    task->priority = priority;

    list_insert_tail(&scheduler->all_tasks, &task->all_link);
    list_insert_tail(&scheduler->active, &task->active_link);
}

int main(void)
{
    struct scheduler scheduler;
    struct timer_event events[2];
    struct buffer_record buffers[4];
    unsigned iteration;

    list_init(&scheduler.active);
    list_init(&scheduler.all_tasks);
    list_init(&scheduler.timers);
    list_init(&scheduler.buffers);
    scheduler.clock = 1000;
    scheduler.dispatches = 0;

    add_task(&scheduler, 1, "init", 1);
    add_task(&scheduler, 7, "worker", 5);
    add_task(&scheduler, 9, "logger", 3);
    add_task(&scheduler, 12, "io", 9);

    memset(events, 0, sizeof(events));

    events[0].deadline = 1030;
    events[0].callback = timer_callback_function;
    snprintf(events[0].name, sizeof(events[0].name), "tmr%d", 0);
    list_insert_tail(&scheduler.timers, &events[0].link);

    events[1].deadline = 1055;
    events[1].callback = timer_callback_function;
    snprintf(events[1].name, sizeof(events[1].name), "tmr%d", 1);
    list_insert_tail(&scheduler.timers, &events[1].link);

    for (unsigned i = 1; i <= 4; ++i) {
        struct buffer_record *record = &buffers[i - 1];

        memset(record, 0, sizeof(*record));
        record->length = (int)(i * 8);
        record->id = (int)i;
        memset(record->data, (int)(i + 63), (size_t)record->length);
        list_insert_tail(&scheduler.buffers, &record->link);
    }

    for (iteration = 0;
         iteration < 8 && scheduler.active.next != &scheduler.active;
         ++iteration) {
        struct list_node *node;
        struct task *selected = NULL;
        unsigned long long quantum;

        for (node = scheduler.active.next;
             node != &scheduler.active;
             node = node->next) {
            struct task *candidate =
                CONTAINER_OF(node, struct task, active_link);

            if (selected == NULL ||
                (unsigned)candidate->priority >
                    (unsigned)selected->priority)
                selected = candidate;
        }

        quantum = (unsigned long long)((iteration % 3) * 5 + 15);
        selected->state = 2;
        selected->runtime += quantum;
        scheduler.clock += quantum;
        ++scheduler.dispatches;

        printf("%d %s %d %llu %llu\n",
               selected->id,
               selected->name,
               selected->priority,
               selected->runtime,
               scheduler.clock);

        node = scheduler.timers.next;
        while (node != &scheduler.timers) {
            struct list_node *next = node->next;
            struct timer_event *event =
                CONTAINER_OF(node, struct timer_event, link);

            if (event->deadline > scheduler.clock)
                break;

            list_remove(node);
            if (event->callback != NULL)
                event->callback(event);

            event->deadline = scheduler.clock + 40;
            list_insert_tail(&scheduler.timers, node);
            node = next;
        }

        if (selected->runtime > 59) {
            selected->state = 4;
            list_remove(&selected->active_link);
            printf("%d %zu\n",
                   selected->id, list_count(&scheduler.active));
        } else {
            selected->state = 1;
            list_remove(&selected->active_link);
            list_insert_tail(&scheduler.active, &selected->active_link);
        }
    }

    printf("%zu\n", list_count(&scheduler.all_tasks));

    for (struct list_node *node = scheduler.all_tasks.next;
         node != &scheduler.all_tasks;
         node = node->next) {
        struct task *task = CONTAINER_OF(node, struct task, all_link);

        printf("%d %s %d %d %llu\n",
               task->id,
               task->name,
               task->state,
               task->priority,
               task->runtime);
    }

    printf("%zu\n", list_count(&scheduler.buffers));

    for (struct list_node *node = scheduler.buffers.next;
         node != &scheduler.buffers;
         node = node->next) {
        struct buffer_record *record =
            CONTAINER_OF(node, struct buffer_record, link);

        printf("%d %d %c\n",
               record->id, record->length, record->data[0]);
    }

    printf("%d %zu %zu\n",
           scheduler.dispatches,
           list_count(&scheduler.active),
           list_count(&scheduler.timers));

    while (scheduler.all_tasks.next != &scheduler.all_tasks) {
        struct task *task =
            CONTAINER_OF(scheduler.all_tasks.next, struct task, all_link);

        list_remove(&task->all_link);
        if (task->active_link.next != &task->active_link)
            list_remove(&task->active_link);
        free(task);
    }

    return 0;
}