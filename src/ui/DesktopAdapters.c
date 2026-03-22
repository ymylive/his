#include "ui/DesktopAdapters.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "repository/AdmissionRepository.h"
#include "repository/DispenseRecordRepository.h"
#include "repository/RegistrationRepository.h"

static void DesktopAdapters_clear_dashboard(DesktopDashboardState *dashboard) {
    if (dashboard == 0) {
        return;
    }

    LinkedList_clear(&dashboard->recent_registrations, free);
    LinkedList_clear(&dashboard->recent_dispensations, free);
    LinkedList_init(&dashboard->recent_registrations);
    LinkedList_init(&dashboard->recent_dispensations);
    dashboard->patient_count = 0;
    dashboard->registration_count = 0;
    dashboard->inpatient_count = 0;
    dashboard->low_stock_count = 0;
    dashboard->loaded = 0;
}

static int DesktopAdapters_is_blank_text(const char *text) {
    if (text == 0) {
        return 1;
    }

    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) {
            return 0;
        }
        text++;
    }

    return 1;
}

static Result DesktopAdapters_require_output_buffer(
    MenuApplication *application,
    char *buffer,
    size_t capacity,
    const char *message
) {
    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure(message);
    }

    return Result_make_success("desktop adapter output ready");
}

static Result DesktopAdapters_append_registration_copy(
    LinkedList *target,
    const Registration *registration
) {
    Registration *copy = 0;

    if (target == 0 || registration == 0) {
        return Result_make_failure("dashboard registration result arguments missing");
    }

    copy = (Registration *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate dashboard registration");
    }

    *copy = *registration;
    if (!LinkedList_append(target, copy)) {
        free(copy);
        return Result_make_failure("failed to append dashboard registration");
    }

    return Result_make_success("dashboard registration appended");
}

static Result DesktopAdapters_append_dispense_copy(
    LinkedList *target,
    const DispenseRecord *record
) {
    DispenseRecord *copy = 0;

    if (target == 0 || record == 0) {
        return Result_make_failure("dashboard dispense result arguments missing");
    }

    copy = (DispenseRecord *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate dashboard dispense");
    }

    *copy = *record;
    if (!LinkedList_append(target, copy)) {
        free(copy);
        return Result_make_failure("failed to append dashboard dispense");
    }

    return Result_make_success("dashboard dispense appended");
}

static Result DesktopAdapters_append_recent_registrations(
    const LinkedList *source,
    LinkedList *target
) {
    size_t source_count = 0;
    size_t keep_start = 0;
    size_t selected_count = 0;
    size_t write_index = 0;
    const Registration **selected = 0;
    const LinkedListNode *current = 0;
    size_t index = 0;

    if (source == 0 || target == 0) {
        return Result_make_failure("dashboard registration source missing");
    }

    source_count = LinkedList_count(source);
    keep_start = source_count > 5 ? source_count - 5 : 0;
    selected_count = source_count - keep_start;
    if (selected_count == 0) {
        return Result_make_success("dashboard recent registrations empty");
    }

    selected = (const Registration **)malloc(selected_count * sizeof(*selected));
    if (selected == 0) {
        return Result_make_failure("failed to allocate dashboard registration window");
    }

    current = source->head;
    while (current != 0) {
        if (index >= keep_start) {
            selected[write_index] = (const Registration *)current->data;
            write_index++;
        }

        current = current->next;
        index++;
    }

    while (write_index > 0) {
        write_index--;
        {
            Result result = DesktopAdapters_append_registration_copy(target, selected[write_index]);
            if (result.success == 0) {
                free(selected);
                return result;
            }
        }
    }

    free(selected);
    return Result_make_success("dashboard recent registrations copied");
}

static Result DesktopAdapters_append_recent_dispensations(
    const LinkedList *source,
    LinkedList *target
) {
    size_t source_count = 0;
    size_t keep_start = 0;
    size_t selected_count = 0;
    size_t write_index = 0;
    const DispenseRecord **selected = 0;
    const LinkedListNode *current = 0;
    size_t index = 0;

    if (source == 0 || target == 0) {
        return Result_make_failure("dashboard dispense source missing");
    }

    source_count = LinkedList_count(source);
    keep_start = source_count > 5 ? source_count - 5 : 0;
    selected_count = source_count - keep_start;
    if (selected_count == 0) {
        return Result_make_success("dashboard recent dispenses empty");
    }

    selected = (const DispenseRecord **)malloc(selected_count * sizeof(*selected));
    if (selected == 0) {
        return Result_make_failure("failed to allocate dashboard dispense window");
    }

    current = source->head;
    while (current != 0) {
        if (index >= keep_start) {
            selected[write_index] = (const DispenseRecord *)current->data;
            write_index++;
        }

        current = current->next;
        index++;
    }

    while (write_index > 0) {
        write_index--;
        {
            Result result = DesktopAdapters_append_dispense_copy(target, selected[write_index]);
            if (result.success == 0) {
                free(selected);
                return result;
            }
        }
    }

    free(selected);
    return Result_make_success("dashboard recent dispenses copied");
}

