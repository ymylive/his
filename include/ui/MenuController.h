/**
 * @file MenuController.h
 * @brief 菜单控制器模块 - 负责菜单的渲染、解析用户输入和角色/操作路由
 *
 * 本模块定义了医院信息系统(HIS)中的菜单角色枚举、菜单操作枚举，
 * 以及菜单渲染与用户选择解析的接口。它是 UI 层的核心控制组件，
 * 将用户的菜单选择映射为具体的业务操作。
 */

#ifndef HIS_UI_MENU_CONTROLLER_H
#define HIS_UI_MENU_CONTROLLER_H

#include <stddef.h>
#include <stdio.h>

#include "common/Result.h"

/**
 * @brief 菜单角色枚举 - 定义系统支持的所有登录角色
 *
 * 每个角色对应不同的功能菜单和权限范围。
 */
typedef enum MenuRole {
    MENU_ROLE_INVALID = 0,            /**< 无效角色 */
    MENU_ROLE_ADMIN = 1,              /**< 系统管理员 */
    MENU_ROLE_DOCTOR = 2,             /**< 医生 */
    MENU_ROLE_PATIENT = 3,            /**< 患者 */
    MENU_ROLE_INPATIENT_MANAGER = 4,  /**< 住院管理员 */
    MENU_ROLE_PHARMACY = 5,           /**< 药房人员 */
    MENU_ROLE_RESET_DEMO = 6,         /**< 重置演示数据（特殊角色） */
    MENU_ROLE_EXIT = 99               /**< 退出系统 */
} MenuRole;

/**
 * @brief 菜单操作枚举 - 定义系统中所有可执行的菜单操作
 *
 * 操作编号按角色分组：
 * - 1xx: 系统管理员操作
 * - 3xx: 医生操作
 * - 4xx: 患者操作
 * - 5xx: 住院管理员操作
 * - 7xx: 药房人员操作
 */
typedef enum MenuAction {
    MENU_ACTION_INVALID = 0,   /**< 无效操作 */
    MENU_ACTION_BACK = 1,      /**< 返回上级菜单 */

    /* 系统管理员操作 (1xx) */
    MENU_ACTION_ADMIN_PATIENT_MANAGEMENT = 101,   /**< 患者信息管理 */
    MENU_ACTION_ADMIN_DOCTOR_DEPARTMENT = 102,     /**< 医生与科室管理 */
    MENU_ACTION_ADMIN_MEDICAL_RECORDS = 103,       /**< 医疗记录管理/按时间范围查询 */
    MENU_ACTION_ADMIN_WARD_BED_OVERVIEW = 104,     /**< 病房与床位管理 */
    MENU_ACTION_ADMIN_MEDICINE_OVERVIEW = 105,     /**< 药房与药品管理 */

    /* 医生操作 (3xx) */
    MENU_ACTION_DOCTOR_PENDING_LIST = 301,         /**< 待诊列表 */
    MENU_ACTION_DOCTOR_QUERY_PATIENT_HISTORY = 302,/**< 查询患者信息与历史 */
    MENU_ACTION_DOCTOR_VISIT_RECORD = 303,         /**< 诊疗记录/诊断结果/医生建议 */
    MENU_ACTION_DOCTOR_PRESCRIPTION_STOCK = 304,   /**< 处方与发药/查询药品库存 */
    MENU_ACTION_DOCTOR_EXAM_RECORD = 305,          /**< 检查记录/检查结果 */

    /* 患者操作 (4xx) */
    MENU_ACTION_PATIENT_BASIC_INFO = 401,          /**< 基本信息 */
    MENU_ACTION_PATIENT_QUERY_REGISTRATION = 402,  /**< 挂号查询 */
    MENU_ACTION_PATIENT_QUERY_VISITS = 403,        /**< 个人看诊历史 */
    MENU_ACTION_PATIENT_QUERY_EXAMS = 404,         /**< 个人检查历史 */
    MENU_ACTION_PATIENT_QUERY_ADMISSIONS = 405,    /**< 个人住院历史 */
    MENU_ACTION_PATIENT_QUERY_DISPENSE = 406,      /**< 个人发药历史 */
    MENU_ACTION_PATIENT_QUERY_MEDICINE_USAGE = 407,/**< 药品使用方法查询 */

    /* 住院管理员操作 (5xx) */
    MENU_ACTION_INPATIENT_ADMIT = 501,             /**< 入院登记 */
    MENU_ACTION_INPATIENT_DISCHARGE = 502,         /**< 出院办理 */
    MENU_ACTION_INPATIENT_QUERY_RECORD = 503,      /**< 住院状态查询 */
    MENU_ACTION_INPATIENT_QUERY_BED = 504,         /**< 病区床位查询 */
    MENU_ACTION_INPATIENT_LIST_WARDS = 505,        /**< 查看病房信息 */
    MENU_ACTION_INPATIENT_LIST_BEDS = 506,         /**< 查看床位状态 */
    MENU_ACTION_INPATIENT_TRANSFER_BED = 507,      /**< 床位调整/转床 */
    MENU_ACTION_INPATIENT_DISCHARGE_CHECK = 508,   /**< 出院前检查 */

    /* 药房人员操作 (7xx) */
    MENU_ACTION_PHARMACY_ADD_MEDICINE = 701,       /**< 添加药品 */
    MENU_ACTION_PHARMACY_RESTOCK = 702,            /**< 药品入库 */
    MENU_ACTION_PHARMACY_DISPENSE = 703,           /**< 药品出库/发药 */
    MENU_ACTION_PHARMACY_QUERY_STOCK = 704,        /**< 库存盘点/查询库存 */
    MENU_ACTION_PHARMACY_LOW_STOCK = 705           /**< 缺药提醒/库存不足提醒 */
} MenuAction;

