/**
 * @file LinkedList.c
 * @brief 通用单向链表模块的实现 - 提供链表的增删查清等基本操作
 */

#include "common/LinkedList.h"

#include <stdlib.h>

/**
 * @brief 初始化链表
 *
 * 将链表的头尾指针置空，计数器归零。
 *
 * @param list 待初始化的链表指针
 */
void LinkedList_init(LinkedList *list) {
    if (list == 0) {
        return;
    }

    list->head = 0;
    list->tail = 0;
    list->count = 0;
}

/**
 * @brief 向链表尾部追加一个数据元素
 *
 * @param list 链表指针
 * @param data 待追加的数据指针
 * @return 成功返回 1，内存分配失败或参数无效返回 0
 */
int LinkedList_append(LinkedList *list, void *data) {
    LinkedListNode *node = 0;

    if (list == 0) {
        return 0;
    }

    /* 为新节点分配内存 */
    node = (LinkedListNode *)malloc(sizeof(LinkedListNode));
    if (node == 0) {
        return 0; /* 内存分配失败 */
    }

    /* 初始化新节点 */
    node->data = data;
    node->next = 0;

    if (list->tail == 0) {
        /* 链表为空，新节点同时作为头节点和尾节点 */
        list->head = node;
        list->tail = node;
    } else {
        /* 链表非空，将新节点追加到尾部 */
        list->tail->next = node;
        list->tail = node;
    }

    list->count++;
    return 1;
}

/**
 * @brief 查找链表中第一个匹配的数据元素
 *
 * @param list    链表指针
 * @param match   匹配回调函数
 * @param context 传递给匹配函数的上下文参数
 * @return 匹配成功返回数据指针，未找到或参数无效返回 NULL
 */
void *LinkedList_find_first(
    LinkedList *list,
    LinkedListMatchFunction match,
    const void *context
) {
    LinkedListNode *current = 0;

    if (list == 0 || match == 0) {
        return 0;
    }

    /* 从头节点开始逐一检查是否匹配 */
    current = list->head;
    while (current != 0) {
        if (match(current->data, context) != 0) {
            return current->data; /* 找到匹配项，返回数据指针 */
        }

        current = current->next;
    }

    return 0; /* 遍历完毕未找到匹配项 */
}

/**
 * @brief 删除链表中第一个匹配的节点
 *
 * 注意：仅释放节点本身的内存，不释放节点中存储的数据。
 *
 * @param list    链表指针
 * @param match   匹配回调函数
 * @param context 传递给匹配函数的上下文参数
 * @return 成功删除返回 1，未找到匹配项或参数无效返回 0
 */
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
            /* 找到匹配节点，调整链表指针 */
            if (previous == 0) {
                /* 匹配的是头节点，更新头指针 */
                list->head = current->next;
            } else {
                /* 匹配的是中间或尾部节点，跳过当前节点 */
                previous->next = current->next;
            }

            /* 如果删除的是尾节点，更新尾指针 */
            if (list->tail == current) {
                list->tail = previous;
            }

            free(current); /* 释放节点内存 */
            list->count--;
            return 1;
        }

        previous = current;
        current = current->next;
    }

    return 0; /* 未找到匹配项 */
}

/**
 * @brief 清空链表中的所有节点
 *
 * @param list          链表指针
 * @param free_function 数据释放回调函数，可以为 NULL
 */
void LinkedList_clear(LinkedList *list, LinkedListFreeFunction free_function) {
    LinkedListNode *current = 0;

    if (list == 0) {
        return;
    }

    current = list->head;
    while (current != 0) {
        LinkedListNode *next = current->next; /* 先保存下一个节点指针 */

        /* 如果提供了释放函数，先释放节点中的数据 */
        if (free_function != 0) {
            free_function(current->data);
        }

        free(current); /* 释放节点本身 */
        current = next;
    }

    /* 重置链表状态 */
    list->head = 0;
    list->tail = 0;
    list->count = 0;
}

/**
 * @brief 获取链表中的节点数量
 *
 * @param list 链表指针
 * @return 节点数量，参数为 NULL 时返回 0
 */
size_t LinkedList_count(const LinkedList *list) {
    if (list == 0) {
        return 0;
    }

    return list->count;
}
