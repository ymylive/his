#include "ui/AdminWorkbenchSupport.h"

#include <stdio.h>
#include <string.h>

static void admin_copy_text(char *target, size_t target_size, const char *source) {
    if (target == 0 || target_size == 0) {
        return;
    }

    if (source == 0) {
        target[0] = '\0';
        return;
    }

    strncpy(target, source, target_size - 1);
    target[target_size - 1] = '\0';
}

int AdminWorkbench_parse_option_id(
    const char *label,
    char *selected_id,
    size_t selected_id_size
) {
    char parsed_id[HIS_DOMAIN_ID_CAPACITY];

    if (label == 0 || selected_id == 0 || selected_id_size == 0) {
        return 0;
    }

    selected_id[0] = '\0';
    if (sscanf(label, "%15s", parsed_id) != 1) {
        return 0;
    }

    admin_copy_text(selected_id, selected_id_size, parsed_id);
    return 1;
}

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
) {
    if (department == 0) {
        return;
    }

    admin_copy_text(department_id, department_id_size, department->department_id);
    admin_copy_text(department_name, department_name_size, department->name);
    admin_copy_text(department_location, department_location_size, department->location);
    admin_copy_text(department_description, department_description_size, department->description);
    admin_copy_text(doctor_department_id, doctor_department_id_size, department->department_id);
}
