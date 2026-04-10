#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui/TuiStyle.h"

/*
 * Smoke tests for animation functions.
 * All animations check tui_is_interactive() and skip when stdout is piped,
 * so these tests verify they don't crash on non-interactive streams.
 */

static void test_animate_matrix_rain_non_interactive(void) {
    FILE *f = tmpfile();
    assert(f != 0);
    tui_animate_matrix_rain(f, 20, 8, 100);
    tui_animate_matrix_rain(f, 0, 0, 0);  /* defaults */
    tui_animate_matrix_rain(0, 20, 8, 100);  /* null stream */
    fclose(f);
}

static void test_animate_particle_explosion_non_interactive(void) {
    FILE *f = tmpfile();
    assert(f != 0);
    tui_animate_particle_explosion(f, 20, 8);
    tui_animate_particle_explosion(f, 0, 0);
    tui_animate_particle_explosion(0, 20, 8);
    fclose(f);
}

static void test_animate_heartbeat_non_interactive(void) {
    FILE *f = tmpfile();
    assert(f != 0);
    tui_animate_heartbeat(f, 20, 1);
    tui_animate_heartbeat(f, 0, 0);
    tui_animate_heartbeat(0, 20, 1);
    fclose(f);
}

static void test_animate_glitch_non_interactive(void) {
    FILE *f = tmpfile();
    assert(f != 0);
    tui_animate_glitch(f, "test", 2, 100);
    tui_animate_glitch(f, "", 0, 0);
    tui_animate_glitch(f, 0, 2, 100);  /* null text */
    tui_animate_glitch(0, "test", 2, 100);
    fclose(f);
}

static void test_animate_fireworks_non_interactive(void) {
    FILE *f = tmpfile();
    assert(f != 0);
    tui_animate_fireworks(f, 20, 8, 1);
    tui_animate_fireworks(f, 0, 0, 0);
    tui_animate_fireworks(0, 20, 8, 1);
    fclose(f);
}

static void test_animate_dna_helix_non_interactive(void) {
    FILE *f = tmpfile();
    assert(f != 0);
    tui_animate_dna_helix(f, 8, 5);
    tui_animate_dna_helix(f, 0, 0);
    tui_animate_dna_helix(0, 8, 5);
    fclose(f);
}

static void test_animate_fade_reveal_non_interactive(void) {
    FILE *f = tmpfile();
    char buf[256];
    size_t n;
    assert(f != 0);
    tui_animate_fade_reveal(f, "hello", 4);
    /* Non-interactive should produce direct output */
    fflush(f);
    rewind(f);
    n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    assert(strstr(buf, "hello") != 0);
    fclose(f);

    /* null cases */
    tui_animate_fade_reveal(0, "hello", 4);
    f = tmpfile();
    tui_animate_fade_reveal(f, 0, 4);
    fclose(f);
}

static void test_animate_wave_text_non_interactive(void) {
    FILE *f = tmpfile();
    char buf[256];
    size_t n;
    assert(f != 0);
    tui_animate_wave_text(f, "wave", 1, 50);
    fflush(f);
    rewind(f);
    n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    assert(strstr(buf, "wave") != 0);
    fclose(f);
}

static void test_animate_plasma_non_interactive(void) {
    FILE *f = tmpfile();
    assert(f != 0);
    tui_animate_plasma(f, 10, 4, 5);
    tui_animate_plasma(f, 0, 0, 0);
    tui_animate_plasma(0, 10, 4, 5);
    fclose(f);
}

static void test_animate_entrance_non_interactive(void) {
    FILE *f = tmpfile();
    assert(f != 0);
    tui_animate_entrance(f, TUI_THEME_ADMIN);
    tui_animate_entrance(f, TUI_THEME_DOCTOR);
    tui_animate_entrance(f, TUI_THEME_PATIENT);
    tui_animate_entrance(f, TUI_THEME_INPATIENT);
    tui_animate_entrance(f, TUI_THEME_PHARMACY);
    tui_animate_entrance(f, TUI_THEME_DEFAULT);
    tui_animate_entrance(0, TUI_THEME_ADMIN);
    fclose(f);
}

int main(void) {
    test_animate_matrix_rain_non_interactive();
    test_animate_particle_explosion_non_interactive();
    test_animate_heartbeat_non_interactive();
    test_animate_glitch_non_interactive();
    test_animate_fireworks_non_interactive();
    test_animate_dna_helix_non_interactive();
    test_animate_fade_reveal_non_interactive();
    test_animate_wave_text_non_interactive();
    test_animate_plasma_non_interactive();
    test_animate_entrance_non_interactive();
    return 0;
}
