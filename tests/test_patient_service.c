#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/Patient.h"
#include "service/PatientService.h"

static void TestPatientService_make_path(
    char *buffer,
    size_t buffer_size,
    const char *test_name
) {
    assert(buffer != 0);
    assert(test_name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_patient_service_data/%s_%ld.txt",
        test_name,
        (long)time(0)
    );
    remove(buffer);
}

static Patient TestPatientService_make_patient(
    const char *patient_id,
    const char *name,
    PatientGender gender,
    int age,
    const char *contact,
    const char *id_card,
    const char *allergy,
    const char *medical_history,
    int is_inpatient,
    const char *remarks
) {
    Patient patient;

    memset(&patient, 0, sizeof(patient));
    strcpy(patient.patient_id, patient_id);
    strcpy(patient.name, name);
    patient.gender = gender;
    patient.age = age;
    strcpy(patient.contact, contact);
    strcpy(patient.id_card, id_card);
    strcpy(patient.allergy, allergy);
    strcpy(patient.medical_history, medical_history);
    patient.is_inpatient = is_inpatient;
    strcpy(patient.remarks, remarks);
    return patient;
}

static int TestPatientService_has_id(const void *data, const void *context) {
    const Patient *patient = (const Patient *)data;
    const char *patient_id = (const char *)context;

    return strcmp(patient->patient_id, patient_id) == 0;
}

static void test_create_and_query_patient_records(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientService service;
    LinkedList matches;
    Patient patient;
    Result result;

    TestPatientService_make_path(path, sizeof(path), "create_query");
    result = PatientService_init(&service, path);
    assert(result.success == 1);

    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT1001",
            "Alice",
            PATIENT_GENDER_FEMALE,
            28,
            "13800000001",
            "110101199001010011",
            "Penicillin",
            "Healthy",
            0,
            "First visit"
        }
    );
    assert(result.success == 1);

    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT1002",
            "Alice",
            PATIENT_GENDER_FEMALE,
            36,
            "13800000002",
            "110101199202020022",
            "Dust",
            "Asthma",
            1,
            "Ward A"
        }
    );
    assert(result.success == 1);

    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT1003",
            "Bob",
            PATIENT_GENDER_MALE,
            41,
            "13800000003",
            "110101198303030033",
            "None",
            "Hypertension",
            0,
            "Follow up"
        }
    );
    assert(result.success == 1);

    result = PatientService_find_patient_by_id(&service, "PAT1002", &patient);
    assert(result.success == 1);
    assert(strcmp(patient.name, "Alice") == 0);
    assert(patient.age == 36);
    assert(strcmp(patient.contact, "13800000002") == 0);
    assert(strcmp(patient.id_card, "110101199202020022") == 0);
    assert(patient.is_inpatient == 1);

    LinkedList_init(&matches);
    result = PatientService_find_patients_by_name(&service, "Alice", &matches);
    assert(result.success == 1);
    assert(LinkedList_count(&matches) == 2);
    assert(LinkedList_find_first(&matches, TestPatientService_has_id, "PAT1001") != 0);
    assert(LinkedList_find_first(&matches, TestPatientService_has_id, "PAT1002") != 0);
    PatientService_clear_search_results(&matches);

    result = PatientService_find_patient_by_contact(&service, "13800000003", &patient);
    assert(result.success == 1);
    assert(strcmp(patient.patient_id, "PAT1003") == 0);

    result = PatientService_find_patient_by_id_card(&service, "110101199001010011", &patient);
    assert(result.success == 1);
    assert(strcmp(patient.patient_id, "PAT1001") == 0);
}

static void test_update_and_delete_patient_record(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientService service;
    Patient updated = TestPatientService_make_patient(
        "PAT2001",
        "Carol Updated",
        PATIENT_GENDER_FEMALE,
        52,
        "13800000022",
        "110101197404040044",
        "Pollen",
        "Diabetes",
        1,
        "Ward B"
    );
    Patient loaded;
    Result result;

    TestPatientService_make_path(path, sizeof(path), "update_delete");
    result = PatientService_init(&service, path);
    assert(result.success == 1);

    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT2001",
            "Carol",
            PATIENT_GENDER_FEMALE,
            51,
            "13800000021",
            "110101197303030033",
            "Pollen",
            "None",
            0,
            "Observe"
        }
    );
    assert(result.success == 1);

    result = PatientService_update_patient(&service, &updated);
    assert(result.success == 1);

    result = PatientService_find_patient_by_id(&service, "PAT2001", &loaded);
    assert(result.success == 1);
    assert(strcmp(loaded.name, "Carol Updated") == 0);
    assert(loaded.age == 52);
    assert(strcmp(loaded.contact, "13800000022") == 0);
    assert(strcmp(loaded.id_card, "110101197404040044") == 0);
    assert(loaded.is_inpatient == 1);

    result = PatientService_find_patient_by_contact(&service, "13800000021", &loaded);
    assert(result.success == 0);

    result = PatientService_delete_patient(&service, "PAT2001");
    assert(result.success == 1);

    result = PatientService_find_patient_by_id(&service, "PAT2001", &loaded);
    assert(result.success == 0);
}

