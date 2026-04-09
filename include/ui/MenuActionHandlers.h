/**
 * @file MenuActionHandlers.h
 * @brief Domain-specific action handler declarations for the HIS menu system
 *
 * This header declares the dispatch functions for each business domain,
 * enabling MenuApplication_execute_action to delegate to domain-specific
 * handler files instead of containing all case logic in a single function.
 *
 * It also exposes shared prompt/UI helper types and functions that are
 * used across multiple domain handlers.
 */

#ifndef MENU_ACTION_HANDLERS_H
#define MENU_ACTION_HANDLERS_H

#include <stdio.h>
#include "common/Result.h"
#include "ui/MenuApplication.h"
#include "ui/MenuController.h"
#include "domain/Patient.h"
#include "domain/Doctor.h"
#include "domain/Department.h"
#include "domain/Medicine.h"

/* ─── Shared types used by all domain handlers ─────────────────────── */

/**
 * @brief Interactive prompt context - holds input/output stream references
 */
typedef struct MenuApplicationPromptContext {
    FILE *input;   /**< Input stream (typically stdin) */
    FILE *output;  /**< Output stream (typically stdout) */
} MenuApplicationPromptContext;

#define MENU_APPLICATION_SELECT_OPTION_MAX 256  /**< Maximum selection candidates */

/**
 * @brief Selection candidate for fuzzy-search selector
 */
typedef struct MenuApplicationSelectionOption {
    char id[HIS_DOMAIN_ID_CAPACITY];
    char label[256];
} MenuApplicationSelectionOption;

/* ─── Shared prompt/UI helper functions ────────────────────────────── */

Result MenuApplication_prompt_line(
    MenuApplicationPromptContext *context,
    const char *prompt,
    char *buffer,
    size_t capacity
);

Result MenuApplication_prompt_int(
    MenuApplicationPromptContext *context,
    const char *prompt,
    int *out_value
);

void MenuApplication_copy_text(
    char *destination,
    size_t capacity,
    const char *source
);

void MenuApplication_print_result(FILE *out, const char *text, int success);

/* ─── Selection helpers ────────────────────────────────────────────── */

Result MenuApplication_prompt_select_patient(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    char *out_patient_id,
    size_t out_patient_id_capacity
);

Result MenuApplication_prompt_select_department(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    char *out_department_id,
    size_t out_department_id_capacity
);

Result MenuApplication_prompt_select_doctor(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *department_id_filter,
    char *out_doctor_id,
    size_t out_doctor_id_capacity
);

Result MenuApplication_prompt_select_registration(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *patient_id_filter,
    const char *doctor_id_filter,
    int status_filter,
    int apply_status_filter,
    char *out_registration_id,
    size_t out_registration_id_capacity
);

Result MenuApplication_prompt_select_visit(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *patient_id_filter,
    const char *doctor_id_filter,
    char *out_visit_id,
    size_t out_visit_id_capacity
);

Result MenuApplication_prompt_select_examination(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *doctor_id_filter,
    int pending_only,
    char *out_examination_id,
    size_t out_examination_id_capacity
);

Result MenuApplication_prompt_select_ward(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    char *out_ward_id,
    size_t out_ward_id_capacity
);

Result MenuApplication_prompt_select_bed(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *ward_id_filter,
    int available_only,
    int occupied_only,
    char *out_bed_id,
    size_t out_bed_id_capacity
);

Result MenuApplication_prompt_select_admission(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *patient_id_filter,
    int active_only,
    char *out_admission_id,
    size_t out_admission_id_capacity
);

Result MenuApplication_prompt_select_medicine(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *department_id_filter,
    char *out_medicine_id,
    size_t out_medicine_id_capacity
);

/* ─── Form helpers ─────────────────────────────────────────────────── */

Result MenuApplication_prompt_patient_form(
    MenuApplicationPromptContext *context,
    Patient *out_patient,
    int require_patient_id
);

Result MenuApplication_prompt_medicine_form(
    MenuApplicationPromptContext *context,
    Medicine *out_medicine,
    int require_medicine_id
);

Result MenuApplication_prompt_department_form(
    MenuApplicationPromptContext *context,
    Department *out_department,
    int require_department_id
);

Result MenuApplication_prompt_doctor_form(
    MenuApplicationPromptContext *context,
    Doctor *out_doctor,
    int require_doctor_id
);

/* ─── Session/query helpers ────────────────────────────────────────── */

Result MenuApplication_require_patient_session(MenuApplication *application);

Result MenuApplication_query_dispense_history_by_patient_id(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);

/* ─── Table/card print helpers ─────────────────────────────────────── */

void MenuApplication_print_ward_table(MenuApplication *application, FILE *out);
void MenuApplication_print_bed_table(MenuApplication *application, FILE *out, const char *ward_id);
void MenuApplication_print_low_stock_table(MenuApplication *application, FILE *out);
void MenuApplication_print_patient_card(MenuApplication *application, FILE *out, const char *patient_id);
void MenuApplication_print_pending_table(MenuApplication *application, FILE *out, const char *doctor_id);

/* ─── Domain dispatch functions ────────────────────────────────────── */

/**
 * @brief Handle admin menu actions (patient mgmt, doctor/dept, records, wards, medicine overview)
 */
Result MenuAction_handle_admin(MenuApplication *app, MenuAction action, FILE *input, FILE *output);

/**
 * @brief Handle doctor menu actions (pending list, patient history, visit record, exam record, stock query)
 */
Result MenuAction_handle_doctor(MenuApplication *app, MenuAction action, FILE *input, FILE *output);

/**
 * @brief Handle patient menu actions (basic info, registration, visits, exams, admissions, dispense, medicine usage)
 */
Result MenuAction_handle_patient(MenuApplication *app, MenuAction action, FILE *input, FILE *output);

/**
 * @brief Handle inpatient menu actions (admit, discharge, query, beds, wards, transfer, discharge check)
 */
Result MenuAction_handle_inpatient(MenuApplication *app, MenuAction action, FILE *input, FILE *output);

/**
 * @brief Handle pharmacy menu actions (add medicine, restock, dispense, stock query, low stock)
 */
Result MenuAction_handle_pharmacy(MenuApplication *app, MenuAction action, FILE *input, FILE *output);

#endif
