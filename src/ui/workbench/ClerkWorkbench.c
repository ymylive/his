#include "ui/Workbench.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "domain/Department.h"
#include "domain/Doctor.h"
#include "raygui.h"
#include "ui/DesktopAdapters.h"
#include "ui/DesktopApp.h"

typedef struct ClerkPatientFormState {
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    char name[HIS_DOMAIN_NAME_CAPACITY];
    char age_text[16];
    char contact[HIS_DOMAIN_CONTACT_CAPACITY];
    char id_card[HIS_DOMAIN_CONTACT_CAPACITY];
    char allergy[HIS_DOMAIN_TEXT_CAPACITY];
    char medical_history[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    char remarks[HIS_DOMAIN_TEXT_CAPACITY];
    int gender_index;
    char output[WORKBENCH_OUTPUT_CAPACITY];
} ClerkPatientFormState;

typedef struct ClerkRegistrationViewState {
    char departments[WORKBENCH_OUTPUT_CAPACITY];
    char doctors[WORKBENCH_OUTPUT_CAPACITY];
    char output[WORKBENCH_OUTPUT_CAPACITY];
} ClerkRegistrationViewState;

static ClerkPatientFormState g_clerk_patient_form;
static ClerkRegistrationViewState g_clerk_registration_view;

static void show_result(DesktopApp *app, Result result) {
    DesktopAppState_show_message(
        &app->state,
        result.success ? DESKTOP_MESSAGE_SUCCESS : DESKTOP_MESSAGE_ERROR,
        result.message
    );
}

static void draw_text_input(
    Rectangle bounds,
    char *text,
    int text_size,
    int editable,
    int *active_field,
    int field_id
) {
    bool edit_mode = false;

    if (editable == 0 || active_field == 0) {
        GuiTextBox(bounds, text, text_size, false);
        return;
    }

    edit_mode = (*active_field == field_id);
    if (GuiTextBox(bounds, text, text_size, edit_mode)) {
        *active_field = edit_mode ? 0 : field_id;
    }
}

static void draw_output_panel(
    const DesktopApp *app,
    Rectangle rect,
    const char *title,
    const char *content,
    const char *empty_text
) {
    WorkbenchTextPanelLayout layout = Workbench_compute_text_panel_layout(rect, 14.0f, 22.0f, 14.0f);

    DrawRectangleRounded(rect, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(rect, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText(title, (int)layout.title_bounds.x, (int)layout.title_bounds.y, 20, app->theme.text_primary);
    DrawText(
        (content != 0 && content[0] != '\0') ? content : empty_text,
        (int)layout.content_bounds.x,
        (int)layout.content_bounds.y,
        17,
        app->theme.text_secondary
    );
}

static int parse_age(const char *text) {
    char *end = 0;
    long value = strtol(text, &end, 10);

    if (text == 0 || text[0] == '\0' || end == text || (end != 0 && *end != '\0')) {
        return -1;
    }
    return (int)value;
}

static void fill_form_from_selected_patient(const DesktopPatientPageState *page_state) {
    const Patient *patient = &page_state->selected_patient;

    if (page_state->has_selected_patient == 0) {
        return;
    }

    strncpy(g_clerk_patient_form.patient_id, patient->patient_id, sizeof(g_clerk_patient_form.patient_id) - 1);
    strncpy(g_clerk_patient_form.name, patient->name, sizeof(g_clerk_patient_form.name) - 1);
    snprintf(g_clerk_patient_form.age_text, sizeof(g_clerk_patient_form.age_text), "%d", patient->age);
    strncpy(g_clerk_patient_form.contact, patient->contact, sizeof(g_clerk_patient_form.contact) - 1);
    strncpy(g_clerk_patient_form.id_card, patient->id_card, sizeof(g_clerk_patient_form.id_card) - 1);
    strncpy(g_clerk_patient_form.allergy, patient->allergy, sizeof(g_clerk_patient_form.allergy) - 1);
    strncpy(g_clerk_patient_form.medical_history, patient->medical_history, sizeof(g_clerk_patient_form.medical_history) - 1);
    strncpy(g_clerk_patient_form.remarks, patient->remarks, sizeof(g_clerk_patient_form.remarks) - 1);
    g_clerk_patient_form.gender_index = patient->gender == PATIENT_GENDER_MALE ? 0 : 1;
}

static void clerk_draw_home(DesktopApp *app, Rectangle panel) {
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_REGISTRATION_CLERK);
    const char *labels[4] = { "今日挂号", "患者档案", "待接待", "最近成功" };
    char registrations[16];
    char patients[16];
    const char *values[4];
    WorkbenchButtonGroupLayout actions;
    Result result;
    int index = 0;

    if (app->state.dashboard.loaded == 0) {
        result = DesktopAdapters_load_dashboard(&app->application, &app->state.dashboard);
        show_result(app, result);
        app->state.dashboard.loaded = result.success ? 1 : -1;
    }

    snprintf(registrations, sizeof(registrations), "%d", app->state.dashboard.registration_count);
    snprintf(patients, sizeof(patients), "%d", app->state.dashboard.patient_count);
    values[0] = registrations;
    values[1] = patients;
    values[2] = app->state.registration_page.patient_id[0] != '\0' ? app->state.registration_page.patient_id : "--";
    values[3] = app->state.registration_page.last_registration_id[0] != '\0'
        ? app->state.registration_page.last_registration_id : "--";

    Workbench_draw_home_cards(app, panel, labels, values, wb->accent);
    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y + 104, "接待台快捷入口");
    actions = Workbench_compute_button_group_layout(
        (Rectangle){ panel.x, panel.y + 144.0f, panel.width, 40.0f },
        4,
        150.0f,
        40.0f,
        14.0f
    );
    for (index = 0; index < actions.count; index++) {
        int clicked = 0;
        const char *label = index == 0 ? "患者建档" :
                            index == 1 ? "患者查询" :
                            index == 2 ? "挂号办理" :
                                         "挂号查询";
        Workbench_draw_quick_action_btn(app, actions.buttons[index], label, wb->accent, &clicked);
        if (clicked) {
            app->state.workbench_page = index + 1;
        }
    }
    draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 214.0f, panel.width, panel.height - 214.0f },
        "接待说明",
        "挂号员工作台已接通患者建档/修改、患者查询、挂号办理、挂号查询与取消。",
        "暂无说明"
    );
}

