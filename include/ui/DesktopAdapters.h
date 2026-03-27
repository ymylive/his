#ifndef HIS_UI_DESKTOP_ADAPTERS_H
#define HIS_UI_DESKTOP_ADAPTERS_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/DispenseRecord.h"
#include "domain/Medicine.h"
#include "domain/Patient.h"
#include "domain/Registration.h"
#include "domain/User.h"
#include "ui/DesktopApp.h"
#include "ui/MenuApplication.h"

Result DesktopAdapters_init_application(
    MenuApplication *application,
    const MenuApplicationPaths *paths
);
Result DesktopAdapters_login(
    MenuApplication *application,
    const char *user_id,
    const char *password,
    UserRole required_role,
    User *out_user
);
Result DesktopAdapters_load_dashboard(
    MenuApplication *application,
    DesktopDashboardState *dashboard
);
Result DesktopAdapters_search_patients(
    MenuApplication *application,
    DesktopPatientSearchMode search_mode,
    const char *query,
    LinkedList *out_results
);
Result DesktopAdapters_get_patient_by_id(
    MenuApplication *application,
    const char *patient_id,
    Patient *out_patient
);
Result DesktopAdapters_submit_self_registration(
    MenuApplication *application,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    Registration *out_registration
);
Result DesktopAdapters_submit_registration(
    MenuApplication *application,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    Registration *out_registration
);
Result DesktopAdapters_query_registration(
    MenuApplication *application,
    const char *registration_id,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_cancel_registration(
    MenuApplication *application,
    const char *registration_id,
    const char *cancelled_at,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_query_registrations_by_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_query_records_by_time_range(
    MenuApplication *application,
    const char *time_from,
    const char *time_to,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_list_departments(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_add_department(
    MenuApplication *application,
    const Department *department,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_update_department(
    MenuApplication *application,
    const Department *department,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_add_doctor(
    MenuApplication *application,
    const Doctor *doctor,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_query_doctor(
    MenuApplication *application,
    const char *doctor_id,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_list_doctors_by_department(
    MenuApplication *application,
    const char *department_id,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_query_pending_registrations_by_doctor(
    MenuApplication *application,
    const char *doctor_id,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_create_visit_record(
    MenuApplication *application,
    const char *registration_id,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_create_visit_record_handoff(
    MenuApplication *application,
    const char *registration_id,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time,
    MenuApplicationVisitHandoff *out_handoff
);
Result DesktopAdapters_query_patient_history(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_create_examination_record(
    MenuApplication *application,
    const char *visit_id,
    const char *exam_item,
    const char *exam_type,
    const char *requested_at,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_complete_examination_record(
    MenuApplication *application,
    const char *examination_id,
    const char *result_text,
    const char *completed_at,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_list_wards(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_list_beds_by_ward(
    MenuApplication *application,
    const char *ward_id,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_admit_patient(
    MenuApplication *application,
    const char *patient_id,
    const char *ward_id,
    const char *bed_id,
    const char *admitted_at,
    const char *summary,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_discharge_patient(
    MenuApplication *application,
    const char *admission_id,
    const char *discharged_at,
    const char *summary,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_query_active_admission_by_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_query_current_patient_by_bed(
    MenuApplication *application,
    const char *bed_id,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_transfer_bed(
    MenuApplication *application,
    const char *admission_id,
    const char *target_bed_id,
    const char *transferred_at,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_discharge_check(
    MenuApplication *application,
    const char *admission_id,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_add_medicine(
    MenuApplication *application,
    const Medicine *medicine,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_restock_medicine(
    MenuApplication *application,
    const char *medicine_id,
    int quantity,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_dispense_medicine(
    MenuApplication *application,
    const char *patient_id,
    const char *prescription_id,
    const char *medicine_id,
    int quantity,
    const char *pharmacist_id,
    const char *dispensed_at,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_query_medicine_stock(
    MenuApplication *application,
    const char *medicine_id,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_query_medicine_detail(
    MenuApplication *application,
    const char *medicine_id,
    int include_instruction_note,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_find_low_stock_medicines(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);
Result DesktopAdapters_load_dispense_history(
    MenuApplication *application,
    const char *patient_id,
    LinkedList *out_records
);
Result DesktopAdapters_delete_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);

#endif