/**
 * @brief 渲染主菜单（角色选择菜单）到缓冲区
 * @param buffer   输出缓冲区，用于存放渲染后的菜单文本
 * @param capacity 缓冲区容量（字节数）
 * @return Result  成功时包含 "menu ready"，失败时包含错误信息
 */
Result MenuController_render_main_menu(char *buffer, size_t capacity);

/**
 * @brief 渲染指定角色的操作菜单到缓冲区
 * @param role     要渲染的角色
 * @param buffer   输出缓冲区
 * @param capacity 缓冲区容量
 * @return Result  成功时包含 "role menu ready"，失败时包含错误信息
 */
Result MenuController_render_role_menu(
    MenuRole role,
    char *buffer,
    size_t capacity
);

/**
 * @brief 解析用户在主菜单中的输入，转换为角色枚举
 * @param input    用户输入的字符串
 * @param out_role 输出参数，解析后的角色枚举值
 * @return Result  成功时包含角色描述，失败时包含错误信息
 */
Result MenuController_parse_main_selection(const char *input, MenuRole *out_role);

/**
 * @brief 解析用户在角色菜单中的输入，转换为操作枚举
 * @param role       当前角色
 * @param input      用户输入的字符串
 * @param out_action 输出参数，解析后的操作枚举值
 * @return Result    成功时包含 "role action selected"，失败时包含错误信息
 */
Result MenuController_parse_role_selection(
    MenuRole role,
    const char *input,
    MenuAction *out_action
);

/**
 * @brief 格式化操作反馈信息到缓冲区
 * @param action   要格式化的操作
 * @param buffer   输出缓冲区
 * @param capacity 缓冲区容量
 * @return Result  成功时包含 "action feedback ready"，失败时包含错误信息
 */
Result MenuController_format_action_feedback(
    MenuAction action,
    char *buffer,
    size_t capacity
);

/**
 * @brief 获取角色的中文标签
 * @param role 角色枚举值
 * @return 角色对应的中文名称字符串
 */
const char *MenuController_role_label(MenuRole role);

/**
 * @brief 获取操作的中文标签
 * @param action 操作枚举值
 * @return 操作对应的中文名称字符串
 */
const char *MenuController_action_label(MenuAction action);

/**
 * @brief 判断角色是否为退出角色
 * @param role 角色枚举值
 * @return 1 表示是退出角色，0 表示不是
 */
int MenuController_is_exit_role(MenuRole role);

/**
 * @brief 判断操作是否为返回上级菜单
 * @param action 操作枚举值
 * @return 1 表示是返回操作，0 表示不是
 */
int MenuController_is_back_action(MenuAction action);

/**
 * @brief 交互式菜单选择（支持方向键导航）
 */
Result MenuController_interactive_select(
    MenuRole role,
    void *panel,
    FILE *input,
    MenuAction *out_action
);

/**
 * @brief 主菜单交互式选择（支持方向键导航）
 * @param input      输入流
 * @param out_role   输出选择的角色
 * @return Result    成功或失败
 */
Result MenuController_interactive_main_select(
    FILE *input,
    MenuRole *out_role
);

#endif