static Result DesktopAdapters_copy_dispense_history_desc(
    LinkedList *out_records,
    const LinkedList *loaded_records
) {
    size_t total = 0;
    const DispenseRecord **ordered = 0;
    const LinkedListNode *current = 0;
    size_t index = 0;

    if (out_records == 0 || loaded_records == 0) {
        return Result_make_failure("dispense history copy arguments missing");
    }

    total = LinkedList_count(loaded_records);
    if (total == 0) {
        return Result_make_success("dispense history empty");
    }

    ordered = (const DispenseRecord **)malloc(total * sizeof(*ordered));
    if (ordered == 0) {
        return Result_make_failure("failed to allocate dispense history window");
    }

    current = loaded_records->head;
    while (current != 0) {
        ordered[index] = (const DispenseRecord *)current->data;
        index++;
        current = current->next;
    }

    while (index > 0) {
        index--;
        {
            Result result = DesktopAdapters_append_dispense_copy(out_records, ordered[index]);
            if (result.success == 0) {
                free(ordered);
                return result;
            }
        }
    }

    free(ordered);
    return Result_make_success("dispense history copied");
}

Result DesktopAdapters_init_application(
    MenuApplication *application,
    const MenuApplicationPaths *paths
) {
    return MenuApplication_init(application, paths);
}

Result DesktopAdapters_login(
    MenuApplication *application,
    const char *user_id,
    const char *password,
    UserRole required_role,
    User *out_user
) {
    Result result;

    if (application == 0 || out_user == 0) {
        return Result_make_failure("desktop login arguments missing");
    }

    result = MenuApplication_login(application, user_id, password, required_role);
    if (result.success == 0 && strcmp(result.message, "user role mismatch") == 0) {
        result = MenuApplication_login(application, user_id, password, USER_ROLE_UNKNOWN);
    }
    if (result.success == 0) {
        return result;
    }

    *out_user = application->authenticated_user;
    return Result_make_success("desktop login complete");
}

Result DesktopAdapters_load_dashboard(
    MenuApplication *application,
    DesktopDashboardState *dashboard
) {
    LinkedList patients;
    LinkedList registrations;
    LinkedList admissions;
    LinkedList medicines;
    LinkedList dispenses;
    const LinkedListNode *current = 0;
    Result result;

    if (application == 0 || dashboard == 0) {
        return Result_make_failure("dashboard arguments missing");
    }

    DesktopAdapters_clear_dashboard(dashboard);
    LinkedList_init(&patients);
    LinkedList_init(&registrations);
    LinkedList_init(&admissions);
    LinkedList_init(&medicines);
    LinkedList_init(&dispenses);

    result = PatientRepository_load_all(&application->patient_service.repository, &patients);
    if (result.success == 0) {
        return result;
    }
    dashboard->patient_count = (int)LinkedList_count(&patients);
    PatientRepository_clear_loaded_list(&patients);

    result = RegistrationRepository_load_all(&application->registration_repository, &registrations);
    if (result.success == 0) {
        return result;
    }
    dashboard->registration_count = (int)LinkedList_count(&registrations);
    result = DesktopAdapters_append_recent_registrations(
        &registrations,
        &dashboard->recent_registrations
    );
    if (result.success == 0) {
        RegistrationRepository_clear_list(&registrations);
        DesktopAdapters_clear_dashboard(dashboard);
        return result;
    }
    RegistrationRepository_clear_list(&registrations);

    result = AdmissionRepository_load_all(&application->inpatient_service.admission_repository, &admissions);
    if (result.success == 0) {
        DesktopAdapters_clear_dashboard(dashboard);
        return result;
    }
    current = admissions.head;
    while (current != 0) {
        const Admission *admission = (const Admission *)current->data;
        if (admission->status == ADMISSION_STATUS_ACTIVE) {
            dashboard->inpatient_count++;
        }
        current = current->next;
    }
    AdmissionRepository_clear_loaded_list(&admissions);

    result = PharmacyService_find_low_stock_medicines(&application->pharmacy_service, &medicines);
    if (result.success == 0) {
        DesktopAdapters_clear_dashboard(dashboard);
        return result;
    }
    dashboard->low_stock_count = (int)LinkedList_count(&medicines);
    PharmacyService_clear_medicine_results(&medicines);

    LinkedList_init(&dispenses);
    result = DispenseRecordRepository_load_all(
        &application->pharmacy_service.dispense_record_repository,
        &dispenses
    );
    if (result.success == 0) {
        DesktopAdapters_clear_dashboard(dashboard);
        return result;
    }
    result = DesktopAdapters_append_recent_dispensations(
        &dispenses,
        &dashboard->recent_dispensations
    );
    if (result.success == 0) {
        DispenseRecordRepository_clear_list(&dispenses);
        DesktopAdapters_clear_dashboard(dashboard);
        return result;
    }
    DispenseRecordRepository_clear_list(&dispenses);

    dashboard->loaded = 1;
    return Result_make_success("dashboard loaded");
}