static void test_validation_rejects_invalid_patient_fields(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientService service;
    Patient invalid = TestPatientService_make_patient(
        "PAT3001",
        "Valid Name",
        PATIENT_GENDER_MALE,
        20,
        "13800000031",
        "110101199505050055",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    Result result;

    TestPatientService_make_path(path, sizeof(path), "validation");
    result = PatientService_init(&service, path);
    assert(result.success == 1);

    invalid.age = 151;
    result = PatientService_create_patient(&service, &invalid);
    assert(result.success == 0);

    invalid = TestPatientService_make_patient(
        "PAT3002",
        "",
        PATIENT_GENDER_MALE,
        20,
        "13800000032",
        "110101199606060066",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    result = PatientService_create_patient(&service, &invalid);
    assert(result.success == 0);

    invalid = TestPatientService_make_patient(
        "PAT3003",
        "Derek",
        PATIENT_GENDER_MALE,
        20,
        "13AB",
        "110101199707070077",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    result = PatientService_create_patient(&service, &invalid);
    assert(result.success == 0);

    invalid = TestPatientService_make_patient(
        "PAT3004",
        "Emma",
        PATIENT_GENDER_FEMALE,
        20,
        "13800000034",
        "",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    result = PatientService_create_patient(&service, &invalid);
    assert(result.success == 0);

    invalid = TestPatientService_make_patient(
        "PAT3005",
        "Fiona",
        PATIENT_GENDER_FEMALE,
        20,
        "13800000035",
        "invalid-card",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    result = PatientService_create_patient(&service, &invalid);
    assert(result.success == 0);

    invalid = TestPatientService_make_patient(
        "PAT3006",
        "Gavin",
        PATIENT_GENDER_MALE,
        20,
        "13800000036",
        "110101199808080088",
        "None",
        "Healthy",
        2,
        "Stable"
    );
    result = PatientService_create_patient(&service, &invalid);
    assert(result.success == 0);
}

static void test_duplicate_contact_and_id_card_are_rejected(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientService service;
    Patient second = TestPatientService_make_patient(
        "PAT4002",
        "Irene",
        PATIENT_GENDER_FEMALE,
        44,
        "13800000042",
        "110101198909090099",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    Result result;

    TestPatientService_make_path(path, sizeof(path), "duplicates");
    result = PatientService_init(&service, path);
    assert(result.success == 1);

    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT4001",
            "Henry",
            PATIENT_GENDER_MALE,
            43,
            "13800000041",
            "110101198808080088",
            "None",
            "Healthy",
            0,
            "Stable"
        }
    );
    assert(result.success == 1);

    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT4003",
            "Jack",
            PATIENT_GENDER_MALE,
            45,
            "13800000041",
            "110101199010100100",
            "None",
            "Healthy",
            0,
            "Stable"
        }
    );
    assert(result.success == 0);

    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT4004",
            "Kate",
            PATIENT_GENDER_FEMALE,
            46,
            "13800000044",
            "110101198808080088",
            "None",
            "Healthy",
            0,
            "Stable"
        }
    );
    assert(result.success == 0);

    result = PatientService_create_patient(&service, &second);
    assert(result.success == 1);

    second.age = 45;
    strcpy(second.contact, "13800000041");
    result = PatientService_update_patient(&service, &second);
    assert(result.success == 0);

    second = TestPatientService_make_patient(
        "PAT4002",
        "Irene",
        PATIENT_GENDER_FEMALE,
        44,
        "13800000042",
        "110101198909090099",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    strcpy(second.id_card, "110101198808080088");
    result = PatientService_update_patient(&service, &second);
    assert(result.success == 0);
}

int main(void) {
    test_create_and_query_patient_records();
    test_update_and_delete_patient_record();
    test_validation_rejects_invalid_patient_fields();
    test_duplicate_contact_and_id_card_are_rejected();
    return 0;
}
