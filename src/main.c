/**
 * @file main.c
 * @brief HIS 医院信息系统 - 程序入口
 *
 * 本文件是 HIS 系统的主入口，负责：
 * 1. 系统初始化（数据加载、Windows 控制台配置）
 * 2. 启动动画展示
 * 3. 主菜单循环（角色选择 -> 登录 -> 角色菜单 -> 操作执行）
 * 4. 用户输入处理（支持 ESC 返回、EOF 退出）
 */

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

/**
 * @brief 安全清零敏感缓冲区，防止编译器优化掉清除操作
 *
 * 在 POSIX 系统上使用 explicit_bzero（如果可用），否则使用
 * volatile 指针逐字节清零，确保编译器不会将清零优化掉。
 */
#if defined(__GLIBC__) && defined(__GLIBC_MINOR__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 25))
#include <string.h>
#define secure_zero(ptr, len) explicit_bzero((ptr), (len))
#else
#define secure_zero(ptr, len) do { volatile char *_p = (volatile char*)(ptr); size_t _n = (len); while (_n--) *_p++ = 0; } while(0)
#endif

#include "common/InputHelper.h"
#include "common/UpdateChecker.h"
#include "service/AuthService.h"
#include "ui/DemoData.h"
#include "ui/MenuActionHandlers.h"
#include "ui/MenuApplication.h"
#include "ui/MenuController.h"
#include "ui/TuiPanel.h"
#include "ui/TuiStyle.h"

/* ── 配置常量 ── */
#define HIS_DATA_DIR           "data/"
#define HIS_DEMO_SEED_DIR      "data/demo_seed/"
#define HIS_MENU_BUFFER_SIZE   8192
#define HIS_INPUT_BUFFER_SIZE  128

/**
 * @brief 启动时收紧数据目录与 *.txt 文件权限，防止 PHI 泄露
 *
 * 合规（HIPAA / 个保法）：`data/*.txt` 含身份证、手机号、病史等 PHI。
 * 仓库内 demo fixture 以 0644 入 git，首次启动时必须收紧到 0600；
 * 目录收紧到 0700。仅处理 HIS 项目自己的 data/ 与 data/demo_seed/。
 * Windows 上无等效 POSIX 权限位，保留 TODO 以 ACL 方式实现。
 */
static void his_harden_data_permissions(void) {
#ifdef _WIN32
    /* TODO(security): Windows 下通过 SetNamedSecurityInfo 设置 ACL，
       限制只有当前用户可读写 data/*.txt。当前版本跳过。 */
    return;
#else
    const char *directories[] = { HIS_DATA_DIR, HIS_DEMO_SEED_DIR };
    size_t i;

    for (i = 0; i < sizeof(directories) / sizeof(directories[0]); i++) {
        const char *dir_path = directories[i];
        DIR *dir = opendir(dir_path);
        struct dirent *entry;
        char file_path[512];

        if (dir == NULL) {
            continue; /* 目录不存在（例如首次运行）由仓储按需创建 */
        }

        /* 先收紧目录本身到 0700 */
        (void)chmod(dir_path, 0700);

        while ((entry = readdir(dir)) != NULL) {
            const char *name = entry->d_name;
            size_t len = strlen(name);

            /* 只处理 *.txt，跳过 . / .. 与子目录 */
            if (len < 5 || strcmp(name + len - 4, ".txt") != 0) {
                continue;
            }
            if (snprintf(file_path, sizeof(file_path), "%s%s", dir_path, name)
                >= (int)sizeof(file_path)) {
                continue;
            }
            (void)chmod(file_path, 0600);
        }
        closedir(dir);
    }
#endif
}

#ifdef _WIN32
/**
 * @brief 初始化 Windows 控制台
 *
 * 设置控制台代码页为 UTF-8，启用 ANSI 转义序列处理，
 * 确保中文字符和彩色输出在 Windows 终端中正确显示。
 */
