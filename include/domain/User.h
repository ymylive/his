/**
 * @file User.h
 * @brief 用户领域模型头文件
 *
 * 定义了系统用户(User)的数据结构和角色枚举。
 * 用户是系统登录和权限控制的基本单元，
 * 不同角色拥有不同的操作权限。
 */

#ifndef HIS_DOMAIN_USER_H
#define HIS_DOMAIN_USER_H

#include "domain/DomainTypes.h"

/** 密码哈希值的最大字符容量 */
#define HIS_USER_PASSWORD_HASH_CAPACITY 98

/**
 * @brief 用户角色枚举
 */
typedef enum UserRole {
    USER_ROLE_UNKNOWN = 0,            /* 未知角色 */
    USER_ROLE_PATIENT = 1,            /* 患者（可挂号、查看报告等） */
    USER_ROLE_DOCTOR = 2,             /* 医生（可问诊、开处方等） */
    USER_ROLE_ADMIN = 3,              /* 系统管理员（拥有全部权限） */
    USER_ROLE_INPATIENT_MANAGER = 5,  /* 住院管理员（管理入院、出院等） */
    USER_ROLE_PHARMACY = 7            /* 药房人员（负责发药等） */
} UserRole;

/**
 * @brief 用户信息结构体
 *
 * 存储用户的认证信息和角色，用于系统登录和权限控制。
 */
typedef struct User {
    char user_id[HIS_DOMAIN_ID_CAPACITY];                  /* 用户名（登录账号） */
    char password_hash[HIS_USER_PASSWORD_HASH_CAPACITY];   /* 密码的哈希值 */
    UserRole role;                                          /* 用户角色 */
    char patient_id[HIS_DOMAIN_ID_CAPACITY];               /* 关联的患者编号（仅患者角色使用） */
    int force_password_change;                              /* 1 = 必须在下次登录时修改密码 */
} User;

#endif
