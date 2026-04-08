/**
 * @file Admission.h
 * @brief 入院记录领域模型头文件
 *
 * 定义了入院记录(Admission)的数据结构、状态枚举及操作函数。
 * 入院记录跟踪患者从入院到出院的完整住院过程，
 * 关联患者、病房和床位信息。
 */

#ifndef HIS_DOMAIN_ADMISSION_H
#define HIS_DOMAIN_ADMISSION_H

#include "domain/DomainTypes.h"

/**
 * @brief 入院状态枚举
 */
typedef enum AdmissionStatus {
    ADMISSION_STATUS_ACTIVE = 0,      /* 住院中（当前在院） */
    ADMISSION_STATUS_DISCHARGED = 1   /* 已出院 */
} AdmissionStatus;

/**
 * @brief 入院记录结构体
 *
 * 存储一次住院的完整信息，包括患者、病房、床位、时间及出院小结。
 */
typedef struct Admission {
    char admission_id[HIS_DOMAIN_ID_CAPACITY];          /* 入院记录唯一标识ID */
    char patient_id[HIS_DOMAIN_ID_CAPACITY];            /* 住院患者ID */
    char ward_id[HIS_DOMAIN_ID_CAPACITY];               /* 所在病房ID */
    char bed_id[HIS_DOMAIN_ID_CAPACITY];                /* 所在床位ID */
    char admitted_at[HIS_DOMAIN_TIME_CAPACITY];          /* 入院时间 */
    AdmissionStatus status;                               /* 当前入院状态（住院中/已出院） */
    char discharged_at[HIS_DOMAIN_TIME_CAPACITY];        /* 出院时间 */
    char summary[HIS_DOMAIN_LARGE_TEXT_CAPACITY];        /* 出院小结/住院摘要 */
} Admission;

/**
 * @brief 检查入院记录是否处于住院中状态
 *
 * @param admission 指向入院记录的常量指针
 * @return int 住院中返回1，已出院或参数无效返回0
 */
int Admission_is_active(const Admission *admission);

/**
 * @brief 办理出院
 *
 * 将入院状态设为"已出院"并记录出院时间。
 * 仅当入院记录处于住院中状态时才可办理。
 *
 * @param admission     指向入院记录的指针
 * @param discharged_at 出院时间字符串
 * @return int 成功返回1，失败返回0（参数无效或非住院状态）
 */
int Admission_discharge(Admission *admission, const char *discharged_at);

#endif