static void init_windows_console(void) {
    HANDLE hOut = INVALID_HANDLE_VALUE;
    HANDLE hIn  = INVALID_HANDLE_VALUE;
    DWORD  mode = 0;

    /* Set console code page to UTF-8 for correct CJK display */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    /* Enable ANSI escape sequence processing on stdout */
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
        SetConsoleMode(hOut, mode);
    }

    /* Enable VT input on stdin */
    hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE && GetConsoleMode(hIn, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
        SetConsoleMode(hIn, mode);
    }
}
#endif

/**
 * @brief 将菜单角色枚举转换为用户角色枚举
 * @param role 菜单角色
 * @return 对应的用户角色（无法映射时返回 USER_ROLE_UNKNOWN）
 */
static UserRole role_to_user_role(MenuRole role) {
    switch (role) {
        case MENU_ROLE_ADMIN:
            return USER_ROLE_ADMIN;
        case MENU_ROLE_DOCTOR:
            return USER_ROLE_DOCTOR;
        case MENU_ROLE_PATIENT:
            return USER_ROLE_PATIENT;
        case MENU_ROLE_INPATIENT_MANAGER:
            return USER_ROLE_INPATIENT_MANAGER;
        case MENU_ROLE_PHARMACY:
            return USER_ROLE_PHARMACY;
        default:
            return USER_ROLE_UNKNOWN;
    }
}

/**
 * @brief 将菜单角色枚举转换为 TUI 主题枚举
 * @param role 菜单角色
 * @return 对应的 TUI 角色主题
 */
static TuiRoleTheme role_to_theme(MenuRole role) {
    switch (role) {
        case MENU_ROLE_ADMIN:
            return TUI_THEME_ADMIN;
        case MENU_ROLE_DOCTOR:
            return TUI_THEME_DOCTOR;
        case MENU_ROLE_PATIENT:
            return TUI_THEME_PATIENT;
        case MENU_ROLE_INPATIENT_MANAGER:
            return TUI_THEME_INPATIENT;
        case MENU_ROLE_PHARMACY:
            return TUI_THEME_PHARMACY;
        default:
            return TUI_THEME_DEFAULT;
    }
}

/**
 * @brief 患者自助注册流程
 *
 * 引导患者填写个人信息表单、设置密码、创建患者记录和用户账号。
 *
 * @param application 应用程序上下文
 * @param input_stream 输入流（stdin）
 * @param output_stream 输出流（stdout）
 * @return 0 表示正常返回（注册成功或取消），-1 表示收到 EOF 需要退出程序
 */
