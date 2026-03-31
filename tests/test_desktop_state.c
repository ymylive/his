#include <assert.h>
#include <string.h>

#include "ui/DesktopApp.h"
#include "ui/DesktopTheme.h"

static void test_default_state_and_message(void) {
    DesktopAppState state;
    int page = 0;

    DesktopAppState_init(&state);
    assert(state.current_page == DESKTOP_PAGE_LOGIN);
    assert(state.logged_in == 0);
    assert(state.login_form.role_index == 0);
    assert(state.page_freshness[DESKTOP_PAGE_DASHBOARD] == DESKTOP_FRESHNESS_CLEAN);
    for (page = 0; page < DESKTOP_PAGE_COUNT; page++) {
        assert(state.page_freshness[page] == DESKTOP_FRESHNESS_CLEAN);
        assert(DesktopApp_page_has_snapshot(&state, (DesktopPage)page) == 0);
    }

    DesktopAppState_show_message(&state, DESKTOP_MESSAGE_INFO, "ready");
    assert(state.message.visible == 1);
    assert(strcmp(state.message.text, "ready") == 0);

    DesktopAppState_dispose(&state);
}

static void test_login_page_switch_and_logout(void) {
    DesktopAppState state;
    User user;

    memset(&user, 0, sizeof(user));
    strcpy(user.user_id, "PAT0001");
    user.role = USER_ROLE_PATIENT;

    DesktopAppState_init(&state);
    DesktopAppState_login_success(&state, &user);
    assert(state.logged_in == 1);
    assert(state.current_page == DESKTOP_PAGE_DASHBOARD);
    assert(strcmp(state.current_user.user_id, "PAT0001") == 0);

    DesktopAppState_set_page(&state, DESKTOP_PAGE_PATIENTS);
    assert(state.current_page == DESKTOP_PAGE_PATIENTS);

    DesktopAppState_logout(&state);
    assert(state.logged_in == 0);
    assert(state.current_page == DESKTOP_PAGE_LOGIN);
    assert(state.current_user.user_id[0] == '\0');

    DesktopAppState_dispose(&state);
}

static void test_role_home_pages_on_login(void) {
    DesktopAppState state;
    User user;

    DesktopAppState_init(&state);

    memset(&user, 0, sizeof(user));
    strcpy(user.user_id, "DOC0001");
    user.role = USER_ROLE_DOCTOR;
    DesktopAppState_login_success(&state, &user);
    assert(state.current_page == DESKTOP_PAGE_DOCTOR);

    memset(&user, 0, sizeof(user));
    strcpy(user.user_id, "INP0001");
    user.role = USER_ROLE_INPATIENT_REGISTRAR;
    DesktopAppState_login_success(&state, &user);
    assert(state.current_page == DESKTOP_PAGE_INPATIENT);

    memset(&user, 0, sizeof(user));
    strcpy(user.user_id, "PHA0001");
    user.role = USER_ROLE_PHARMACY;
    DesktopAppState_login_success(&state, &user);
    assert(state.current_page == DESKTOP_PAGE_PHARMACY);

    DesktopAppState_dispose(&state);
}

static void test_role_mapping(void) {
    assert(DesktopApp_role_from_index(0) == USER_ROLE_PATIENT);
    assert(DesktopApp_role_from_index(2) == USER_ROLE_ADMIN);
    assert(DesktopApp_role_from_index(6) == USER_ROLE_PHARMACY);
    assert(DesktopApp_role_index_from_role(USER_ROLE_WARD_MANAGER) == 5);
    assert(DesktopApp_home_page_for_role(USER_ROLE_PATIENT) == DESKTOP_PAGE_DASHBOARD);
    assert(DesktopApp_home_page_for_role(USER_ROLE_DOCTOR) == DESKTOP_PAGE_DOCTOR);
    assert(DesktopApp_home_page_for_role(USER_ROLE_INPATIENT_REGISTRAR) == DESKTOP_PAGE_INPATIENT);
    assert(DesktopApp_home_page_for_role(USER_ROLE_PHARMACY) == DESKTOP_PAGE_PHARMACY);
    assert(strcmp(DesktopApp_page_label(DESKTOP_PAGE_DISPENSE), "发药记录") == 0);
    assert(strcmp(DesktopApp_page_label(DESKTOP_PAGE_DOCTOR), "医生工作台") == 0);
    assert(strcmp(DesktopApp_page_label(DESKTOP_PAGE_INPATIENT), "住院管理") == 0);
    assert(strcmp(DesktopApp_page_label(DESKTOP_PAGE_PHARMACY), "药房工作台") == 0);
}

