#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Record Record;

struct Record {
    int32_t identifier;
    char name[20];
    char group[12];
    uint32_t value;
    uint16_t quantity;
    Record *next;
};

typedef struct {
    Record *head;
    Record *tail;
    unsigned long count;
    unsigned long total_value;
} RecordList;

static void append_record(RecordList *list, Record *record)
{
    record->next = NULL;

    if (list->tail != NULL)
        list->tail->next = record;
    else
        list->head = record;

    list->tail = record;
    ++list->count;
    list->total_value += record->value;
}

static Record *sort_records(Record *head)
{
    Record *slow;
    Record *fast;
    Record *right;
    Record *left;
    Record dummy = {0};
    Record *tail = &dummy;

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

    right = sort_records(right);
    left = sort_records(head);

    while (left != NULL && right != NULL) {
        if (left->value < right->value) {
            tail->next = right;
            right = right->next;
        } else {
            tail->next = left;
            left = left->next;
        }
        tail = tail->next;
    }

    tail->next = left != NULL ? left : right;
    return dummy.next;
}

static Record *create_record(int32_t identifier,
                             const char *name,
                             const char *group,
                             uint32_t value,
                             uint16_t quantity)
{
    Record *record = calloc(1, sizeof(*record));

    if (record == NULL)
        exit(1);

    record->identifier = identifier;
    strncpy(record->name, name, 19);
    strncpy(record->group, group, 11);
    record->value = value;
    record->quantity = quantity;

    return record;
}

static void print_list(const RecordList *list, const char *unused_label)
{
    const Record *record;
    int32_t first_identifier;
    int32_t last_identifier;

    (void)unused_label;

    printf("%lu %lu\n", list->count, list->total_value);

    for (record = list->head; record != NULL; record = record->next) {
        printf("%d %s %s %u %u\n",
               record->identifier,
               record->name,
               record->group,
               record->value,
               (unsigned int)record->quantity);
    }

    first_identifier = list->head != NULL ? list->head->identifier : 0;
    last_identifier = list->tail != NULL ? list->tail->identifier : 0;
    printf("%d %d\n", first_identifier, last_identifier);
}

int main(void)
{
    static const char group_1[] = "group_01";
    static const char group_2[] = "group_02";
    static const char group_3[] = "grp_03";

    RecordList list = {0};
    Record *record;
    Record *current;
    Record *next;
    Record **link;
    int removed;

    append_record(&list, create_record(101, "item_alpha01",
                                       group_1, 145000, 9));
    append_record(&list, create_record(102, "item_beta002",
                                       group_2, 162000, 14));
    append_record(&list, create_record(103, "item_gamma03",
                                       group_3, 158000, 12));
    append_record(&list, create_record(104, "item_delta0001",
                                       group_1, 151000, 11));

    record = create_record(100, "item_zero01", group_1, 170000, 20);
    record->next = list.head;
    list.head = record;
    if (list.tail == NULL)
        list.tail = record;
    ++list.count;
    list.total_value += record->value;

    append_record(&list, create_record(105, "item_epsilon01",
                                       group_2, 149000, 13));

    print_list(&list, "initial");

    for (record = list.head; record != NULL; record = record->next) {
        if (record->identifier == 103) {
            printf("%d %s %s\n", 103, record->name, record->group);
            break;
        }
    }

    list.head = sort_records(list.head);
    list.tail = list.head;
    while (list.tail != NULL && list.tail->next != NULL)
        list.tail = list.tail->next;

    print_list(&list, "after sorting records");

    list.tail = list.head;
    current = list.head;
    record = NULL;

    while (current != NULL) {
        next = current->next;
        current->next = record;
        record = current;
        current = next;
    }

    list.head = record;
    print_list(&list, "reversed");

    removed = 0;
    link = &list.head;

    while (*link != NULL) {
        record = *link;

        if (strcmp(record->group, group_1) == 0) {
            *link = record->next;
            --list.count;
            list.total_value -= record->value;
            free(record);
            ++removed;
        } else {
            link = &record->next;
        }
    }

    list.tail = NULL;
    for (record = list.head; record != NULL; record = record->next)
        list.tail = record;

    printf("%d\n", removed);
    print_list(&list, "after removal");

    current = list.head;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }

    return 0;
}