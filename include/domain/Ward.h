/**
 * @file Ward.h
 * @brief 病房领域模型头文件
 *
 * 定义了病房(Ward)的数据结构、状态枚举及操作函数。
 * 病房隶属于科室，包含多个床位，负责住院患者的空间管理。
 */

#ifndef HIS_DOMAIN_WARD_H
#define HIS_DOMAIN_WARD_H

#include "domain/DomainTypes.h"

/**
 * @brief 病房状态枚举
 */
typedef enum WardStatus {
    WARD_STATUS_ACTIVE = 0,  /* 正常开放状态 */
    WARD_STATUS_CLOSED = 1   /* 关闭状态（不接收患者） */
} WardStatus;

/**
 * @brief 病房类型枚举
 */
typedef enum WardType {
    WARD_TYPE_STANDARD = 0,   /* 普通病房 */
    WARD_TYPE_DOUBLE = 1,     /* 双人间 */
    WARD_TYPE_VIP = 2         /* VIP单人间 */
} WardType;

/**
 * @brief 病房信息结构体
 *
 * 存储病房的基本信息、容量、已占用床位数及运行状态。
 */
typedef struct Ward {
    char ward_id[HIS_DOMAIN_ID_CAPACITY];          /* 病房唯一标识ID */
    char name[HIS_DOMAIN_NAME_CAPACITY];            /* 病房名称 */
    char department_id[HIS_DOMAIN_ID_CAPACITY];    /* 所属科室ID */
    char location[HIS_DOMAIN_LOCATION_CAPACITY];   /* 病房位置（如住院楼5层） */
    int capacity;                                    /* 病房总床位容量 */
    int occupied_beds;                               /* 当前已占用床位数 */
    WardStatus status;                               /* 病房当前状态（开放/关闭） */
    WardType ward_type;                              /* 病房类型 */
    double daily_fee;                                /* 每日床位费(元) */
} Ward;

/**
 * @brief 检查病房是否还有剩余床位容量
 *
 * 病房必须处于开放状态且已占用数小于总容量时才有剩余。
 *
 * @param ward 指向病房的常量指针
 * @return int 有剩余容量返回1，无剩余返回0
 */
int Ward_has_capacity(const Ward *ward);

/**
 * @brief 占用一个床位（已占用数加1）
 *
 * 仅当病房有剩余容量时才可占用。
 *
 * @param ward 指向病房的指针
 * @return int 成功返回1，失败返回0（无剩余容量或参数无效）
 */
int Ward_occupy_bed(Ward *ward);

/**
 * @brief 释放一个床位（已占用数减1）
 *
 * 仅当已占用床位数大于0时才可释放。
 *
 * @param ward 指向病房的指针
 * @return int 成功返回1，失败返回0（无已占用床位或参数无效）
 */
int Ward_release_bed(Ward *ward);

/**
 * @brief 关闭病房
 *
 * 仅当病房处于开放状态且无患者占用时才可关闭。
 *
 * @param ward 指向病房的指针
 * @return int 成功返回1，失败返回0（病房非开放或仍有患者）
 */
int Ward_close(Ward *ward);

/**
 * @brief 重新开放病房
 *
 * 仅当病房处于关闭状态时才可重新开放。
 *
 * @param ward 指向病房的指针
 * @return int 成功返回1，失败返回0（病房非关闭状态或参数无效）
 */
int Ward_open(Ward *ward);

#endif
