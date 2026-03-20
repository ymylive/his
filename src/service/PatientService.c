#include "service/PatientService.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "repository/RepositoryUtils.h"

enum {
    PATIENT_SERVICE_MATCH_CONTACT = 1,
    PATIENT_SERVICE_MATCH_ID_CARD = 2
};

static int PatientService_is_blank_text(const char *text) {
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

static Result PatientService_validate_required_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (PatientService_is_blank_text(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("text valid");
}

static Result PatientService_validate_contact(const char *contact) {
    size_t length = 0;
    Result result = PatientService_validate_required_text(contact, "patient contact");

    if (result.success == 0) {
        return result;
    }

    length = strlen(contact);
    if (length < 6 || length > 20) {
        return Result_make_failure("patient contact invalid");
    }

    while (*contact != '\0') {
        if (!isdigit((unsigned char)*contact)) {
            return Result_make_failure("patient contact invalid");
        }

        contact++;
    }

    return Result_make_success("patient contact valid");
}

static int PatientService_is_valid_id_card_character(char character, size_t index, size_t length) {
    if (isdigit((unsigned char)character)) {
        return 1;
    }

    if (length == 18 && index == length - 1 && (character == 'X' || character == 'x')) {
        return 1;
    }

    return 0;
}

static Result PatientService_validate_id_card(const char *id_card) {
    size_t index = 0;
    size_t length = 0;
    Result result = PatientService_validate_required_text(id_card, "patient id card");

    if (result.success == 0) {
        return result;
    }

    length = strlen(id_card);
    if (length != 15 && length != 18) {
        return Result_make_failure("patient id card invalid");
    }

    for (index = 0; index < length; index++) {
        if (!PatientService_is_valid_id_card_character(id_card[index], index, length)) {
            return Result_make_failure("patient id card invalid");
        }
    }

    return Result_make_success("patient id card valid");
}

static Result PatientService_validate_patient(const Patient *patient) {
    Result result;

    if (patient == 0) {
        return Result_make_failure("patient missing");
    }

    result = PatientService_validate_required_text(patient->patient_id, "patient id");
    if (result.success == 0) {
        return result;
    }

    result = PatientService_validate_required_text(patient->name, "patient name");
    if (result.success == 0) {
        return result;
    }

    if (patient->gender < PATIENT_GENDER_UNKNOWN || patient->gender > PATIENT_GENDER_FEMALE) {
        return Result_make_failure("patient gender invalid");
    }

    if (!Patient_is_valid_age(patient->age)) {
        return Result_make_failure("patient age invalid");
    }

    result = PatientService_validate_contact(patient->contact);
    if (result.success == 0) {
        return result;
    }

    result = PatientService_validate_id_card(patient->id_card);
    if (result.success == 0) {
        return result;
    }

    if (!RepositoryUtils_is_safe_field_text(patient->allergy) ||
        !RepositoryUtils_is_safe_field_text(patient->medical_history) ||
        !RepositoryUtils_is_safe_field_text(patient->remarks)) {
        return Result_make_failure("patient field contains reserved characters");
    }

    if (patient->is_inpatient != 0 && patient->is_inpatient != 1) {
        return Result_make_failure("patient inpatient flag invalid");
    }

    return Result_make_success("patient valid");
}

static int PatientService_matches_patient_id(const void *data, const void *context) {
    const Patient *patient = (const Patient *)data;
    const char *patient_id = (const char *)context;

    return strcmp(patient->patient_id, patient_id) == 0;
}

static Result PatientService_load_patients(PatientService *service, LinkedList *patients) {
    if (service == 0 || patients == 0) {
        return Result_make_failure("patient service arguments missing");
    }

    return PatientRepository_load_all(&service->repository, patients);
}

static Result PatientService_append_match(LinkedList *out_patients, const Patient *patient) {
    Patient *copy = (Patient *)malloc(sizeof(Patient));

    if (copy == 0) {
        return Result_make_failure("failed to allocate patient match");
    }

    *copy = *patient;
    if (!LinkedList_append(out_patients, copy)) {
        free(copy);
        return Result_make_failure("failed to append patient match");
    }

    return Result_make_success("patient match appended");
}

static Result PatientService_ensure_unique_identity_fields(
    const LinkedList *patients,
    const Patient *candidate,
    const char *ignore_patient_id
) {
    const LinkedListNode *current = 0;

    current = patients->head;
    while (current != 0) {
        const Patient *stored_patient = (const Patient *)current->data;

        if (ignore_patient_id != 0 && strcmp(stored_patient->patient_id, ignore_patient_id) == 0) {
            current = current->next;
            continue;
        }

        if (strcmp(stored_patient->patient_id, candidate->patient_id) == 0) {
            return Result_make_failure("patient id already exists");
        }

        if (strcmp(stored_patient->contact, candidate->contact) == 0) {
            return Result_make_failure("patient contact already exists");
        }

        if (strcmp(stored_patient->id_card, candidate->id_card) == 0) {
            return Result_make_failure("patient id card already exists");
        }

        current = current->next;
    }

    return Result_make_success("patient identity unique");
}

static Result PatientService_remove_patient_from_list(
    LinkedList *patients,
    const char *patient_id
) {
    LinkedListNode *current = 0;
    LinkedListNode *previous = 0;

    current = patients->head;
    while (current != 0) {
        Patient *patient = (Patient *)current->data;

        if (strcmp(patient->patient_id, patient_id) == 0) {
            if (previous == 0) {
                patients->head = current->next;
            } else {
                previous->next = current->next;
            }

            if (patients->tail == current) {
                patients->tail = previous;
            }

            free(current->data);
            free(current);
            patients->count--;
            return Result_make_success("patient deleted");
        }

        previous = current;
        current = current->next;
    }

    return Result_make_failure("patient not found");
}

static Result PatientService_find_patient_in_loaded_list(
    const LinkedList *patients,
    const char *value,
    int field_type,
    Patient *out_patient
) {
    const LinkedListNode *current = 0;
    int matched_count = 0;

    current = patients->head;
    while (current != 0) {
        const Patient *patient = (const Patient *)current->data;
        const char *field_value = field_type == PATIENT_SERVICE_MATCH_CONTACT
            ? patient->contact
            : patient->id_card;

        if (strcmp(field_value, value) == 0) {
            *out_patient = *patient;
            matched_count++;
        }

        current = current->next;
    }

    if (matched_count == 0) {
        return Result_make_failure(
            field_type == PATIENT_SERVICE_MATCH_CONTACT
                ? "patient contact not found"
                : "patient id card not found"
        );
    }

    if (matched_count > 1) {
        return Result_make_failure(
            field_type == PATIENT_SERVICE_MATCH_CONTACT
                ? "duplicate patient contact found"
                : "duplicate patient id card found"
        );
    }

    return Result_make_success("patient found");
}

Result PatientService_init(PatientService *service, const char *path) {
    if (service == 0) {
        return Result_make_failure("patient service missing");
    }

    return PatientRepository_init(&service->repository, path);
}

Result PatientService_create_patient(PatientService *service, const Patient *patient) {
    LinkedList patients;
    Result result = PatientService_validate_patient(patient);

    if (result.success == 0) {
        return result;
    }

    result = PatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    result = PatientService_ensure_unique_identity_fields(&patients, patient, 0);
    PatientRepository_clear_loaded_list(&patients);
    if (result.success == 0) {
        return result;
    }

    return PatientRepository_append(&service->repository, patient);
}

Result PatientService_update_patient(PatientService *service, const Patient *patient) {
    LinkedList patients;
    Patient *stored_patient = 0;
    Result result = PatientService_validate_patient(patient);

    if (result.success == 0) {
        return result;
    }

    result = PatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    stored_patient = (Patient *)LinkedList_find_first(
        &patients,
        PatientService_matches_patient_id,
        patient->patient_id
    );
    if (stored_patient == 0) {
        PatientRepository_clear_loaded_list(&patients);
        return Result_make_failure("patient not found");
    }

    result = PatientService_ensure_unique_identity_fields(&patients, patient, patient->patient_id);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        return result;
    }

    *stored_patient = *patient;
    result = PatientRepository_save_all(&service->repository, &patients);
    PatientRepository_clear_loaded_list(&patients);
    return result;
}

Result PatientService_delete_patient(PatientService *service, const char *patient_id) {
    LinkedList patients;
    Result result = PatientService_validate_required_text(patient_id, "patient id");

    if (result.success == 0) {
        return result;
    }

    result = PatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    result = PatientService_remove_patient_from_list(&patients, patient_id);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        return result;
    }

    result = PatientRepository_save_all(&service->repository, &patients);
    PatientRepository_clear_loaded_list(&patients);
    return result;
}