Result DesktopAdapters_search_patients(
    MenuApplication *application,
    DesktopPatientSearchMode search_mode,
    const char *query,
    LinkedList *out_results
) {
    Patient patient;

    if (application == 0 || out_results == 0) {
        return Result_make_failure("patient search arguments missing");
    }

    if (LinkedList_count(out_results) != 0) {
        return Result_make_failure("desktop patient output must be empty");
    }

    if (DesktopAdapters_is_blank_text(query)) {
        return Result_make_failure("patient search query missing");
    }

    if (search_mode == DESKTOP_PATIENT_SEARCH_BY_ID) {
        Result result = PatientService_find_patient_by_id(
            &application->patient_service,
            query,
            &patient
        );
        Patient *copy = 0;

        if (result.success == 0) {
            return result;
        }

        copy = (Patient *)malloc(sizeof(*copy));
        if (copy == 0) {
            return Result_make_failure("failed to allocate patient result");
        }

        *copy = patient;
        if (!LinkedList_append(out_results, copy)) {
            free(copy);
            return Result_make_failure("failed to append patient result");
        }

        return Result_make_success("patient search complete");
    }

    if (search_mode == DESKTOP_PATIENT_SEARCH_BY_NAME) {
        return PatientService_find_patients_by_name(
            &application->patient_service,
            query,
            out_results
        );
    }

    return Result_make_failure("desktop patient search mode invalid");
}

Result DesktopAdapters_get_patient_by_id(
    MenuApplication *application,
    const char *patient_id,
    Patient *out_patient
) {
    if (application == 0 || out_patient == 0) {
        return Result_make_failure("patient detail arguments missing");
    }

    return PatientService_find_patient_by_id(
        &application->patient_service,
        patient_id,
        out_patient
    );
}

Result DesktopAdapters_submit_registration(
    MenuApplication *application,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    Registration *out_registration
) {
    if (application == 0 || out_registration == 0) {
        return Result_make_failure("registration adapter arguments missing");
    }

    return RegistrationService_create(
        &application->registration_service,
        patient_id,
        doctor_id,
        department_id,
        registered_at,
        out_registration
    );
}

Result DesktopAdapters_query_registration(
    MenuApplication *application,
    const char *registration_id,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "registration query adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_query_registration(application, registration_id, buffer, capacity);
}

Result DesktopAdapters_cancel_registration(
    MenuApplication *application,
    const char *registration_id,
    const char *cancelled_at,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "registration cancel adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_cancel_registration(
        application,
        registration_id,
        cancelled_at,
        buffer,
        capacity
    );
}

Result DesktopAdapters_query_registrations_by_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "registration patient query adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_query_registrations_by_patient(
        application,
        patient_id,
        buffer,
        capacity
    );
}

Result DesktopAdapters_query_records_by_time_range(
    MenuApplication *application,
    const char *time_from,
    const char *time_to,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "time range adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_query_records_by_time_range(
        application,
        time_from,
        time_to,
        buffer,
        capacity
    );
}

Result DesktopAdapters_query_pending_registrations_by_doctor(
    MenuApplication *application,
    const char *doctor_id,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "doctor pending adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_query_pending_registrations_by_doctor(
        application,
        doctor_id,
        buffer,
        capacity
    );
}

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
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "doctor visit adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_create_visit_record(
        application,
        registration_id,
        chief_complaint,
        diagnosis,
        advice,
        need_exam,
        need_admission,
        need_medicine,
        visit_time,
        buffer,
        capacity
    );
}

Result DesktopAdapters_query_patient_history(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "doctor history adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_query_patient_history(
        application,
        patient_id,
        buffer,
        capacity
    );
}

Result DesktopAdapters_create_examination_record(
    MenuApplication *application,
    const char *visit_id,
    const char *exam_item,
    const char *exam_type,
    const char *requested_at,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "doctor exam create adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_create_examination_record(
        application,
        visit_id,
        exam_item,
        exam_type,
        requested_at,
        buffer,
        capacity
    );
}

Result DesktopAdapters_complete_examination_record(
    MenuApplication *application,
    const char *examination_id,
    const char *result_text,
    const char *completed_at,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "doctor exam complete adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_complete_examination_record(
        application,
        examination_id,
        result_text,
        completed_at,
        buffer,
        capacity
    );
}

