#ifndef HIS_UI_TUI_PANEL_H
#define HIS_UI_TUI_PANEL_H

#include <stdio.h>

typedef struct TuiPanel {
    int top;
    int left;
    int width;
    int height;
} TuiPanel;

typedef struct TuiLayout {
    TuiPanel sidebar;
    TuiPanel main;
    TuiPanel footer;
    int term_width;
    int term_height;
} TuiLayout;

void TuiLayout_compute(TuiLayout *layout);
void TuiPanel_clear(FILE *out, const TuiPanel *panel);
void TuiPanel_write_at(FILE *out, const TuiPanel *panel, int row, int col, const char *text);
void TuiPanel_draw_left_border(FILE *out, const TuiPanel *panel, const char *color);
int TuiPanel_write_lines(FILE *out, const TuiPanel *panel, int start_row, const char *text);
void TuiPanel_draw_bottom_border(FILE *out, const TuiPanel *panel, const char *color);
void TuiPanel_move_to(FILE *out, const TuiPanel *panel, int row, int col);

#endif
