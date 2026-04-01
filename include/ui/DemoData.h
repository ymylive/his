#ifndef HIS_UI_DEMO_DATA_H
#define HIS_UI_DEMO_DATA_H

#include <stddef.h>

#include "common/Result.h"
#include "ui/MenuApplication.h"

Result DemoData_reset(const MenuApplicationPaths *paths, char *buffer, size_t capacity);
Result DemoData_resolve_seed_path(
    const char *runtime_path,
    char *seed_path,
    size_t seed_path_capacity
);

#endif