static void test_freshness_helpers_and_window_guards(void) {
    DesktopAppState state;

    DesktopAppState_init(&state);
    assert(DesktopApp_min_width() >= 1366);
    assert(DesktopApp_min_height() >= 768);

    DesktopApp_mark_dirty(&state, DESKTOP_PAGE_DASHBOARD);
    assert(state.page_freshness[DESKTOP_PAGE_DASHBOARD] == DESKTOP_FRESHNESS_DIRTY);

    DesktopApp_mark_stale(&state, DESKTOP_PAGE_DASHBOARD);
    assert(state.page_freshness[DESKTOP_PAGE_DASHBOARD] == DESKTOP_FRESHNESS_STALE);

    DesktopApp_mark_clean(&state, DESKTOP_PAGE_DASHBOARD);
    assert(state.page_freshness[DESKTOP_PAGE_DASHBOARD] == DESKTOP_FRESHNESS_CLEAN);

    DesktopApp_record_page_snapshot(&state, DESKTOP_PAGE_DASHBOARD);
    assert(DesktopApp_page_has_snapshot(&state, DESKTOP_PAGE_DASHBOARD) == 1);

    DesktopApp_mark_dirty(&state, DESKTOP_PAGE_DASHBOARD);
    DesktopApp_mark_reload_failed(&state, DESKTOP_PAGE_DASHBOARD);
    assert(state.page_freshness[DESKTOP_PAGE_DASHBOARD] == DESKTOP_FRESHNESS_STALE);
    assert(DesktopApp_page_has_snapshot(&state, DESKTOP_PAGE_DASHBOARD) == 1);

    DesktopAppState_dispose(&state);
}

static void test_initial_window_size_fits_monitor_work_area(void) {
    int width = 0;
    int height = 0;

    DesktopApp_resolve_initial_window_size(1600, 960, 1512, 982, &width, &height);
    assert(width == 1512);
    assert(height == 960);

    DesktopApp_resolve_initial_window_size(1200, 700, 1512, 982, &width, &height);
    assert(width == 1366);
    assert(height == 768);

    DesktopApp_resolve_initial_window_size(1600, 960, 1280, 720, &width, &height);
    assert(width == 1280);
    assert(height == 720);
}

static int test_font_exists_for_noto(const char *path) {
    if (path == 0) {
        return 0;
    }

    /* Platform-specific mock: return true for the first font in each platform's list */
#ifdef _WIN32
    return strcmp(path, "C:/Windows/Fonts/NotoSansSC-VF.ttf") == 0;
#elif __APPLE__
    return strcmp(path, "/System/Library/Fonts/STHeiti Light.ttc") == 0;
#else
    return strcmp(path, "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc") == 0;
#endif
}

static int test_font_exists_for_none(const char *path) {
    (void)path;
    return 0;
}

static int test_path_exists_prefers_project_data(const char *path) {
    if (path == 0) {
        return 0;
    }

    return strcmp(path, "E:/project/his/build/../data/users.txt") == 0 ||
           strcmp(path, "E:/project/his/build/data/users.txt") == 0;
}

