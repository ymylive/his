#include "ui/Workbench.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "repository/DepartmentRepository.h"
#include "repository/DoctorRepository.h"
#include "raygui.h"
#include "ui/DesktopAdapters.h"
#include "ui/DesktopApp.h"

typedef struct PatientRegistrationViewState {
    char registrations[WORKBENCH_OUTPUT_CAPACITY];
} PatientRegistrationViewState;

static PatientRegistrationViewState g_patient_registration_view;
static WorkbenchSearchSelectState g_patient_department_select;
static WorkbenchSearchSelectState g_patient_doctor_select;

static void patient_refresh_department_select(DesktopApp *app, DesktopRegistrationPageState *state) {
    LinkedList departments;
    const LinkedListNode *current = 0;
    int selected_index = -1;

    if (app == 0 || state == 0) {
        return;
    }
    if (!WorkbenchSearchSelectState_needs_refresh(&g_patient_department_select)) return;

    WorkbenchSearchSelectState_reset(&g_patient_department_select);
    LinkedList_init(&departments);
    if (DepartmentRepository_load_all(&app->application.doctor_service.department_repository, &departments).success != 1) {
        return;
    }

    current = departments.head;
    while (current != 0) {
        const Department *department = (const Department *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];

        snprintf(label, sizeof(label), "%s | %s | %s", department->department_id, department->name, department->location);
        if (Workbench_text_contains_ignore_case(label, g_patient_department_select.query)) {
            if (strcmp(state->department_id, department->department_id) == 0) {
                selected_index = g_patient_department_select.option_count;
            }
            WorkbenchSearchSelectState_add_option(&g_patient_department_select, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_patient_department_select);
    g_patient_department_select.active_index = selected_index;
    DepartmentRepository_clear_list(&departments);
}

static void patient_refresh_doctor_select(DesktopApp *app, DesktopRegistrationPageState *state) {
    LinkedList doctors;
    const LinkedListNode *current = 0;
    int selected_index = -1;
    Result result;

    if (app == 0 || state == 0) {
        return;
    }
    if (!WorkbenchSearchSelectState_needs_refresh(&g_patient_doctor_select)) return;

    WorkbenchSearchSelectState_reset(&g_patient_doctor_select);
    LinkedList_init(&doctors);
    if (state->department_id[0] != '\0') {
        result = DoctorService_list_by_department(&app->application.doctor_service, state->department_id, &doctors);
    } else {
        result = DoctorRepository_load_all(&app->application.doctor_service.doctor_repository, &doctors);
    }
    if (result.success != 1) {
        return;
    }

    current = doctors.head;
    while (current != 0) {
        const Doctor *doctor = (const Doctor *)current->data;
        char label[WORKBENCH_SELECT_LABEL_CAPACITY];

        snprintf(label, sizeof(label), "%s | %s | %s | %s", doctor->doctor_id, doctor->name, doctor->title, doctor->schedule);
        if (Workbench_text_contains_ignore_case(label, g_patient_doctor_select.query)) {
            if (strcmp(state->doctor_id, doctor->doctor_id) == 0) {
                selected_index = g_patient_doctor_select.option_count;
            }
            WorkbenchSearchSelectState_add_option(&g_patient_doctor_select, label);
        }
        current = current->next;
    }

    WorkbenchSearchSelectState_finalize(&g_patient_doctor_select);
    g_patient_doctor_select.active_index = selected_index;
    DoctorService_clear_list(&doctors);
}

static void auto_bind_patient_id(DesktopApp *app) {
    if (app->state.current_user.role != USER_ROLE_PATIENT) {
        return;
    }

    if (app->state.registration_page.patient_id[0] == '\0') {
        strncpy(
            app->state.registration_page.patient_id,
            app->state.current_user.user_id,
            sizeof(app->state.registration_page.patient_id) - 1
        );
    }
    if (app->state.doctor_page.patient_id[0] == '\0') {
        strncpy(
            app->state.doctor_page.patient_id,
            app->state.current_user.user_id,
            sizeof(app->state.doctor_page.patient_id) - 1
        );
    }
    if (app->state.dispense_page.patient_id[0] == '\0') {
        strncpy(
            app->state.dispense_page.patient_id,
            app->state.current_user.user_id,
            sizeof(app->state.dispense_page.patient_id) - 1
        );
    }
    if (app->state.inpatient_page.query_patient_id[0] == '\0') {
        strncpy(
            app->state.inpatient_page.query_patient_id,
            app->state.current_user.user_id,
            sizeof(app->state.inpatient_page.query_patient_id) - 1
        );
    }
}

static void patient_draw_home(DesktopApp *app, Rectangle panel) {
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_PATIENT);
    const char *labels[4] = { "患者编号", "挂号记录", "住院状态", "发药记录" };
    const char *values[4];
    WorkbenchButtonGroupLayout actions;
    char patient_id_value[32];
    char registration_count[16];
    const char *admission_status = "门诊";
    char dispense_count[16];
    int index = 0;
    Result result;

    auto_bind_patient_id(app);

    /* Load dashboard data if needed */
    if (app->state.dashboard.loaded == 0) {
        result = DesktopAdapters_load_dashboard(&app->application, &app->state.dashboard);
        if (result.success) {
            app->state.dashboard.loaded = 1;
        }
    }

    snprintf(patient_id_value, sizeof(patient_id_value), "%s", app->state.current_user.user_id);
    snprintf(registration_count, sizeof(registration_count), "%d次",
             app->state.registration_page.last_registration_id[0] != '\0' ? 1 : 0);
    if (app->state.inpatient_page.output[0] != '\0') {
        admission_status = "住院中";
    }
    snprintf(dispense_count, sizeof(dispense_count), "%d条",
             app->state.dispense_page.loaded > 0 ? (int)LinkedList_count(&app->state.dispense_page.results) : 0);

    values[0] = patient_id_value;
    values[1] = registration_count;
    values[2] = admission_status;
    values[3] = dispense_count;

    Workbench_draw_home_cards(app, panel, labels, values, wb->accent);

    Workbench_draw_section_header(app, (int)panel.x, (int)panel.y + 104, "快速导航");
    actions = Workbench_compute_button_group_layout(
        (Rectangle){ panel.x, panel.y + 144.0f, panel.width, 40.0f },
        4,
        150.0f,
        40.0f,
        14.0f
    );
    for (index = 0; index < actions.count; index++) {
        int clicked = 0;
        const char *label = index == 0 ? "自助挂号" :
                            index == 1 ? "看诊记录" :
                            index == 2 ? "住院信息" :
                                         "发药记录";
        Workbench_draw_quick_action_btn(app, actions.buttons[index], label, wb->accent, &clicked);
        if (clicked) {
            app->state.workbench_page = index + 1;
        }
    }

    Workbench_draw_output_panel(
        app,
        (Rectangle){ panel.x, panel.y + 214.0f, panel.width, panel.height - 214.0f },
        "服务说明",
        "欢迎使用患者自助服务台。您可以在这里进行自助挂号、查看看诊记录、了解住院信息以及查询发药记录和药品说明。所有信息均与您的患者编号关联，无需重复输入。",
        "暂无说明"
    );
}

static void patient_draw_registration(DesktopApp *app, Rectangle panel) {
    DesktopRegistrationPageState *state = &app->state.registration_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.46f, 18.0f, 280.0f);
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_PATIENT);
    Result result;
    float ly = split.list_bounds.y;
    float lx = split.list_bounds.x;
    Rectangle department_search = { split.detail_bounds.x + 16, split.detail_bounds.y + 50, split.detail_bounds.width - 32, 34 };
    Rectangle department_list = { split.detail_bounds.x + 16, split.detail_bounds.y + 92, split.detail_bounds.width - 32, split.detail_bounds.height * 0.36f };
    Rectangle doctor_search = { split.detail_bounds.x + 16, split.detail_bounds.y + split.detail_bounds.height * 0.48f, split.detail_bounds.width - 32, 34 };
    Rectangle doctor_list = { split.detail_bounds.x + 16, split.detail_bounds.y + split.detail_bounds.height * 0.48f + 42, split.detail_bounds.width - 32, split.detail_bounds.height * 0.30f };

    auto_bind_patient_id(app);
    patient_refresh_department_select(app, state);
    patient_refresh_doctor_select(app, state);

    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("自助挂号", (int)lx + 16, (int)ly + 14, 24, app->theme.text_primary);

    /* Patient ID display with badge */
    DrawText("患者编号", (int)lx + 16, (int)ly + 52, 18, app->theme.text_secondary);
    Rectangle patient_badge = { lx + 126, ly + 48, 160, 28 };
    Workbench_draw_status_badge(app, patient_badge, state->patient_id, wb->accent);

    /* Step-by-step guidance */
    Workbench_draw_section_header(app, (int)lx + 16, (int)ly + 92, "挂号流程");

    DrawText("步骤 1: 选择科室", (int)lx + 16, (int)ly + 126, 17, app->theme.text_primary);
    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 150, "已选科室", 1);
    Workbench_draw_status_badge(
        app,
        (Rectangle){ lx + 120, ly + 146, split.list_bounds.width - 136, 28 },
        state->department_id[0] != '\0' ? state->department_id : "请在右侧搜索并选择科室",
        wb->accent
    );

    DrawText("步骤 2: 选择医生", (int)lx + 16, (int)ly + 220, 17, app->theme.text_primary);
    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 244, "已选医生", 1);
    Workbench_draw_status_badge(
        app,
        (Rectangle){ lx + 120, ly + 240, split.list_bounds.width - 136, 28 },
        state->doctor_id[0] != '\0' ? state->doctor_id : "请在右侧搜索并选择医生",
        wb->accent
    );

    DrawText("步骤 3: 挂号时间", (int)lx + 16, (int)ly + 314, 17, app->theme.text_primary);
    Workbench_draw_form_label(app, (int)lx + 16, (int)ly + 338, "挂号时间", 0);
    Workbench_draw_status_badge(
        app,
        (Rectangle){ lx + 16, ly + 360, split.list_bounds.width - 32, 34 },
        "由系统自动生成（提交时记录当前时间）",
        (Color){ 156, 163, 175, 255 }
    );

    if (GuiButton((Rectangle){ lx + 16, ly + 416, split.list_bounds.width - 32, 40 }, "提交挂号")) {
        Registration registration;
        memset(&registration, 0, sizeof(registration));
        {
            time_t now = time(NULL);
            struct tm *local = localtime(&now);
            strftime(state->registered_at, sizeof(state->registered_at), "%Y-%m-%d %H:%M", local);
        }
        result = DesktopAdapters_submit_self_registration(
            &app->application,
            state->doctor_id,
            state->department_id,
            state->registered_at,
            &registration
        );
        Workbench_show_result(app, result);
        if (result.success) {
            strncpy(
                state->last_registration_id,
                registration.registration_id,
                sizeof(state->last_registration_id) - 1
            );
            DesktopAdapters_query_registrations_by_patient(
                &app->application,
                state->patient_id,
                g_patient_registration_view.registrations,
                sizeof(g_patient_registration_view.registrations)
            );
            DesktopApp_mark_dirty(&app->state, DESKTOP_PAGE_DASHBOARD);
            WorkbenchSearchSelectState_mark_dirty(&g_patient_department_select);
            WorkbenchSearchSelectState_mark_dirty(&g_patient_doctor_select);
        }
    }

    /* Last registration status */
    if (state->last_registration_id[0] != '\0') {
        DrawText("最近挂号", (int)lx + 16, (int)ly + 472, 18, app->theme.text_secondary);
        Rectangle reg_badge = { lx + 126, ly + 468, 200, 28 };
        Workbench_draw_status_badge(app, reg_badge, state->last_registration_id,
                                    (Color){ 34, 197, 94, 255 });
    }

    DrawRectangleRounded(split.detail_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.detail_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("科室搜索与选择", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 16, 20, app->theme.text_primary);
    Workbench_draw_search_select(app, department_search, department_list, &g_patient_department_select, &state->active_field, 21);
    if (g_patient_department_select.active_index >= 0 &&
        g_patient_department_select.active_index < g_patient_department_select.option_count) {
        sscanf(g_patient_department_select.labels[g_patient_department_select.active_index], "%15s", state->department_id);
        state->doctor_id[0] = '\0';
    }

    DrawText("医生搜索与选择", (int)split.detail_bounds.x + 16, (int)(split.detail_bounds.y + split.detail_bounds.height * 0.48f - 28), 20, app->theme.text_primary);
    Workbench_draw_search_select(app, doctor_search, doctor_list, &g_patient_doctor_select, &state->active_field, 22);
    if (g_patient_doctor_select.active_index >= 0 &&
        g_patient_doctor_select.active_index < g_patient_doctor_select.option_count) {
        sscanf(g_patient_doctor_select.labels[g_patient_doctor_select.active_index], "%15s", state->doctor_id);
    }
    Workbench_draw_output_panel(
        app,
        (Rectangle){
            split.detail_bounds.x + 12,
            split.detail_bounds.y + split.detail_bounds.height * 0.82f,
            split.detail_bounds.width - 24,
            split.detail_bounds.height * 0.16f - 12
        },
        "我的挂号记录",
        g_patient_registration_view.registrations,
        "提交挂号成功后，您的挂号记录会在这里显示。"
    );
}

