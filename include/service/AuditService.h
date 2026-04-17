/**
 * @file AuditService.h
 * @brief 审计日志服务模块头文件
 *
 * 提供 HIPAA §164.312(b) 要求的审计日志记录功能：谁在何时对谁
 * 做了什么操作。写入 append-only 文本日志 data/audit.log。
 *
 * 记录格式（管道分隔，与项目其他数据文件一致）：
 *   <iso8601>|<event_type>|<actor_user_id>|<actor_role>|
 *   <target_entity>|<target_id>|<result>|<detail>
 */

#ifndef HIS_SERVICE_AUDIT_SERVICE_H
#define HIS_SERVICE_AUDIT_SERVICE_H

#include "common/Result.h"

/** 审计日志文件路径的最大长度（含终止符） */
#define HIS_FILE_PATH_CAPACITY 512

/**
 * @brief 审计事件类型枚举
 *
 * 覆盖登录/登出/密码变更以及关键 CRUD 操作，用于合规追溯。
 */
typedef enum {
    AUDIT_EVENT_LOGIN_SUCCESS,    /**< 登录成功 */
    AUDIT_EVENT_LOGIN_FAILURE,    /**< 登录失败 */
    AUDIT_EVENT_LOGOUT,           /**< 登出 */
    AUDIT_EVENT_PASSWORD_CHANGE,  /**< 修改密码 */
    AUDIT_EVENT_CREATE,           /**< 创建对象（患者/挂号/处方/医嘱等） */
    AUDIT_EVENT_UPDATE,           /**< 更新对象 */
    AUDIT_EVENT_DELETE,           /**< 删除对象 */
    AUDIT_EVENT_READ,             /**< 查阅他人记录（PHI 访问） */
    AUDIT_EVENT_DISCHARGE,        /**< 出院办理 */
    AUDIT_EVENT_DISPENSE          /**< 发药 */
} AuditEventType;

/**
 * @brief 审计服务结构体
 *
 * 仅持有日志文件的完整路径。日志采用 append-only 方式追加，
 * 单线程环境下无需加锁。
 */
typedef struct AuditService {
    char log_path[HIS_FILE_PATH_CAPACITY]; /**< 日志文件完整路径 */
} AuditService;

/**
 * @brief 初始化审计服务
 *
 * 在 data_dir 下生成 audit.log 的完整路径，确保父目录存在，
 * 并把文件权限收紧到 0600（仅所有者可读写）。
 *
 * @param self     指向待初始化的审计服务结构体
 * @param data_dir 数据目录（如 "data/"，尾部斜杠可选）
 * @return Result  操作结果
 */
Result AuditService_init(AuditService *self, const char *data_dir);

/**
 * @brief 记录一条审计事件
 *
 * 采用 append-only 写入，格式为：
 *   <iso8601>|<type>|<actor_user_id>|<actor_role>|
 *   <target_entity>|<target_id>|<result>|<detail>
 * 空字段留空但保留分隔符；字段中的 '|' 与 '\\' 自动转义。
 *
 * @param self            审计服务实例
 * @param type            事件类型
 * @param actor_user_id   操作者用户ID（登录前阶段可传 NULL）
 * @param actor_role      操作者角色（如 "ADMIN"/"DOCTOR"；可 NULL）
 * @param target_entity   目标对象类型（如 "PATIENT"；可 NULL）
 * @param target_id       目标对象ID（如 "PAT0001"；可 NULL）
 * @param result_tag      结果标签（如 "OK"/"FAIL"/"DENIED"）
 * @param detail_utf8     简短描述（可 NULL）
 * @return Result         写入结果
 */
Result AuditService_record(
    AuditService *self,
    AuditEventType type,
    const char *actor_user_id,
    const char *actor_role,
    const char *target_entity,
    const char *target_id,
    const char *result_tag,
    const char *detail_utf8
);

#endif
