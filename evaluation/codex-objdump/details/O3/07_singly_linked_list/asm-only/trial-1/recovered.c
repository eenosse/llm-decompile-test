#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Item Item;

struct Item {
    int32_t id;
    char name[20];
    char category[12];
    uint32_t value;
    uint16_t amount;
    Item *next;
};

typedef struct {
    Item *head;
    Item *tail;
    uint64_t count;
    uint64_t total;
} Collection;

_Static_assert(offsetof(Item, id) == 0, "unexpected layout");
_Static_assert(offsetof(Item, name) == 4, "unexpected layout");
_Static_assert(offsetof(Item, category) == 24, "unexpected layout");
_Static_assert(offsetof(Item, value) == 36, "unexpected layout");
_Static_assert(offsetof(Item, amount) == 40, "unexpected layout");
_Static_assert(offsetof(Item, next) == 48, "unexpected layout");
_Static_assert(sizeof(Item) == 56, "unexpected layout");

static const char category_a[] = "kind0001";
static const char category_b[] = "kind0002";
static const char category_c[] = "kind03";

static const char name_101[] = "item-101-001";
static const char name_102[] = "item-102-001";
static const char name_103[] = "item-103-001";
static const char name_104[] = "item-104-00001";
static const char name_100[] = "item-100-01";
static const char name_105[] = "item-105-00001";

static Item *new_item(int32_t id, const char *name, const char *category,
                      uint32_t value, uint16_t amount)
{
    Item *item = calloc(1, sizeof(*item));

    if (item == NULL)
        exit(1);

    item->id = id;
    strncpy(item->name, name, 19);
    strncpy(item->category, category, 11);
    item->value = value;
    item->amount = amount;
    return item;
}

static Item *merge_descending(Item *left, Item *right)
{
    Item dummy = {0};
    Item *tail = &dummy;

    while (left != NULL && right != NULL) {
        if (left->value >= right->value) {
            tail->next = left;
            tail = left;
            left = left->next;
        } else {
            tail->next = right;
            tail = right;
            right = right->next;
        }
    }

    tail->next = left != NULL ? left : right;
    return dummy.next;
}

static Item *sort_descending(Item *head)
{
    Item *slow;
    Item *fast;
    Item *right;
    Item *sorted_right;
    Item *sorted_left;

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

    right = slow->next;
    slow->next = NULL;

    sorted_right = sort_descending(right);
    sorted_left = sort_descending(head);
    return merge_descending(sorted_left, sorted_right);
}

static void print_collection(const Collection *collection)
{
    const Item *item;

    printf("%lu, %lu\n",
           (unsigned long)collection->count,
           (unsigned long)collection->total);

    for (item = collection->head; item != NULL; item = item->next) {
        printf("%d %s %s %u %u\n",
               item->id,
               item->name,
               item->category,
               item->value,
               (unsigned int)item->amount);
    }

    printf("%d %d\n",
           collection->head != NULL ? collection->head->id : 0,
           collection->tail != NULL ? collection->tail->id : 0);
}

int main(void)
{
    Item *item_101;
    Item *item_102;
    Item *item_103;
    Item *item_104;
    Item *item_100;
    Item *item_105;
    Item *item;
    Item **link;
    Item *previous;
    Item *next;
    Collection collection;
    uint32_t removed = 0;
    uint64_t total;

    item_101 = new_item(101, name_101, category_a, 145000, 9);
    item_101->next = NULL;
    total = item_101->value;

    item_102 = new_item(102, name_102, category_b, 162000, 14);
    item_102->next = NULL;
    item_101->next = item_102;
    total += item_102->value;

    item_103 = new_item(103, name_103, category_c, 158000, 12);
    item_103->next = NULL;
    item_102->next = item_103;
    total += item_103->value;

    item_104 = new_item(104, name_104, category_a, 150000, 11);
    item_104->next = NULL;
    item_103->next = item_104;
    total += item_104->value;

    item_100 = new_item(100, name_100, category_a, 170000, 20);
    item_100->next = item_101;
    item_104->next = item_100;
    total += item_100->value;

    item_105 = new_item(105, name_105, category_b, 149000, 13);
    item_105->next = NULL;
    item_100->next = item_105;
    total += item_105->value;

    collection.head = item_100;
    collection.tail = item_105;
    collection.count = 6;
    collection.total = total;

    print_collection(&collection);

    for (item = collection.head; item != NULL; item = item->next) {
        if (item->id == 103) {
            printf("%d %s %s\n", 103, item->name, item->category);
            break;
        }
    }

    collection.head = sort_descending(collection.head);
    collection.tail = NULL;
    for (item = collection.head; item != NULL; item = item->next)
        collection.tail = item;

    print_collection(&collection);

    collection.tail = collection.head;
    previous = NULL;
    item = collection.head;

    while (item != NULL) {
        next = item->next;
        item->next = previous;
        previous = item;
        if (next == NULL)
            break;
        item = next;
    }

    collection.head = item;
    print_collection(&collection);

    link = &collection.head;
    item = collection.head;

    while (item != NULL) {
        if (strcmp(item->category, category_a) == 0) {
            *link = item->next;
            collection.count--;
            collection.total -= item->value;
            free(item);
            removed++;
            item = *link;
        } else {
            link = &item->next;
            item = item->next;
        }
    }

    collection.tail = NULL;
    for (item = collection.head; item != NULL; item = item->next)
        collection.tail = item;

    printf("%d\n", (int)removed);
    print_collection(&collection);

    item = collection.head;
    while (item != NULL) {
        next = item->next;
        free(item);
        item = next;
    }

    return 0;
}