static int handle_patient_registration(MenuApplication *application,
                                       FILE *input_stream,
                                       FILE *output_stream) {
    Patient new_patient;
    char reg_username[HIS_DOMAIN_ID_CAPACITY];
    char reg_password[HIS_INPUT_BUFFER_SIZE];
    char reg_confirm[HIS_INPUT_BUFFER_SIZE];
    char reg_output[2048];
    char choice[16];
    MenuApplicationPromptContext reg_ctx;
    Result result;
    int read_result = 0;

    reg_ctx.input = input_stream;
    reg_ctx.output = output_stream;

    tui_print_section(output_stream, TUI_SPARKLE, "患者注册");
    fprintf(output_stream, "\n");

    /* 填写患者信息表单 */
    result = MenuApplication_prompt_patient_form(&reg_ctx, &new_patient, 0);
    if (result.success == 0) {
        if (InputHelper_is_esc_cancel(result.message)) {
            return 0; /* back to role selection */
        }
        tui_animate_error(output_stream, result.message);
        tui_delay(800);
        return 0;
    }

    /* 设置用户名 */
    tui_print_prompt(output_stream, "设置登录用户名: ");
    read_result = InputHelper_read_line(input_stream, reg_username, sizeof(reg_username));
    if (read_result <= 0 || read_result == -2) {
        if (read_result == 0) return -1;
        return 0;
    }

    /* 设置密码 */
    tui_print_prompt(output_stream, "设置登录密码: ");
    read_result = InputHelper_read_line(input_stream, reg_password, sizeof(reg_password));
    if (read_result <= 0 || read_result == -2) {
        secure_zero(reg_password, sizeof(reg_password));
        if (read_result == 0) return -1;
        return 0;
    }

    tui_print_prompt(output_stream, "确认登录密码: ");
    read_result = InputHelper_read_line(input_stream, reg_confirm, sizeof(reg_confirm));
    if (read_result <= 0 || read_result == -2) {
        secure_zero(reg_password, sizeof(reg_password));
        secure_zero(reg_confirm, sizeof(reg_confirm));
        if (read_result == 0) return -1;
        return 0;
    }

    if (strcmp(reg_password, reg_confirm) != 0) {
        tui_animate_error(output_stream, "两次输入的密码不一致");
        secure_zero(reg_password, sizeof(reg_password));
        secure_zero(reg_confirm, sizeof(reg_confirm));
        tui_delay(800);
        return 0;
    }
    secure_zero(reg_confirm, sizeof(reg_confirm));

    /* 创建患者记录 */
    tui_spinner_run(output_stream, "正在注册...", 500);
    result = MenuApplication_add_patient(
        application, &new_patient,
        reg_output, sizeof(reg_output));
    if (result.success == 0) {
        tui_animate_error(output_stream, result.message);
        secure_zero(reg_password, sizeof(reg_password));
        tui_delay(800);
        return 0;
    }

    /* 从输出中提取系统生成的患者编号
     * 输出格式: "患者已添加: PAT0101 | ..."
     * 提取 ": " 后到 " |" 之间的 ID */
    {
        const char *id_start = strstr(reg_output, ": ");
        const char *id_end = 0;
        if (id_start) {
            id_start += 2; /* skip ": " */
            id_end = strstr(id_start, " |");
            if (id_end && (size_t)(id_end - id_start) < sizeof(new_patient.patient_id)) {
                memset(new_patient.patient_id, 0, sizeof(new_patient.patient_id));
                memcpy(new_patient.patient_id, id_start, (size_t)(id_end - id_start));
            }
        }
    }

    /* 创建用户账号（用自定义用户名作为登录账号） */
    result = AuthService_register_user(
        &application->auth_service,
        reg_username,
        reg_password,
        USER_ROLE_PATIENT,
        new_patient.patient_id);
    secure_zero(reg_password, sizeof(reg_password));

    if (result.success == 0) {
        tui_animate_error(output_stream, result.message);
        tui_delay(800);
        return 0;
    }

    /* 注册成功提示 */
    fprintf(output_stream, "\n");
    tui_animate_success(output_stream, "注册成功！");
    fprintf(output_stream, "\n");
    tui_print_kv_colored(output_stream, "您的用户名", reg_username, TUI_BOLD_CYAN);
    tui_print_kv_colored(output_stream, "您的患者编号", new_patient.patient_id, TUI_BOLD_CYAN);
    tui_print_info(output_stream, "请牢记您的用户名和密码，使用用户名登录系统。");
    fprintf(output_stream, "\n");
    tui_print_info(output_stream, "按回车返回登录...");
    fflush(output_stream);
    InputHelper_read_line(input_stream, choice, sizeof(choice));
    return 0; /* back to role selection so user can login */
}

/**
 * @brief 登录验证流程
 *
 * 循环提示用户输入编号和密码，调用认证服务验证。
 * 用户编号处按 ESC 返回角色选择，密码处按 ESC 返回编号输入。
 *
 * @param application 应用程序上下文
 * @param role 当前选择的菜单角色
 * @param required_role 需要验证的用户角色
 * @param input_stream 输入流
 * @param output_stream 输出流
 * @param out_user_id 输出参数，登录成功后存放用户编号
 * @param user_id_size out_user_id 缓冲区大小
 * @return 1 表示登录成功，0 表示返回角色选择，-1 表示收到 EOF 需要退出
 */
