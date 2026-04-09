/**
 * @file MenuController.c
 * @brief 菜单控制器实现 - 菜单渲染、用户输入解析与操作路由
 *
 * 本文件实现了 HIS 系统的菜单渲染与交互控制逻辑，包括：
 * 1. 主菜单（角色选择）和各角色子菜单的文本渲染
 * 2. 用户输入解析（字符串 -> 角色/操作枚举）
 * 3. 角色和操作的中文标签映射
 * 4. 使用 TUI 样式系统绘制美观的菜单框
 */

#include "ui/MenuController.h"
#include "ui/TuiStyle.h"
#include "common/UpdateChecker.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MENU_BOX_WIDTH 44              /**< 菜单框的总宽度（字符数） */
#define MENU_INNER     (MENU_BOX_WIDTH - 2)  /**< 菜单框内部可用宽度 */

/**
 * @brief 菜单选项结构体 - 描述菜单中的一个可选项
 */
typedef struct MenuOption {
    int selection;       /**< 用户输入的选择编号（0 表示返回） */
    MenuAction action;   /**< 对应的操作枚举值 */
    const char *label;   /**< 菜单项的中文标签 */
    const char *icon;    /**< 菜单项前的 Unicode 图标 */
} MenuOption;

/* ---- 菜单框渲染辅助函数（使用 snprintf 将样式化文本写入缓冲区） ---- */

/**
 * @brief 向缓冲区追加一条水平分隔线
 * @param buf   目标缓冲区
 * @param cap   缓冲区容量
 * @param used  已使用字节数（会被更新）
 * @param left  左端角字符（如 TUI_TL, TUI_LT, TUI_BL）
 * @param right 右端角字符（如 TUI_TR, TUI_RT, TUI_BR）
 * @return 0 成功，-1 缓冲区不足
 */
static int menu_append_hline(
    char *buf, size_t cap, size_t *used,
    const char *left, const char *right
) {
    int w = 0;
    int i = 0;
    (void)left;
    (void)right;
    w = snprintf(buf + *used, cap - *used, TUI_OC_DIM);
    if (w < 0 || (size_t)w >= cap - *used) return -1;
    *used += (size_t)w;
    for (i = 0; i < MENU_INNER + 2; i++) {
        w = snprintf(buf + *used, cap - *used, "\xe2\x94\x80");
        if (w < 0 || (size_t)w >= cap - *used) return -1;
        *used += (size_t)w;
    }
    w = snprintf(buf + *used, cap - *used, TUI_RESET "\n");
    if (w < 0 || (size_t)w >= cap - *used) return -1;
    *used += (size_t)w;
    return 0;
}

/**
 * @brief 向缓冲区追加居中的标题行
 * @param buf   目标缓冲区
 * @param cap   缓冲区容量
 * @param used  已使用字节数（会被更新）
 * @param title 标题文本（UTF-8 编码）
 * @return 0 成功，-1 缓冲区不足
 */
static int menu_append_title(
    char *buf, size_t cap, size_t *used,
    const char *title
) {
    int w = 0;
    w = snprintf(buf + *used, cap - *used,
                 TUI_OC_ACCENT "\xe2\x94\x83" TUI_RESET
                 " " TUI_OC_TEXT TUI_BOLD "%s" TUI_RESET "\n",
                 title);
    if (w < 0 || (size_t)w >= cap - *used) return -1;
    *used += (size_t)w;
    return 0;
}

/**
 * @brief 向缓冲区追加一个菜单选项行
 * @param buf    目标缓冲区
 * @param cap    缓冲区容量
 * @param used   已使用字节数（会被更新）
 * @param number 选项编号
 * @param icon   选项图标（可为 NULL）
 * @param label  选项文字标签
 * @param is_dim 是否以暗淡样式显示（用于"返回"等次要选项）
 * @return 0 成功，-1 缓冲区不足
 */
