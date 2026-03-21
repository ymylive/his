#include <assert.h>
#include <string.h>

#include "ui/DesktopApp.h"
#include "ui/DesktopTheme.h"

static void test_default_state_and_message(void) {
    DesktopAppState state;

    DesktopAppState_init(&state);
    assert(state.current_page == DESKTOP_PAGE_LOGIN);
    assert(state.logged_in == 0);
    assert(state.login_form.role_index == 0);

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

static int test_font_exists_for_noto(const char *path) {
    if (path == 0) {
        return 0;
    }
    return strcmp(path, "C:/Windows/Fonts/NotoSansSC-VF.ttf") == 0;
}

static int test_font_exists_for_none(const char *path) {
    (void)path;
    return 0;
}

static void test_cjk_font_path_resolution(void) {
    const char *picked = DesktopTheme_resolve_cjk_font_path(test_font_exists_for_noto);
    const char *missing = DesktopTheme_resolve_cjk_font_path(test_font_exists_for_none);

    assert(picked != 0);
    assert(strcmp(picked, "C:/Windows/Fonts/NotoSansSC-VF.ttf") == 0);
    assert(missing == 0);
}

int main(void) {
    test_default_state_and_message();
    test_login_page_switch_and_logout();
    test_role_home_pages_on_login();
    test_role_mapping();
    test_cjk_font_path_resolution();
    return 0;
}