static int handle_login_flow(MenuApplication *application,
                             MenuRole role,
                             UserRole required_role,
                             FILE *input_stream,
                             FILE *output_stream,
                             char *out_user_id,
                             size_t user_id_size) {
    char input[HIS_INPUT_BUFFER_SIZE];
    char password[HIS_INPUT_BUFFER_SIZE];
    char prompt_buf[256];
    Result result;

    for (;;) {
        int read_result = 0;

        snprintf(prompt_buf, sizeof(prompt_buf),
            "请输入[%s]用户名 (ESC返回): ",
            MenuController_role_label(role));
        tui_print_prompt(output_stream, prompt_buf);
        read_result = InputHelper_read_line(input_stream, input, sizeof(input));
        if (read_result == 0) {
            return -1; /* EOF, exit program */
        }
        if (read_result == -2) {
            return 0; /* back to role selection */
        }
        if (read_result < 0) {
            tui_print_warning(output_stream, "输入过长，请重新输入。");
            tui_delay(800);
            continue; /* retry user-ID */
        }

        tui_print_prompt(output_stream, "请输入密码 (ESC返回): ");
        read_result = InputHelper_read_line(input_stream, password, sizeof(password));
        if (read_result == 0) {
            return -1; /* EOF, exit program */
        }
        if (read_result == -2) {
            continue; /* back to user-ID prompt */
        }
        if (read_result < 0) {
            tui_print_warning(output_stream, "输入过长，请重新输入。");
            tui_delay(800);
            continue; /* retry from user-ID */
        }

        /* 尝试登录验证。失败计数与锁定由 AuthService 持久化处理， */
        /* 不再依赖内存中的计数器（防止通过返回角色选择重置攻击）。 */
        result = MenuApplication_login(application, input, password, required_role);
        /* 安全清除敏感缓冲区：密码和用户输入 */
        secure_zero(password, sizeof(password));

        if (result.success == 0) {
            secure_zero(input, sizeof(input));
            tui_animate_error(output_stream, result.message);
            tui_delay(500);
            continue; /* retry from user-ID */
        }

        /* 登录成功，保存用户编号 */
        strncpy(out_user_id, input, user_id_size - 1);
        out_user_id[user_id_size - 1] = '\0';
        secure_zero(input, sizeof(input));
        return 1; /* login successful */
    }
}

/**
 * @brief 角色菜单循环
 *
 * 登录成功后，在分屏布局中显示角色菜单，处理用户操作选择。
 * 按 ESC 或选择返回操作退出菜单循环。
 *
 * @param application 应用程序上下文
 * @param role 当前菜单角色
 * @param theme 角色对应的 TUI 主题
 * @param user_id 已登录的用户编号
 * @param input_stream 输入流
 * @param output_stream 输出流
 */
