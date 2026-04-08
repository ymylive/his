/**
 * @file UserRepository.h
 * @brief 用户账户数据仓储层头文件
 *
 * 提供用户账户（User）数据的持久化存储功能，基于文本文件存储。
 * 支持用户的追加注册和按用户ID查找操作。
 *
 * 数据文件格式：每行一条用户记录，字段以管道符 '|' 分隔。
 * 表头：user_id|password_hash|role
 */

#ifndef HIS_REPOSITORY_USER_REPOSITORY_H
#define HIS_REPOSITORY_USER_REPOSITORY_H

#include "common/Result.h"
#include "domain/User.h"
#include "repository/TextFileRepository.h"

/** 用户数据文件的表头行 */
#define USER_REPOSITORY_HEADER "user_id|password_hash|role"

/** 用户记录的字段数量 */
#define USER_REPOSITORY_FIELD_COUNT 3

/**
 * @brief 用户仓储结构体
 */
typedef struct UserRepository {
    TextFileRepository file_repository; /**< 底层文本文件仓储 */
} UserRepository;

/**
 * @brief 初始化用户仓储
 *
 * 初始化底层文件仓储，并确保文件中包含正确的表头。
 *
 * @param repository 用户仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效或文件操作失败时返回 failure
 */
Result UserRepository_init(UserRepository *repository, const char *path);

/**
 * @brief 追加一条用户记录
 *
 * 将用户信息追加到文件末尾。会先检查是否已存在相同ID的用户。
 *
 * @param repository 用户仓储实例指针
 * @param user       要追加的用户数据
 * @return 成功返回 success；ID重复或写入失败时返回 failure
 */
Result UserRepository_append(const UserRepository *repository, const User *user);

/**
 * @brief 按用户ID查找用户
 * @param repository 用户仓储实例指针
 * @param user_id    要查找的用户ID
 * @param out_user   输出参数，存放找到的用户数据
 * @return 找到返回 success；未找到或参数无效时返回 failure
 */
Result UserRepository_find_by_user_id(
    const UserRepository *repository,
    const char *user_id,
    User *out_user
);

#endif
