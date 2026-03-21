#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "domain/Admission.h"
#include "domain/Bed.h"
#include "domain/Patient.h"
#include "domain/Ward.h"
#include "repository/AdmissionRepository.h"
#include "repository/BedRepository.h"
#include "repository/PatientRepository.h"
#include "repository/WardRepository.h"
#include "service/InpatientService.h"

typedef struct InpatientServiceTestContext {
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char ward_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char bed_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char admission_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientRepository patient_repository;
    WardRepository ward_repository;
    BedRepository bed_repository;
    AdmissionRepository admission_repository;
    InpatientService service;
} InpatientServiceTestContext;

static void build_test_path(
    char *buffer,
    size_t buffer_size,
    const char *test_name,
    const char *file_name
) {
    assert(buffer != 0);
    assert(test_name != 0);
    assert(file_name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_inpatient_service_data/%s_%ld_%lu_%s",
        test_name,
        (long)time(0),
        (unsigned long)clock(),
        file_name
    );
}

static void copy_text(char *destination, size_t capacity, const char *source) {
    assert(destination != 0);
    assert(capacity > 0);

    if (source == 0) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

static Patient make_patient(const char *patient_id, const char *name, int is_inpatient) {
    Patient patient;

    memset(&patient, 0, sizeof(patient));
    copy_text(patient.patient_id, sizeof(patient.patient_id), patient_id);
    copy_text(patient.name, sizeof(patient.name), name);
    patient.gender = PATIENT_GENDER_UNKNOWN;
    patient.age = 30;
    copy_text(patient.contact, sizeof(patient.contact), "13800000000");
    copy_text(patient.id_card, sizeof(patient.id_card), "110101199001011234");
    copy_text(patient.allergy, sizeof(patient.allergy), "None");
    copy_text(patient.medical_history, sizeof(patient.medical_history), "None");
    patient.is_inpatient = is_inpatient;
    copy_text(patient.remarks, sizeof(patient.remarks), "test");
    return patient;
}

static Ward make_ward(
    const char *ward_id,
    int capacity,
    int occupied_beds,
    WardStatus status
) {
    Ward ward;

    memset(&ward, 0, sizeof(ward));
    copy_text(ward.ward_id, sizeof(ward.ward_id), ward_id);
    copy_text(ward.name, sizeof(ward.name), "Ward A");
    copy_text(ward.department_id, sizeof(ward.department_id), "DEP0001");
    copy_text(ward.location, sizeof(ward.location), "Floor 5");
    ward.capacity = capacity;
    ward.occupied_beds = occupied_beds;
    ward.status = status;
    return ward;
}

static Bed make_bed(
    const char *bed_id,
    const char *ward_id,
    BedStatus status,
    const char *admission_id
) {
    Bed bed;

    memset(&bed, 0, sizeof(bed));
    copy_text(bed.bed_id, sizeof(bed.bed_id), bed_id);
    copy_text(bed.ward_id, sizeof(bed.ward_id), ward_id);
    copy_text(bed.room_no, sizeof(bed.room_no), "501");
    copy_text(bed.bed_no, sizeof(bed.bed_no), "01");
    bed.status = status;
    copy_text(bed.current_admission_id, sizeof(bed.current_admission_id), admission_id);
    if (status == BED_STATUS_OCCUPIED) {
        copy_text(bed.occupied_at, sizeof(bed.occupied_at), "2026-03-20T08:00");
    }
    return bed;
}

static Admission make_admission(
    const char *admission_id,
    const char *patient_id,
    const char *ward_id,
    const char *bed_id,
    const char *admitted_at,
    AdmissionStatus status,
    const char *discharged_at,
    const char *summary
) {
    Admission admission;

    memset(&admission, 0, sizeof(admission));
    copy_text(admission.admission_id, sizeof(admission.admission_id), admission_id);
    copy_text(admission.patient_id, sizeof(admission.patient_id), patient_id);
    copy_text(admission.ward_id, sizeof(admission.ward_id), ward_id);
    copy_text(admission.bed_id, sizeof(admission.bed_id), bed_id);
    copy_text(admission.admitted_at, sizeof(admission.admitted_at), admitted_at);
    admission.status = status;
    copy_text(admission.discharged_at, sizeof(admission.discharged_at), discharged_at);
    copy_text(admission.summary, sizeof(admission.summary), summary);
    return admission;
}

static void setup_context(InpatientServiceTestContext *context, const char *test_name) {
    Result result;

    memset(context, 0, sizeof(*context));
    build_test_path(context->patient_path, sizeof(context->patient_path), test_name, "patients.txt");
    build_test_path(context->ward_path, sizeof(context->ward_path), test_name, "wards.txt");
    build_test_path(context->bed_path, sizeof(context->bed_path), test_name, "beds.txt");
    build_test_path(
        context->admission_path,
        sizeof(context->admission_path),
        test_name,
        "admissions.txt"
    );

    result = PatientRepository_init(&context->patient_repository, context->patient_path);
    assert(result.success == 1);
    result = WardRepository_init(&context->ward_repository, context->ward_path);
    assert(result.success == 1);
    result = BedRepository_init(&context->bed_repository, context->bed_path);
    assert(result.success == 1);
    result = AdmissionRepository_init(&context->admission_repository, context->admission_path);
    assert(result.success == 1);

    result = InpatientService_init(
        &context->service,
        context->patient_path,
        context->ward_path,
        context->bed_path,
        context->admission_path
    );
    assert(result.success == 1);
}

static void test_admit_patient_updates_patient_ward_bed_and_admission(void) {
    InpatientServiceTestContext context;
    Patient patient_seed = make_patient("PAT0001", "Alice", 0);
    Ward ward_seed = make_ward("WRD0001", 2, 0, WARD_STATUS_ACTIVE);
    Bed bed_seed = make_bed("BED0001", "WRD0001", BED_STATUS_AVAILABLE, "");
    Admission created;
    Patient patient;
    Ward ward;
    Bed bed;
    Admission admission;
    Result result;

    setup_context(&context, "admit");

    assert(PatientRepository_append(&context.patient_repository, &patient_seed).success == 1);
    assert(WardRepository_append(&context.ward_repository, &ward_seed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &bed_seed).success == 1);

    result = InpatientService_admit_patient(
        &context.service,
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        "observation",
        &created
    );
    assert(result.success == 1);
    assert(strcmp(created.admission_id, "ADM0001") == 0);
    assert(created.status == ADMISSION_STATUS_ACTIVE);

    assert(PatientRepository_find_by_id(&context.patient_repository, "PAT0001", &patient).success == 1);
    assert(patient.is_inpatient == 1);

    assert(WardRepository_find_by_id(&context.ward_repository, "WRD0001", &ward).success == 1);
    assert(ward.occupied_beds == 1);

    assert(BedRepository_find_by_id(&context.bed_repository, "BED0001", &bed).success == 1);
    assert(bed.status == BED_STATUS_OCCUPIED);
    assert(strcmp(bed.current_admission_id, "ADM0001") == 0);

    assert(
        AdmissionRepository_find_by_id(&context.admission_repository, "ADM0001", &admission).success == 1
    );
    assert(strcmp(admission.patient_id, "PAT0001") == 0);
    assert(strcmp(admission.summary, "observation") == 0);
}

static void test_admit_patient_rejects_duplicate_active_patient_and_occupied_bed(void) {
    InpatientServiceTestContext context;
    Patient first_patient = make_patient("PAT0001", "Alice", 0);
    Patient second_patient = make_patient("PAT0002", "Bob", 0);
    Ward ward_seed = make_ward("WRD0001", 2, 0, WARD_STATUS_ACTIVE);
    Bed first_bed = make_bed("BED0001", "WRD0001", BED_STATUS_AVAILABLE, "");
    Bed second_bed = make_bed("BED0002", "WRD0001", BED_STATUS_AVAILABLE, "");
    Admission created;
    Result result;

    setup_context(&context, "duplicate");

    assert(PatientRepository_append(&context.patient_repository, &first_patient).success == 1);
    assert(PatientRepository_append(&context.patient_repository, &second_patient).success == 1);
    assert(WardRepository_append(&context.ward_repository, &ward_seed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &first_bed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &second_bed).success == 1);

    result = InpatientService_admit_patient(
        &context.service,
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        "observation",
        &created
    );
    assert(result.success == 1);

    result = InpatientService_admit_patient(
        &context.service,
        "PAT0001",
        "WRD0001",
        "BED0002",
        "2026-03-20T09:00",
        "repeat",
        &created
    );
    assert(result.success == 0);

    result = InpatientService_admit_patient(
        &context.service,
        "PAT0002",
        "WRD0001",
        "BED0001",
        "2026-03-20T09:10",
        "occupied bed",
        &created
    );
    assert(result.success == 0);
}

static void test_discharge_patient_releases_bed_and_clears_inpatient_flag(void) {
    InpatientServiceTestContext context;
    Patient patient_seed = make_patient("PAT0001", "Alice", 1);
    Ward ward_seed = make_ward("WRD0001", 2, 1, WARD_STATUS_ACTIVE);
    Bed bed_seed = make_bed("BED0001", "WRD0001", BED_STATUS_OCCUPIED, "ADM0001");
    Admission admission_seed = make_admission(
        "ADM0001",
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        ADMISSION_STATUS_ACTIVE,
        "",
        "observation"
    );
    Admission discharged;
    Patient patient;
    Ward ward;
    Bed bed;
    Admission admission;
    Result result;

    setup_context(&context, "discharge");

    assert(PatientRepository_append(&context.patient_repository, &patient_seed).success == 1);
    assert(WardRepository_append(&context.ward_repository, &ward_seed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &bed_seed).success == 1);
    assert(AdmissionRepository_append(&context.admission_repository, &admission_seed).success == 1);

    result = InpatientService_discharge_patient(
        &context.service,
        "ADM0001",
        "2026-03-22T10:00",
        "stable",
        &discharged
    );
    assert(result.success == 1);
    assert(discharged.status == ADMISSION_STATUS_DISCHARGED);
    assert(strcmp(discharged.discharged_at, "2026-03-22T10:00") == 0);
    assert(strcmp(discharged.summary, "stable") == 0);

    assert(PatientRepository_find_by_id(&context.patient_repository, "PAT0001", &patient).success == 1);
    assert(patient.is_inpatient == 0);

    assert(WardRepository_find_by_id(&context.ward_repository, "WRD0001", &ward).success == 1);
    assert(ward.occupied_beds == 0);

    assert(BedRepository_find_by_id(&context.bed_repository, "BED0001", &bed).success == 1);
    assert(bed.status == BED_STATUS_AVAILABLE);
    assert(strcmp(bed.current_admission_id, "") == 0);
    assert(strcmp(bed.released_at, "2026-03-22T10:00") == 0);

    assert(
        AdmissionRepository_find_by_id(&context.admission_repository, "ADM0001", &admission).success == 1
    );
    assert(admission.status == ADMISSION_STATUS_DISCHARGED);
    assert(strcmp(admission.summary, "stable") == 0);
}

static void test_transfer_bed_updates_admission_and_bed_states(void) {
    InpatientServiceTestContext context;
    Patient patient_seed = make_patient("PAT0001", "Alice", 1);
    Ward ward_seed = make_ward("WRD0001", 3, 1, WARD_STATUS_ACTIVE);
    Bed occupied_bed = make_bed("BED0001", "WRD0001", BED_STATUS_OCCUPIED, "ADM0001");
    Bed target_bed = make_bed("BED0002", "WRD0001", BED_STATUS_AVAILABLE, "");
    Admission admission_seed = make_admission(
        "ADM0001",
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        ADMISSION_STATUS_ACTIVE,
        "",
        "observation"
    );
    Admission transferred;
    Bed first_bed;
    Bed second_bed;
    Admission admission;
    Result result;

    setup_context(&context, "transfer");

    assert(PatientRepository_append(&context.patient_repository, &patient_seed).success == 1);
    assert(WardRepository_append(&context.ward_repository, &ward_seed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &occupied_bed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &target_bed).success == 1);
    assert(AdmissionRepository_append(&context.admission_repository, &admission_seed).success == 1);

    result = InpatientService_transfer_bed(
        &context.service,
        "ADM0001",
        "BED0002",
        "2026-03-21T09:00",
        &transferred
    );
    assert(result.success == 1);
    assert(strcmp(transferred.bed_id, "BED0002") == 0);

    assert(BedRepository_find_by_id(&context.bed_repository, "BED0001", &first_bed).success == 1);
    assert(first_bed.status == BED_STATUS_AVAILABLE);
    assert(strcmp(first_bed.current_admission_id, "") == 0);
    assert(strcmp(first_bed.released_at, "2026-03-21T09:00") == 0);

    assert(BedRepository_find_by_id(&context.bed_repository, "BED0002", &second_bed).success == 1);
    assert(second_bed.status == BED_STATUS_OCCUPIED);
    assert(strcmp(second_bed.current_admission_id, "ADM0001") == 0);
    assert(strcmp(second_bed.occupied_at, "2026-03-21T09:00") == 0);

    assert(
        AdmissionRepository_find_by_id(&context.admission_repository, "ADM0001", &admission).success == 1
    );
    assert(strcmp(admission.bed_id, "BED0002") == 0);
}

static void test_transfer_bed_fails_when_target_ward_not_active_and_keeps_persisted_state(void) {
    InpatientServiceTestContext context;
    Patient patient_seed = make_patient("PAT0001", "Alice", 1);
    Ward current_ward_seed = make_ward("WRD0001", 2, 1, WARD_STATUS_ACTIVE);
    Ward target_ward_seed = make_ward("WRD0002", 2, 0, WARD_STATUS_CLOSED);
    Bed occupied_bed = make_bed("BED0001", "WRD0001", BED_STATUS_OCCUPIED, "ADM0001");
    Bed target_bed = make_bed("BED0002", "WRD0002", BED_STATUS_AVAILABLE, "");
    Admission admission_seed = make_admission(
        "ADM0001",
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        ADMISSION_STATUS_ACTIVE,
        "",
        "observation"
    );
    Admission transferred;
    Ward current_ward;
    Ward target_ward;
    Bed first_bed;
    Bed second_bed;
    Admission admission;
    Result result;

    setup_context(&context, "transfer_fail_target_ward");

    assert(PatientRepository_append(&context.patient_repository, &patient_seed).success == 1);
    assert(WardRepository_append(&context.ward_repository, &current_ward_seed).success == 1);
    assert(WardRepository_append(&context.ward_repository, &target_ward_seed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &occupied_bed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &target_bed).success == 1);
    assert(AdmissionRepository_append(&context.admission_repository, &admission_seed).success == 1);

    result = InpatientService_transfer_bed(
        &context.service,
        "ADM0001",
        "BED0002",
        "2026-03-21T09:00",
        &transferred
    );
    assert(result.success == 0);

    assert(WardRepository_find_by_id(&context.ward_repository, "WRD0001", &current_ward).success == 1);
    assert(current_ward.occupied_beds == 1);
    assert(WardRepository_find_by_id(&context.ward_repository, "WRD0002", &target_ward).success == 1);
    assert(target_ward.occupied_beds == 0);

    assert(BedRepository_find_by_id(&context.bed_repository, "BED0001", &first_bed).success == 1);
    assert(first_bed.status == BED_STATUS_OCCUPIED);
    assert(strcmp(first_bed.current_admission_id, "ADM0001") == 0);
    assert(BedRepository_find_by_id(&context.bed_repository, "BED0002", &second_bed).success == 1);
    assert(second_bed.status == BED_STATUS_AVAILABLE);
    assert(strcmp(second_bed.current_admission_id, "") == 0);

    assert(AdmissionRepository_find_by_id(&context.admission_repository, "ADM0001", &admission).success == 1);
    assert(strcmp(admission.ward_id, "WRD0001") == 0);
    assert(strcmp(admission.bed_id, "BED0001") == 0);
}

int main(void) {
    test_admit_patient_updates_patient_ward_bed_and_admission();
    test_admit_patient_rejects_duplicate_active_patient_and_occupied_bed();
    test_discharge_patient_releases_bed_and_clears_inpatient_flag();
    test_transfer_bed_updates_admission_and_bed_states();
    test_transfer_bed_fails_when_target_ward_not_active_and_keeps_persisted_state();
    return 0;
}