static void patient_draw_visits(DesktopApp *app, Rectangle panel) {
    DesktopDoctorPageState *state = &app->state.doctor_page;
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_PATIENT);
    Result result;
    float card_height = 0.0f;

    auto_bind_patient_id(app);

    /* Header with patient ID badge and refresh button */
    DrawText("我的看诊记录", (int)panel.x, (int)panel.y, 24, app->theme.text_primary);

    DrawText("患者编号", (int)panel.x, (int)panel.y + 36, 18, app->theme.text_secondary);
    Rectangle patient_badge = { panel.x + 110, panel.y + 32, 160, 28 };
    Workbench_draw_status_badge(app, patient_badge, state->patient_id, wb->accent);

    if (GuiButton((Rectangle){ panel.x + panel.width - 118, panel.y, 118, 34 }, "刷新记录")) {
        result = DesktopAdapters_query_patient_history(
            &app->application,
            state->patient_id,
            state->output,
            sizeof(state->output)
        );
        Workbench_show_result(app, result);
    }

    /* Card-style layout for medical records */
    card_height = (panel.height - 88) / 2.0f - 10.0f;

    /* Consultation records card */
    DrawRectangleRounded(
        (Rectangle){ panel.x, panel.y + 78, panel.width, card_height },
        0.12f, 8, app->theme.panel
    );
    DrawRectangleRoundedLinesEx(
        (Rectangle){ panel.x, panel.y + 78, panel.width, card_height },
        0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f)
    );
    DrawRectangle((int)panel.x, (int)panel.y + 82, 5, (int)card_height - 8, (Color){ 13, 148, 136, 255 });
    DrawText("看诊记录", (int)panel.x + 20, (int)panel.y + 92, 20, app->theme.text_primary);
    BeginScissorMode((int)panel.x, (int)(panel.y + 78), (int)panel.width, (int)card_height);
    DrawText(
        state->output[0] != '\0' ? state->output : "暂无看诊记录。点击【刷新记录】加载您的挂号、看诊、检查和住院摘要。",
        (int)panel.x + 20,
        (int)panel.y + 126,
        17,
        app->theme.text_secondary
    );
    EndScissorMode();

    /* Examination results card */
    DrawRectangleRounded(
        (Rectangle){ panel.x, panel.y + 88 + card_height, panel.width, card_height },
        0.12f, 8, app->theme.panel
    );
    DrawRectangleRoundedLinesEx(
        (Rectangle){ panel.x, panel.y + 88 + card_height, panel.width, card_height },
        0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f)
    );
    DrawRectangle((int)panel.x, (int)panel.y + 92 + (int)card_height, 5, (int)card_height - 8, (Color){ 245, 158, 11, 255 });
    DrawText("检查结果", (int)panel.x + 20, (int)panel.y + 102 + (int)card_height, 20, app->theme.text_primary);
    BeginScissorMode((int)panel.x, (int)(panel.y + 88 + card_height), (int)panel.width, (int)card_height);
    DrawText(
        "检查结果会在看诊记录中一并显示。如有检查项目，医生会在看诊时录入检查信息。",
        (int)panel.x + 20,
        (int)panel.y + 136 + (int)card_height,
        17,
        app->theme.text_secondary
    );
    EndScissorMode();
}

