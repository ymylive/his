/**
 * @file Doctor.h
 * @brief 医生领域模型头文件
 *
 * 定义了医生(Doctor)的数据结构和状态枚举。
 * 医生是医院信息系统中的核心实体，负责出诊、问诊、
 * 开具处方和医嘱等业务活动。
 */

#ifndef HIS_DOMAIN_DOCTOR_H
#define HIS_DOMAIN_DOCTOR_H

#include "domain/DomainTypes.h"

/**
 * @brief 医生状态枚举
 */
typedef enum DoctorStatus {
    DOCTOR_STATUS_INACTIVE = 0,  /* 停诊/未激活状态 */
    DOCTOR_STATUS_ACTIVE = 1     /* 在岗/激活状态 */
} DoctorStatus;

/**
 * @brief 医生信息结构体
 *
 * 存储医生的基本信息、所属科室以及排班情况。
 */
typedef struct Doctor {
    char doctor_id[HIS_DOMAIN_ID_CAPACITY];       /* 医生唯一标识ID */
    char name[HIS_DOMAIN_NAME_CAPACITY];           /* 医生姓名 */
    char title[HIS_DOMAIN_TITLE_CAPACITY];         /* 医生职称（如主任医师、副主任医师等） */
    char department_id[HIS_DOMAIN_ID_CAPACITY];    /* 所属科室ID */
    char schedule[HIS_DOMAIN_TEXT_CAPACITY];        /* 排班信息 */
    DoctorStatus status;                            /* 医生当前状态（在岗/停诊） */
} Doctor;

#endif
