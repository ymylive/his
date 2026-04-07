#ifndef HIS_COMMON_INPUT_HELPER_H
#define HIS_COMMON_INPUT_HELPER_H

#include <stdio.h>
#include <stddef.h>

int InputHelper_is_non_empty(const char *text);
int InputHelper_is_positive_integer(const char *text);
int InputHelper_is_non_negative_amount(const char *text);

/* ── ESC-aware line reading ──────────────────────────────────────
 * Returns:
 *   1  = line read successfully (newline stripped)
 *   0  = EOF / error
 *  -1  = input too long (line discarded)
 *  -2  = ESC pressed (buffer set to "")
 *
 * On interactive terminals the first keystroke is peeked in raw mode
 * so that a bare ESC (0x1B) is caught before canonical-mode fgets().
 * Any trailing escape-sequence bytes (e.g. arrow keys) are drained
 * so they don't leak into the next read.
 * ────────────────────────────────────────────────────────────────── */
int InputHelper_read_line(FILE *input, char *buffer, size_t capacity);

/* Sentinel message returned by prompt helpers when ESC is pressed. */
#define INPUT_HELPER_ESC_MESSAGE "input cancelled by user"

/* Check whether a Result message indicates ESC cancellation. */
int InputHelper_is_esc_cancel(const char *message);

#endif
