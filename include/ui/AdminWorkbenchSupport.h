#ifndef HIS_UI_ADMIN_WORKBENCH_SUPPORT_H
#define HIS_UI_ADMIN_WORKBENCH_SUPPORT_H

#include <stddef.h>

#include "domain/Department.h"

int AdminWorkbench_parse_option_id(
    const char *label,
    char *selected_id,
    size_t selected_id_size
);

void AdminWorkbench_apply_department_selection(
    const Department *department,
    char *department_id,
    size_t department_id_size,
    char *department_name,
    size_t department_name_size,
    char *department_location,
    size_t department_location_size,
    char *department_description,
    size_t department_description_size,
    char *doctor_department_id,
    size_t doctor_department_id_size
);

#endif