static void patient_draw_admission(DesktopApp *app, Rectangle panel) {
    DesktopInpatientPageState *state = &app->state.inpatient_page;
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_PATIENT);
    Result result;
    int has_admission = state->output[0] != '\0' && strstr(state->output, "未找到") == 0;
    Color status_color = has_admission ? (Color){ 245, 158, 11, 255 } : (Color){ 34, 197, 94, 255 };
    const char *status_text = has_admission ? "住院中" : "门诊";

    auto_bind_patient_id(app);

    /* Header with status badge */
    DrawText("我的住院信息", (int)panel.x, (int)panel.y, 24, app->theme.text_primary);

    DrawText("患者编号", (int)panel.x, (int)panel.y + 36, 18, app->theme.text_secondary);
    Rectangle patient_badge = { panel.x + 110, panel.y + 32, 160, 28 };
    Workbench_draw_status_badge(app, patient_badge, state->query_patient_id, wb->accent);

    DrawText("住院状态", (int)panel.x + 290, (int)panel.y + 36, 18, app->theme.text_secondary);
    Rectangle status_badge = { panel.x + 390, panel.y + 32, 100, 28 };
    Workbench_draw_status_badge(app, status_badge, status_text, status_color);

    if (GuiButton((Rectangle){ panel.x + panel.width - 118, panel.y, 118, 34 }, "刷新状态")) {
        result = DesktopAdapters_query_active_admission_by_patient(
            &app->application,
            state->query_patient_id,
            state->output,
            sizeof(state->output)
        );
        Workbench_show_result(app, result);
    }

    /* Admission information card */
    DrawRectangleRounded(
        (Rectangle){ panel.x, panel.y + 78, panel.width, panel.height - 78 },
        0.12f, 8, app->theme.panel
    );
    DrawRectangleRoundedLinesEx(
        (Rectangle){ panel.x, panel.y + 78, panel.width, panel.height - 78 },
        0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f)
    );
    DrawRectangle((int)panel.x, (int)panel.y + 82, 5, (int)(panel.height - 86), status_color);

    if (has_admission) {
        DrawText("住院详情", (int)panel.x + 20, (int)panel.y + 92, 20, app->theme.text_primary);
        BeginScissorMode((int)panel.x, (int)(panel.y + 78), (int)panel.width, (int)(panel.height - 78));
        DrawText(
            state->output,
            (int)panel.x + 20,
            (int)panel.y + 126,
            17,
            app->theme.text_secondary
        );
        EndScissorMode();
    } else {
        DrawText("当前状态", (int)panel.x + 20, (int)panel.y + 92, 20, app->theme.text_primary);
        BeginScissorMode((int)panel.x, (int)(panel.y + 78), (int)panel.width, (int)(panel.height - 78));
        DrawText(
            "您当前没有在院记录。如需住院，请前往医院住院登记处办理入院手续。",
            (int)panel.x + 20,
            (int)panel.y + 126,
            17,
            app->theme.text_secondary
        );
        DrawText(
            "住院流程说明：",
            (int)panel.x + 20,
            (int)panel.y + 166,
            18,
            app->theme.text_primary
        );
        DrawText(
            "1. 医生开具住院通知单",
            (int)panel.x + 20,
            (int)panel.y + 196,
            17,
            app->theme.text_secondary
        );
        DrawText(
            "2. 前往住院登记处办理入院手续",
            (int)panel.x + 20,
            (int)panel.y + 224,
            17,
            app->theme.text_secondary
        );
        DrawText(
            "3. 病区管理员安排床位",
            (int)panel.x + 20,
            (int)panel.y + 252,
            17,
            app->theme.text_secondary
        );
        DrawText(
            "4. 入住病房开始治疗",
            (int)panel.x + 20,
            (int)panel.y + 280,
            17,
            app->theme.text_secondary
        );
        EndScissorMode();
    }
}

