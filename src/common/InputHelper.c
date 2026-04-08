/**
 * @file InputHelper.c
 * @brief 输入辅助模块的实现 - 提供输入验证和支持 ESC 取消的行读取功能
 *
 * 本模块实现了文本输入的验证（非空、正整数、非负数）以及
 * 支持 ESC 键取消的终端行读取。针对 Windows 和 POSIX 平台
 * 分别使用 _getch/termios 进行原始模式的按键预读。
 */

#include "common/InputHelper.h"

#include <ctype.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#endif

/**
 * @brief 跳过字符串开头的空白字符
 *
 * 内部辅助函数，返回指向第一个非空白字符的指针。
 *
 * @param text 输入字符串
 * @return 指向第一个非空白字符的指针
 */
static const char *InputHelper_skip_spaces(const char *text) {
    while (text != 0 && *text != '\0' && isspace((unsigned char)*text) != 0) {
        text++;
    }

    return text;
}

/**
 * @brief 定位字符串末尾（去除尾部空白后的位置）
 *
 * 内部辅助函数，返回指向去除尾部空白后最后一个有效字符之后的位置。
 *
 * @param start 字符串起始位置
 * @return 指向有效内容末尾之后的位置
 */
static const char *InputHelper_trim_end(const char *start) {
    const char *end = start;

    /* 先找到字符串末尾 */
    while (*end != '\0') {
        end++;
    }

    /* 从末尾往回跳过空白字符 */
    while (end > start && isspace((unsigned char)*(end - 1)) != 0) {
        end--;
    }

    return end;
}

/**
 * @brief 判断文本是否非空（去除首尾空白后仍有内容）
 *
 * @param text 待检测的字符串，可以为 NULL
 * @return 非空返回 1，为空或 NULL 返回 0
 */
int InputHelper_is_non_empty(const char *text) {
    const char *start = 0;
    const char *end = 0;

    if (text == 0) {
        return 0;
    }

    /* 跳过前导空白 */
    start = InputHelper_skip_spaces(text);
    /* 找到去除尾部空白后的末尾位置 */
    end = InputHelper_trim_end(start);
    /* 如果 end > start，说明中间有非空白内容 */
    return end > start ? 1 : 0;
}

/**
 * @brief 判断文本是否为正整数
 *
 * 允许首尾空白，数字部分必须全为数字且至少有一位非零数字。
 *
 * @param text 待检测的字符串，可以为 NULL
 * @return 是正整数返回 1，否则返回 0
 */
int InputHelper_is_positive_integer(const char *text) {
    const char *current = 0;
    int has_digit = 0;       /* 是否包含数字 */
    int non_zero_found = 0;  /* 是否包含非零数字 */

    if (InputHelper_is_non_empty(text) == 0) {
        return 0;
    }

    /* 跳过前导空白 */
    current = InputHelper_skip_spaces(text);

    /* 扫描数字部分 */
    while (*current != '\0' && isspace((unsigned char)*current) == 0) {
        /* 遇到非数字字符，直接返回失败 */
        if (isdigit((unsigned char)*current) == 0) {
            return 0;
        }

        /* 记录是否出现了非零数字 */
        if (*current != '0') {
            non_zero_found = 1;
        }

        has_digit = 1;
        current++;
    }

    /* 数字部分之后只允许尾部空白 */
    while (*current != '\0') {
        if (isspace((unsigned char)*current) == 0) {
            return 0;
        }

        current++;
    }

    /* 必须有数字且包含非零数字（排除 "0"、"000" 等情况） */
    return has_digit != 0 && non_zero_found != 0 ? 1 : 0;
}

/**
 * @brief 判断文本是否为非负数（整数或小数）
 *
 * 允许首尾空白，支持一个小数点，不允许负号。
 *
 * @param text 待检测的字符串，可以为 NULL
 * @return 是非负数返回 1，否则返回 0
 */
