#ifndef HIS_UI_MENU_CONTROLLER_H
#define HIS_UI_MENU_CONTROLLER_H

#include <stddef.h>

#include "common/Result.h"

typedef enum MenuRole {
    MENU_ROLE_INVALID = 0,
    MENU_ROLE_ADMIN = 1,
    MENU_ROLE_DOCTOR = 2,
    MENU_ROLE_PATIENT = 3,
    MENU_ROLE_INPATIENT_MANAGER = 4,
    MENU_ROLE_PHARMACY = 5,
    MENU_ROLE_RESET_DEMO = 6,
    MENU_ROLE_EXIT = 99
} MenuRole;

typedef enum MenuAction {
    MENU_ACTION_INVALID = 0,
    MENU_ACTION_BACK = 1,

    MENU_ACTION_ADMIN_PATIENT_MANAGEMENT = 101,
    MENU_ACTION_ADMIN_DOCTOR_DEPARTMENT = 102,
    MENU_ACTION_ADMIN_MEDICAL_RECORDS = 103,
    MENU_ACTION_ADMIN_WARD_BED_OVERVIEW = 104,
    MENU_ACTION_ADMIN_MEDICINE_OVERVIEW = 105,

    MENU_ACTION_DOCTOR_PENDING_LIST = 301,
    MENU_ACTION_DOCTOR_QUERY_PATIENT_HISTORY = 302,
    MENU_ACTION_DOCTOR_VISIT_RECORD = 303,
    MENU_ACTION_DOCTOR_PRESCRIPTION_STOCK = 304,
    MENU_ACTION_DOCTOR_EXAM_RECORD = 305,

    MENU_ACTION_PATIENT_BASIC_INFO = 401,
    MENU_ACTION_PATIENT_QUERY_REGISTRATION = 402,
    MENU_ACTION_PATIENT_QUERY_VISITS = 403,
    MENU_ACTION_PATIENT_QUERY_EXAMS = 404,
    MENU_ACTION_PATIENT_QUERY_ADMISSIONS = 405,
    MENU_ACTION_PATIENT_QUERY_DISPENSE = 406,
    MENU_ACTION_PATIENT_QUERY_MEDICINE_USAGE = 407,

    MENU_ACTION_INPATIENT_ADMIT = 501,
    MENU_ACTION_INPATIENT_DISCHARGE = 502,
    MENU_ACTION_INPATIENT_QUERY_RECORD = 503,
    MENU_ACTION_INPATIENT_QUERY_BED = 504,
    MENU_ACTION_INPATIENT_LIST_WARDS = 505,
    MENU_ACTION_INPATIENT_LIST_BEDS = 506,
    MENU_ACTION_INPATIENT_TRANSFER_BED = 507,
    MENU_ACTION_INPATIENT_DISCHARGE_CHECK = 508,

    MENU_ACTION_PHARMACY_ADD_MEDICINE = 701,
    MENU_ACTION_PHARMACY_RESTOCK = 702,
    MENU_ACTION_PHARMACY_DISPENSE = 703,
    MENU_ACTION_PHARMACY_QUERY_STOCK = 704,
    MENU_ACTION_PHARMACY_LOW_STOCK = 705
} MenuAction;

Result MenuController_render_main_menu(char *buffer, size_t capacity);
Result MenuController_render_role_menu(
    MenuRole role,
    char *buffer,
    size_t capacity
);
Result MenuController_parse_main_selection(const char *input, MenuRole *out_role);
Result MenuController_parse_role_selection(
    MenuRole role,
    const char *input,
    MenuAction *out_action
);
Result MenuController_format_action_feedback(
    MenuAction action,
    char *buffer,
    size_t capacity
);
const char *MenuController_role_label(MenuRole role);
const char *MenuController_action_label(MenuAction action);
int MenuController_is_exit_role(MenuRole role);
int MenuController_is_back_action(MenuAction action);

#endif
