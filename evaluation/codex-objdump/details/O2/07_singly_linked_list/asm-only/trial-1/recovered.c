#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Record Record;

struct Record {
    int32_t id;
    char name[20];
    char category[12];
    uint32_t value;
    uint16_t quantity;
    Record *next;
};

typedef struct {
    Record *head;
    Record *tail;
    size_t count;
    size_t total;
} RecordList;

static Record *create_record(int32_t id, const char *name,
                             const char *category, uint32_t value,
                             uint16_t quantity)
{
    Record *record = calloc(1, sizeof(*record));

    if (record == NULL)
        exit(1);

    record->id = id;
    strncpy(record->name, name, sizeof(record->name) - 1);
    strncpy(record->category, category, sizeof(record->category) - 1);
    record->value = value;
    record->quantity = quantity;

    return record;
}

static void append_record(RecordList *list, Record *record)
{
    record->next = NULL;

    if (list->tail != NULL)
        list->tail->next = record;
    else
        list->head = record;

    list->count++;
    list->total += record->value;
    list->tail = record;
}

static void print_list(const RecordList *list)
{
    const Record *record;

    printf("%zu %zu\n", list->count, list->total);

    for (record = list->head; record != NULL; record = record->next) {
        printf("%d %s %s %u %u\n",
               record->id,
               record->name,
               record->category,
               record->value,
               (unsigned int)record->quantity);
    }

    printf("%d %d\n",
           list->head != NULL ? list->head->id : 0,
           list->tail != NULL ? list->tail->id : 0);
}

static Record *merge_sorted(Record *left, Record *right)
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

static Record *merge_sort(Record *head)
{
    Record *slow;
    Record *fast;
    Record *second;
    Record *right;
    Record *left;

    if (head == NULL || head->next == NULL)
        return head;

    slow = head;
    fast = head->next;

    while (fast != NULL) {
        fast = fast->next;
        if (fast != NULL) {
            slow = slow->next;
            fast = fast->next;
        }
    }

    second = slow->next;
    slow->next = NULL;

    right = merge_sort(second);
    left = merge_sort(head);
    return merge_sorted(left, right);
}

int main(void)
{
    static const char category_a[] = "group_a";
    static const char category_b[] = "group_b";
    static const char category_c[] = "set_c";

    RecordList list = {0};
    Record *record;
    Record *previous;
    Record *next;
    Record **link;
    unsigned int removed = 0;

    append_record(&list,
                  create_record(101, "record_101", category_a, 145000, 9));
    append_record(&list,
                  create_record(102, "record_102", category_b, 162000, 14));
    append_record(&list,
                  create_record(103, "record_103", category_c, 158000, 12));
    append_record(&list,
                  create_record(104, "record_104", category_a, 151000, 11));

    record = create_record(100, "record_100", category_a, 170000, 20);
    record->next = list.head;
    list.head = record;
    if (list.tail == NULL)
        list.tail = record;
    list.count++;
    list.total += record->value;

    append_record(&list,
                  create_record(105, "record_105", category_b, 149000, 13));

    print_list(&list);

    for (record = list.head; record != NULL; record = record->next) {
        if (record->id == 103) {
            printf("%d %s %s\n",
                   record->id, record->name, record->category);
            break;
        }
    }

    list.head = merge_sort(list.head);
    list.tail = NULL;
    for (record = list.head; record != NULL; record = record->next)
        list.tail = record;

    print_list(&list);

    list.tail = list.head;
    record = list.head;
    previous = NULL;

    while (record != NULL) {
        next = record->next;
        record->next = previous;
        previous = record;

        if (next == NULL)
            break;

        record = next;
    }

    list.head = record;
    print_list(&list);

    link = &list.head;
    record = list.head;

    while (record != NULL) {
        if (strcmp(record->category, category_a) != 0) {
            link = &record->next;
            record = record->next;
        } else {
            *link = record->next;
            list.count--;
            list.total -= record->value;
            free(record);
            removed++;
            record = *link;
        }
    }

    list.tail = NULL;
    for (record = list.head; record != NULL; record = record->next)
        list.tail = record;

    printf("%u\n", removed);
    print_list(&list);

    record = list.head;
    while (record != NULL) {
        next = record->next;
        free(record);
        record = next;
    }

    return 0;
}