int InputHelper_is_non_negative_amount(const char *text) {
    const char *current = 0;
    int has_digit = 0;   /* 是否包含数字 */
    int dot_count = 0;   /* 小数点的数量 */

    if (InputHelper_is_non_empty(text) == 0) {
        return 0;
    }

    /* 跳过前导空白 */
    current = InputHelper_skip_spaces(text);

    /* 扫描数字部分（含小数点） */
    while (*current != '\0' && isspace((unsigned char)*current) == 0) {
        /* 不允许负号 */
        if (*current == '-') {
            return 0;
        }

        if (*current == '.') {
            dot_count++;
            /* 最多允许一个小数点 */
            if (dot_count > 1) {
                return 0;
            }
        } else if (isdigit((unsigned char)*current) == 0) {
            /* 遇到非数字且非小数点的字符，返回失败 */
            return 0;
        } else {
            has_digit = 1;
        }

        current++;
    }

    /* 数字部分之后只允许尾部空白 */
    while (*current != '\0') {
        if (isspace((unsigned char)*current) == 0) {
            return 0;
        }

        current++;
    }

    /* 必须至少包含一个数字 */
    return has_digit;
}

/* ── 辅助函数：丢弃当前行的剩余内容 ──────────────────────── */

/**
 * @brief 丢弃输入流中当前行的剩余字符
 *
 * 持续读取字符直到遇到换行符或 EOF，用于处理输入过长的情况。
 *
 * @param input 输入流
 */
static void InputHelper_discard_rest_of_line(FILE *input) {
    int ch = 0;

    while ((ch = fgetc(input)) != EOF) {
        if (ch == '\n') {
            return;
        }
    }
}

/* ── 辅助函数：排空转义序列尾部字节（仅 Unix） ────────────── */

#ifndef _WIN32
/**
 * @brief 排空终端输入缓冲区中残留的转义序列字节
 *
 * 当检测到 ESC 键（0x1B）后，方向键等特殊按键会产生多字节的
 * 转义序列（如 ESC [ A）。本函数使用 select + raw 模式将这些
 * 后续字节全部读取丢弃，防止它们污染下一次输入读取。
 */
static void InputHelper_drain_escape_sequence(void) {
    struct termios old_settings;
    struct termios raw_settings;
    struct timeval tv;
    fd_set fds;

    /* 非终端输入无需处理 */
    if (!isatty(STDIN_FILENO)) {
        return;
    }

    /* 保存当前终端设置并切换到 raw 模式 */
    tcgetattr(STDIN_FILENO, &old_settings);
    raw_settings = old_settings;
    raw_settings.c_lflag &= ~((tcflag_t)ICANON | (tcflag_t)ECHO); /* 关闭规范模式和回显 */
    raw_settings.c_cc[VMIN] = 0;   /* 非阻塞读取 */
    raw_settings.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_settings);

    /* 使用 select 检测并读取缓冲区中的残留字节，超时 100ms */
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    while (select(STDIN_FILENO + 1, &fds, 0, 0, &tv) > 0) {
        (void)getchar(); /* 读取并丢弃一个字节 */
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 10000; /* 后续字节间隔更短，10ms 超时即可 */
    }

    /* 恢复原始终端设置 */
    tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
}
#endif

/* ── InputHelper_read_line 实现 ────────────────────────────── */

/**
 * @brief 支持 ESC 取消的行读取函数
 *
 * @param input    输入流（通常为 stdin）
 * @param buffer   输出缓冲区
 * @param capacity 缓冲区容量（字节数，含末尾 '\0'）
 * @return  1 = 成功读取一行
 *          0 = 遇到 EOF 或错误
 *         -1 = 输入过长（该行被丢弃）
 *         -2 = 用户按下了 ESC 键
 */