static int menu_append_item(
    char *buf, size_t cap, size_t *used,
    int number, const char *icon, const char *label, int is_dim
) {
    int w = 0;
    const char *num_color = is_dim ? TUI_OC_DIM : TUI_OC_AMBER;
    const char *lbl_color = is_dim ? TUI_OC_DIM : TUI_OC_MUTED;
    (void)icon;
    w = snprintf(buf + *used, cap - *used,
                 TUI_OC_BORDER "\xe2\x94\x83" TUI_RESET
                 "  %s%d." TUI_RESET
                 " %s%s" TUI_RESET "\n",
                 num_color, number, lbl_color, label);
    if (w < 0 || (size_t)w >= cap - *used) return -1;
    *used += (size_t)w;
    return 0;
}

/**
 * @brief 向缓冲区追加菜单底部品牌页脚
 */
static int menu_append_footer(
    char *buf, size_t cap, size_t *used
) {
    int w = 0;
    w = snprintf(buf + *used, cap - *used,
                 TUI_OC_DIM "HIS v" HIS_VERSION TUI_RESET "\n");
    if (w < 0 || (size_t)w >= cap - *used) return -1;
    *used += (size_t)w;
    return 0;
}

/* ---- 各角色的菜单选项定义 ---- */

/** @brief 系统管理员菜单选项 */
static const MenuOption MENU_ADMIN_OPTIONS[] = {
    {1, MENU_ACTION_ADMIN_PATIENT_MANAGEMENT, "患者信息管理", TUI_HEART},
    {2, MENU_ACTION_ADMIN_DOCTOR_DEPARTMENT, "医生与科室管理/按科室查看医生", TUI_MEDICAL},
    {3, MENU_ACTION_ADMIN_MEDICAL_RECORDS, "医疗记录管理/按时间范围查询", TUI_LOZENGE},
    {4, MENU_ACTION_ADMIN_WARD_BED_OVERVIEW, "病房与床位管理/查看床位状态", TUI_CIRCLE},
    {5, MENU_ACTION_ADMIN_MEDICINE_OVERVIEW, "药房与药品管理/库存不足提醒", TUI_FLASK},
    {0, MENU_ACTION_BACK, "返回上级菜单", TUI_ARROW_R}
};

/** @brief 医生菜单选项 */
static const MenuOption MENU_DOCTOR_OPTIONS[] = {
    {1, MENU_ACTION_DOCTOR_PENDING_LIST, "待诊列表", TUI_LOZENGE},
    {2, MENU_ACTION_DOCTOR_QUERY_PATIENT_HISTORY, "查询患者信息与历史", TUI_HEART},
    {3, MENU_ACTION_DOCTOR_VISIT_RECORD, "诊疗记录/诊断结果/医生建议", TUI_HEAVY_CROSS},
    {4, MENU_ACTION_DOCTOR_PRESCRIPTION_STOCK, "处方与发药/查询药品库存", TUI_FLASK},
    {5, MENU_ACTION_DOCTOR_EXAM_RECORD, "检查记录/检查结果", TUI_STAR},
    {0, MENU_ACTION_BACK, "返回上级菜单", TUI_ARROW_R}
};

/** @brief 患者菜单选项 */
static const MenuOption MENU_PATIENT_OPTIONS[] = {
    {1, MENU_ACTION_PATIENT_BASIC_INFO, "基本信息", TUI_HEART},
    {2, MENU_ACTION_PATIENT_QUERY_REGISTRATION, "挂号查询", TUI_LOZENGE},
    {3, MENU_ACTION_PATIENT_QUERY_VISITS, "个人看诊历史", TUI_HEAVY_CROSS},
    {4, MENU_ACTION_PATIENT_QUERY_EXAMS, "个人检查历史", TUI_STAR},
    {5, MENU_ACTION_PATIENT_QUERY_ADMISSIONS, "个人住院历史", TUI_CIRCLE},
    {6, MENU_ACTION_PATIENT_QUERY_DISPENSE, "个人发药历史", TUI_FLASK},
    {7, MENU_ACTION_PATIENT_QUERY_MEDICINE_USAGE, "药品使用方法", TUI_DIAMOND},
    {0, MENU_ACTION_BACK, "返回上级菜单", TUI_ARROW_R}
};

