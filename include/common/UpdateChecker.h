#ifndef HIS_COMMON_UPDATE_CHECKER_H
#define HIS_COMMON_UPDATE_CHECKER_H

#include <stdio.h>

#define HIS_VERSION "3.3.0"

typedef struct UpdateInfo {
    int has_update;
    char latest_version[32];
    char current_version[32];
    char download_url[256];   /* GitHub release page URL */
    char asset_url[512];      /* Direct zip download URL for this platform */
} UpdateInfo;

/*
 * Check GitHub releases for a newer version.
 * Returns 1 if check succeeded, 0 on failure (network error, etc.).
 * Fills info->has_update = 1 when a newer release exists.
 */
int UpdateChecker_check(UpdateInfo *info);

/*
 * Display update prompt to user via TUI.
 * Returns:
 *   0 — user skipped update
 *   1 — user opened browser
 *   2 — update installed, caller should exit for restart
 */
int UpdateChecker_prompt(FILE *out, FILE *in, const UpdateInfo *info);

#endif
