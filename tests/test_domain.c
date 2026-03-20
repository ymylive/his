#include <assert.h>

#include "domain/Department.h"
#include "domain/DispenseRecord.h"
#include "domain/Doctor.h"
#include "domain/ExaminationRecord.h"
#include "domain/Medicine.h"
#include "domain/Patient.h"
#include "domain/Prescription.h"
#include "domain/Registration.h"
#include "domain/VisitRecord.h"

static void test_registration_status_flow(void) {
    Registration pending = {
        "REG0001",
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20",
        REG_STATUS_PENDING,
        "",
        ""
    };
    Registration cancelled = {
        "REG0002",
        "PAT0002",
        "DOC0002",
        "DEP0001",
        "2026-03-20",
        REG_STATUS_PENDING,
        "",
        ""
    };

    assert(pending.status == REG_STATUS_PENDING);
    assert(Registration_mark_diagnosed(&pending, "2026-03-20T09:10") == 1);
    assert(pending.status == REG_STATUS_DIAGNOSED);
    assert(pending.diagnosed_at[0] == '2');
    assert(Registration_cancel(&pending, "2026-03-20T09:20") == 0);
    assert(pending.status == REG_STATUS_DIAGNOSED);
    assert(pending.cancelled_at[0] == '\0');

    assert(Registration_cancel(&cancelled, "2026-03-20T09:30") == 1);
    assert(cancelled.status == REG_STATUS_CANCELLED);
    assert(cancelled.cancelled_at[0] == '2');
    assert(Registration_mark_diagnosed(&cancelled, "2026-03-20T09:40") == 0);
    assert(cancelled.status == REG_STATUS_CANCELLED);
}

static void test_prescription_quantity_boundary(void) {
    assert(Prescription_has_valid_quantity(1, 5) == 1);
    assert(Prescription_has_valid_quantity(0, 5) == 0);
    assert(Prescription_has_valid_quantity(-1, 5) == 0);
    assert(Prescription_has_valid_quantity(6, 5) == 0);
}

static void test_domain_entities_exist(void) {
    Department department = {"DEP0001", "Cardiology", "Floor2", "Heart care"};
    Doctor doctor = {
        "DOC0001",
        "Dr.Alice",
        "Chief",
        "DEP0001",
        "Mon-Fri",
        DOCTOR_STATUS_ACTIVE
    };
    Patient patient = {
        "PAT0001",
        "Bob",
        PATIENT_GENDER_MALE,
        36,
        "13800000000",
        "ID1234567890",
        "Penicillin",
        "Diabetic history",
        0,
        "None"
    };
    VisitRecord visit = {
        "VIS0001",
        "REG0001",
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "Headache",
        "Migraine",
        "Plenty of rest",
        1,
        0,
        1,
        "2026-03-20T09:00"
    };
    ExaminationRecord exam = {
        "EXM0001",
        "VIS0001",
        "PAT0001",
        "DOC0001",
        "CT scan",
        "CT",
        EXAM_STATUS_COMPLETED,
        "Normal",
        "2026-03-20T09:30",
        "2026-03-20T10:00"
    };
    Prescription prescription = {
        "PRE0001",
        "VIS0001",
        "MED0001",
        2,
        "twice daily"
    };
    DispenseRecord dispense = {
        "DSP0001",
        "PRE0001",
        "MED0001",
        2,
        "PHA0001",
        "2026-03-20T10:30",
        DISPENSE_STATUS_COMPLETED
    };
    Medicine medicine = {
        "MED0001",
        "Aspirin",
        12.5,
        20,
        "DEP0001",
        5
    };

    assert(department.department_id[0] == 'D');
    assert(doctor.status == DOCTOR_STATUS_ACTIVE);
    assert(Patient_is_valid_age(patient.age) == 1);
    assert(Patient_is_valid_age(151) == 0);
    assert(patient.id_card[0] == 'I');
    assert(visit.registration_id[0] == 'R');
    assert(visit.need_exam == 1);
    assert(visit.need_admission == 0);
    assert(exam.exam_type[0] == 'C');
    assert(exam.status == EXAM_STATUS_COMPLETED);
    assert(prescription.quantity == 2);
    assert(dispense.status == DISPENSE_STATUS_COMPLETED);
    assert(Medicine_has_valid_inventory(&medicine) == 1);
}

int main(void) {
    test_registration_status_flow();
    test_prescription_quantity_boundary();
    test_domain_entities_exist();
    return 0;
}
