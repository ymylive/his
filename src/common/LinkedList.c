#include "common/LinkedList.h"

#include <stdlib.h>

void LinkedList_init(LinkedList *list) {
    if (list == 0) {
        return;
    }

    list->head = 0;
    list->tail = 0;
    list->count = 0;
}

int LinkedList_append(LinkedList *list, void *data) {
    LinkedListNode *node = 0;

    if (list == 0) {
        return 0;
    }

    node = (LinkedListNode *)malloc(sizeof(LinkedListNode));
    if (node == 0) {
        return 0;
    }

    node->data = data;
    node->next = 0;

    if (list->tail == 0) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }

    list->count++;
    return 1;
}

void *LinkedList_find_first(
    LinkedList *list,
    LinkedListMatchFunction match,
    const void *context
) {
    LinkedListNode *current = 0;

    if (list == 0 || match == 0) {
        return 0;
    }

    current = list->head;
    while (current != 0) {
        if (match(current->data, context) != 0) {
            return current->data;
        }

        current = current->next;
    }

    return 0;
}

int LinkedList_remove_first(
    LinkedList *list,
    LinkedListMatchFunction match,
    const void *context
) {
    LinkedListNode *current = 0;
    LinkedListNode *previous = 0;

    if (list == 0 || match == 0) {
        return 0;
    }

    current = list->head;
    while (current != 0) {
        if (match(current->data, context) != 0) {
            if (previous == 0) {
                list->head = current->next;
            } else {
                previous->next = current->next;
            }

            if (list->tail == current) {
                list->tail = previous;
            }

            free(current);
            list->count--;
            return 1;
        }

        previous = current;
        current = current->next;
    }

    return 0;
}

void LinkedList_clear(LinkedList *list, LinkedListFreeFunction free_function) {
    LinkedListNode *current = 0;

    if (list == 0) {
        return;
    }

    current = list->head;
    while (current != 0) {
        LinkedListNode *next = current->next;

        if (free_function != 0) {
            free_function(current->data);
        }

        free(current);
        current = next;
    }

    list->head = 0;
    list->tail = 0;
    list->count = 0;
}

size_t LinkedList_count(const LinkedList *list) {
    if (list == 0) {
        return 0;
    }

    return list->count;
}
