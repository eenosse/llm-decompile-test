#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Record Record;

struct Record {
    int32_t identifier;
    char name[20];
    char category[12];
    uint32_t value;
    uint16_t quantity;
    Record *next;
};

typedef struct {
    Record *head;
    Record *tail;
    unsigned long count;
    unsigned long total;
} RecordList;

static void list_init(RecordList *list)
{
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    list->total = 0;
}

static Record *record_create(int32_t identifier, const char *name,
                             const char *category, uint32_t value,
                             uint16_t quantity)
{
    Record *record = calloc(1, sizeof(*record));

    if (record == NULL)
        exit(1);

    record->identifier = identifier;
    strncpy(record->name, name, 19);
    strncpy(record->category, category, 11);
    record->value = value;
    record->quantity = quantity;

    return record;
}

static void list_append(RecordList *list, Record *record)
{
    record->next = NULL;

    if (list->tail != NULL)
        list->tail->next = record;
    else
        list->head = record;

    list->tail = record;
    ++list->count;
    list->total += record->value;
}

static void list_prepend(RecordList *list, Record *record)
{
    record->next = list->head;
    list->head = record;

    if (list->tail == NULL)
        list->tail = record;

    ++list->count;
    list->total += record->value;
}

static Record *list_find(const RecordList *list, int32_t identifier)
{
    Record *record = list->head;

    while (record != NULL) {
        if (record->identifier == identifier)
            return record;
        record = record->next;
    }

    return NULL;
}

static int list_remove_category(RecordList *list, const char *category)
{
    Record **link = &list->head;
    int removed = 0;

    while (*link != NULL) {
        Record *record = *link;

        if (strcmp(record->category, category) == 0) {
            *link = record->next;
            --list->count;
            list->total -= record->value;
            free(record);
            ++removed;
        } else {
            link = &record->next;
        }
    }

    list->tail = NULL;
    link = &list->head;

    while (*link != NULL) {
        list->tail = *link;
        link = &(*link)->next;
    }

    return removed;
}

static void list_reverse(RecordList *list)
{
    Record *previous = NULL;
    Record *current = list->head;

    list->tail = list->head;

    while (current != NULL) {
        Record *next = current->next;
        current->next = previous;
        previous = current;
        current = next;
    }

    list->head = previous;
}

static Record *merge_records(Record *left, Record *right)
{
    Record dummy = {0};
    Record *tail = &dummy;

    while (left != NULL && right != NULL) {
        if (left->value >= right->value) {
            tail->next = left;
            left = left->next;
        } else {
            tail->next = right;
            right = right->next;
        }
        tail = tail->next;
    }

    tail->next = left != NULL ? left : right;
    return dummy.next;
}

static Record *merge_sort_records(Record *head)
{
    Record *slow;
    Record *fast;
    Record *right;

    if (head == NULL || head->next == NULL)
        return head;

    slow = head;
    fast = head->next;

    while (fast != NULL && fast->next != NULL) {
        slow = slow->next;
        fast = fast->next->next;
    }

    right = slow->next;
    slow->next = NULL;

    return merge_records(merge_sort_records(head),
                         merge_sort_records(right));
}

static void list_sort(RecordList *list)
{
    Record *record;

    list->head = merge_sort_records(list->head);
    record = list->head;

    while (record != NULL && record->next != NULL)
        record = record->next;

    list->tail = record;
}

static void list_print(const RecordList *list, const char *unused_heading)
{
    Record *record;

    (void)unused_heading;

    printf("%lu %lu\n", list->count, list->total);

    for (record = list->head; record != NULL; record = record->next) {
        printf("%d %s %s %u %u\n",
               (int)record->identifier,
               record->name,
               record->category,
               (unsigned)record->value,
               (unsigned)record->quantity);
    }

    printf("%d %d\n",
           list->head != NULL ? (int)list->head->identifier : 0,
           list->tail != NULL ? (int)list->tail->identifier : 0);
}

static void list_destroy(RecordList *list)
{
    Record *record = list->head;

    while (record != NULL) {
        Record *next = record->next;
        free(record);
        record = next;
    }

    list_init(list);
}

int main(void)
{
    static const char category_a[] = "group_a";
    static const char category_b[] = "group_b";
    static const char category_c[] = "group_c";

    RecordList list;
    Record *found;
    int removed;

    list_init(&list);

    list_append(&list, record_create(101, "item_101", category_a,
                                     145000U, 9U));
    list_append(&list, record_create(102, "item_102", category_b,
                                     162000U, 14U));
    list_append(&list, record_create(103, "item_103", category_c,
                                     158000U, 12U));
    list_append(&list, record_create(104, "item_104", category_a,
                                     151000U, 11U));
    list_prepend(&list, record_create(100, "item_100", category_a,
                                      170000U, 20U));
    list_append(&list, record_create(105, "item_105", category_b,
                                     149000U, 13U));

    list_print(&list, "stage_1");

    found = list_find(&list, 103);
    if (found != NULL) {
        printf("%d %s %s\n",
               (int)found->identifier,
               found->name,
               found->category);
    }

    list_sort(&list);
    list_print(&list, "stage_2");

    list_reverse(&list);
    list_print(&list, "stage_3");

    removed = list_remove_category(&list, category_a);
    printf("%d\n", removed);

    list_print(&list, "stage_4");
    list_destroy(&list);

    return 0;
}