/** @brief 住院管理员菜单选项 */
static const MenuOption MENU_INPATIENT_OPTIONS[] = {
    {1, MENU_ACTION_INPATIENT_QUERY_BED, "病区床位查询", TUI_CIRCLE},
    {2, MENU_ACTION_INPATIENT_ADMIT, "入院登记", TUI_HEAVY_CROSS},
    {3, MENU_ACTION_INPATIENT_DISCHARGE, "出院办理", TUI_TRIANGLE},
    {4, MENU_ACTION_INPATIENT_QUERY_RECORD, "住院状态查询", TUI_LOZENGE},
    {5, MENU_ACTION_INPATIENT_LIST_WARDS, "查看病房信息", TUI_STAR},
    {6, MENU_ACTION_INPATIENT_LIST_BEDS, "查看床位状态", TUI_CIRCLE_O},
    {7, MENU_ACTION_INPATIENT_TRANSFER_BED, "床位调整/转床", TUI_DIAMOND},
    {8, MENU_ACTION_INPATIENT_DISCHARGE_CHECK, "出院前检查", TUI_MEDICAL},
    {0, MENU_ACTION_BACK, "返回上级菜单", TUI_ARROW_R}
};

/** @brief 药房人员菜单选项 */
static const MenuOption MENU_PHARMACY_OPTIONS[] = {
    {1, MENU_ACTION_PHARMACY_ADD_MEDICINE, "添加药品", TUI_HEAVY_CROSS},
    {2, MENU_ACTION_PHARMACY_RESTOCK, "药品入库", TUI_TRIANGLE},
    {3, MENU_ACTION_PHARMACY_DISPENSE, "药品出库/发药", TUI_DIAMOND},
    {4, MENU_ACTION_PHARMACY_QUERY_STOCK, "库存盘点/查询库存", TUI_STAR},
    {5, MENU_ACTION_PHARMACY_LOW_STOCK, "缺药提醒/库存不足提醒", TUI_LIGHTNING},
    {0, MENU_ACTION_BACK, "返回上级菜单", TUI_ARROW_R}
};

/**
 * @brief 安全复制文本到缓冲区
 * @param buffer   目标缓冲区
 * @param capacity 缓冲区容量
 * @param text     源文本
 * @return Result  成功或失败的结果
 */
static Result MenuController_copy_text(char *buffer, size_t capacity, const char *text) {
    int written = 0;

    if (buffer == 0 || capacity == 0 || text == 0) {
        return Result_make_failure("menu buffer invalid");
    }

    written = snprintf(buffer, capacity, "%s", text);
    if (written < 0 || (size_t)written >= capacity) {
        return Result_make_failure("menu buffer too small");
    }

    return Result_make_success("menu ready");
}

/**
 * @brief 根据角色获取对应的菜单选项数组
 * @param role      角色枚举
 * @param out_count 输出参数，选项数量
 * @return 菜单选项数组指针，角色无效时返回 NULL
 */
static const MenuOption *MenuController_options_for_role(
    MenuRole role,
    size_t *out_count
) {
    if (out_count == 0) {
        return 0;
    }

    switch (role) {
        case MENU_ROLE_ADMIN:
            *out_count = sizeof(MENU_ADMIN_OPTIONS) / sizeof(MENU_ADMIN_OPTIONS[0]);
            return MENU_ADMIN_OPTIONS;
        case MENU_ROLE_DOCTOR:
            *out_count = sizeof(MENU_DOCTOR_OPTIONS) / sizeof(MENU_DOCTOR_OPTIONS[0]);
            return MENU_DOCTOR_OPTIONS;
        case MENU_ROLE_PATIENT:
            *out_count = sizeof(MENU_PATIENT_OPTIONS) / sizeof(MENU_PATIENT_OPTIONS[0]);
            return MENU_PATIENT_OPTIONS;
        case MENU_ROLE_INPATIENT_MANAGER:
            *out_count = sizeof(MENU_INPATIENT_OPTIONS) / sizeof(MENU_INPATIENT_OPTIONS[0]);
            return MENU_INPATIENT_OPTIONS;
        case MENU_ROLE_PHARMACY:
            *out_count = sizeof(MENU_PHARMACY_OPTIONS) / sizeof(MENU_PHARMACY_OPTIONS[0]);
            return MENU_PHARMACY_OPTIONS;
        default:
            *out_count = 0;
            return 0;
    }
}