Result PatientService_find_patient_by_id(
    PatientService *service,
    const char *patient_id,
    Patient *out_patient
) {
    Result result = PatientService_validate_required_text(patient_id, "patient id");

    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_patient == 0) {
        return Result_make_failure("patient service arguments missing");
    }

    return PatientRepository_find_by_id(&service->repository, patient_id, out_patient);
}

Result PatientService_find_patients_by_name(
    PatientService *service,
    const char *name,
    LinkedList *out_patients
) {
    LinkedList patients;
    const LinkedListNode *current = 0;
    Result result = PatientService_validate_required_text(name, "patient name");

    if (result.success == 0) {
        return result;
    }

    if (out_patients == 0) {
        return Result_make_failure("patient search results missing");
    }

    LinkedList_init(out_patients);
    result = PatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    current = patients.head;
    while (current != 0) {
        const Patient *patient = (const Patient *)current->data;

        if (strcmp(patient->name, name) == 0) {
            result = PatientService_append_match(out_patients, patient);
            if (result.success == 0) {
                PatientRepository_clear_loaded_list(&patients);
                PatientService_clear_search_results(out_patients);
                return result;
            }
        }

        current = current->next;
    }

    PatientRepository_clear_loaded_list(&patients);
    return Result_make_success("patient name search complete");
}

Result PatientService_find_patient_by_contact(
    PatientService *service,
    const char *contact,
    Patient *out_patient
) {
    LinkedList patients;
    Result result = PatientService_validate_contact(contact);

    if (result.success == 0) {
        return result;
    }

    if (out_patient == 0) {
        return Result_make_failure("patient output missing");
    }

    result = PatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    result = PatientService_find_patient_in_loaded_list(
        &patients,
        contact,
        PATIENT_SERVICE_MATCH_CONTACT,
        out_patient
    );
    PatientRepository_clear_loaded_list(&patients);
    return result;
}

Result PatientService_find_patient_by_id_card(
    PatientService *service,
    const char *id_card,
    Patient *out_patient
) {
    LinkedList patients;
    Result result = PatientService_validate_id_card(id_card);

    if (result.success == 0) {
        return result;
    }

    if (out_patient == 0) {
        return Result_make_failure("patient output missing");
    }

    result = PatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    result = PatientService_find_patient_in_loaded_list(
        &patients,
        id_card,
        PATIENT_SERVICE_MATCH_ID_CARD,
        out_patient
    );
    PatientRepository_clear_loaded_list(&patients);
    return result;
}

void PatientService_clear_search_results(LinkedList *patients) {
    LinkedList_clear(patients, free);
}
