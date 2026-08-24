/*
 * 07_singly_linked_list.c
 * Target feature: classic singly linked list with a separate list-header
 * struct, a struct payload per node, tail pointer, in-place reversal,
 * merge sort over links, and pointer-to-pointer removal idiom.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "result_only/output_mode.h"

typedef struct Employee {
    uint32_t id;
    char     name[20];
    char     dept[12];
    uint32_t salary;
    uint16_t years;
} Employee;

typedef struct ListNode {
    Employee         data;
    struct ListNode *next;
} ListNode;

typedef struct EmployeeList {
    ListNode *head;
    ListNode *tail;
    size_t    count;
    uint64_t  salary_total;
} EmployeeList;

static void list_init(EmployeeList *l)
{
    l->head = NULL;
    l->tail = NULL;
    l->count = 0;
    l->salary_total = 0;
}

static ListNode *node_new(uint32_t id, const char *name, const char *dept,
                          uint32_t salary, uint16_t years)
{
    ListNode *n = (ListNode *)calloc(1, sizeof(ListNode));
    if (!n)
        exit(1);
    n->data.id = id;
    strncpy(n->data.name, name, sizeof(n->data.name) - 1);
    strncpy(n->data.dept, dept, sizeof(n->data.dept) - 1);
    n->data.salary = salary;
    n->data.years  = years;
    return n;
}

static void list_push_back(EmployeeList *l, ListNode *n)
{
    n->next = NULL;
    if (l->tail)
        l->tail->next = n;
    else
        l->head = n;
    l->tail = n;
    l->count++;
    l->salary_total += n->data.salary;
}

static void list_push_front(EmployeeList *l, ListNode *n)
{
    n->next = l->head;
    l->head = n;
    if (!l->tail)
        l->tail = n;
    l->count++;
    l->salary_total += n->data.salary;
}

static ListNode *list_find(EmployeeList *l, uint32_t id)
{
    ListNode *p;
    for (p = l->head; p; p = p->next)
        if (p->data.id == id)
            return p;
    return NULL;
}

/* pointer-to-pointer removal: no special case for the head node */
static int list_remove_dept(EmployeeList *l, const char *dept)
{
    ListNode **pp = &l->head;
    int removed = 0;

    while (*pp) {
        ListNode *cur = *pp;
        if (strcmp(cur->data.dept, dept) == 0) {
            *pp = cur->next;
            l->count--;
            l->salary_total -= cur->data.salary;
            free(cur);
            removed++;
        } else {
            pp = &cur->next;
        }
    }
    l->tail = NULL;
    for (pp = &l->head; *pp; pp = &(*pp)->next)
        l->tail = *pp;
    return removed;
}

static void list_reverse(EmployeeList *l)
{
    ListNode *prev = NULL;
    ListNode *cur  = l->head;

    l->tail = l->head;
    while (cur) {
        ListNode *next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    l->head = prev;
}

static ListNode *merge_sorted(ListNode *a, ListNode *b)
{
    ListNode dummy;
    ListNode *t = &dummy;

    dummy.next = NULL;
    while (a && b) {
        if (a->data.salary >= b->data.salary) {
            t->next = a;
            a = a->next;
        } else {
            t->next = b;
            b = b->next;
        }
        t = t->next;
    }
    t->next = a ? a : b;
    return dummy.next;
}

static ListNode *merge_sort(ListNode *head)
{
    ListNode *slow, *fast, *mid;

    if (!head || !head->next)
        return head;
    slow = head;
    fast = head->next;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    mid = slow->next;
    slow->next = NULL;
    return merge_sorted(merge_sort(head), merge_sort(mid));
}

static void list_sort_by_salary(EmployeeList *l)
{
    ListNode *p;
    l->head = merge_sort(l->head);
    for (p = l->head; p && p->next; p = p->next)
        ;
    l->tail = p;
}

static void list_dump(const EmployeeList *l, const char *label)
{
    const ListNode *p;
    BENCH_VERBOSE_ARG(label);
    BENCH_OUTPUT(
        printf("-- %s (count=%zu, payroll=%llu) --\n", label, l->count,
               (unsigned long long)l->salary_total),
        printf("%zu %llu\n", l->count, (unsigned long long)l->salary_total));
    for (p = l->head; p; p = p->next)
        BENCH_OUTPUT(
            printf("  #%u %-18s %-10s $%u (%u yr)\n", p->data.id,
                   p->data.name, p->data.dept, p->data.salary, p->data.years),
            printf("%u|%s|%s|%u|%u\n", p->data.id, p->data.name,
                   p->data.dept, p->data.salary, p->data.years));
    BENCH_OUTPUT(
        printf("  head=%s tail=%s\n",
               l->head ? l->head->data.name : "(nil)",
               l->tail ? l->tail->data.name : "(nil)"),
        printf("%u %u\n", l->head ? l->head->data.id : 0,
               l->tail ? l->tail->data.id : 0));
}

static void list_free(EmployeeList *l)
{
    ListNode *p = l->head;
    while (p) {
        ListNode *n = p->next;
        free(p);
        p = n;
    }
    list_init(l);
}

int main(void)
{
    EmployeeList list;
    ListNode *hit;

    list_init(&list);
    list_push_back(&list, node_new(101, "Ada Lovelace",  "research", 145000, 9));
    list_push_back(&list, node_new(102, "Grace Hopper",  "compiler", 162000, 14));
    list_push_back(&list, node_new(103, "Ken Thompson",  "kernel",   158000, 12));
    list_push_back(&list, node_new(104, "Barbara Liskov","research", 151000, 11));
    list_push_front(&list,node_new(100, "Alan Turing",   "research", 170000, 20));
    list_push_back(&list, node_new(105, "Dennis Ritchie","compiler", 149000, 13));

    list_dump(&list, "initial");

    hit = list_find(&list, 103);
    if (hit)
        BENCH_OUTPUT(printf("found #%u -> %s (%s)\n", hit->data.id,
                           hit->data.name, hit->data.dept),
                     printf("%u|%s|%s\n", hit->data.id,
                            hit->data.name, hit->data.dept));

    list_sort_by_salary(&list);
    list_dump(&list, "sorted by salary desc");

    list_reverse(&list);
    list_dump(&list, "reversed");

    {
        int removed = list_remove_dept(&list, "research");
        BENCH_OUTPUT(printf("removed %d from research\n", removed),
                     printf("%d\n", removed));
    }
    list_dump(&list, "after removal");

    list_free(&list);
    return 0;
}
