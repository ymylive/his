#include "ui/DesktopApp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "ui/DesktopAdapters.h"
#include "ui/DesktopPages.h"

static const int DESKTOP_APP_MIN_WIDTH = 1366;
static const int DESKTOP_APP_MIN_HEIGHT = 768;

static int DesktopApp_page_is_valid(DesktopPage page) {
    return page >= DESKTOP_PAGE_LOGIN && page < DESKTOP_PAGE_COUNT;
}

static void DesktopApp_reset_page_tracking(DesktopAppState *state) {
    int page = 0;

    if (state == 0) {
        return;
    }

    for (page = 0; page < DESKTOP_PAGE_COUNT; page++) {
        state->page_freshness[page] = DESKTOP_FRESHNESS_CLEAN;
        state->page_snapshots[page] = 0;
    }
}

static void DesktopApp_clear_dashboard(DesktopDashboardState *dashboard) {
    if (dashboard == 0) {
        return;
    }

    LinkedList_clear(&dashboard->recent_registrations, free);
    LinkedList_clear(&dashboard->recent_dispensations, free);
    LinkedList_init(&dashboard->recent_registrations);
    LinkedList_init(&dashboard->recent_dispensations);
    dashboard->patient_count = 0;
    dashboard->registration_count = 0;
    dashboard->inpatient_count = 0;
    dashboard->low_stock_count = 0;
    dashboard->loaded = 0;
}

static void DesktopApp_clear_patient_page(DesktopPatientPageState *patient_page) {
    if (patient_page == 0) {
        return;
    }

    LinkedList_clear(&patient_page->results, free);
    LinkedList_init(&patient_page->results);
    patient_page->query[0] = '\0';
    patient_page->search_mode = DESKTOP_PATIENT_SEARCH_BY_ID;
    patient_page->selected_index = -1;
    memset(&patient_page->selected_patient, 0, sizeof(patient_page->selected_patient));
    patient_page->has_selected_patient = 0;
}

static void DesktopApp_clear_dispense_page(DesktopDispensePageState *dispense_page) {
    if (dispense_page == 0) {
        return;
    }

    LinkedList_clear(&dispense_page->results, free);
    LinkedList_init(&dispense_page->results);
    dispense_page->patient_id[0] = '\0';
    dispense_page->loaded = 0;
}

static void DesktopApp_clear_doctor_page(DesktopDoctorPageState *doctor_page) {
    if (doctor_page == 0) {
        return;
    }

    memset(doctor_page, 0, sizeof(*doctor_page));
}

static void DesktopApp_clear_inpatient_page(DesktopInpatientPageState *inpatient_page) {
    if (inpatient_page == 0) {
        return;
    }

    memset(inpatient_page, 0, sizeof(*inpatient_page));
}

static void DesktopApp_clear_pharmacy_page(DesktopPharmacyPageState *pharmacy_page) {
    if (pharmacy_page == 0) {
        return;
    }

    memset(pharmacy_page, 0, sizeof(*pharmacy_page));
}

static void DesktopApp_clear_registration_page(DesktopRegistrationPageState *registration_page) {
    if (registration_page == 0) {
        return;
    }

    memset(registration_page, 0, sizeof(*registration_page));
}

static int DesktopApp_file_exists(const char *path) {
    return (path != 0 && FileExists(path)) ? 1 : 0;
}

