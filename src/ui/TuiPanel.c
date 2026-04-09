#include "ui/TuiPanel.h"
#include "ui/TuiStyle.h"
#include <string.h>

void TuiLayout_compute(TuiLayout *layout) {
    int tw, th, sidebar_w;
    if (layout == 0) return;
    tw = tui_get_terminal_width();
    th = tui_get_terminal_height();
    layout->term_width = tw;
    layout->term_height = th;
    sidebar_w = tw * 30 / 100;
    if (sidebar_w < 20) sidebar_w = 20;
    if (sidebar_w > 40) sidebar_w = 40;
    layout->sidebar.top = 1;
    layout->sidebar.left = 1;
    layout->sidebar.width = sidebar_w;
    layout->sidebar.height = th - 1;
    layout->main.top = 1;
    layout->main.left = sidebar_w + 1;
    layout->main.width = tw - sidebar_w;
    layout->main.height = th - 1;
    layout->footer.top = th;
    layout->footer.left = 1;
    layout->footer.width = tw;
    layout->footer.height = 1;
}

void TuiPanel_clear(FILE *out, const TuiPanel *panel) {
    int r, c;
    if (out == 0 || panel == 0) return;
    for (r = 0; r < panel->height; r++) {
        tui_move_cursor(out, panel->top + r, panel->left);
        for (c = 0; c < panel->width; c++) fputc(' ', out);
    }
    fflush(out);
}

void TuiPanel_write_at(FILE *out, const TuiPanel *panel, int row, int col, const char *text) {
    if (out == 0 || panel == 0 || text == 0) return;
    if (row < 0 || row >= panel->height) return;
    tui_move_cursor(out, panel->top + row, panel->left + col);
    fputs(text, out);
}

void TuiPanel_draw_left_border(FILE *out, const TuiPanel *panel, const char *color) {
    int r;
    if (out == 0 || panel == 0) return;
    if (color != 0) fputs(color, out);
    for (r = 0; r < panel->height; r++) {
        tui_move_cursor(out, panel->top + r, panel->left + panel->width - 1);
        fputs(TUI_HV, out);
    }
    fputs(TUI_RESET, out);
    fflush(out);
}

void TuiPanel_draw_bottom_border(FILE *out, const TuiPanel *panel, const char *color) {
    int c;
    if (out == 0 || panel == 0) return;
    tui_move_cursor(out, panel->top + panel->height, panel->left);
    if (color != 0) fputs(color, out);
    for (c = 0; c < panel->width; c++) fputs(TUI_H, out);
    fputs(TUI_RESET, out);
    fflush(out);
}

void TuiPanel_move_to(FILE *out, const TuiPanel *panel, int row, int col) {
    if (out == 0 || panel == 0) return;
    tui_move_cursor(out, panel->top + row, panel->left + col);
}

int TuiPanel_write_lines(FILE *out, const TuiPanel *panel, int start_row, const char *text) {
    const char *p = text;
    int row = start_row;
    if (out == 0 || panel == 0 || text == 0) return 0;
    while (*p != '\0' && row < panel->height) {
        const char *nl = p;
        int line_len;
        int c;
        while (*nl != '\0' && *nl != '\n') nl++;
        line_len = (int)(nl - p);
        tui_move_cursor(out, panel->top + row, panel->left);
        for (c = 0; c < panel->width; c++) fputc(' ', out);
        tui_move_cursor(out, panel->top + row, panel->left + 2);
        fwrite(p, 1, (size_t)line_len, out);
        row++;
        if (*nl == '\n') nl++;
        p = nl;
    }
    fflush(out);
    return row - start_row;
}