static void clerk_draw_patient_create(DesktopApp *app, Rectangle panel) {
    DesktopPatientPageState *page_state = &app->state.patient_page;
    Rectangle form_bounds = { panel.x, panel.y, panel.width * 0.56f, panel.height };
    Rectangle output_bounds = { panel.x + panel.width * 0.60f, panel.y, panel.width * 0.40f, panel.height };
    int age = 0;
    Patient patient;
    Result result;

    if (page_state->has_selected_patient != 0 &&
        strcmp(g_clerk_patient_form.patient_id, page_state->selected_patient.patient_id) != 0) {
        fill_form_from_selected_patient(page_state);
    }

    DrawRectangleRounded(form_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(form_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("患者建档与修改", (int)form_bounds.x + 16, (int)form_bounds.y + 14, 24, app->theme.text_primary);

    /* Basic Information Section */
    Workbench_draw_section_header(app, (int)form_bounds.x + 16, (int)form_bounds.y + 50, "基本信息");

    Workbench_draw_form_label(app, (int)form_bounds.x + 16, (int)form_bounds.y + 84, "患者编号", 1);
    draw_text_input((Rectangle){ form_bounds.x + 16, form_bounds.y + 106, 220, 34 }, g_clerk_patient_form.patient_id, sizeof(g_clerk_patient_form.patient_id), 1, &page_state->active_field, 1);

    Workbench_draw_form_label(app, (int)form_bounds.x + 252, (int)form_bounds.y + 84, "姓名", 1);
    draw_text_input((Rectangle){ form_bounds.x + 252, form_bounds.y + 106, 220, 34 }, g_clerk_patient_form.name, sizeof(g_clerk_patient_form.name), 1, &page_state->active_field, 2);

    Workbench_draw_form_label(app, (int)form_bounds.x + 16, (int)form_bounds.y + 150, "年龄", 1);
    draw_text_input((Rectangle){ form_bounds.x + 16, form_bounds.y + 172, 100, 34 }, g_clerk_patient_form.age_text, sizeof(g_clerk_patient_form.age_text), 1, &page_state->active_field, 3);

    Workbench_draw_form_label(app, (int)form_bounds.x + 132, (int)form_bounds.y + 150, "性别", 1);
    GuiToggleGroup(
        (Rectangle){ form_bounds.x + 132, form_bounds.y + 172, 180, 30 },
        "男;女",
        &g_clerk_patient_form.gender_index
    );

    Workbench_draw_form_label(app, (int)form_bounds.x + 328, (int)form_bounds.y + 150, "联系方式", 1);
    draw_text_input((Rectangle){ form_bounds.x + 328, form_bounds.y + 172, 220, 34 }, g_clerk_patient_form.contact, sizeof(g_clerk_patient_form.contact), 1, &page_state->active_field, 4);

    /* Identity Section */
    Workbench_draw_section_header(app, (int)form_bounds.x + 16, (int)form_bounds.y + 226, "身份信息");

    Workbench_draw_form_label(app, (int)form_bounds.x + 16, (int)form_bounds.y + 260, "身份证号", 0);
    draw_text_input((Rectangle){ form_bounds.x + 16, form_bounds.y + 282, form_bounds.width - 32, 34 }, g_clerk_patient_form.id_card, sizeof(g_clerk_patient_form.id_card), 1, &page_state->active_field, 5);

    /* Medical History Section */
    Workbench_draw_section_header(app, (int)form_bounds.x + 16, (int)form_bounds.y + 336, "医疗信息");

    Workbench_draw_form_label(app, (int)form_bounds.x + 16, (int)form_bounds.y + 370, "过敏史", 0);
    draw_text_input((Rectangle){ form_bounds.x + 16, form_bounds.y + 392, form_bounds.width - 32, 34 }, g_clerk_patient_form.allergy, sizeof(g_clerk_patient_form.allergy), 1, &page_state->active_field, 6);

    Workbench_draw_form_label(app, (int)form_bounds.x + 16, (int)form_bounds.y + 436, "病史", 0);
    draw_text_input((Rectangle){ form_bounds.x + 16, form_bounds.y + 458, form_bounds.width - 32, 34 }, g_clerk_patient_form.medical_history, sizeof(g_clerk_patient_form.medical_history), 1, &page_state->active_field, 7);

    Workbench_draw_form_label(app, (int)form_bounds.x + 16, (int)form_bounds.y + 502, "备注", 0);
    draw_text_input((Rectangle){ form_bounds.x + 16, form_bounds.y + 524, form_bounds.width - 32, 34 }, g_clerk_patient_form.remarks, sizeof(g_clerk_patient_form.remarks), 1, &page_state->active_field, 8);

    /* Action Buttons */
    if (GuiButton((Rectangle){ form_bounds.x + 16, form_bounds.y + 580, 150, 38 }, "新增患者")) {
        age = parse_age(g_clerk_patient_form.age_text);
        if (age < 0) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "年龄格式无效");
        } else {
            memset(&patient, 0, sizeof(patient));
            strncpy(patient.patient_id, g_clerk_patient_form.patient_id, sizeof(patient.patient_id) - 1);
            strncpy(patient.name, g_clerk_patient_form.name, sizeof(patient.name) - 1);
            patient.gender = g_clerk_patient_form.gender_index == 0 ? PATIENT_GENDER_MALE : PATIENT_GENDER_FEMALE;
            patient.age = age;
            strncpy(patient.contact, g_clerk_patient_form.contact, sizeof(patient.contact) - 1);
            strncpy(patient.id_card, g_clerk_patient_form.id_card, sizeof(patient.id_card) - 1);
            strncpy(patient.allergy, g_clerk_patient_form.allergy, sizeof(patient.allergy) - 1);
            strncpy(patient.medical_history, g_clerk_patient_form.medical_history, sizeof(patient.medical_history) - 1);
            strncpy(patient.remarks, g_clerk_patient_form.remarks, sizeof(patient.remarks) - 1);
            result = MenuApplication_add_patient(&app->application, &patient, g_clerk_patient_form.output, sizeof(g_clerk_patient_form.output));
            show_result(app, result);
            if (result.success) {
                DesktopApp_mark_dirty(&app->state, DESKTOP_PAGE_DASHBOARD);
            }
        }
    }
    if (GuiButton((Rectangle){ form_bounds.x + 184, form_bounds.y + 580, 150, 38 }, "更新患者")) {
        age = parse_age(g_clerk_patient_form.age_text);
        if (age < 0) {
            DesktopAppState_show_message(&app->state, DESKTOP_MESSAGE_ERROR, "年龄格式无效");
        } else {
            memset(&patient, 0, sizeof(patient));
            strncpy(patient.patient_id, g_clerk_patient_form.patient_id, sizeof(patient.patient_id) - 1);
            strncpy(patient.name, g_clerk_patient_form.name, sizeof(patient.name) - 1);
            patient.gender = g_clerk_patient_form.gender_index == 0 ? PATIENT_GENDER_MALE : PATIENT_GENDER_FEMALE;
            patient.age = age;
            strncpy(patient.contact, g_clerk_patient_form.contact, sizeof(patient.contact) - 1);
            strncpy(patient.id_card, g_clerk_patient_form.id_card, sizeof(patient.id_card) - 1);
            strncpy(patient.allergy, g_clerk_patient_form.allergy, sizeof(patient.allergy) - 1);
            strncpy(patient.medical_history, g_clerk_patient_form.medical_history, sizeof(patient.medical_history) - 1);
            strncpy(patient.remarks, g_clerk_patient_form.remarks, sizeof(patient.remarks) - 1);
            result = MenuApplication_update_patient(&app->application, &patient, g_clerk_patient_form.output, sizeof(g_clerk_patient_form.output));
            show_result(app, result);
        }
    }

    draw_output_panel(
        app,
        output_bounds,
        "建档结果",
        g_clerk_patient_form.output,
        "可以先去【患者查询】选中患者，再回到本页快速修改。"
    );
}