/**
 * @brief 将用户输入字符串解析为整数
 *
 * 自动跳过前后空白字符，不允许包含非空白的非数字字符。
 *
 * @param input     用户输入字符串
 * @param out_value 输出参数，解析后的整数值
 * @return Result   成功或失败的结果
 */
static Result MenuController_parse_number(const char *input, int *out_value) {
    const char *start = input;
    char *end_pointer = 0;
    long value = 0;

    if (input == 0 || out_value == 0) {
        return Result_make_failure("menu input missing");
    }

    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }

    if (*start == '\0') {
        return Result_make_failure("menu input blank");
    }

    value = strtol(start, &end_pointer, 10);
    if (end_pointer == start || end_pointer == 0) {
        return Result_make_failure("menu input invalid");
    }

    while (*end_pointer != '\0') {
        if (!isspace((unsigned char)*end_pointer)) {
            return Result_make_failure("menu input invalid");
        }

        end_pointer++;
    }

    *out_value = (int)value;
    return Result_make_success("menu input parsed");
}

Result MenuController_render_main_menu(char *buffer, size_t capacity) {
    size_t used = 0;

    if (buffer == 0 || capacity == 0) {
        return Result_make_failure("menu buffer invalid");
    }

    if (menu_append_hline(buffer, capacity, &used, "+", "+") < 0) {
        return Result_make_failure("menu buffer too small");
    }
    if (menu_append_title(buffer, capacity, &used, "登录与角色菜单") < 0) {
        return Result_make_failure("menu buffer too small");
    }
    if (menu_append_hline(buffer, capacity, &used, "+", "+") < 0) {
        return Result_make_failure("menu buffer too small");
    }
    if (menu_append_item(buffer, capacity, &used, 1, TUI_GEAR, "系统管理员", 0) < 0) {
        return Result_make_failure("menu buffer too small");
    }
    if (menu_append_item(buffer, capacity, &used, 2, TUI_MEDICAL, "医生", 0) < 0) {
        return Result_make_failure("menu buffer too small");
    }
    if (menu_append_item(buffer, capacity, &used, 3, TUI_HEART, "患者", 0) < 0) {
        return Result_make_failure("menu buffer too small");
    }
    if (menu_append_item(buffer, capacity, &used, 4, TUI_CIRCLE, "住院管理员", 0) < 0) {
        return Result_make_failure("menu buffer too small");
    }
    if (menu_append_item(buffer, capacity, &used, 5, TUI_FLASK, "药房人员", 0) < 0) {
        return Result_make_failure("menu buffer too small");
    }
    if (menu_append_item(buffer, capacity, &used, 6, TUI_SPARKLE, "重置演示数据", 0) < 0) {
        return Result_make_failure("menu buffer too small");
    }
    if (menu_append_item(buffer, capacity, &used, 0, TUI_ARROW_R, "退出系统", 1) < 0) {
        return Result_make_failure("menu buffer too small");
    }
    if (menu_append_hline(buffer, capacity, &used, "+", "+") < 0) {
        return Result_make_failure("menu buffer too small");
    }
    if (menu_append_footer(buffer, capacity, &used) < 0) {
        return Result_make_failure("menu buffer too small");
    }

    return Result_make_success("menu ready");
}