Result DesktopAdapters_list_wards(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "inpatient ward list adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_list_wards(application, buffer, capacity);
}

Result DesktopAdapters_list_beds_by_ward(
    MenuApplication *application,
    const char *ward_id,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "inpatient bed list adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_list_beds_by_ward(
        application,
        ward_id,
        buffer,
        capacity
    );
}

Result DesktopAdapters_admit_patient(
    MenuApplication *application,
    const char *patient_id,
    const char *ward_id,
    const char *bed_id,
    const char *admitted_at,
    const char *summary,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "inpatient admit adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_admit_patient(
        application,
        patient_id,
        ward_id,
        bed_id,
        admitted_at,
        summary,
        buffer,
        capacity
    );
}

Result DesktopAdapters_discharge_patient(
    MenuApplication *application,
    const char *admission_id,
    const char *discharged_at,
    const char *summary,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "inpatient discharge adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_discharge_patient(
        application,
        admission_id,
        discharged_at,
        summary,
        buffer,
        capacity
    );
}

Result DesktopAdapters_query_active_admission_by_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "inpatient active query adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_query_active_admission_by_patient(
        application,
        patient_id,
        buffer,
        capacity
    );
}

Result DesktopAdapters_query_current_patient_by_bed(
    MenuApplication *application,
    const char *bed_id,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "inpatient bed query adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_query_current_patient_by_bed(
        application,
        bed_id,
        buffer,
        capacity
    );
}

Result DesktopAdapters_transfer_bed(
    MenuApplication *application,
    const char *admission_id,
    const char *target_bed_id,
    const char *transferred_at,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "inpatient transfer adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_transfer_bed(
        application,
        admission_id,
        target_bed_id,
        transferred_at,
        buffer,
        capacity
    );
}

Result DesktopAdapters_discharge_check(
    MenuApplication *application,
    const char *admission_id,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "inpatient discharge check adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_discharge_check(application, admission_id, buffer, capacity);
}

Result DesktopAdapters_add_medicine(
    MenuApplication *application,
    const Medicine *medicine,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "pharmacy add adapter arguments missing"
    );

    if (result.success == 0 || medicine == 0) {
        if (medicine == 0) {
            return Result_make_failure("pharmacy add adapter medicine missing");
        }
        return result;
    }

    return MenuApplication_add_medicine(
        application,
        medicine,
        buffer,
        capacity
    );
}

Result DesktopAdapters_restock_medicine(
    MenuApplication *application,
    const char *medicine_id,
    int quantity,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "pharmacy restock adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_restock_medicine(
        application,
        medicine_id,
        quantity,
        buffer,
        capacity
    );
}

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
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "pharmacy dispense adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_dispense_medicine(
        application,
        patient_id,
        prescription_id,
        medicine_id,
        quantity,
        pharmacist_id,
        dispensed_at,
        buffer,
        capacity
    );
}

Result DesktopAdapters_query_medicine_stock(
    MenuApplication *application,
    const char *medicine_id,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "pharmacy stock query adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_query_medicine_stock(
        application,
        medicine_id,
        buffer,
        capacity
    );
}

Result DesktopAdapters_query_medicine_detail(
    MenuApplication *application,
    const char *medicine_id,
    int include_instruction_note,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "pharmacy medicine detail adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_query_medicine_detail(
        application,
        medicine_id,
        buffer,
        capacity,
        include_instruction_note
    );
}

Result DesktopAdapters_find_low_stock_medicines(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    Result result = DesktopAdapters_require_output_buffer(
        application,
        buffer,
        capacity,
        "pharmacy low stock adapter arguments missing"
    );

    if (result.success == 0) {
        return result;
    }

    return MenuApplication_find_low_stock_medicines(
        application,
        buffer,
        capacity
    );
}

Result DesktopAdapters_load_dispense_history(
    MenuApplication *application,
    const char *patient_id,
    LinkedList *out_records
) {
    LinkedList loaded_records;
    Result result;

    if (application == 0 || out_records == 0) {
        return Result_make_failure("dispense adapter arguments missing");
    }

    if (DesktopAdapters_is_blank_text(patient_id)) {
        return Result_make_failure("patient id missing");
    }

    if (LinkedList_count(out_records) != 0) {
        return Result_make_failure("dispense adapter output must be empty");
    }

    LinkedList_init(&loaded_records);
    result = PharmacyService_find_dispense_records_by_patient_id(
        &application->pharmacy_service,
        patient_id,
        &loaded_records
    );
    if (result.success == 0) {
        return result;
    }

    result = DesktopAdapters_copy_dispense_history_desc(out_records, &loaded_records);
    DispenseRecordRepository_clear_list(&loaded_records);
    if (result.success == 0) {
        LinkedList_clear(out_records, free);
        return result;
    }

    return Result_make_success("dispense history loaded");
}