static void clerk_draw_patient_search(DesktopApp *app, Rectangle panel) {
    DesktopPatientPageState *state = &app->state.patient_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(
        (Rectangle){ panel.x, panel.y + 56.0f, panel.width, panel.height - 56.0f },
        0.46f,
        18.0f,
        260.0f
    );
    const LinkedListNode *current = 0;
    Result result;
    int row = 0;
    int search_clicked = 0;

    {
        int active_mode = (int)state->search_mode;
        GuiToggleGroup((Rectangle){ panel.x, panel.y, 220, 30 }, "按编号;按姓名", &active_mode);
        state->search_mode = (DesktopPatientSearchMode)active_mode;
    }

    Workbench_draw_search_box(
        app,
        (Rectangle){ panel.x + 236, panel.y, panel.width - 236, 34 },
        state->query, sizeof(state->query),
        &state->active_field, 1,
        &search_clicked
    );

    if (search_clicked) {
        LinkedList_clear(&state->results, free);
        LinkedList_init(&state->results);
        result = DesktopAdapters_search_patients(&app->application, state->search_mode, state->query, &state->results);
        show_result(app, result);
        state->selected_index = -1;
        state->has_selected_patient = 0;
    }

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawRectangleRounded(split.detail_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.detail_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("患者列表", (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 14, 20, app->theme.text_primary);
    DrawText("患者详情", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 14, 20, app->theme.text_primary);

    current = state->results.head;
    while (current != 0 && row < 9) {
        const Patient *patient = (const Patient *)current->data;
        Rectangle row_bounds = {
            split.list_bounds.x + 14,
            split.list_bounds.y + 48 + row * 38.0f,
            split.list_bounds.width - 28,
            30
        };
        int is_selected = state->has_selected_patient &&
                         strcmp(state->selected_patient.patient_id, patient->patient_id) == 0;

        if (is_selected) {
            DrawRectangleRounded(row_bounds, 0.15f, 8, Fade(app->theme.border, 0.2f));
        }

        if (GuiButton(row_bounds, TextFormat("%s | %s", patient->patient_id, patient->name))) {
            state->selected_index = row;
            state->selected_patient = *patient;
            state->has_selected_patient = 1;
        }
        current = current->next;
        row++;
    }
    if (state->has_selected_patient) {
        float detail_y = split.detail_bounds.y + 56;
        Workbench_draw_info_row(app, (int)split.detail_bounds.x + 16, (int)detail_y, "编号", state->selected_patient.patient_id);
        detail_y += 32;
        Workbench_draw_info_row(app, (int)split.detail_bounds.x + 16, (int)detail_y, "姓名", state->selected_patient.name);
        detail_y += 32;
        Workbench_draw_info_row(app, (int)split.detail_bounds.x + 16, (int)detail_y, "联系方式", state->selected_patient.contact);
        detail_y += 32;
        Workbench_draw_info_row(app, (int)split.detail_bounds.x + 16, (int)detail_y, "过敏史", state->selected_patient.allergy);
        detail_y += 32;

        Rectangle status_badge = { split.detail_bounds.x + 126, detail_y, 80, 26 };
        Color status_color = state->selected_patient.is_inpatient ?
                            (Color){ 245, 158, 11, 255 } : (Color){ 34, 197, 94, 255 };
        Workbench_draw_status_badge(app, status_badge,
                                    state->selected_patient.is_inpatient ? "住院" : "门诊",
                                    status_color);
        DrawText("住院状态", (int)split.detail_bounds.x + 16, (int)detail_y + 4, 18, app->theme.text_secondary);

        detail_y += 48;
        if (GuiButton((Rectangle){ split.detail_bounds.x + 16, detail_y, 150, 36 }, "带入建档表单")) {
            fill_form_from_selected_patient(state);
            app->state.workbench_page = 1;
        }
    } else {
        DrawText("查询后选择一名患者，可带入建档表单继续修改。", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 56, 17, app->theme.text_secondary);
    }
}

static void clerk_draw_registration(DesktopApp *app, Rectangle panel) {
    DesktopRegistrationPageState *state = &app->state.registration_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.46f, 18.0f, 280.0f);
    WorkbenchButtonGroupLayout actions;
    Result result;
    int index = 0;

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("挂号办理", (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 14, 24, app->theme.text_primary);

    /* Step 1: Patient Information */
    Workbench_draw_section_header(app, (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 54, "步骤 1: 患者信息");

    Workbench_draw_form_label(app, (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 88, "患者编号", 1);
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 110, split.list_bounds.width - 32, 34 }, state->patient_id, sizeof(state->patient_id), 1, &state->active_field, 1);

    /* Step 2: Department and Doctor */
    Workbench_draw_section_header(app, (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 158, "步骤 2: 科室与医生");

    Workbench_draw_form_label(app, (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 192, "科室编号", 1);
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 214, split.list_bounds.width - 32, 34 }, state->department_id, sizeof(state->department_id), 1, &state->active_field, 2);

    Workbench_draw_form_label(app, (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 258, "医生编号", 1);
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 280, split.list_bounds.width - 32, 34 }, state->doctor_id, sizeof(state->doctor_id), 1, &state->active_field, 3);

    /* Step 3: Appointment Time */
    Workbench_draw_section_header(app, (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 328, "步骤 3: 挂号时间");

    Workbench_draw_form_label(app, (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 362, "挂号时间", 1);
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 384, split.list_bounds.width - 32, 34 }, state->registered_at, sizeof(state->registered_at), 1, &state->active_field, 4);

    /* Action Buttons */
    actions = Workbench_compute_button_group_layout(
        (Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 440.0f, split.list_bounds.width - 32, 36.0f },
        3,
        110.0f,
        36.0f,
        12.0f
    );
    for (index = 0; index < actions.count; index++) {
        int clicked = 0;
        const char *label = index == 0 ? "科室列表" : index == 1 ? "医生列表" : "提交挂号";
        Workbench_draw_quick_action_btn(app, actions.buttons[index], label, app->theme.nav_active, &clicked);
        if (!clicked) {
            continue;
        }
        if (index == 0) {
            result = DesktopAdapters_list_departments(&app->application, g_clerk_registration_view.departments, sizeof(g_clerk_registration_view.departments));
            show_result(app, result);
        } else if (index == 1) {
            result = DesktopAdapters_list_doctors_by_department(&app->application, state->department_id, g_clerk_registration_view.doctors, sizeof(g_clerk_registration_view.doctors));
            show_result(app, result);
        } else {
            Registration registration;
            memset(&registration, 0, sizeof(registration));
            result = DesktopAdapters_submit_registration(
                &app->application,
                state->patient_id,
                state->doctor_id,
                state->department_id,
                state->registered_at,
                &registration
            );
            show_result(app, result);
            if (result.success) {
                strncpy(state->last_registration_id, registration.registration_id, sizeof(state->last_registration_id) - 1);
                DesktopAdapters_query_registrations_by_patient(
                    &app->application,
                    state->patient_id,
                    g_clerk_registration_view.output,
                    sizeof(g_clerk_registration_view.output)
                );
                DesktopApp_mark_dirty(&app->state, DESKTOP_PAGE_DASHBOARD);
            }
        }
    }

    DrawRectangleRounded(split.detail_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.detail_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    draw_output_panel(
        app,
        (Rectangle){ split.detail_bounds.x + 12, split.detail_bounds.y + 12, split.detail_bounds.width - 24, (split.detail_bounds.height - 40) / 3.0f },
        "科室列表",
        g_clerk_registration_view.departments,
        "用于前台向患者展示科室。"
    );
    draw_output_panel(
        app,
        (Rectangle){ split.detail_bounds.x + 12, split.detail_bounds.y + 18 + (split.detail_bounds.height - 40) / 3.0f, split.detail_bounds.width - 24, (split.detail_bounds.height - 40) / 3.0f },
        "医生列表",
        g_clerk_registration_view.doctors,
        "先输入科室编号，再查询该科医生。"
    );
    draw_output_panel(
        app,
        (Rectangle){ split.detail_bounds.x + 12, split.detail_bounds.y + 24 + (split.detail_bounds.height - 40) / 3.0f * 2.0f, split.detail_bounds.width - 24, (split.detail_bounds.height - 40) / 3.0f },
        "挂号结果",
        g_clerk_registration_view.output,
        "成功挂号后，这里会刷新当前患者的挂号记录。"
    );
}

static void clerk_draw_reg_query(DesktopApp *app, Rectangle panel) {
    static char query_registration_id[HIS_DOMAIN_ID_CAPACITY];
    static char cancel_registration_id[HIS_DOMAIN_ID_CAPACITY];
    static char cancel_time[HIS_DOMAIN_TIME_CAPACITY];
    static char query_patient_id[HIS_DOMAIN_ID_CAPACITY];
    static char output[WORKBENCH_OUTPUT_CAPACITY];
    static int active_field = 0;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.48f, 18.0f, 280.0f);
    Result result;
    int search_clicked = 0;

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("挂号查询与取消", (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 14, 24, app->theme.text_primary);

    /* Query by Registration ID */
    Workbench_draw_section_header(app, (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 54, "按挂号编号查询");

    Workbench_draw_search_box(
        app,
        (Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 88, split.list_bounds.width - 32, 34 },
        query_registration_id, sizeof(query_registration_id),
        &active_field, 1,
        &search_clicked
    );

    if (search_clicked) {
        result = DesktopAdapters_query_registration(&app->application, query_registration_id, output, sizeof(output));
        show_result(app, result);
        search_clicked = 0;
    }

    /* Query by Patient ID */
    Workbench_draw_section_header(app, (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 136, "按患者编号查询");

    search_clicked = 0;
    Workbench_draw_search_box(
        app,
        (Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 170, split.list_bounds.width - 32, 34 },
        query_patient_id, sizeof(query_patient_id),
        &active_field, 2,
        &search_clicked
    );

    if (search_clicked) {
        result = DesktopAdapters_query_registrations_by_patient(&app->application, query_patient_id, output, sizeof(output));
        show_result(app, result);
    }

    /* Cancel Registration */
    Workbench_draw_section_header(app, (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 218, "取消挂号");

    Workbench_draw_form_label(app, (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 252, "挂号编号", 1);
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 274, split.list_bounds.width - 32, 34 }, cancel_registration_id, sizeof(cancel_registration_id), 1, &active_field, 3);

    Workbench_draw_form_label(app, (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 318, "取消时间", 1);
    draw_text_input((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 340, split.list_bounds.width - 32, 34 }, cancel_time, sizeof(cancel_time), 1, &active_field, 4);

    if (GuiButton((Rectangle){ split.list_bounds.x + 16, split.list_bounds.y + 388, 120, 36 }, "取消挂号")) {
        result = DesktopAdapters_cancel_registration(&app->application, cancel_registration_id, cancel_time, output, sizeof(output));
        show_result(app, result);
    }

    draw_output_panel(
        app,
        split.detail_bounds,
        "挂号查询结果",
        output,
        "可按挂号编号查询，也可按患者编号查询并执行取消。"
    );
}

void ClerkWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    switch (page) {
        case 0:
            clerk_draw_home(app, panel);
            break;
        case 1:
            clerk_draw_patient_create(app, panel);
            break;
        case 2:
            clerk_draw_patient_search(app, panel);
            break;
        case 3:
            clerk_draw_registration(app, panel);
            break;
        case 4:
            clerk_draw_reg_query(app, panel);
            break;
        default:
            clerk_draw_home(app, panel);
            break;
    }
}
