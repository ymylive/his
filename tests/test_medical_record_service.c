#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/Admission.h"
#include "domain/ExaminationRecord.h"
#include "domain/Registration.h"
#include "domain/VisitRecord.h"
#include "repository/AdmissionRepository.h"
#include "repository/ExaminationRecordRepository.h"
#include "repository/RegistrationRepository.h"
#include "repository/VisitRecordRepository.h"
#include "service/MedicalRecordService.h"

typedef struct MedicalRecordServiceTestContext {
    char registration_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char visit_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char examination_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char admission_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    RegistrationRepository registration_repository;
    VisitRecordRepository visit_repository;
    ExaminationRecordRepository examination_repository;
    AdmissionRepository admission_repository;
    MedicalRecordService service;
} MedicalRecordServiceTestContext;

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
        "build/test_medical_record_service_data/%s_%ld_%lu_%s",
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

static Registration make_registration(
    const char *registration_id,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at
) {
    Registration registration;

    memset(&registration, 0, sizeof(registration));
    copy_text(
        registration.registration_id,
        sizeof(registration.registration_id),
        registration_id
    );
    copy_text(registration.patient_id, sizeof(registration.patient_id), patient_id);
    copy_text(registration.doctor_id, sizeof(registration.doctor_id), doctor_id);
    copy_text(registration.department_id, sizeof(registration.department_id), department_id);
    copy_text(registration.registered_at, sizeof(registration.registered_at), registered_at);
    registration.status = REG_STATUS_PENDING;
    return registration;
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

static const Registration *registration_at(const LinkedList *list, size_t index) {
    const LinkedListNode *node = 0;
    size_t current_index = 0;

    assert(list != 0);
    node = list->head;
    while (node != 0) {
        if (current_index == index) {
            return (const Registration *)node->data;
        }

        node = node->next;
        current_index++;
    }

    return 0;
}

static const VisitRecord *visit_at(const LinkedList *list, size_t index) {
    const LinkedListNode *node = 0;
    size_t current_index = 0;

    assert(list != 0);
    node = list->head;
    while (node != 0) {
        if (current_index == index) {
            return (const VisitRecord *)node->data;
        }

        node = node->next;
        current_index++;
    }

    return 0;
}

static const ExaminationRecord *examination_at(const LinkedList *list, size_t index) {
    const LinkedListNode *node = 0;
    size_t current_index = 0;

    assert(list != 0);
    node = list->head;
    while (node != 0) {
        if (current_index == index) {
            return (const ExaminationRecord *)node->data;
        }

        node = node->next;
        current_index++;
    }

    return 0;
}

static const Admission *admission_at(const LinkedList *list, size_t index) {
    const LinkedListNode *node = 0;
    size_t current_index = 0;

    assert(list != 0);
    node = list->head;
    while (node != 0) {
        if (current_index == index) {
            return (const Admission *)node->data;
        }

        node = node->next;
        current_index++;
    }

    return 0;
}

static void setup_context(MedicalRecordServiceTestContext *context, const char *test_name) {
    Result result;

    memset(context, 0, sizeof(*context));
    build_test_path(
        context->registration_path,
        sizeof(context->registration_path),
        test_name,
        "registrations.txt"
    );
    build_test_path(context->visit_path, sizeof(context->visit_path), test_name, "visits.txt");
    build_test_path(
        context->examination_path,
        sizeof(context->examination_path),
        test_name,
        "examinations.txt"
    );
    build_test_path(
        context->admission_path,
        sizeof(context->admission_path),
        test_name,
        "admissions.txt"
    );

    result = RegistrationRepository_init(
        &context->registration_repository,
        context->registration_path
    );
    assert(result.success == 1);
    result = VisitRecordRepository_init(&context->visit_repository, context->visit_path);
    assert(result.success == 1);
    result = ExaminationRecordRepository_init(
        &context->examination_repository,
        context->examination_path
    );
    assert(result.success == 1);
    result = AdmissionRepository_init(&context->admission_repository, context->admission_path);
    assert(result.success == 1);

    result = MedicalRecordService_init(
        &context->service,
        context->registration_path,
        context->visit_path,
        context->examination_path,
        context->admission_path
    );
    assert(result.success == 1);
}

static void seed_registration(
    MedicalRecordServiceTestContext *context,
    const char *registration_id,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at
) {
    Registration registration = make_registration(
        registration_id,
        patient_id,
        doctor_id,
        department_id,
        registered_at
    );

    assert(
        RegistrationRepository_append(&context->registration_repository, &registration).success == 1
    );
}

static void seed_admission(
    MedicalRecordServiceTestContext *context,
    const char *admission_id,
    const char *patient_id,
    const char *bed_id,
    const char *admitted_at
) {
    Admission admission = make_admission(
        admission_id,
        patient_id,
        "WAR0001",
        bed_id,
        admitted_at,
        ADMISSION_STATUS_ACTIVE,
        "",
        "Observation"
    );

    assert(AdmissionRepository_append(&context->admission_repository, &admission).success == 1);
}

static void test_create_update_and_delete_visit_record(void) {
    MedicalRecordServiceTestContext context;
    VisitRecord created;
    VisitRecord updated;
    Registration registration;
    Result result;

    setup_context(&context, "visit_crud");
    seed_registration(&context, "REG0001", "PAT0001", "DOC0001", "DEP0001", "2026-03-20T08:00");

    result = MedicalRecordService_create_visit_record(
        &context.service,
        "REG0001",
        "Fever",
        "Influenza",
        "Rest",
        1,
        0,
        1,
        "2026-03-20T08:30",
        &created
    );
    assert(result.success == 1);
    assert(strcmp(created.visit_id, "VIS0001") == 0);
    assert(strcmp(created.patient_id, "PAT0001") == 0);
    assert(created.need_exam == 1);

    result = RegistrationRepository_find_by_registration_id(
        &context.registration_repository,
        "REG0001",
        &registration
    );
    assert(result.success == 1);
    assert(registration.status == REG_STATUS_DIAGNOSED);
    assert(strcmp(registration.diagnosed_at, "2026-03-20T08:30") == 0);

    result = MedicalRecordService_update_visit_record(
        &context.service,
        "VIS0001",
        "High fever",
        "Pneumonia",
        "Ward observation",
        1,
        1,
        1,
        "2026-03-20T09:00",
        &updated
    );
    assert(result.success == 1);
    assert(strcmp(updated.diagnosis, "Pneumonia") == 0);
    assert(updated.need_admission == 1);

    result = VisitRecordRepository_find_by_visit_id(
        &context.visit_repository,
        "VIS0001",
        &created
    );
    assert(result.success == 1);
    assert(strcmp(created.chief_complaint, "High fever") == 0);
    assert(strcmp(created.visit_time, "2026-03-20T09:00") == 0);

    result = MedicalRecordService_delete_visit_record(&context.service, "VIS0001");
    assert(result.success == 1);
    assert(
        VisitRecordRepository_find_by_visit_id(&context.visit_repository, "VIS0001", &created)
            .success == 0
    );

    result = RegistrationRepository_find_by_registration_id(
        &context.registration_repository,
        "REG0001",
        &registration
    );
    assert(result.success == 1);
    assert(registration.status == REG_STATUS_PENDING);
    assert(strcmp(registration.diagnosed_at, "") == 0);
}

static void test_delete_visit_rejects_when_examination_exists(void) {
    MedicalRecordServiceTestContext context;
    VisitRecord visit;
    ExaminationRecord examination;
    Result result;

    setup_context(&context, "visit_delete_guard");
    seed_registration(&context, "REG0001", "PAT0001", "DOC0001", "DEP0001", "2026-03-20T08:00");

    result = MedicalRecordService_create_visit_record(
        &context.service,
        "REG0001",
        "Cough",
        "Bronchitis",
        "Chest exam",
        1,
        0,
        0,
        "2026-03-20T08:20",
        &visit
    );
    assert(result.success == 1);

    result = MedicalRecordService_create_examination_record(
        &context.service,
        visit.visit_id,
        "Chest CT",
        "Imaging",
        "2026-03-20T08:40",
        &examination
    );
    assert(result.success == 1);

    result = MedicalRecordService_delete_visit_record(&context.service, visit.visit_id);
    assert(result.success == 0);
}

static void test_create_update_and_delete_examination_record(void) {
    MedicalRecordServiceTestContext context;
    VisitRecord visit;
    ExaminationRecord examination;
    Result result;

    setup_context(&context, "exam_crud");
    seed_registration(&context, "REG0001", "PAT0001", "DOC0001", "DEP0001", "2026-03-20T08:00");

    result = MedicalRecordService_create_visit_record(
        &context.service,
        "REG0001",
        "Headache",
        "Migraine",
        "Observe",
        1,
        0,
        0,
        "2026-03-20T08:15",
        &visit
    );
    assert(result.success == 1);

    result = MedicalRecordService_create_examination_record(
        &context.service,
        visit.visit_id,
        "Blood test",
        "Lab",
        "2026-03-20T08:25",
        &examination
    );
    assert(result.success == 1);
    assert(strcmp(examination.examination_id, "EXM0001") == 0);
    assert(examination.status == EXAM_STATUS_PENDING);

    result = MedicalRecordService_update_examination_record(
        &context.service,
        examination.examination_id,
        EXAM_STATUS_COMPLETED,
        "",
        "2026-03-20T09:10",
        &examination
    );
    assert(result.success == 0);

    result = MedicalRecordService_update_examination_record(
        &context.service,
        examination.examination_id,
        EXAM_STATUS_COMPLETED,
        "Normal",
        "2026-03-20T09:10",
        &examination
    );
    assert(result.success == 1);
    assert(examination.status == EXAM_STATUS_COMPLETED);
    assert(strcmp(examination.result, "Normal") == 0);

    result = MedicalRecordService_delete_examination_record(
        &context.service,
        examination.examination_id
    );
    assert(result.success == 1);
    assert(
        ExaminationRecordRepository_find_by_examination_id(
            &context.examination_repository,
            "EXM0001",
            &examination
        ).success == 0
    );
}

static void test_history_query_collects_four_record_types(void) {
    MedicalRecordServiceTestContext context;
    MedicalRecordHistory history;
    VisitRecord visit;
    ExaminationRecord examination;
    Result result;

    setup_context(&context, "patient_history");
    seed_registration(&context, "REG0001", "PAT0001", "DOC0001", "DEP0001", "2026-03-20T08:00");
    seed_registration(&context, "REG0002", "PAT0002", "DOC0002", "DEP0002", "2026-03-20T09:00");
    seed_admission(&context, "ADM0001", "PAT0001", "BED0001", "2026-03-20T10:00");
    seed_admission(&context, "ADM0002", "PAT0002", "BED0002", "2026-03-21T10:00");

    result = MedicalRecordService_create_visit_record(
        &context.service,
        "REG0001",
        "Pain",
        "Appendicitis",
        "Admit",
        1,
        1,
        0,
        "2026-03-20T08:30",
        &visit
    );
    assert(result.success == 1);

    result = MedicalRecordService_create_examination_record(
        &context.service,
        visit.visit_id,
        "Ultrasound",
        "Imaging",
        "2026-03-20T08:45",
        &examination
    );
    assert(result.success == 1);

    result = MedicalRecordService_find_patient_history(&context.service, "PAT0001", &history);
    assert(result.success == 1);
    assert(LinkedList_count(&history.registrations) == 1);
    assert(LinkedList_count(&history.visits) == 1);
    assert(LinkedList_count(&history.examinations) == 1);
    assert(LinkedList_count(&history.admissions) == 1);
    assert(strcmp(registration_at(&history.registrations, 0)->registration_id, "REG0001") == 0);
    assert(strcmp(visit_at(&history.visits, 0)->visit_id, visit.visit_id) == 0);
    assert(
        strcmp(
            examination_at(&history.examinations, 0)->examination_id,
            examination.examination_id
        ) == 0
    );
    assert(strcmp(admission_at(&history.admissions, 0)->admission_id, "ADM0001") == 0);
    MedicalRecordHistory_clear(&history);
}

static void test_time_range_query_filters_occurrence_time(void) {
    MedicalRecordServiceTestContext context;
    MedicalRecordHistory history;
    VisitRecord visit;
    ExaminationRecord examination;
    Result result;

    setup_context(&context, "time_range");
    seed_registration(&context, "REG0001", "PAT0001", "DOC0001", "DEP0001", "2026-03-20T08:00");
    seed_registration(&context, "REG0002", "PAT0002", "DOC0002", "DEP0002", "2026-03-25T08:00");
    seed_admission(&context, "ADM0001", "PAT0001", "BED0001", "2026-03-23T08:00");
    seed_admission(&context, "ADM0002", "PAT0002", "BED0002", "2026-03-26T08:00");

    result = MedicalRecordService_create_visit_record(
        &context.service,
        "REG0001",
        "Nausea",
        "Gastritis",
        "Diet",
        1,
        0,
        1,
        "2026-03-21T09:00",
        &visit
    );
    assert(result.success == 1);

    result = MedicalRecordService_create_examination_record(
        &context.service,
        visit.visit_id,
        "Gastroscopy",
        "Endoscopy",
        "2026-03-22T11:00",
        &examination
    );
    assert(result.success == 1);

    result = MedicalRecordService_find_records_by_time_range(
        &context.service,
        "2026-03-21T00:00",
        "2026-03-22T23:59",
        &history
    );
    assert(result.success == 1);
    assert(LinkedList_count(&history.registrations) == 0);
    assert(LinkedList_count(&history.visits) == 1);
    assert(LinkedList_count(&history.examinations) == 1);
    assert(LinkedList_count(&history.admissions) == 0);
    assert(strcmp(visit_at(&history.visits, 0)->visit_id, visit.visit_id) == 0);
    assert(
        strcmp(
            examination_at(&history.examinations, 0)->examination_id,
            examination.examination_id
        ) == 0
    );
    MedicalRecordHistory_clear(&history);
}

int main(void) {
    test_create_update_and_delete_visit_record();
    test_delete_visit_rejects_when_examination_exists();
    test_create_update_and_delete_examination_record();
    test_history_query_collects_four_record_types();
    test_time_range_query_filters_occurrence_time();
    return 0;
}