static void handle_role_menu_loop(MenuApplication *application,
                                  MenuRole role,
                                  TuiRoleTheme theme,
                                  const char *user_id,
                                  FILE *input_stream,
                                  FILE *output_stream) {
    TuiLayout layout;
    char sidebar_title[256];
    char input[HIS_INPUT_BUFFER_SIZE];
    Result result;

    tui_clear_screen();
    tui_hide_cursor(output_stream);
    TuiLayout_compute(&layout);

    /* 绘制侧边栏左边框 */
    TuiPanel_draw_left_border(output_stream, &layout.sidebar, TUI_OC_BORDER);

    /* 侧边栏：显示角色标题 */
    snprintf(sidebar_title, sizeof(sidebar_title),
             "%s %s",
             tui_role_icon(theme),
             MenuController_role_label(role));
    TuiPanel_write_at(output_stream, &layout.sidebar, 1, 2, sidebar_title);

    /* 底部状态栏：角色 + 用户编号 */
    {
        char status_text[256];
        int col = 0;
        snprintf(status_text, sizeof(status_text),
                 " %s %s | %s",
                 tui_role_icon(theme),
                 MenuController_role_label(role),
                 user_id);
        TuiPanel_move_to(output_stream, &layout.footer, 0, 0);
        fprintf(output_stream, TUI_OC_BG_PANEL TUI_OC_TEXT);
        for (col = 0; col < layout.term_width; col++) {
            fputc(' ', output_stream);
        }
        TuiPanel_move_to(output_stream, &layout.footer, 0, 0);
        fprintf(output_stream, TUI_OC_BG_PANEL TUI_OC_TEXT "%s" TUI_RESET, status_text);
    }
    fflush(output_stream);

    /* ── 角色菜单循环：交互式选择操作 ── */
    for (;;) {
        MenuAction action = MENU_ACTION_INVALID;

        /* 会话空闲超时检查：超过阈值则强制登出并返回主菜单 */
        if (MenuApplication_is_session_idle(application, time(NULL))) {
            tui_clear_screen();
            tui_show_cursor(output_stream);
            tui_animate_error(output_stream,
                "\xe4\xbc\x9a\xe8\xaf\x9d\xe8\xb6\x85\xe6\x97\xb6\xef\xbc\x8c"
                "\xe8\xaf\xb7\xe9\x87\x8d\xe6\x96\xb0\xe7\x99\xbb\xe5\xbd\x95"
                /* 会话超时，请重新登录 */);
            fprintf(stderr,
                "AUDIT: LOGOUT user=%s reason=idle_timeout\n",
                user_id != 0 ? user_id : "<unknown>");
            tui_delay(1200);
            break;
        }

        /* 每次回到菜单前，完整重绘分屏界面 */
        tui_clear_screen();
        tui_hide_cursor(output_stream);
        TuiLayout_compute(&layout);
        TuiPanel_draw_left_border(output_stream, &layout.sidebar, TUI_OC_BORDER);
        TuiPanel_write_at(output_stream, &layout.sidebar, 1, 2, "");
        fprintf(output_stream, TUI_OC_ACCENT TUI_BOLD "%s %s" TUI_RESET,
                tui_role_icon(theme), MenuController_role_label(role));
        /* 重绘底栏 */
        {
            int col = 0;
            TuiPanel_move_to(output_stream, &layout.footer, 0, 0);
            fprintf(output_stream, TUI_OC_BG_PANEL TUI_OC_TEXT);
            for (col = 0; col < layout.term_width; col++) fputc(' ', output_stream);
            TuiPanel_move_to(output_stream, &layout.footer, 0, 0);
            fprintf(output_stream, TUI_OC_BG_PANEL TUI_OC_TEXT " %s %s | %s" TUI_RESET,
                    tui_role_icon(theme), MenuController_role_label(role), user_id);
        }
        fflush(output_stream);

        /* 使用交互式菜单选择（方向键导航 + 数字直选） */
        result = MenuController_interactive_select(role, &layout.sidebar, input_stream, &action);
        if (result.success == 0) {
            break;
        }

        if (MenuController_is_back_action(action)) {
            break;
        }

        /* 成功读取到一次菜单选择 → 刷新活动时间 */
        MenuApplication_touch_activity(application);

        /* 全屏显示操作内容 */
        tui_clear_screen();
        tui_show_cursor(output_stream);
        fflush(output_stream);

        /* 执行选中的操作 */
        result = MenuApplication_execute_action(application, action, input_stream, output_stream);
        /* 仅在真正的错误时显示错误消息 */
        if (result.success == 0 &&
            strcmp(result.message, "action not implemented yet") != 0 &&
            !InputHelper_is_esc_cancel(result.message)) {
            tui_print_error(output_stream, result.message);
        }

        /* 操作完成后等待回车或ESC返回菜单 */
        tui_print_info(output_stream, "\xe6\x8c\x89\xe5\x9b\x9e\xe8\xbd\xa6\xe8\xbf\x94\xe5\x9b\x9e\xe8\x8f\x9c\xe5\x8d\x95..." /* 按回车返回菜单... */);
        fflush(output_stream);
        InputHelper_read_line(input_stream, input, sizeof(input));
        MenuApplication_touch_activity(application);
    }

    tui_show_cursor(output_stream);
}

/**
 * @brief 程序主入口
 *
 * 执行流程：
 * 1. 初始化控制台和应用程序
 * 2. 显示启动动画（矩阵雨 -> Logo -> 标题闪烁 -> Banner）
 * 3. 检查更新
 * 4. 进入主循环：角色选择 -> 登录验证 -> 角色菜单 -> 操作执行
 * 5. 用户可通过 ESC 逐级返回，输入 0 或 EOF 退出系统
 */
