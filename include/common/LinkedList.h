#ifndef HIS_COMMON_LINKED_LIST_H
#define HIS_COMMON_LINKED_LIST_H

#include <stddef.h>

typedef int (*LinkedListMatchFunction)(const void *data, const void *context);
typedef void (*LinkedListFreeFunction)(void *data);

typedef struct LinkedListNode {
    void *data;
    struct LinkedListNode *next;
} LinkedListNode;

typedef struct LinkedList {
    LinkedListNode *head;
    LinkedListNode *tail;
    size_t count;
} LinkedList;

/* LinkedList stores payload pointers as-is; caller owns payload allocation/freeing. */
void LinkedList_init(LinkedList *list);
int LinkedList_append(LinkedList *list, void *data);
void *LinkedList_find_first(
    LinkedList *list,
    LinkedListMatchFunction match,
    const void *context
);
int LinkedList_remove_first(
    LinkedList *list,
    LinkedListMatchFunction match,
    const void *context
);
void LinkedList_clear(LinkedList *list, LinkedListFreeFunction free_function);
size_t LinkedList_count(const LinkedList *list);

#endif
