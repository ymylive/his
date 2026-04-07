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

static const char *InputHelper_skip_spaces(const char *text) {
    while (text != 0 && *text != '\0' && isspace((unsigned char)*text) != 0) {
        text++;
    }

    return text;
}

static const char *InputHelper_trim_end(const char *start) {
    const char *end = start;

    while (*end != '\0') {
        end++;
    }

    while (end > start && isspace((unsigned char)*(end - 1)) != 0) {
        end--;
    }

    return end;
}

int InputHelper_is_non_empty(const char *text) {
    const char *start = 0;
    const char *end = 0;

    if (text == 0) {
        return 0;
    }

    start = InputHelper_skip_spaces(text);
    end = InputHelper_trim_end(start);
    return end > start ? 1 : 0;
}

int InputHelper_is_positive_integer(const char *text) {
    const char *current = 0;
    int has_digit = 0;
    int non_zero_found = 0;

    if (InputHelper_is_non_empty(text) == 0) {
        return 0;
    }

    current = InputHelper_skip_spaces(text);
    while (*current != '\0' && isspace((unsigned char)*current) == 0) {
        if (isdigit((unsigned char)*current) == 0) {
            return 0;
        }

        if (*current != '0') {
            non_zero_found = 1;
        }

        has_digit = 1;
        current++;
    }

    while (*current != '\0') {
        if (isspace((unsigned char)*current) == 0) {
            return 0;
        }

        current++;
    }

    return has_digit != 0 && non_zero_found != 0 ? 1 : 0;
}

int InputHelper_is_non_negative_amount(const char *text) {
    const char *current = 0;
    int has_digit = 0;
    int dot_count = 0;

    if (InputHelper_is_non_empty(text) == 0) {
        return 0;
    }

    current = InputHelper_skip_spaces(text);
    while (*current != '\0' && isspace((unsigned char)*current) == 0) {
        if (*current == '-') {
            return 0;
        }

        if (*current == '.') {
            dot_count++;
            if (dot_count > 1) {
                return 0;
            }
        } else if (isdigit((unsigned char)*current) == 0) {
            return 0;
        } else {
            has_digit = 1;
        }

        current++;
    }

    while (*current != '\0') {
        if (isspace((unsigned char)*current) == 0) {
            return 0;
        }

        current++;
    }

    return has_digit;
}

/* ── Helper: discard remaining bytes on a line ──────────────── */

static void InputHelper_discard_rest_of_line(FILE *input) {
    int ch = 0;

    while ((ch = fgetc(input)) != EOF) {
        if (ch == '\n') {
            return;
        }
    }
}

/* ── Helper: drain escape-sequence tail bytes (Unix) ────────── */

#ifndef _WIN32
static void InputHelper_drain_escape_sequence(void) {
    struct termios old_settings;
    struct termios raw_settings;
    struct timeval tv;
    fd_set fds;

    if (!isatty(STDIN_FILENO)) {
        return;
    }

    tcgetattr(STDIN_FILENO, &old_settings);
    raw_settings = old_settings;
    raw_settings.c_lflag &= ~((tcflag_t)ICANON | (tcflag_t)ECHO);
    raw_settings.c_cc[VMIN] = 0;
    raw_settings.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_settings);

    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    while (select(STDIN_FILENO + 1, &fds, 0, 0, &tv) > 0) {
        (void)getchar();
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 10000;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
}
#endif

/* ── InputHelper_read_line ──────────────────────────────────── */

int InputHelper_read_line(FILE *input, char *buffer, size_t capacity) {
    size_t length = 0;

    if (buffer == 0 || capacity == 0) {
        return 0;
    }

    if (input == 0) {
        buffer[0] = '\0';
        return 0;
    }

#ifdef _WIN32
    /* Windows (MSVC & MinGW): use _getch for ESC peek */
    if (_isatty(_fileno(input))) {
        int ch = _getch();
        if (ch == 27) {
            buffer[0] = '\0';
            return -2;
        }
        _ungetch(ch);
    }
#else
    /* POSIX (macOS / Linux) */
    if (isatty(STDIN_FILENO)) {
        struct termios old_settings;
        struct termios new_settings;
        int ch = 0;

        tcgetattr(STDIN_FILENO, &old_settings);
        new_settings = old_settings;
        new_settings.c_lflag &= ~((tcflag_t)ICANON | (tcflag_t)ECHO);
        new_settings.c_cc[VMIN] = 1;
        new_settings.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);

        ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);

        if (ch == 27) {
            InputHelper_drain_escape_sequence();
            buffer[0] = '\0';
            return -2;
        }
        if (ch == EOF) {
            buffer[0] = '\0';
            return 0;
        }
        /* Echo the character that was swallowed in raw mode */
        fputc(ch, stdout);
        fflush(stdout);
        ungetc(ch, input);
    }
#endif

    if (fgets(buffer, (int)capacity, input) == 0) {
        buffer[0] = '\0';
        return 0;
    }

    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] != '\n' && !feof(input)) {
        InputHelper_discard_rest_of_line(input);
        buffer[0] = '\0';
        return -1;
    }

    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    }

    return 1;
}

/* ── InputHelper_is_esc_cancel ──────────────────────────────── */

int InputHelper_is_esc_cancel(const char *message) {
    if (message == 0) {
        return 0;
    }
    return strcmp(message, INPUT_HELPER_ESC_MESSAGE) == 0 ? 1 : 0;
}