Result MenuController_render_role_menu(
    MenuRole role,
    char *buffer,
    size_t capacity
) {
    const MenuOption *options = 0;
    size_t option_count = 0;
    size_t index = 0;
    size_t used = 0;
    int is_dim = 0;

    if (buffer == 0 || capacity == 0) {
        return Result_make_failure("role menu buffer invalid");
    }

    options = MenuController_options_for_role(role, &option_count);
    if (options == 0 || option_count == 0) {
        return Result_make_failure("role menu not found");
    }

    if (menu_append_hline(buffer, capacity, &used, "+", "+") < 0) {
        return Result_make_failure("role menu buffer too small");
    }
    if (menu_append_title(buffer, capacity, &used,
                          MenuController_role_label(role)) < 0) {
        return Result_make_failure("role menu buffer too small");
    }
    if (menu_append_hline(buffer, capacity, &used, "+", "+") < 0) {
        return Result_make_failure("role menu buffer too small");
    }

    for (index = 0; index < option_count; index++) {
        is_dim = (options[index].action == MENU_ACTION_BACK) ? 1 : 0;
        if (menu_append_item(buffer, capacity, &used,
                             options[index].selection,
                             options[index].icon,
                             options[index].label,
                             is_dim) < 0) {
            return Result_make_failure("role menu buffer too small");
        }
    }

    if (menu_append_hline(buffer, capacity, &used, "+", "+") < 0) {
        return Result_make_failure("role menu buffer too small");
    }
    if (menu_append_footer(buffer, capacity, &used) < 0) {
        return Result_make_failure("role menu buffer too small");
    }

    return Result_make_success("role menu ready");
}

Result MenuController_parse_main_selection(const char *input, MenuRole *out_role) {
    int value = 0;
    Result result = MenuController_parse_number(input, &value);

    if (result.success == 0) {
        return result;
    }

    if (out_role == 0) {
        return Result_make_failure("role output missing");
    }

    switch (value) {
        case 0:
            *out_role = MENU_ROLE_EXIT;
            return Result_make_success("exit selected");
        case 1:
            *out_role = MENU_ROLE_ADMIN;
            return Result_make_success("admin selected");
        case 2:
            *out_role = MENU_ROLE_DOCTOR;
            return Result_make_success("doctor selected");
        case 3:
            *out_role = MENU_ROLE_PATIENT;
            return Result_make_success("patient selected");
        case 4:
            *out_role = MENU_ROLE_INPATIENT_MANAGER;
            return Result_make_success("inpatient selected");
        case 5:
            *out_role = MENU_ROLE_PHARMACY;
            return Result_make_success("pharmacy selected");
        case 6:
            *out_role = MENU_ROLE_RESET_DEMO;
            return Result_make_success("reset demo selected");
        default:
            return Result_make_failure("main menu selection invalid");
    }
}

Result MenuController_parse_role_selection(
    MenuRole role,
    const char *input,
    MenuAction *out_action
) {
    const MenuOption *options = 0;
    size_t option_count = 0;
    size_t index = 0;
    int value = 0;
    Result result = MenuController_parse_number(input, &value);

    if (result.success == 0) {
        return result;
    }

    if (out_action == 0) {
        return Result_make_failure("action output missing");
    }

    options = MenuController_options_for_role(role, &option_count);
    if (options == 0) {
        return Result_make_failure("role menu not found");
    }

    for (index = 0; index < option_count; index++) {
        if (options[index].selection == value) {
            *out_action = options[index].action;
            return Result_make_success("role action selected");
        }
    }

    return Result_make_failure("role menu selection invalid");
}

Result MenuController_format_action_feedback(
    MenuAction action,
    char *buffer,
    size_t capacity
) {
    const char *label = MenuController_action_label(action);
    int written = 0;

    if (buffer == 0 || capacity == 0) {
        return Result_make_failure("action feedback buffer invalid");
    }

    if (action == MENU_ACTION_INVALID || action == MENU_ACTION_BACK) {
        return Result_make_failure("action feedback unavailable");
    }

    written = snprintf(
        buffer,
        capacity,
        "编号: ACT-%03d\n状态: 已路由\n下一步: 进入[%s]流程。\n",
        (int)action,
        label
    );
    if (written < 0 || (size_t)written >= capacity) {
        return Result_make_failure("action feedback buffer too small");
    }

    return Result_make_success("action feedback ready");
}