static void patient_draw_dispense(DesktopApp *app, Rectangle panel) {
    DesktopDispensePageState *history_state = &app->state.dispense_page;
    DesktopPharmacyPageState *medicine_state = &app->state.pharmacy_page;
    WorkbenchListDetailLayout split = Workbench_compute_list_detail_layout(panel, 0.5f, 18.0f, 260.0f);
    const WorkbenchDef *wb = Workbench_get(USER_ROLE_PATIENT);
    const LinkedListNode *current = 0;
    int row = 0;
    Result result;
    float timeline_y = 0.0f;

    auto_bind_patient_id(app);

    /* Left panel: Dispense history with timeline */
    DrawRectangleRounded(split.list_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.list_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("我的发药记录", (int)split.list_bounds.x + 16, (int)split.list_bounds.y + 14, 24, app->theme.text_primary);

    if (GuiButton((Rectangle){ split.list_bounds.x + split.list_bounds.width - 118, split.list_bounds.y + 10, 102, 34 }, "刷新记录")) {
        LinkedList_clear(&history_state->results, free);
        LinkedList_init(&history_state->results);
        result = DesktopAdapters_load_dispense_history(
            &app->application,
            history_state->patient_id,
            &history_state->results
        );
        history_state->loaded = result.success ? 1 : -1;
        Workbench_show_result(app, result);
    }

    /* Timeline-style display */
    timeline_y = split.list_bounds.y + 60;
    current = history_state->results.head;
    while (current != 0 && row < 8) {
        const DispenseRecord *record = (const DispenseRecord *)current->data;
        Color timeline_color = (Color){ 22, 163, 74, 255 };

        /* Timeline dot and line */
        DrawCircle((int)split.list_bounds.x + 26, (int)timeline_y + 14, 6, timeline_color);
        if (current->next != 0 && row < 7) {
            DrawRectangle((int)split.list_bounds.x + 24, (int)timeline_y + 20, 4, 28, Fade(timeline_color, 0.3f));
        }

        /* Record card */
        Rectangle record_card = {
            split.list_bounds.x + 44,
            timeline_y,
            split.list_bounds.width - 60,
            42
        };
        DrawRectangleRounded(record_card, 0.15f, 8, Fade(timeline_color, 0.08f));
        DrawRectangleRoundedLinesEx(record_card, 0.15f, 8, 1.0f, Fade(timeline_color, 0.25f));

        /* Record info */
        DrawText(
            TextFormat("发药单号: %s", record->dispense_id),
            (int)record_card.x + 12,
            (int)record_card.y + 6,
            16,
            app->theme.text_primary
        );
        DrawText(
            TextFormat("药品: %s  数量: %d  时间: %s", record->medicine_id, record->quantity, record->dispensed_at),
            (int)record_card.x + 12,
            (int)record_card.y + 24,
            15,
            app->theme.text_secondary
        );

        timeline_y += 48;
        current = current->next;
        row++;
    }

    if (row == 0) {
        DrawText("暂无发药记录。", (int)split.list_bounds.x + 44, (int)split.list_bounds.y + 74, 17, app->theme.text_secondary);
        DrawText("医生开具处方并由药房发药后，记录会在这里显示。", (int)split.list_bounds.x + 44, (int)split.list_bounds.y + 102, 16, Fade(app->theme.text_secondary, 0.8f));
    }

    /* Right panel: Medicine information query */
    DrawRectangleRounded(split.detail_bounds, 0.12f, 8, app->theme.panel);
    DrawRectangleRoundedLinesEx(split.detail_bounds, 0.12f, 8, 1.0f, Fade(app->theme.border, 0.85f));
    DrawText("药品说明查询", (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 14, 24, app->theme.text_primary);

    Workbench_draw_form_label(app, (int)split.detail_bounds.x + 16, (int)split.detail_bounds.y + 56, "药品编号", 0);
    Workbench_draw_text_input(
        (Rectangle){ split.detail_bounds.x + 16, split.detail_bounds.y + 78, split.detail_bounds.width - 152, 34 },
        medicine_state->stock_query_medicine_id,
        sizeof(medicine_state->stock_query_medicine_id),
        1,
        &medicine_state->active_field,
        1
    );
    if (GuiButton((Rectangle){ split.detail_bounds.x + split.detail_bounds.width - 118, split.detail_bounds.y + 78, 102, 34 }, "查询说明")) {
        result = DesktopAdapters_query_medicine_detail(
            &app->application,
            medicine_state->stock_query_medicine_id,
            1,
            medicine_state->output,
            sizeof(medicine_state->output)
        );
        Workbench_show_result(app, result);
    }

    /* Medicine information display */
    DrawRectangleRounded(
        (Rectangle){ split.detail_bounds.x + 12, split.detail_bounds.y + 128, split.detail_bounds.width - 24, split.detail_bounds.height - 140 },
        0.12f, 8, Fade(app->theme.border, 0.05f)
    );
    DrawRectangleRoundedLinesEx(
        (Rectangle){ split.detail_bounds.x + 12, split.detail_bounds.y + 128, split.detail_bounds.width - 24, split.detail_bounds.height - 140 },
        0.12f, 8, 1.0f, Fade(app->theme.border, 0.3f)
    );
    DrawText(
        "药品说明",
        (int)split.detail_bounds.x + 24,
        (int)split.detail_bounds.y + 140,
        18,
        app->theme.text_primary
    );
    DrawText(
        medicine_state->output[0] != '\0' ? medicine_state->output : "输入药品编号后点击【查询说明】查看药品的详细信息，包括名称、规格、用法用量等。",
        (int)split.detail_bounds.x + 24,
        (int)split.detail_bounds.y + 168,
        17,
        app->theme.text_secondary
    );
}

void PatientWorkbench_draw(DesktopApp *app, Rectangle panel, int page) {
    static int initialized = 0;
    if (!initialized) {
        WorkbenchSearchSelectState_mark_dirty(&g_patient_department_select);
        WorkbenchSearchSelectState_mark_dirty(&g_patient_doctor_select);
        initialized = 1;
    }
    switch (page) {
        case 0:
            patient_draw_home(app, panel);
            break;
        case 1:
            patient_draw_registration(app, panel);
            break;
        case 2:
            patient_draw_visits(app, panel);
            break;
        case 3:
            patient_draw_admission(app, panel);
            break;
        case 4:
            patient_draw_dispense(app, panel);
            break;
        default:
            patient_draw_home(app, panel);
            break;
    }
}
