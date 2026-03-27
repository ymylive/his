#include <assert.h>

#include "ui/DesktopPages.h"
#include "ui/DesktopTheme.h"
#include "ui/Workbench.h"

static void test_button_group_spacing_and_stack_mode(void) {
    WorkbenchButtonGroupLayout wide = Workbench_compute_button_group_layout(
        (Rectangle){ 0.0f, 0.0f, 720.0f, 44.0f },
        3,
        160.0f,
        44.0f,
        16.0f
    );
    WorkbenchButtonGroupLayout narrow = Workbench_compute_button_group_layout(
        (Rectangle){ 0.0f, 0.0f, 300.0f, 44.0f },
        3,
        160.0f,
        44.0f,
        16.0f
    );

    assert(wide.count == 3);
    assert(wide.mode == WORKBENCH_ACTION_LAYOUT_ROW);
    assert(wide.gap > 0.0f);
    assert(wide.buttons[0].x + wide.buttons[0].width + wide.gap <= wide.buttons[1].x);

    assert(narrow.count == 3);
    assert(narrow.mode == WORKBENCH_ACTION_LAYOUT_STACK);
    assert(narrow.buttons[1].y >= narrow.buttons[0].y + narrow.buttons[0].height + narrow.gap);
}

static void test_wrapped_info_rows_and_text_panel_bounds(void) {
    WorkbenchInfoRowLayout info = Workbench_compute_info_row_layout(
        (Rectangle){ 0.0f, 0.0f, 360.0f, 72.0f },
        110.0f,
        14.0f
    );
    WorkbenchTextPanelLayout panel = Workbench_compute_text_panel_layout(
        (Rectangle){ 0.0f, 0.0f, 420.0f, 220.0f },
        14.0f,
        28.0f,
        18.0f
    );

    assert(info.gap > 0.0f);
    assert(info.value_bounds.width > 0.0f);
    assert(info.value_bounds.x >= info.label_bounds.x + info.label_bounds.width + info.gap);
    assert(info.wrap_lines >= 2);

    assert(panel.content_bounds.y > panel.title_bounds.y);
    assert(panel.content_bounds.width < 420.0f);
    assert(panel.wrap_width == panel.content_bounds.width);
}

static void test_list_detail_layout_stacks_when_space_is_tight(void) {
    WorkbenchListDetailLayout wide = Workbench_compute_list_detail_layout(
        (Rectangle){ 0.0f, 0.0f, 960.0f, 480.0f },
        0.48f,
        20.0f,
        260.0f
    );
    WorkbenchListDetailLayout narrow = Workbench_compute_list_detail_layout(
        (Rectangle){ 0.0f, 0.0f, 520.0f, 480.0f },
        0.48f,
        20.0f,
        260.0f
    );

    assert(wide.is_stacked == 0);
    assert(wide.list_bounds.width >= 260.0f);
    assert(wide.list_bounds.x + wide.list_bounds.width + wide.gap <= wide.detail_bounds.x);

    assert(narrow.is_stacked != 0);
    assert(narrow.detail_bounds.y >= narrow.list_bounds.y + narrow.list_bounds.height + narrow.gap);
}

static void test_topbar_and_login_shell_avoid_overlap(void) {
    DesktopTheme theme = DesktopTheme_make_default();
    DesktopTopbarLayout topbar = DesktopPages_compute_topbar_layout(
        1366,
        &theme,
        320.0f,
        420.0f,
        148.0f
    );
    DesktopLoginLayout login = DesktopPages_compute_login_layout(1366, 768, &theme);

    assert(topbar.gap > 0.0f);
    assert(topbar.session_bounds.x >= topbar.title_bounds.x + topbar.title_bounds.width + topbar.gap);
    assert(topbar.session_bounds.x + topbar.session_bounds.width + topbar.gap <= topbar.time_bounds.x);
    assert(topbar.time_bounds.x + topbar.time_bounds.width + topbar.gap <= topbar.logout_bounds.x);

    assert(login.gap > 0.0f);
    assert(login.user_box.width > 0.0f);
    assert(login.role_prev_bounds.x + login.role_prev_bounds.width + login.gap <= login.role_value_bounds.x);
    assert(login.role_value_bounds.x + login.role_value_bounds.width + login.gap <= login.role_next_bounds.x);
    assert(login.login_button_bounds.y >= login.role_value_bounds.y + login.role_value_bounds.height + login.gap);
}

int main(void) {
    test_button_group_spacing_and_stack_mode();
    test_wrapped_info_rows_and_text_panel_bounds();
    test_list_detail_layout_stacks_when_space_is_tight();
    test_topbar_and_login_shell_avoid_overlap();
    return 0;
}