static void test_cjk_font_path_resolution(void) {
    const char *picked = DesktopTheme_resolve_cjk_font_path(test_font_exists_for_noto);
    const char *missing = DesktopTheme_resolve_cjk_font_path(test_font_exists_for_none);

    assert(DesktopTheme_candidate_font_count() >= 3);
    assert(DesktopTheme_has_cjk_seed_text() == 1);
    assert(picked != 0);

    /* Platform-specific font path validation */
#ifdef _WIN32
    assert(strcmp(picked, "C:/Windows/Fonts/NotoSansSC-VF.ttf") == 0);
#elif __APPLE__
    /* macOS should pick the first available font from the candidate list */
    assert(strstr(picked, "/System/Library/Fonts/") != 0 ||
           strstr(picked, "/Library/Fonts/") != 0);
#else
    /* Linux */
    assert(strstr(picked, "/usr/share/fonts/") != 0);
#endif

    assert(missing == 0);
}

static int test_utf8_codepoint_length(unsigned char lead_byte) {
    if ((lead_byte & 0x80U) == 0) {
        return 1;
    }
    if ((lead_byte & 0xE0U) == 0xC0U) {
        return 2;
    }
    if ((lead_byte & 0xF0U) == 0xE0U) {
        return 3;
    }
    if ((lead_byte & 0xF8U) == 0xF0U) {
        return 4;
    }
    return 0;
}

static int test_seed_contains_utf8_span(const char *seed, const char *span, int span_length) {
    const char *cursor = seed;

    if (seed == 0 || span == 0 || span_length <= 0) {
        return 0;
    }

    while (*cursor != '\0') {
        if (memcmp(cursor, span, (size_t)span_length) == 0) {
            return 1;
        }
        cursor++;
    }

    return 0;
}

static void test_assert_seed_covers_utf8_text(const char *seed, const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;

    assert(seed != 0);
    assert(text != 0);

    while (*cursor != '\0') {
        int span_length = test_utf8_codepoint_length(*cursor);

        assert(span_length > 0);
        if (*cursor <= 126U) {
            cursor += span_length;
            continue;
        }

        if (test_seed_contains_utf8_span(seed, (const char *)cursor, span_length) == 0) {
            fprintf(stderr, "missing glyph span: %.*s\n", span_length, (const char *)cursor);
        }
        assert(test_seed_contains_utf8_span(seed, (const char *)cursor, span_length) == 1);
        cursor += span_length;
    }
}

static void test_cjk_seed_covers_recent_ui_copy(void) {
    const char *seed = DesktopTheme_cjk_glyph_seed_text();
    const char *samples[] = {
        "步骤 1: 选择科室",
        "点击【查询】",
        "填写转床时间（可选）",
        "• 当前住院记录",
        "欢迎使用患者自助服务台。",
        "床位状态: 空闲",
        "病房状态: 启用",
        "库存盘点/查询库存",
        "缺药提醒/库存不足提醒"
    };
    int sample_index = 0;

    for (sample_index = 0; sample_index < (int)(sizeof(samples) / sizeof(samples[0])); sample_index++) {
        test_assert_seed_covers_utf8_text(seed, samples[sample_index]);
    }
}

static void test_data_path_resolution_prefers_project_root_over_build_data(void) {
    char resolved[512];
    int found = DesktopApp_resolve_data_path_for_base_dir(
        "E:/project/his/build",
        "users.txt",
        resolved,
        sizeof(resolved),
        test_path_exists_prefers_project_data
    );

    assert(found == 1);
    assert(strcmp(resolved, "E:/project/his/build/../data/users.txt") == 0);
}

int main(void) {
    test_default_state_and_message();
    test_login_page_switch_and_logout();
    test_role_home_pages_on_login();
    test_role_mapping();
    test_freshness_helpers_and_window_guards();
    test_initial_window_size_fits_monitor_work_area();
    test_cjk_font_path_resolution();
    test_cjk_seed_covers_recent_ui_copy();
    test_data_path_resolution_prefers_project_root_over_build_data();
    return 0;
}