int DesktopApp_resolve_data_path_for_base_dir(
    const char *base_dir,
    const char *file_name,
    char *out_path,
    size_t out_path_size,
    DesktopAppPathExistsFn path_exists
) {
    char candidates[4][512];
    int index = 0;

    if (file_name == 0 || out_path == 0 || out_path_size == 0 || path_exists == 0) {
        return 0;
    }

    snprintf(candidates[0], sizeof(candidates[0]), "%s/../data/%s", base_dir != 0 ? base_dir : ".", file_name);
    snprintf(candidates[1], sizeof(candidates[1]), "data/%s", file_name);
    snprintf(candidates[2], sizeof(candidates[2]), "%s/data/%s", base_dir != 0 ? base_dir : ".", file_name);
    snprintf(candidates[3], sizeof(candidates[3]), "../data/%s", file_name);

    for (index = 0; index < 4; index++) {
        if (path_exists(candidates[index]) != 0) {
            strncpy(out_path, candidates[index], out_path_size - 1);
            out_path[out_path_size - 1] = '\0';
            return 1;
        }
    }

    strncpy(out_path, candidates[0], out_path_size - 1);
    out_path[out_path_size - 1] = '\0';
    return 0;
}

int DesktopApp_resolve_data_path(
    const char *file_name,
    char *out_path,
    size_t out_path_size
) {
    return DesktopApp_resolve_data_path_for_base_dir(
        GetApplicationDirectory(),
        file_name,
        out_path,
        out_path_size,
        DesktopApp_file_exists
    );
}

int DesktopApp_min_width(void) {
    return DESKTOP_APP_MIN_WIDTH;
}

int DesktopApp_min_height(void) {
    return DESKTOP_APP_MIN_HEIGHT;
}

void DesktopApp_mark_dirty(DesktopAppState *state, DesktopPage page) {
    if (state == 0 || DesktopApp_page_is_valid(page) == 0) {
        return;
    }

    state->page_freshness[page] = DESKTOP_FRESHNESS_DIRTY;
}

void DesktopApp_mark_stale(DesktopAppState *state, DesktopPage page) {
    if (state == 0 || DesktopApp_page_is_valid(page) == 0) {
        return;
    }

    state->page_freshness[page] = DESKTOP_FRESHNESS_STALE;
}

void DesktopApp_mark_clean(DesktopAppState *state, DesktopPage page) {
    if (state == 0 || DesktopApp_page_is_valid(page) == 0) {
        return;
    }

    state->page_freshness[page] = DESKTOP_FRESHNESS_CLEAN;
}

void DesktopApp_mark_reload_failed(DesktopAppState *state, DesktopPage page) {
    DesktopApp_mark_stale(state, page);
}

void DesktopApp_record_page_snapshot(DesktopAppState *state, DesktopPage page) {
    if (state == 0 || DesktopApp_page_is_valid(page) == 0) {
        return;
    }

    state->page_snapshots[page] = 1;
    state->page_freshness[page] = DESKTOP_FRESHNESS_CLEAN;
}

int DesktopApp_page_has_snapshot(const DesktopAppState *state, DesktopPage page) {
    if (state == 0 || DesktopApp_page_is_valid(page) == 0) {
        return 0;
    }

    return state->page_snapshots[page] != 0 ? 1 : 0;
}

void DesktopAppState_init(DesktopAppState *state) {
    if (state == 0) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->current_page = DESKTOP_PAGE_LOGIN;
    state->login_form.role_index = DesktopApp_role_index_from_role(USER_ROLE_PATIENT);
    state->patient_page.selected_index = -1;
    LinkedList_init(&state->dashboard.recent_registrations);
    LinkedList_init(&state->dashboard.recent_dispensations);
    LinkedList_init(&state->patient_page.results);
    LinkedList_init(&state->dispense_page.results);
    DesktopApp_reset_page_tracking(state);
}

void DesktopAppState_dispose(DesktopAppState *state) {
    if (state == 0) {
        return;
    }

    DesktopApp_clear_dashboard(&state->dashboard);
    DesktopApp_clear_patient_page(&state->patient_page);
    DesktopApp_clear_registration_page(&state->registration_page);
    DesktopApp_clear_dispense_page(&state->dispense_page);
    DesktopApp_clear_doctor_page(&state->doctor_page);
    DesktopApp_clear_inpatient_page(&state->inpatient_page);
    DesktopApp_clear_pharmacy_page(&state->pharmacy_page);
}