int main(void) {
    char user_id[HIS_INPUT_BUFFER_SIZE];  /* 登录用户编号缓存 */
    MenuApplication application;
    MenuApplicationPaths paths;
    Result init_result;

#ifdef _WIN32
    init_windows_console();
#endif

    /* 合规：启动时收紧 data/ 目录与 *.txt 权限（防御 git checkout 留下的 0644） */
    his_harden_data_permissions();

    /* 配置所有数据文件的路径 */
    paths = (MenuApplicationPaths){
        HIS_DATA_DIR "users.txt",
        HIS_DATA_DIR "patients.txt",
        HIS_DATA_DIR "departments.txt",
        HIS_DATA_DIR "doctors.txt",
        HIS_DATA_DIR "registrations.txt",
        HIS_DATA_DIR "visits.txt",
        HIS_DATA_DIR "examinations.txt",
        HIS_DATA_DIR "wards.txt",
        HIS_DATA_DIR "beds.txt",
        HIS_DATA_DIR "admissions.txt",
        HIS_DATA_DIR "medicines.txt",
        HIS_DATA_DIR "dispense_records.txt",
        HIS_DATA_DIR "prescriptions.txt",
        HIS_DATA_DIR "inpatient_orders.txt",
        HIS_DATA_DIR "nursing_records.txt",
        HIS_DATA_DIR "round_records.txt",
        HIS_DATA_DIR,
        HIS_DATA_DIR "sequences.txt"
    };

    /* 初始化应用程序（加载所有业务服务） */
    init_result = MenuApplication_init(&application, &paths);

    if (RESULT_FAILED(init_result)) {
        fprintf(stderr, "系统初始化失败: %s\n", init_result.message);
        return 1;
    }

    /* 启动画面：Logo + Banner */
    tui_clear_screen();
    tui_print_logo(stdout);
    tui_print_banner(stdout, "\xe8\xbd\xbb\xe9\x87\x8f\xe7\xba\xa7\xe5\x8c\xbb\xe9\x99\xa2\xe4\xbf\xa1\xe6\x81\xaf\xe7\xb3\xbb\xe7\xbb\x9f" /* 轻量级医院信息系统 */);

    /* ── 启动时检查更新 ── */
    {
        UpdateInfo update_info;
        tui_spinner_run(stdout, "正在检查更新...", 300);
        if (UpdateChecker_check(&update_info) && update_info.has_update) {
            int update_result = UpdateChecker_prompt(stdout, stdin, &update_info);
            if (update_result == 2) {
                /* 更新已安装，退出以便新版本启动 */
                tui_delay(1500);
                tui_show_cursor(stdout);
                return 0;
            }
            tui_delay(500);
        }
    }

    /* ═══════════════ 主循环：角色选择 -> 登录 -> 操作 ═══════════════ */
    for (;;) {
        MenuRole role = MENU_ROLE_INVALID;
        TuiRoleTheme theme = TUI_THEME_DEFAULT;
        Result result = Result_make_success("ready");
        UserRole required_role;
        int login_result = 0;

        tui_clear_screen();
        tui_animate_logo(stdout);
        tui_print_gradient_hline(stdout, 46);
        tui_print_section(stdout, TUI_MEDICAL, "\xe8\xaf\xb7\xe9\x80\x89\xe6\x8b\xa9\xe6\x82\xa8\xe7\x9a\x84\xe8\xa7\x92\xe8\x89\xb2" /* 请选择您的角色 */);
        fputc('\n', stdout);

        /* 交互式主菜单：方向键选择角色 */
        result = MenuController_interactive_main_select(stdin, &role);
        if (RESULT_FAILED(result)) {
            continue;
        }

        if (MenuController_is_exit_role(role)) {
            tui_clear_screen();
            tui_animate_goodbye(stdout);
            tui_show_cursor(stdout);
            return 0;
        }

        /* 处理演示数据重置（特殊角色，不需要登录） */
        if (role == MENU_ROLE_RESET_DEMO) {
            char reset_message[RESULT_MESSAGE_CAPACITY];
            tui_print_section(stdout, TUI_SPARKLE, "重置演示数据");
            tui_spinner_run(stdout, "正在重置数据...", 600);
            result = DemoData_reset(&paths, reset_message, sizeof(reset_message));
            if (RESULT_FAILED(result)) {
                tui_animate_error(stdout, reset_message[0] != '\0' ? reset_message : result.message);
            } else {
                tui_animate_success(stdout, reset_message);
            }
            continue;
        }

        /* 获取角色对应的主题配色 */
        theme = role_to_theme(role);
        required_role = role_to_user_role(role);

        if (required_role == USER_ROLE_UNKNOWN) {
            tui_print_warning(stdout, "角色未配置登录映射。");
            tui_delay(1000);
            continue;
        }

        /* 患者角色：提供登录/注册选择 */
        if (role == MENU_ROLE_PATIENT) {
            char choice[16];
            int read_result = 0;

            tui_print_section(stdout, TUI_HEART, "患者入口");
            fprintf(stdout,
                "\n  " TUI_BOLD_YELLOW "[1]" TUI_RESET " 登录已有账号\n"
                "  " TUI_BOLD_YELLOW "[2]" TUI_RESET " 注册新账号\n\n");
            tui_print_prompt(stdout, "请选择 (ESC返回): ");
            read_result = InputHelper_read_line(stdin, choice, sizeof(choice));
            if (read_result == 0) {
                tui_clear_screen();
                tui_animate_goodbye(stdout);
                tui_show_cursor(stdout);
                return 0;
            }
            if (read_result == -2) {
                continue; /* back to role selection */
            }

            if (strcmp(choice, "2") == 0) {
                int reg_result = handle_patient_registration(&application, stdin, stdout);
                if (reg_result == -1) {
                    tui_clear_screen();
                    tui_animate_goodbye(stdout);
                    tui_show_cursor(stdout);
                    return 0;
                }
                continue; /* back to role selection */
            }

            if (strcmp(choice, "1") != 0) {
                continue; /* invalid choice, back to role selection */
            }
            /* choice == "1": fall through to normal login */
        }

        /* 登录流程 */
        login_result = handle_login_flow(
            &application, role, required_role,
            stdin, stdout,
            user_id, sizeof(user_id));

        if (login_result == -1) {
            /* EOF received */
            tui_clear_screen();
            tui_animate_goodbye(stdout);
            tui_show_cursor(stdout);
            return 0;
        }
        if (login_result == 0) {
            continue; /* back to role selection */
        }

        /* 首次登录强制修改默认密码 */
        if (application.authenticated_user.force_password_change == 1) {
            char old_pw[HIS_INPUT_BUFFER_SIZE];
            char new_pw[HIS_INPUT_BUFFER_SIZE];
            char confirm_pw[HIS_INPUT_BUFFER_SIZE];
            int pw_changed = 0;
            int read_res = 0;

            tui_print_warning(stdout,
                "\xe9\xa6\x96\xe6\xac\xa1\xe7\x99\xbb\xe5\xbd\x95\xef\xbc\x8c"
                "\xe8\xaf\xb7\xe4\xbf\xae\xe6\x94\xb9\xe9\xbb\x98\xe8\xae\xa4"
                "\xe5\xaf\x86\xe7\xa0\x81" /* 首次登录，请修改默认密码 */);

            while (pw_changed == 0) {
                tui_print_prompt(stdout,
                    "\xe8\xaf\xb7\xe8\xbe\x93\xe5\x85\xa5\xe5\xbd\x93\xe5\x89\x8d"
                    "\xe5\xaf\x86\xe7\xa0\x81 (ESC\xe8\xbf\x94\xe5\x9b\x9e): "
                    /* 请输入当前密码 (ESC返回):  */);
                read_res = InputHelper_read_line(stdin, old_pw, sizeof(old_pw));
                if (read_res == 0) {
                    secure_zero(old_pw, sizeof(old_pw));
                    tui_clear_screen();
                    tui_animate_goodbye(stdout);
                    tui_show_cursor(stdout);
                    return 0;
                }
                if (read_res == -2) {
                    break; /* ESC - cancel, will log out */
                }

                tui_print_prompt(stdout,
                    "\xe8\xaf\xb7\xe8\xbe\x93\xe5\x85\xa5\xe6\x96\xb0\xe5\xaf\x86"
                    "\xe7\xa0\x81: " /* 请输入新密码:  */);
                read_res = InputHelper_read_line(stdin, new_pw, sizeof(new_pw));
                if (read_res == 0) {
                    secure_zero(old_pw, sizeof(old_pw));
                    secure_zero(new_pw, sizeof(new_pw));
                    tui_clear_screen();
                    tui_animate_goodbye(stdout);
                    tui_show_cursor(stdout);
                    return 0;
                }
                if (read_res == -2) {
                    secure_zero(old_pw, sizeof(old_pw));
                    break;
                }

                tui_print_prompt(stdout,
                    "\xe8\xaf\xb7\xe7\xa1\xae\xe8\xae\xa4\xe6\x96\xb0\xe5\xaf\x86"
                    "\xe7\xa0\x81: " /* 请确认新密码:  */);
                read_res = InputHelper_read_line(stdin, confirm_pw, sizeof(confirm_pw));
                if (read_res == 0) {
                    secure_zero(old_pw, sizeof(old_pw));
                    secure_zero(new_pw, sizeof(new_pw));
                    secure_zero(confirm_pw, sizeof(confirm_pw));
                    tui_clear_screen();
                    tui_animate_goodbye(stdout);
                    tui_show_cursor(stdout);
                    return 0;
                }
                if (read_res == -2) {
                    secure_zero(old_pw, sizeof(old_pw));
                    secure_zero(new_pw, sizeof(new_pw));
                    break;
                }

                if (strcmp(new_pw, confirm_pw) != 0) {
                    tui_animate_error(stdout,
                        "\xe4\xb8\xa4\xe6\xac\xa1\xe8\xbe\x93\xe5\x85\xa5\xe7\x9a\x84"
                        "\xe5\xaf\x86\xe7\xa0\x81\xe4\xb8\x8d\xe4\xb8\x80\xe8\x87\xb4"
                        /* 两次输入的密码不一致 */);
                    secure_zero(old_pw, sizeof(old_pw));
                    secure_zero(new_pw, sizeof(new_pw));
                    secure_zero(confirm_pw, sizeof(confirm_pw));
                    tui_delay(500);
                    continue;
                }

                {
                    Result pw_result = AuthService_change_password(
                        &application.auth_service, user_id, old_pw, new_pw);
                    secure_zero(old_pw, sizeof(old_pw));
                    secure_zero(new_pw, sizeof(new_pw));
                    secure_zero(confirm_pw, sizeof(confirm_pw));

                    if (pw_result.success == 0) {
                        /* 审计：密码修改失败 */
                        (void)AuditService_record(
                            &application.audit_service,
                            AUDIT_EVENT_PASSWORD_CHANGE,
                            user_id,
                            0,
                            "USER",
                            user_id,
                            "FAIL",
                            pw_result.message);
                        tui_animate_error(stdout, pw_result.message);
                        tui_delay(500);
                        continue;
                    }

                    /* 审计：密码修改成功 */
                    (void)AuditService_record(
                        &application.audit_service,
                        AUDIT_EVENT_PASSWORD_CHANGE,
                        user_id,
                        0,
                        "USER",
                        user_id,
                        "OK",
                        0);
                    application.authenticated_user.force_password_change = 0;
                    pw_changed = 1;
                    tui_animate_success(stdout,
                        "\xe5\xaf\x86\xe7\xa0\x81\xe4\xbf\xae\xe6\x94\xb9\xe6\x88\x90"
                        "\xe5\x8a\x9f" /* 密码修改成功 */);
                    tui_delay(800);
                }
            }

            if (pw_changed == 0) {
                /* User cancelled - log out and return to role selection */
                MenuApplication_logout(&application);
                secure_zero(user_id, sizeof(user_id));
                continue;
            }
        }

        /* 登录成功：进入角色菜单循环 */
        handle_role_menu_loop(&application, role, theme, user_id, stdin, stdout);

        /* 退出角色菜单循环后，执行登出操作 */
        MenuApplication_logout(&application);
        secure_zero(user_id, sizeof(user_id));
    }
}
