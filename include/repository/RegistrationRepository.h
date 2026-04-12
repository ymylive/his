/**
 * @file RegistrationRepository.h
 * @brief 挂号记录数据仓储层头文件
 *
 * 提供挂号记录（Registration）数据的持久化存储功能，基于文本文件存储。
 * 支持挂号记录的追加、按挂号ID查找、按患者ID筛选（支持过滤条件）、
 * 全量加载和全量保存操作。
 *
 * 数据文件格式：每行一条挂号记录，字段以管道符 '|' 分隔。
 * 表头：registration_id|patient_id|doctor_id|department_id|registered_at|status|diagnosed_at|cancelled_at|registration_type|registration_fee
 */

#ifndef HIS_REPOSITORY_REGISTRATION_REPOSITORY_H
#define HIS_REPOSITORY_REGISTRATION_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Registration.h"
#include "repository/TextFileRepository.h"

/** 挂号记录的字段数量 */
#define REGISTRATION_REPOSITORY_FIELD_COUNT 10

/**
 * @brief 挂号记录仓储结构体
 */
typedef struct RegistrationRepository {
    TextFileRepository storage; /**< 底层文本文件仓储 */
} RegistrationRepository;

/**
 * @brief 挂号记录查询过滤条件
 *
 * 用于按患者ID查找挂号记录时指定额外的过滤条件。
 */
typedef struct RegistrationRepositoryFilter {
    int use_status;                  /**< 是否启用状态过滤（0=不过滤，1=过滤） */
    RegistrationStatus status;       /**< 要匹配的挂号状态 */
    const char *registered_at_from;  /**< 挂号时间范围起始（字符串比较，可为 NULL 表示不限） */
    const char *registered_at_to;    /**< 挂号时间范围结束（字符串比较，可为 NULL 表示不限） */
} RegistrationRepositoryFilter;

/**
 * @brief 初始化挂号记录仓储
 * @param repository 仓储实例指针
 * @param path       数据文件路径
 * @return 成功返回 success；参数无效时返回 failure
 */
Result RegistrationRepository_init(RegistrationRepository *repository, const char *path);

/**
 * @brief 确保存储文件已就绪（含表头校验/写入）
 * @param repository 仓储实例指针
 * @return 成功返回 success；文件操作失败或表头不匹配时返回 failure
 */
Result RegistrationRepository_ensure_storage(const RegistrationRepository *repository);

/**
 * @brief 初始化查询过滤条件为默认值（不启用任何过滤）
 * @param filter 过滤条件结构体指针
 */
void RegistrationRepositoryFilter_init(RegistrationRepositoryFilter *filter);

/**
 * @brief 追加一条挂号记录到文件
 * @param repository   仓储实例指针
 * @param registration 要追加的挂号记录
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result RegistrationRepository_append(
    const RegistrationRepository *repository,
    const Registration *registration
);

/**
 * @brief 按挂号ID查找挂号记录
 * @param repository       仓储实例指针
 * @param registration_id  要查找的挂号ID
 * @param out_registration 输出参数，存放找到的挂号记录
 * @return 找到返回 success；未找到时返回 failure
 */
Result RegistrationRepository_find_by_registration_id(
    const RegistrationRepository *repository,
    const char *registration_id,
    Registration *out_registration
);

/**
 * @brief 按患者ID查找挂号记录（支持过滤条件）
 * @param repository        仓储实例指针
 * @param patient_id        患者ID
 * @param filter            过滤条件（可为 NULL 表示不过滤）
 * @param out_registrations 输出参数，存放匹配的挂号记录链表（必须为空链表）
 * @return 成功返回 success；参数无效或加载失败时返回 failure
 */
Result RegistrationRepository_find_by_patient_id(
    const RegistrationRepository *repository,
    const char *patient_id,
    const RegistrationRepositoryFilter *filter,
    LinkedList *out_registrations
);

/**
 * @brief 加载所有挂号记录到链表
 * @param repository        仓储实例指针
 * @param out_registrations 输出参数，存放挂号记录的链表（必须为空链表）
 * @return 成功返回 success；加载失败时返回 failure
 */
Result RegistrationRepository_load_all(
    const RegistrationRepository *repository,
    LinkedList *out_registrations
);

/**
 * @brief 全量保存挂号记录列表到文件（覆盖写入）
 * @param repository    仓储实例指针
 * @param registrations 要保存的挂号记录链表
 * @return 成功返回 success；数据无效或写入失败时返回 failure
 */
Result RegistrationRepository_save_all(
    const RegistrationRepository *repository,
    const LinkedList *registrations
);

/**
 * @brief 清空并释放挂号记录链表中的所有元素
 * @param registrations 要清空的挂号记录链表
 */
void RegistrationRepository_clear_list(LinkedList *registrations);

#endif
