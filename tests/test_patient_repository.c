#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/Patient.h"
#include "repository/PatientRepository.h"

static void TestPatientRepository_make_path(
    char *buffer,
    size_t buffer_size,
    const char *test_name
) {
    assert(buffer != 0);
    assert(test_name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_patient_repository_data/%s_%ld.txt",
        test_name,
        (long)time(0)
    );
    remove(buffer);
}

static Patient TestPatientRepository_make_patient(
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

static int TestPatientRepository_has_id(const void *data, const void *context) {
    const Patient *patient = (const Patient *)data;
    const char *expected_id = (const char *)context;

    return strcmp(patient->patient_id, expected_id) == 0;
}

static Patient *TestPatientRepository_allocate_patient(const Patient *source) {
    Patient *copy = (Patient *)malloc(sizeof(Patient));

    assert(copy != 0);
    *copy = *source;
    return copy;
}

static void test_append_and_find_by_id(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientRepository repository;
    Patient expected = TestPatientRepository_make_patient(
        "PAT0002",
        "Bob",
        PATIENT_GENDER_MALE,
        35,
        "13800000001",
        "ID0002",
        "None",
        "Flu",
        1,
        "WardA"
    );
    Patient actual;
    FILE *file = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result;

    TestPatientRepository_make_path(path, sizeof(path), "append_find");
    result = PatientRepository_init(&repository, path);
    assert(result.success == 1);

    result = PatientRepository_append(
        &repository,
        &(Patient){
            "PAT0001",
            "Alice",
            PATIENT_GENDER_FEMALE,
            28,
            "13800000000",
            "ID0001",
            "Penicillin",
            "Healthy",
            0,
            "First visit"
        }
    );
    assert(result.success == 1);

    result = PatientRepository_append(&repository, &expected);
    assert(result.success == 1);

    result = PatientRepository_find_by_id(&repository, "PAT0002", &actual);
    assert(result.success == 1);
    assert(strcmp(actual.patient_id, "PAT0002") == 0);
    assert(strcmp(actual.name, "Bob") == 0);
    assert(actual.gender == PATIENT_GENDER_MALE);
    assert(actual.age == 35);
    assert(strcmp(actual.contact, "13800000001") == 0);
    assert(strcmp(actual.id_card, "ID0002") == 0);
    assert(actual.is_inpatient == 1);
    assert(strcmp(actual.remarks, "WardA") == 0);

    file = fopen(path, "r");
    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strcmp(line, PATIENT_REPOSITORY_HEADER "\n") == 0);
    fclose(file);
}

static void test_load_all_reads_back_records(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientRepository repository;
    LinkedList patients;
    Patient first = TestPatientRepository_make_patient(
        "PAT0101",
        "Carol",
        PATIENT_GENDER_FEMALE,
        41,
        "13800000002",
        "ID0101",
        "Dust",
        "Asthma",
        0,
        "Observe"
    );
    Patient second = TestPatientRepository_make_patient(
        "PAT0102",
        "David",
        PATIENT_GENDER_MALE,
        52,
        "13800000003",
        "ID0102",
        "None",
        "Hypertension",
        1,
        "WardB"
    );
    Patient *loaded = 0;
    Result result;

    TestPatientRepository_make_path(path, sizeof(path), "load_all");
    result = PatientRepository_init(&repository, path);
    assert(result.success == 1);
    assert(PatientRepository_append(&repository, &first).success == 1);
    assert(PatientRepository_append(&repository, &second).success == 1);

    LinkedList_init(&patients);
    result = PatientRepository_load_all(&repository, &patients);
    assert(result.success == 1);
    assert(LinkedList_count(&patients) == 2);

    loaded = (Patient *)LinkedList_find_first(&patients, TestPatientRepository_has_id, "PAT0102");
    assert(loaded != 0);
    assert(strcmp(loaded->name, "David") == 0);
    assert(loaded->age == 52);
    assert(strcmp(loaded->medical_history, "Hypertension") == 0);

    PatientRepository_clear_loaded_list(&patients);
}

static void test_save_all_rewrites_table(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientRepository repository;
    LinkedList patients;
    Patient first = TestPatientRepository_make_patient(
        "PAT0201",
        "Eva",
        PATIENT_GENDER_FEMALE,
        30,
        "13800000004",
        "ID0201",
        "Seafood",
        "None",
        0,
        "Follow up"
    );
    Patient second = TestPatientRepository_make_patient(
        "PAT0202",
        "Frank",
        PATIENT_GENDER_MALE,
        46,
        "13800000005",
        "ID0202",
        "None",
        "Diabetes",
        1,
        "WardC"
    );
    FILE *file = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result;

    TestPatientRepository_make_path(path, sizeof(path), "save_all");
    result = PatientRepository_init(&repository, path);
    assert(result.success == 1);
    assert(PatientRepository_append(&repository, &first).success == 1);

    LinkedList_init(&patients);
    assert(LinkedList_append(&patients, TestPatientRepository_allocate_patient(&second)) == 1);

    result = PatientRepository_save_all(&repository, &patients);
    assert(result.success == 1);

    file = fopen(path, "r");
    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strcmp(line, PATIENT_REPOSITORY_HEADER "\n") == 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strcmp(line, "PAT0202|Frank|1|46|13800000005|ID0202|None|Diabetes|1|WardC\n") == 0);
    assert(fgets(line, sizeof(line), file) == 0);
    fclose(file);

    PatientRepository_clear_loaded_list(&patients);
}

static void test_duplicate_patient_id_is_rejected(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientRepository repository;
    LinkedList patients;
    Patient *loaded = 0;
    Result result;

    TestPatientRepository_make_path(path, sizeof(path), "duplicate");
    result = PatientRepository_init(&repository, path);
    assert(result.success == 1);

    result = PatientRepository_append(
        &repository,
        &(Patient){
            "PAT0301",
            "Grace",
            PATIENT_GENDER_FEMALE,
            33,
            "13800000006",
            "ID0301",
            "Pollen",
            "None",
            0,
            "Stable"
        }
    );
    assert(result.success == 1);

    result = PatientRepository_append(
        &repository,
        &(Patient){
            "PAT0301",
            "Grace Updated",
            PATIENT_GENDER_FEMALE,
            34,
            "13800009999",
            "ID0301X",
            "Pollen",
            "Cold",
            1,
            "Changed"
        }
    );
    assert(result.success == 0);

    LinkedList_init(&patients);
    result = PatientRepository_load_all(&repository, &patients);
    assert(result.success == 1);
    assert(LinkedList_count(&patients) == 1);

    loaded = (Patient *)LinkedList_find_first(&patients, TestPatientRepository_has_id, "PAT0301");
    assert(loaded != 0);
    assert(strcmp(loaded->name, "Grace") == 0);
    assert(loaded->age == 33);

    PatientRepository_clear_loaded_list(&patients);
}

int main(void) {
    test_append_and_find_by_id();
    test_load_all_reads_back_records();
    test_save_all_rewrites_table();
    test_duplicate_patient_id_is_rejected();
    return 0;
}