void DesktopAppState_login_success(DesktopAppState *state, const User *user) {
    if (state == 0 || user == 0) {
        return;
    }

    state->current_user = *user;
    state->logged_in = 1;
    state->workbench_page = 0;
    state->current_page = DesktopApp_home_page_for_role(user->role);
    DesktopApp_reset_page_tracking(state);
    memset(state->login_form.password, 0, sizeof(state->login_form.password));
    state->dashboard.loaded = 0;
    DesktopApp_clear_patient_page(&state->patient_page);
    DesktopApp_clear_registration_page(&state->registration_page);
    DesktopApp_clear_dispense_page(&state->dispense_page);
    DesktopApp_clear_doctor_page(&state->doctor_page);
    DesktopApp_clear_inpatient_page(&state->inpatient_page);
    DesktopApp_clear_pharmacy_page(&state->pharmacy_page);
}

void DesktopAppState_logout(DesktopAppState *state) {
    if (state == 0) {
        return;
    }

    DesktopApp_clear_dashboard(&state->dashboard);
    DesktopApp_clear_patient_page(&state->patient_page);
    DesktopApp_clear_registration_page(&state->registration_page);
    DesktopApp_clear_dispense_page(&state->dispense_page);
    DesktopApp_clear_doctor_page(&state->doctor_page);
    DesktopApp_clear_inpatient_page(&state->inpatient_page);
    DesktopApp_clear_pharmacy_page(&state->pharmacy_page);
    memset(&state->current_user, 0, sizeof(state->current_user));
    state->logged_in = 0;
    state->current_page = DESKTOP_PAGE_LOGIN;
    state->should_close = 0;
    memset(state->login_form.password, 0, sizeof(state->login_form.password));
    DesktopApp_reset_page_tracking(state);
}

void DesktopAppState_set_page(DesktopAppState *state, DesktopPage page) {
    if (state == 0 || DesktopApp_page_is_valid(page) == 0) {
        return;
    }

    state->current_page = page;
}

void DesktopAppState_show_message(
    DesktopAppState *state,
    DesktopMessageKind kind,
    const char *text
) {
    if (state == 0) {
        return;
    }

    state->message.kind = kind;
    state->message.visible = (text != 0 && text[0] != '\0') ? 1 : 0;
    if (text == 0) {
        state->message.text[0] = '\0';
        return;
    }

    strncpy(state->message.text, text, sizeof(state->message.text) - 1);
    state->message.text[sizeof(state->message.text) - 1] = '\0';
}

const char *DesktopApp_page_label(DesktopPage page) {
    switch (page) {
        case DESKTOP_PAGE_LOGIN:
            return "登录";
        case DESKTOP_PAGE_DASHBOARD:
            return "首页";
        case DESKTOP_PAGE_PATIENTS:
            return "患者";
        case DESKTOP_PAGE_REGISTRATION:
            return "挂号";
        case DESKTOP_PAGE_DISPENSE:
            return "发药记录";
        case DESKTOP_PAGE_SYSTEM:
            return "系统";
        case DESKTOP_PAGE_DOCTOR:
            return "医生工作台";
        case DESKTOP_PAGE_INPATIENT:
            return "住院管理";
        case DESKTOP_PAGE_PHARMACY:
            return "药房工作台";
        default:
            return "未知";
    }
}

const char *DesktopApp_user_role_label(UserRole role) {
    switch (role) {
        case USER_ROLE_PATIENT:
            return "患者";
        case USER_ROLE_DOCTOR:
            return "医生";
        case USER_ROLE_ADMIN:
            return "系统管理员";
        case USER_ROLE_REGISTRATION_CLERK:
            return "挂号员";
        case USER_ROLE_INPATIENT_REGISTRAR:
            return "住院登记员";
        case USER_ROLE_WARD_MANAGER:
            return "病区管理员";
        case USER_ROLE_PHARMACY:
            return "药房";
        default:
            return "未知角色";
    }
}

