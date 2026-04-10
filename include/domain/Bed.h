/**
 * @file Bed.h
 * @brief 床位领域模型头文件
 *
 * 定义了床位(Bed)的数据结构、状态枚举及操作函数。
 * 床位属于病房，用于住院管理，支持分配、释放和维护等操作。
 */

#ifndef HIS_DOMAIN_BED_H
#define HIS_DOMAIN_BED_H

#include "domain/DomainTypes.h"

/**
 * @brief 床位状态枚举
 */
typedef enum BedStatus {
    BED_STATUS_AVAILABLE = 0,    /* 空闲可用状态 */
    BED_STATUS_OCCUPIED = 1,     /* 已占用状态（有患者入住） */
    BED_STATUS_MAINTENANCE = 2   /* 维护中状态（暂停使用） */
} BedStatus;

/**
 * @brief 床位信息结构体
 *
 * 存储床位的基本信息、当前状态及关联的入院记录。
 */
typedef struct Bed {
    char bed_id[HIS_DOMAIN_ID_CAPACITY];                /* 床位唯一标识ID */
    char ward_id[HIS_DOMAIN_ID_CAPACITY];               /* 所属病房ID */
    char room_no[HIS_DOMAIN_TEXT_CAPACITY];              /* 房间号 */
    char bed_no[HIS_DOMAIN_TEXT_CAPACITY];               /* 床位号 */
    BedStatus status;                                     /* 床位当前状态 */
    char current_admission_id[HIS_DOMAIN_ID_CAPACITY];  /* 当前入院记录ID（占用时有效） */
    char occupied_at[HIS_DOMAIN_TIME_CAPACITY];          /* 床位被占用的时间 */
    char released_at[HIS_DOMAIN_TIME_CAPACITY];          /* 床位被释放的时间 */
} Bed;

/**
 * @brief 检查床位是否可分配
 *
 * 床位状态为"空闲"且当前无关联入院记录时可分配。
 *
 * @param bed 指向床位的常量指针
 * @return int 可分配返回1，不可分配返回0
 */
int Bed_is_assignable(const Bed *bed);

/**
 * @brief 为床位分配患者（入院）
 *
 * 将床位状态设为"已占用"，并记录入院ID和占用时间。
 *
 * @param bed         指向床位的指针
 * @param admission_id 入院记录ID
 * @param occupied_at  占用时间字符串
 * @return int 成功返回1，失败返回0（参数无效或床位不可分配）
 */
int Bed_assign(Bed *bed, const char *admission_id, const char *occupied_at);

/**
 * @brief 释放床位（出院/转床）
 *
 * 将床位状态恢复为"空闲"，清除入院关联信息。
 *
 * @param bed         指向床位的指针
 * @param released_at 释放时间字符串
 * @return int 成功返回1，失败返回0（参数无效或床位未占用）
 */
int Bed_release(Bed *bed, const char *released_at);

/**
 * @brief 将床位标记为维护状态
 *
 * 仅当床位未被占用时才可进入维护状态。
 *
 * @param bed 指向床位的指针
 * @return int 成功返回1，失败返回0（床位已占用或参数无效）
 */
int Bed_mark_maintenance(Bed *bed);

/**
 * @brief 将床位从维护状态恢复为可用状态
 *
 * 仅当床位处于维护状态时才可恢复。
 *
 * @param bed 指向床位的指针
 * @return int 成功返回1，失败返回0（床位非维护状态或参数无效）
 */
int Bed_mark_available(Bed *bed);

#endif
