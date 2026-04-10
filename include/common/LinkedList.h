/**
 * @file LinkedList.h
 * @brief 通用单向链表模块 - 提供链表的初始化、追加、查找、删除和清空操作
 *
 * 本模块实现了一个通用的单向链表数据结构，使用 void* 指针存储数据，
 * 支持自定义匹配函数和释放函数，适用于各种数据类型的链表管理。
 * 注意：链表只存储数据指针，数据本身的分配和释放由调用者负责。
 */

#ifndef HIS_COMMON_LINKED_LIST_H
#define HIS_COMMON_LINKED_LIST_H

#include <stddef.h>

/**
 * @brief 链表匹配回调函数类型
 *
 * 用于在查找和删除操作中判断节点数据是否匹配。
 *
 * @param data    当前节点存储的数据指针
 * @param context 调用者传入的匹配上下文（如搜索关键字）
 * @return 非零表示匹配，零表示不匹配
 */
typedef int (*LinkedListMatchFunction)(const void *data, const void *context);

/**
 * @brief 链表数据释放回调函数类型
 *
 * 用于在清空链表时释放每个节点所存储的数据。
 *
 * @param data 待释放的数据指针
 */
typedef void (*LinkedListFreeFunction)(void *data);

/**
 * @brief 链表节点结构体
 */
typedef struct LinkedListNode {
    void *data;                  /**< 指向节点所存储数据的指针 */
    struct LinkedListNode *next; /**< 指向下一个节点的指针 */
} LinkedListNode;

/**
 * @brief 链表结构体
 */
typedef struct LinkedList {
    LinkedListNode *head; /**< 指向链表头节点的指针 */
    LinkedListNode *tail; /**< 指向链表尾节点的指针 */
    size_t count;         /**< 链表中节点的数量 */
} LinkedList;

/**
 * @brief 初始化链表
 *
 * 将链表的头尾指针置空，计数器归零。
 * 注意：链表只存储数据指针，数据本身的分配和释放由调用者负责。
 *
 * @param list 待初始化的链表指针
 */
void LinkedList_init(LinkedList *list);

/**
 * @brief 向链表尾部追加一个数据元素
 *
 * 分配新节点并将数据指针存入节点，追加到链表末尾。
 *
 * @param list 链表指针
 * @param data 待追加的数据指针
 * @return 成功返回 1，内存分配失败或参数无效返回 0
 */
int LinkedList_append(LinkedList *list, void *data);

/**
 * @brief 查找链表中第一个匹配的数据元素
 *
 * 从头节点开始依次调用匹配函数，返回第一个匹配的数据指针。
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
);

/**
 * @brief 删除链表中第一个匹配的节点
 *
 * 从头节点开始查找，删除第一个匹配的节点并释放节点内存。
 * 注意：不会释放节点所存储的数据，数据的释放由调用者负责。
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
);

/**
 * @brief 清空链表中的所有节点
 *
 * 遍历并释放所有节点。如果提供了 free_function，
 * 则在释放节点前先调用它来释放节点中的数据。
 *
 * @param list          链表指针
 * @param free_function 数据释放回调函数，可以为 NULL（表示不释放数据）
 */
void LinkedList_clear(LinkedList *list, LinkedListFreeFunction free_function);

/**
 * @brief 获取链表中的节点数量
 *
 * @param list 链表指针
 * @return 节点数量，参数为 NULL 时返回 0
 */
size_t LinkedList_count(const LinkedList *list);

#endif