int InputHelper_read_line(FILE *input, char *buffer, size_t capacity) {
    size_t length = 0;

    /* 参数合法性检查 */
    if (buffer == 0 || capacity == 0) {
        return 0;
    }

    if (input == 0) {
        buffer[0] = '\0';
        return 0;
    }

#ifdef _WIN32
    /* Windows 平台：使用 _getch 在非回显模式下预读首个按键 */
    if (_isatty(_fileno(input))) {
        int ch = _getch();
        if (ch == 27) { /* 检测到 ESC 键 (ASCII 27 = 0x1B) */
            buffer[0] = '\0';
            return -2;
        }
        _ungetch(ch); /* 非 ESC 键，放回输入缓冲区 */
    }
#else
    /* POSIX 平台（macOS / Linux）：使用 termios raw 模式预读 */
    {
        int input_fd = fileno(input);
        if (input_fd >= 0 && isatty(input_fd)) {
            struct termios old_settings;
            struct termios new_settings;
            int ch = 0;

            /* 保存终端设置并切换到 raw 模式（关闭规范模式和回显） */
            tcgetattr(input_fd, &old_settings);
            new_settings = old_settings;
            new_settings.c_lflag &= ~((tcflag_t)ICANON | (tcflag_t)ECHO);
            new_settings.c_cc[VMIN] = 1;  /* 至少读取 1 个字符 */
            new_settings.c_cc[VTIME] = 0; /* 无超时 */
            tcsetattr(input_fd, TCSANOW, &new_settings);

            /* 在 raw 模式下读取首个字符 */
            ch = fgetc(input);
            /* 立即恢复终端设置 */
            tcsetattr(input_fd, TCSANOW, &old_settings);

            if (ch == 27) { /* 检测到 ESC 键 */
                InputHelper_drain_escape_sequence(); /* 排空转义序列尾部字节 */
                buffer[0] = '\0';
                return -2;
            }
            if (ch == EOF) {
                buffer[0] = '\0';
                return 0;
            }
            /* 将 raw 模式下读取的字符回显到终端并放回输入流 */
            fputc(ch, stdout);
            fflush(stdout);
            ungetc(ch, input);
        }
    }
#endif

    /* 使用 fgets 读取剩余输入（此时终端已恢复为规范模式） */
    if (fgets(buffer, (int)capacity, input) == 0) {
        buffer[0] = '\0';
        return 0; /* EOF 或读取错误 */
    }

    length = strlen(buffer);

    /* 如果读取的内容末尾不是换行符且未到文件末尾，说明输入过长 */
    if (length > 0 && buffer[length - 1] != '\n' && !feof(input)) {
        InputHelper_discard_rest_of_line(input); /* 丢弃超出部分 */
        buffer[0] = '\0';
        return -1; /* 输入过长 */
    }

    /* 去除末尾的换行符 */
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
        length--;
    }

    /*
     * 后备 ESC 检测：当平台特定的预读（_getch / termios raw）未能拦截 ESC 时，
     * fgets 会将 ESC 字符（0x1B）读入缓冲区。某些终端会将其显示为 "^["。
     * 如果缓冲区内容仅包含 ESC 字符或其文本表示 "^["，则视为 ESC 取消。
     */
    if (length > 0) {
        const char *p = buffer;
        int all_esc = 1;
        /* 检查是否全部由 ESC 字符(0x1B)和转义序列尾部字节([A-Z~)组成 */
        while (*p != '\0') {
            unsigned char ch = (unsigned char)*p;
            if (ch != 0x1B && ch != '[' && !(ch >= 'A' && ch <= 'Z') && ch != '~') {
                /* 也检查 "^[" 文本表示 */
                if (ch == '^' && *(p + 1) == '[') {
                    p += 2;
                    continue;
                }
                all_esc = 0;
                break;
            }
            p++;
        }
        if (all_esc && length > 0) {
            buffer[0] = '\0';
            return -2;
        }
    }

    return 1; /* 成功读取一行 */
}

/* ── InputHelper_is_esc_cancel 实现 ────────────────────────── */

/**
 * @brief 检查消息是否为 ESC 取消标识
 *
 * @param message 待检查的消息字符串
 * @return 是 ESC 取消消息返回 1，否则返回 0
 */
int InputHelper_is_esc_cancel(const char *message) {
    if (message == 0) {
        return 0;
    }
    /* 与预定义的 ESC 取消标识消息进行比较 */
    return strcmp(message, INPUT_HELPER_ESC_MESSAGE) == 0 ? 1 : 0;
}