const char *MenuController_role_label(MenuRole role) {
    switch (role) {
        case MENU_ROLE_ADMIN:
            return "系统管理员";
        case MENU_ROLE_DOCTOR:
            return "医生";
        case MENU_ROLE_PATIENT:
            return "患者";
        case MENU_ROLE_INPATIENT_MANAGER:
            return "住院管理员";
        case MENU_ROLE_PHARMACY:
            return "药房人员";
        case MENU_ROLE_RESET_DEMO:
            return "重置演示数据";
        case MENU_ROLE_EXIT:
            return "退出系统";
        default:
            return "未知角色";
    }
}

const char *MenuController_action_label(MenuAction action) {
    switch (action) {
        case MENU_ACTION_ADMIN_PATIENT_MANAGEMENT:
            return "患者信息管理";
        case MENU_ACTION_ADMIN_DOCTOR_DEPARTMENT:
            return "医生与科室管理";
        case MENU_ACTION_ADMIN_MEDICAL_RECORDS:
            return "医疗记录查询";
        case MENU_ACTION_ADMIN_WARD_BED_OVERVIEW:
            return "病房与床位总览";
        case MENU_ACTION_ADMIN_MEDICINE_OVERVIEW:
            return "药房与库存总览";
        case MENU_ACTION_DOCTOR_QUERY_PATIENT_HISTORY:
            return "查询患者信息与历史";
        case MENU_ACTION_DOCTOR_PENDING_LIST:
            return "待诊列表";
        case MENU_ACTION_DOCTOR_VISIT_RECORD:
            return "诊疗记录/诊断结果/医生建议";
        case MENU_ACTION_DOCTOR_PRESCRIPTION_STOCK:
            return "处方与发药/查询药品库存";
        case MENU_ACTION_DOCTOR_EXAM_RECORD:
            return "检查记录/检查结果";
        case MENU_ACTION_PATIENT_BASIC_INFO:
            return "基本信息";
        case MENU_ACTION_PATIENT_QUERY_REGISTRATION:
            return "挂号查询";
        case MENU_ACTION_PATIENT_QUERY_VISITS:
            return "个人看诊历史";
        case MENU_ACTION_PATIENT_QUERY_EXAMS:
            return "个人检查历史";
        case MENU_ACTION_PATIENT_QUERY_ADMISSIONS:
            return "个人住院历史";
        case MENU_ACTION_PATIENT_QUERY_DISPENSE:
            return "个人发药历史";
        case MENU_ACTION_PATIENT_QUERY_MEDICINE_USAGE:
            return "药品使用方法";
        case MENU_ACTION_INPATIENT_ADMIT:
            return "入院登记";
        case MENU_ACTION_INPATIENT_DISCHARGE:
            return "出院办理";
        case MENU_ACTION_INPATIENT_QUERY_RECORD:
            return "住院状态查询";
        case MENU_ACTION_INPATIENT_QUERY_BED:
            return "病区床位查询";
        case MENU_ACTION_INPATIENT_LIST_WARDS:
            return "查看病房信息";
        case MENU_ACTION_INPATIENT_LIST_BEDS:
            return "查看床位状态";
        case MENU_ACTION_INPATIENT_TRANSFER_BED:
            return "床位调整/转床";
        case MENU_ACTION_INPATIENT_DISCHARGE_CHECK:
            return "出院前检查";
        case MENU_ACTION_PHARMACY_ADD_MEDICINE:
            return "添加药品";
        case MENU_ACTION_PHARMACY_RESTOCK:
            return "药品入库";
        case MENU_ACTION_PHARMACY_DISPENSE:
            return "药品出库/发药";
        case MENU_ACTION_PHARMACY_QUERY_STOCK:
            return "库存盘点/查询库存";
        case MENU_ACTION_PHARMACY_LOW_STOCK:
            return "缺药提醒/库存不足提醒";
        case MENU_ACTION_BACK:
            return "返回上级菜单";
        default:
            return "未知操作";
    }
}

int MenuController_is_exit_role(MenuRole role) {
    return role == MENU_ROLE_EXIT ? 1 : 0;
}

int MenuController_is_back_action(MenuAction action) {
    return action == MENU_ACTION_BACK ? 1 : 0;
}
