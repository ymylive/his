/**
 * @file Registration.h
 * @brief 挂号领域模型头文件
 *
 * 定义了挂号记录(Registration)的数据结构、状态枚举及操作函数。
 * 挂号是门诊就医流程的第一步，患者选择科室和医生后进行挂号，
 * 挂号记录跟踪从挂号到就诊或取消的完整生命周期。
 */

#ifndef HIS_DOMAIN_REGISTRATION_H
#define HIS_DOMAIN_REGISTRATION_H

#include "domain/DomainTypes.h"

/**
 * @brief 挂号状态枚举
 */
typedef enum RegistrationStatus {
    REG_STATUS_PENDING = 0,     /* 待诊状态（已挂号，等待就诊） */
    REG_STATUS_DIAGNOSED = 1,   /* 已就诊状态（医生已完成诊断） */
    REG_STATUS_CANCELLED = 2    /* 已取消状态（挂号已被取消） */
} RegistrationStatus;

/**
 * @brief 挂号类型枚举
 */
typedef enum RegistrationType {
    REG_TYPE_STANDARD = 0,      /* 普通号 */
    REG_TYPE_SPECIALIST = 1,    /* 专家号 */
    REG_TYPE_EMERGENCY = 2      /* 急诊号 */
} RegistrationType;

/**
 * @brief 挂号记录结构体
 *
 * 存储一次挂号的完整信息，包括患者、医生、科室及各时间节点。
 */
typedef struct Registration {
    char registration_id[HIS_DOMAIN_ID_CAPACITY];  /* 挂号记录唯一标识ID */
    char patient_id[HIS_DOMAIN_ID_CAPACITY];       /* 挂号患者ID */
    char doctor_id[HIS_DOMAIN_ID_CAPACITY];        /* 挂号医生ID */
    char department_id[HIS_DOMAIN_ID_CAPACITY];    /* 挂号科室ID */
    char registered_at[HIS_DOMAIN_TIME_CAPACITY];  /* 挂号时间 */
    RegistrationStatus status;                      /* 当前挂号状态 */
    char diagnosed_at[HIS_DOMAIN_TIME_CAPACITY];   /* 就诊完成时间 */
    char cancelled_at[HIS_DOMAIN_TIME_CAPACITY];   /* 取消时间 */
    RegistrationType registration_type;             /* 挂号类型 */
    double registration_fee;                        /* 挂号费(元) */
} Registration;

/**
 * @brief 将挂号记录标记为已就诊
 *
 * 仅当挂号状态为"待诊"时才可标记为已就诊。
 *
 * @param registration 指向挂号记录的指针
 * @param diagnosed_at 就诊完成的时间字符串
 * @return int 成功返回1，失败返回0（参数无效或状态不允许）
 */
int Registration_mark_diagnosed(Registration *registration, const char *diagnosed_at);

/**
 * @brief 取消挂号记录
 *
 * 仅当挂号状态为"待诊"时才可取消。
 *
 * @param registration 指向挂号记录的指针
 * @param cancelled_at 取消时间字符串
 * @return int 成功返回1，失败返回0（参数无效或状态不允许）
 */
int Registration_cancel(Registration *registration, const char *cancelled_at);

#endif
