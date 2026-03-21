#ifndef HIS_UI_MENU_APPLICATION_H
#define HIS_UI_MENU_APPLICATION_H

#include <stddef.h>
#include <stdio.h>

#include "common/Result.h"
#include "domain/Medicine.h"
#include "domain/Patient.h"
#include "domain/User.h"
#include "service/AuthService.h"
#include "service/BedService.h"
#include "service/DoctorService.h"
#include "service/InpatientService.h"
#include "service/MedicalRecordService.h"
#include "service/PatientService.h"
#include "service/PharmacyService.h"
#include "service/RegistrationService.h"
#include "ui/MenuController.h"

typedef struct MenuApplicationPaths {
    const char *user_path;
    const char *patient_path;
    const char *department_path;
    const char *doctor_path;
    const char *registration_path;
    const char *visit_path;
    const char *examination_path;
    const char *ward_path;
    const char *bed_path;
    const char *admission_path;
    const char *medicine_path;
    const char *dispense_record_path;
} MenuApplicationPaths;

typedef struct MenuApplication {
    AuthService auth_service;
    PatientService patient_service;
    DoctorService doctor_service;
    RegistrationRepository registration_repository;
    RegistrationService registration_service;
    MedicalRecordService medical_record_service;
    InpatientService inpatient_service;
    BedService bed_service;
    PharmacyService pharmacy_service;
    User authenticated_user;
    int has_authenticated_user;
    char bound_patient_id[HIS_DOMAIN_ID_CAPACITY];
    int has_bound_patient_session;
} MenuApplication;

Result MenuApplication_init(MenuApplication *application, const MenuApplicationPaths *paths);
Result MenuApplication_login(
    MenuApplication *application,
    const char *user_id,
    const char *password,
    UserRole required_role
);
void MenuApplication_logout(MenuApplication *application);
Result MenuApplication_bind_patient_session(
    MenuApplication *application,
    const char *patient_id
);
void MenuApplication_reset_patient_session(MenuApplication *application);
Result MenuApplication_add_patient(
    MenuApplication *application,
    const Patient *patient,
    char *buffer,
    size_t capacity
);
Result MenuApplication_update_patient(
    MenuApplication *application,
    const Patient *patient,
    char *buffer,
    size_t capacity
);
Result MenuApplication_query_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);
Result MenuApplication_create_registration(
    MenuApplication *application,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    char *buffer,
    size_t capacity
);
Result MenuApplication_query_registration(
    MenuApplication *application,
    const char *registration_id,
    char *buffer,
    size_t capacity
);
Result MenuApplication_cancel_registration(
    MenuApplication *application,
    const char *registration_id,
    const char *cancelled_at,
    char *buffer,
    size_t capacity
);
Result MenuApplication_query_registrations_by_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);
Result MenuApplication_query_pending_registrations_by_doctor(
    MenuApplication *application,
    const char *doctor_id,
    char *buffer,
    size_t capacity
);
Result MenuApplication_create_visit_record(
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
Result MenuApplication_query_patient_history(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);
Result MenuApplication_create_examination_record(
    MenuApplication *application,
    const char *visit_id,
    const char *exam_item,
    const char *exam_type,
    const char *requested_at,
    char *buffer,
    size_t capacity
);
Result MenuApplication_complete_examination_record(
    MenuApplication *application,
    const char *examination_id,
    const char *result_text,
    const char *completed_at,
    char *buffer,
    size_t capacity
);
Result MenuApplication_list_wards(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);
Result MenuApplication_list_beds_by_ward(
    MenuApplication *application,
    const char *ward_id,
    char *buffer,
    size_t capacity
);
Result MenuApplication_admit_patient(
    MenuApplication *application,
    const char *patient_id,
    const char *ward_id,
    const char *bed_id,
    const char *admitted_at,
    const char *summary,
    char *buffer,
    size_t capacity
);
Result MenuApplication_discharge_patient(
    MenuApplication *application,
    const char *admission_id,
    const char *discharged_at,
    const char *summary,
    char *buffer,
    size_t capacity
);
Result MenuApplication_query_active_admission_by_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
);
Result MenuApplication_query_current_patient_by_bed(
    MenuApplication *application,
    const char *bed_id,
    char *buffer,
    size_t capacity
);
Result MenuApplication_add_medicine(
    MenuApplication *application,
    const Medicine *medicine,
    char *buffer,
    size_t capacity
);
Result MenuApplication_restock_medicine(
    MenuApplication *application,
    const char *medicine_id,
    int quantity,
    char *buffer,
    size_t capacity
);
Result MenuApplication_dispense_medicine(
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
Result MenuApplication_query_medicine_stock(
    MenuApplication *application,
    const char *medicine_id,
    char *buffer,
    size_t capacity
);
Result MenuApplication_find_low_stock_medicines(
    MenuApplication *application,
    char *buffer,
    size_t capacity
);
Result MenuApplication_execute_action(
    MenuApplication *application,
    MenuAction action,
    FILE *input,
    FILE *output
);

#endif
