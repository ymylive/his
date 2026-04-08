/**
 * @file Patient.h
 * @brief 患者领域模型头文件
 *
 * 定义了患者(Patient)的数据结构、性别枚举，以及相关的校验函数。
 * 患者是医院信息系统中的核心实体，记录患者的基本信息、
 * 过敏史、病历以及住院状态等。
 */

#ifndef HIS_DOMAIN_PATIENT_H
#define HIS_DOMAIN_PATIENT_H

#include "domain/DomainTypes.h"

/**
 * @brief 患者性别枚举
 */
typedef enum PatientGender {
    PATIENT_GENDER_UNKNOWN = 0,  /* 未知性别 */
    PATIENT_GENDER_MALE = 1,     /* 男性 */
    PATIENT_GENDER_FEMALE = 2    /* 女性 */
} PatientGender;

/**
 * @brief 患者信息结构体
 *
 * 存储患者的基本信息和医疗相关数据。
 */
typedef struct Patient {
    char patient_id[HIS_DOMAIN_ID_CAPACITY];               /* 患者唯一标识ID */
    char name[HIS_DOMAIN_NAME_CAPACITY];                   /* 患者姓名 */
    PatientGender gender;                                   /* 患者性别 */
    int age;                                                /* 患者年龄 */
    char contact[HIS_DOMAIN_CONTACT_CAPACITY];             /* 联系方式（电话号码等） */
    char id_card[HIS_DOMAIN_CONTACT_CAPACITY];             /* 身份证号码 */
    char allergy[HIS_DOMAIN_TEXT_CAPACITY];                 /* 过敏史信息 */
    char medical_history[HIS_DOMAIN_LARGE_TEXT_CAPACITY];  /* 既往病史 */
    int is_inpatient;                                       /* 是否为住院患者（1=住院，0=非住院） */
    char remarks[HIS_DOMAIN_TEXT_CAPACITY];                /* 备注信息 */
} Patient;

/**
 * @brief 校验患者年龄是否合法
 *
 * @param age 待校验的年龄值
 * @return int 合法返回1（0~150岁），不合法返回0
 */
static inline int Patient_is_valid_age(int age) {
    return age >= 0 && age <= 150;
}

#endif
