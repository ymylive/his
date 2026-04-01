#include <assert.h>
#include <string.h>

#include "ui/AdminWorkbenchSupport.h"

static void test_parse_option_id_extracts_leading_identifier(void) {
    char selected_id[HIS_DOMAIN_ID_CAPACITY];

    assert(
        AdminWorkbench_parse_option_id(
            "DEP0501 | 感染科 | 门诊四层",
            selected_id,
            sizeof(selected_id)
        ) == 1
    );
    assert(strcmp(selected_id, "DEP0501") == 0);
}

static void test_parse_option_id_rejects_empty_label(void) {
    char selected_id[HIS_DOMAIN_ID_CAPACITY] = "unchanged";

    assert(AdminWorkbench_parse_option_id("", selected_id, sizeof(selected_id)) == 0);
    assert(selected_id[0] == '\0');
}

static void test_apply_department_selection_syncs_doctor_department_id(void) {
    Department department = { "DEP0501", "感染科", "门诊四层", "感染门诊" };
    char department_id[HIS_DOMAIN_ID_CAPACITY] = "";
    char department_name[HIS_DOMAIN_NAME_CAPACITY] = "";
    char department_location[HIS_DOMAIN_LOCATION_CAPACITY] = "";
    char department_description[HIS_DOMAIN_TEXT_CAPACITY] = "";
    char doctor_department_id[HIS_DOMAIN_ID_CAPACITY] = "DEP0000";

    AdminWorkbench_apply_department_selection(
        &department,
        department_id,
        sizeof(department_id),
        department_name,
        sizeof(department_name),
        department_location,
        sizeof(department_location),
        department_description,
        sizeof(department_description),
        doctor_department_id,
        sizeof(doctor_department_id)
    );

    assert(strcmp(department_id, "DEP0501") == 0);
    assert(strcmp(department_name, "感染科") == 0);
    assert(strcmp(department_location, "门诊四层") == 0);
    assert(strcmp(department_description, "感染门诊") == 0);
    assert(strcmp(doctor_department_id, "DEP0501") == 0);
}

int main(void) {
    test_parse_option_id_extracts_leading_identifier();
    test_parse_option_id_rejects_empty_label();
    test_apply_department_selection_syncs_doctor_department_id();
    return 0;
}
