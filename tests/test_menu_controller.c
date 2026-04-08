#include <assert.h>
#include <string.h>

#include "ui/MenuController.h"

static void test_render_main_menu_contains_all_roles(void) {
    char buffer[8192];
    Result result = MenuController_render_main_menu(buffer, sizeof(buffer));

    assert(result.success == 1);
    assert(strstr(buffer, "系统管理员") != 0);
    assert(strstr(buffer, "医生") != 0);
    assert(strstr(buffer, "患者") != 0);
    assert(strstr(buffer, "住院管理员") != 0);
    assert(strstr(buffer, "药房人员") != 0);
    assert(strstr(buffer, "重置演示数据") != 0);
}

static void test_parse_main_menu_selection(void) {
    MenuRole role = MENU_ROLE_INVALID;
    Result result = MenuController_parse_main_selection(" 2 ", &role);

    assert(result.success == 1);
    assert(role == MENU_ROLE_DOCTOR);

    result = MenuController_parse_main_selection("0", &role);
    assert(result.success == 1);
    assert(MenuController_is_exit_role(role) == 1);

    result = MenuController_parse_main_selection("6", &role);
    assert(result.success == 1);
    assert(role == MENU_ROLE_RESET_DEMO);

    result = MenuController_parse_main_selection("7", &role);
    assert(result.success == 0);
}

static void test_render_patient_and_inpatient_menus(void) {
    char buffer[8192];
    Result result = MenuController_render_role_menu(
        MENU_ROLE_PATIENT,
        buffer,
        sizeof(buffer)
    );

    assert(result.success == 1);
    assert(strstr(buffer, "基本信息") != 0);
    assert(strstr(buffer, "挂号查询") != 0);
    assert(strstr(buffer, "个人看诊历史") != 0);
    assert(strstr(buffer, "个人检查历史") != 0);
    assert(strstr(buffer, "个人住院历史") != 0);
    assert(strstr(buffer, "个人发药历史") != 0);
    assert(strstr(buffer, "药品使用方法") != 0);

    result = MenuController_render_role_menu(
        MENU_ROLE_INPATIENT_MANAGER,
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "病区床位查询") != 0);
    assert(strstr(buffer, "入院登记") != 0);
    assert(strstr(buffer, "出院办理") != 0);
    assert(strstr(buffer, "住院状态查询") != 0);
    assert(strstr(buffer, "查看病房信息") != 0);
    assert(strstr(buffer, "床位调整/转床") != 0);
    assert(strstr(buffer, "出院前检查") != 0);

    result = MenuController_render_role_menu(
        MENU_ROLE_DOCTOR,
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "待诊列表") != 0);
    assert(strstr(buffer, "处方与发药/查询药品库存") != 0);
    assert(strstr(buffer, "检查记录/检查结果") != 0);

    result = MenuController_render_role_menu(
        MENU_ROLE_ADMIN,
        buffer,
        sizeof(buffer)
    );
    assert(result.success == 1);
    assert(strstr(buffer, "按科室查看医生") != 0);
    assert(strstr(buffer, "按时间范围查询") != 0);
    assert(strstr(buffer, "库存不足提醒") != 0);
}

static void test_parse_role_selection_and_back(void) {
    MenuAction action = MENU_ACTION_INVALID;
    Result result = MenuController_parse_role_selection(
        MENU_ROLE_PHARMACY,
        "3",
        &action
    );

    assert(result.success == 1);
    assert(action == MENU_ACTION_PHARMACY_DISPENSE);

    result = MenuController_parse_role_selection(
        MENU_ROLE_INPATIENT_MANAGER,
        "0",
        &action
    );
    assert(result.success == 1);
    assert(MenuController_is_back_action(action) == 1);

    result = MenuController_parse_role_selection(
        MENU_ROLE_DOCTOR,
        "9",
        &action
    );
    assert(result.success == 0);
}

static void test_format_action_feedback(void) {
    char buffer[256];
    Result result = MenuController_format_action_feedback(
        MENU_ACTION_PATIENT_QUERY_MEDICINE_USAGE,
        buffer,
        sizeof(buffer)
    );

    assert(result.success == 1);
    assert(strstr(buffer, "编号: ACT-407") != 0);
    assert(strstr(buffer, "状态: 已路由") != 0);
    assert(strstr(buffer, "下一步: 进入[药品使用方法]流程。") != 0);
}

int main(void) {
    test_render_main_menu_contains_all_roles();
    test_parse_main_menu_selection();
    test_render_patient_and_inpatient_menus();
    test_parse_role_selection_and_back();
    test_format_action_feedback();
    return 0;
}
