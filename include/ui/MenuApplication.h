#ifndef HIS_UI_MENU_APPLICATION_H
#define HIS_UI_MENU_APPLICATION_H

#include <stddef.h>
#include <stdio.h>

#include "common/Result.h"
#include "domain/Medicine.h"
#include "domain/Patient.h"
#include "service/BedService.h"
#include "service/DoctorService.h"
#include "service/InpatientService.h"
#include "service/MedicalRecordService.h"
#include "service/PatientService.h"
#include "service/PharmacyService.h"
#include "service/RegistrationService.h"
#include "ui/MenuController.h"

typedef struct MenuApplicationPaths {
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
    PatientService patient_service;
    DoctorService doctor_service;
    RegistrationRepository registration_repository;
    RegistrationService registration_service;
    MedicalRecordService medical_record_service;
    InpatientService inpatient_service;
    BedService bed_service;
    PharmacyService pharmacy_service;
} MenuApplication;

Result MenuApplication_init(MenuApplication *application, const MenuApplicationPaths *paths);
Result MenuApplication_add_patient(
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
