/**
 * @file AuthService.h
 * @brief 认证服务模块头文件
 *
 * 提供用户注册和身份认证功能。该服务依赖用户仓库（UserRepository）
 * 进行用户数据的持久化存储，并可选地依赖患者仓库（PatientRepository）
 * 来验证患者角色用户的合法性。
 */

#ifndef HIS_SERVICE_AUTH_SERVICE_H
#define HIS_SERVICE_AUTH_SERVICE_H

#include "common/Result.h"
#include "domain/User.h"
#include "repository/PatientRepository.h"
#include "repository/UserRepository.h"

/**
 * @brief 认证服务结构体
 *
 * 封装了用户仓库和患者仓库，提供统一的认证与注册接口。
 */
typedef struct AuthService {
    UserRepository user_repository;           /* 用户数据仓库 */
    PatientRepository patient_repository;     /* 患者数据仓库 */
    int patient_repository_enabled;           /* 患者仓库是否已启用（1=启用，0=未启用） */
} AuthService;

/**
 * @brief 初始化认证服务
 *
 * 初始化用户仓库，并根据是否提供患者数据路径来决定是否启用患者仓库。
 *
 * @param service       指向待初始化的认证服务结构体
 * @param user_path     用户数据文件路径
 * @param patient_path  患者数据文件路径（可为 NULL 或空字符串表示不启用患者仓库）
 * @return Result       操作结果，success=1 表示成功
 */
Result AuthService_init(
    AuthService *service,
    const char *user_path,
    const char *patient_path
);

/**
 * @brief 注册新用户
 *
 * 验证用户名、密码和角色的合法性，若角色为患者则检查患者记录是否存在，
 * 然后生成加盐密码哈希并将用户信息追加到存储中。
 *
 * @param service     指向认证服务结构体
 * @param user_id     用户名（登录账号，不能为空，不能包含保留字符）
 * @param password    用户密码（不能为空，不能包含保留字符）
 * @param role        用户角色（必须为合法的 UserRole 枚举值）
 * @param patient_id  关联的患者编号（仅患者角色需要，其他角色可传 NULL 或空字符串）
 * @return Result     操作结果，success=1 表示注册成功
 */
Result AuthService_register_user(
    AuthService *service,
    const char *user_id,
    const char *password,
    UserRole role,
    const char *patient_id
);

/**
 * @brief 用户身份认证（登录验证）
 *
 * 根据用户ID查找用户记录，对输入密码进行哈希后与存储的密码哈希进行比对。
 * 支持加盐哈希和旧版无盐哈希两种格式。可选地验证用户角色是否匹配。
 *
 * @param service        指向认证服务结构体
 * @param user_id        用户ID
 * @param password       用户密码
 * @param required_role  要求的用户角色（USER_ROLE_UNKNOWN 表示不限制角色）
 * @param out_user       输出参数，认证成功时存放用户信息
 * @return Result        操作结果，success=1 表示认证成功
 */
Result AuthService_authenticate(
    AuthService *service,
    const char *user_id,
    const char *password,
    UserRole required_role,
    User *out_user
);

/**
 * @brief 修改用户密码
 *
 * 验证旧密码后，使用新密码重新生成加盐哈希并更新存储。
 * 同时将 force_password_change 标志清零。
 *
 * @param service       指向认证服务结构体
 * @param user_id       用户ID
 * @param old_password  当前密码（用于身份验证）
 * @param new_password  新密码
 * @return Result       操作结果，success=1 表示修改成功
 */
Result AuthService_change_password(
    AuthService *service,
    const char *user_id,
    const char *old_password,
    const char *new_password
);

#endif