UserRole DesktopApp_role_from_index(int index) {
    switch (index) {
        case 0:
            return USER_ROLE_PATIENT;
        case 1:
            return USER_ROLE_DOCTOR;
        case 2:
            return USER_ROLE_ADMIN;
        case 3:
            return USER_ROLE_REGISTRATION_CLERK;
        case 4:
            return USER_ROLE_INPATIENT_REGISTRAR;
        case 5:
            return USER_ROLE_WARD_MANAGER;
        case 6:
            return USER_ROLE_PHARMACY;
        default:
            return USER_ROLE_UNKNOWN;
    }
}

int DesktopApp_role_index_from_role(UserRole role) {
    switch (role) {
        case USER_ROLE_PATIENT:
            return 0;
        case USER_ROLE_DOCTOR:
            return 1;
        case USER_ROLE_ADMIN:
            return 2;
        case USER_ROLE_REGISTRATION_CLERK:
            return 3;
        case USER_ROLE_INPATIENT_REGISTRAR:
            return 4;
        case USER_ROLE_WARD_MANAGER:
            return 5;
        case USER_ROLE_PHARMACY:
            return 6;
        default:
            return 0;
    }
}

DesktopPage DesktopApp_home_page_for_role(UserRole role) {
    switch (role) {
        case USER_ROLE_REGISTRATION_CLERK:
            return DESKTOP_PAGE_REGISTRATION;
        case USER_ROLE_DOCTOR:
            return DESKTOP_PAGE_DOCTOR;
        case USER_ROLE_INPATIENT_REGISTRAR:
        case USER_ROLE_WARD_MANAGER:
            return DESKTOP_PAGE_INPATIENT;
        case USER_ROLE_PHARMACY:
            return DESKTOP_PAGE_PHARMACY;
        default:
            return DESKTOP_PAGE_DASHBOARD;
    }
}

int DesktopApp_run(const DesktopAppConfig *config) {
    DesktopApp app;
    Result result;
    char loaded_font_path[260];
    int window_width = 0;
    int window_height = 0;

    if (config == 0 || config->paths == 0) {
        return 1;
    }

    memset(&app, 0, sizeof(app));
    app.paths = config->paths;
    app.theme = DesktopTheme_make_default();
    DesktopAppState_init(&app.state);

    result = DesktopAdapters_init_application(&app.application, config->paths);
    if (result.success == 0) {
        fprintf(stderr, "桌面控制页初始化失败: %s\n", result.message);
        DesktopAppState_dispose(&app.state);
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT);
    window_width = config->width > DesktopApp_min_width() ? config->width : DesktopApp_min_width();
    window_height = config->height > DesktopApp_min_height() ? config->height : DesktopApp_min_height();
    InitWindow(
        window_width,
        window_height,
        config->title != 0 ? config->title : "Lightweight HIS Desktop"
    );
    SetWindowMinSize(DesktopApp_min_width(), DesktopApp_min_height());
    SetTargetFPS(60);
    DesktopTheme_apply_raygui_style(&app.theme);
    if (DesktopTheme_enable_cjk_font(40, loaded_font_path, sizeof(loaded_font_path)) == 0) {
        fprintf(stderr, "未找到可用 CJK 字体，回退到默认字体\n");
    }
    DesktopTheme_apply_raygui_style(&app.theme);

    if (config->smoke_mode != 0) {
        BeginDrawing();
        ClearBackground(app.theme.background);
        EndDrawing();
        DesktopTheme_shutdown_fonts();
        CloseWindow();
        DesktopAppState_dispose(&app.state);
        return 0;
    }

    while (!WindowShouldClose() && app.state.should_close == 0) {
        BeginDrawing();
        ClearBackground(app.theme.background);
        DesktopPages_draw(&app);
        EndDrawing();
    }

    DesktopTheme_shutdown_fonts();
    CloseWindow();
    MenuApplication_logout(&app.application);
    DesktopAppState_dispose(&app.state);
    return 0;
}
