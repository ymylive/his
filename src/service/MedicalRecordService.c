#include "service/MedicalRecordService.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/IdGenerator.h"
#include "repository/RepositoryUtils.h"

#define MEDICAL_RECORD_VISIT_ID_PREFIX "VIS"
#define MEDICAL_RECORD_VISIT_ID_WIDTH 4
#define MEDICAL_RECORD_EXAM_ID_PREFIX "EXM"
#define MEDICAL_RECORD_EXAM_ID_WIDTH 4

static int MedicalRecordService_is_blank_text(const char *text) {
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

static void MedicalRecordService_copy_text(
    char *destination,
    size_t capacity,
    const char *source
) {
    if (destination == 0 || capacity == 0) {
        return;
    }

    if (source == 0) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

static Result MedicalRecordService_validate_text(
    const char *text,
    const char *field_name,
    int allow_empty
) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (!allow_empty && MedicalRecordService_is_blank_text(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    if (text != 0 && !RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("medical record text valid");
}

static Result MedicalRecordService_validate_flags(
    int need_exam,
    int need_admission,
    int need_medicine
) {
    if ((need_exam != 0 && need_exam != 1) ||
        (need_admission != 0 && need_admission != 1) ||
        (need_medicine != 0 && need_medicine != 1)) {
        return Result_make_failure("visit flags invalid");
    }

    return Result_make_success("visit flags valid");
}

static int MedicalRecordService_is_in_time_range(
    const char *value,
    const char *time_from,
    const char *time_to
) {
    if (MedicalRecordService_is_blank_text(value)) {
        return 0;
    }

    if (!MedicalRecordService_is_blank_text(time_from) && strcmp(value, time_from) < 0) {
        return 0;
    }

    if (!MedicalRecordService_is_blank_text(time_to) && strcmp(value, time_to) > 0) {
        return 0;
    }

    return 1;
}

static Result MedicalRecordService_load_registrations(
    MedicalRecordService *service,
    LinkedList *out_registrations
) {
    LinkedList_init(out_registrations);
    return RegistrationRepository_load_all(&service->registration_repository, out_registrations);
}

static Result MedicalRecordService_load_visits(
    MedicalRecordService *service,
    LinkedList *out_visits
) {
    LinkedList_init(out_visits);
    return VisitRecordRepository_load_all(&service->visit_repository, out_visits);
}

static Result MedicalRecordService_load_examinations(
    MedicalRecordService *service,
    LinkedList *out_examinations
) {
    LinkedList_init(out_examinations);
    return ExaminationRecordRepository_load_all(
        &service->examination_repository,
        out_examinations
    );
}

static Result MedicalRecordService_load_admissions(
    MedicalRecordService *service,
    LinkedList *out_admissions
) {
    LinkedList_init(out_admissions);
    return AdmissionRepository_load_all(&service->admission_repository, out_admissions);
}

static Registration *MedicalRecordService_find_registration(
    LinkedList *registrations,
    const char *registration_id
) {
    LinkedListNode *current = 0;

    if (registrations == 0 || registration_id == 0) {
        return 0;
    }

    current = registrations->head;
    while (current != 0) {
        Registration *registration = (Registration *)current->data;

        if (strcmp(registration->registration_id, registration_id) == 0) {
            return registration;
        }

        current = current->next;
    }

    return 0;
}

static VisitRecord *MedicalRecordService_find_visit(
    LinkedList *visits,
    const char *visit_id
) {
    LinkedListNode *current = 0;

    if (visits == 0 || visit_id == 0) {
        return 0;
    }

    current = visits->head;
    while (current != 0) {
        VisitRecord *record = (VisitRecord *)current->data;

        if (strcmp(record->visit_id, visit_id) == 0) {
            return record;
        }

        current = current->next;
    }

    return 0;
}

static VisitRecord *MedicalRecordService_find_visit_by_registration(
    LinkedList *visits,
    const char *registration_id
) {
    LinkedListNode *current = 0;

    if (visits == 0 || registration_id == 0) {
        return 0;
    }

    current = visits->head;
    while (current != 0) {
        VisitRecord *record = (VisitRecord *)current->data;

        if (strcmp(record->registration_id, registration_id) == 0) {
            return record;
        }

        current = current->next;
    }

    return 0;
}

static ExaminationRecord *MedicalRecordService_find_examination(
    LinkedList *examinations,
    const char *examination_id
) {
    LinkedListNode *current = 0;

    if (examinations == 0 || examination_id == 0) {
        return 0;
    }

    current = examinations->head;
    while (current != 0) {
        ExaminationRecord *record = (ExaminationRecord *)current->data;

        if (strcmp(record->examination_id, examination_id) == 0) {
            return record;
        }

        current = current->next;
    }

    return 0;
}

static Result MedicalRecordService_append_registration_copy(
    LinkedList *target,
    const Registration *registration
) {
    Registration *copy = (Registration *)malloc(sizeof(*copy));

    if (copy == 0) {
        return Result_make_failure("failed to allocate registration history");
    }

    *copy = *registration;
    if (!LinkedList_append(target, copy)) {
        free(copy);
        return Result_make_failure("failed to append registration history");
    }

    return Result_make_success("registration appended");
}

static Result MedicalRecordService_append_visit_copy(
    LinkedList *target,
    const VisitRecord *record
) {
    VisitRecord *copy = (VisitRecord *)malloc(sizeof(*copy));

    if (copy == 0) {
        return Result_make_failure("failed to allocate visit history");
    }

    *copy = *record;
    if (!LinkedList_append(target, copy)) {
        free(copy);
        return Result_make_failure("failed to append visit history");
    }

    return Result_make_success("visit appended");
}

static Result MedicalRecordService_append_examination_copy(
    LinkedList *target,
    const ExaminationRecord *record
) {
    ExaminationRecord *copy = (ExaminationRecord *)malloc(sizeof(*copy));

    if (copy == 0) {
        return Result_make_failure("failed to allocate exam history");
    }

    *copy = *record;
    if (!LinkedList_append(target, copy)) {
        free(copy);
        return Result_make_failure("failed to append exam history");
    }

    return Result_make_success("exam appended");
}

static Result MedicalRecordService_append_admission_copy(
    LinkedList *target,
    const Admission *admission
) {
    Admission *copy = (Admission *)malloc(sizeof(*copy));

    if (copy == 0) {
        return Result_make_failure("failed to allocate admission history");
    }

    *copy = *admission;
    if (!LinkedList_append(target, copy)) {
        free(copy);
        return Result_make_failure("failed to append admission history");
    }

    return Result_make_success("admission appended");
}

static void *MedicalRecordService_remove_node(
    LinkedList *list,
    LinkedListNode *previous,
    LinkedListNode *current
) {
    void *data = 0;

    if (list == 0 || current == 0) {
        return 0;
    }

    data = current->data;
    if (previous == 0) {
        list->head = current->next;
    } else {
        previous->next = current->next;
    }

    if (list->tail == current) {
        list->tail = previous;
    }

    if (list->count > 0) {
        list->count--;
    }

    free(current);
    return data;
}

static Result MedicalRecordService_next_visit_sequence_from_list(
    const LinkedList *visits,
    int *out_sequence
) {
    LinkedListNode *current = 0;
    int max_sequence = 0;

    if (out_sequence == 0) {
        return Result_make_failure("visit sequence output missing");
    }

    current = visits->head;
    while (current != 0) {
        const VisitRecord *record = (const VisitRecord *)current->data;
        const char *suffix = record->visit_id + strlen(MEDICAL_RECORD_VISIT_ID_PREFIX);
        char *end_pointer = 0;
        long value = 0;

        if (strncmp(
                record->visit_id,
                MEDICAL_RECORD_VISIT_ID_PREFIX,
                strlen(MEDICAL_RECORD_VISIT_ID_PREFIX)
            ) != 0) {
            return Result_make_failure("visit id format invalid");
        }

        errno = 0;
        value = strtol(suffix, &end_pointer, 10);
        if (errno != 0 || end_pointer == suffix || end_pointer == 0 || *end_pointer != '\0') {
            return Result_make_failure("visit id format invalid");
        }

        if ((int)value > max_sequence) {
            max_sequence = (int)value;
        }

        current = current->next;
    }

    *out_sequence = max_sequence + 1;
    return Result_make_success("visit sequence ready");
}

static Result MedicalRecordService_next_visit_sequence(
    MedicalRecordService *service,
    int *out_sequence
) {
    LinkedList visits;
    LinkedListNode *current = 0;
    int max_sequence = 0;
    Result result;

    if (out_sequence == 0) {
        return Result_make_failure("visit sequence output missing");
    }

    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        return result;
    }

    current = visits.head;
    while (current != 0) {
        const VisitRecord *record = (const VisitRecord *)current->data;
        const char *suffix = record->visit_id + strlen(MEDICAL_RECORD_VISIT_ID_PREFIX);
        char *end_pointer = 0;
        long value = 0;

        if (strncmp(
                record->visit_id,
                MEDICAL_RECORD_VISIT_ID_PREFIX,
                strlen(MEDICAL_RECORD_VISIT_ID_PREFIX)
            ) != 0) {
            VisitRecordRepository_clear_list(&visits);
            return Result_make_failure("visit id format invalid");
        }

        errno = 0;
        value = strtol(suffix, &end_pointer, 10);
        if (errno != 0 || end_pointer == suffix || end_pointer == 0 || *end_pointer != '\0') {
            VisitRecordRepository_clear_list(&visits);
            return Result_make_failure("visit id format invalid");
        }

        if ((int)value > max_sequence) {
            max_sequence = (int)value;
        }

        current = current->next;
    }

    VisitRecordRepository_clear_list(&visits);
    *out_sequence = max_sequence + 1;
    return Result_make_success("visit sequence ready");
}

static Result MedicalRecordService_next_examination_sequence_from_list(
    const LinkedList *examinations,
    int *out_sequence
) {
    LinkedListNode *current = 0;
    int max_sequence = 0;

    if (out_sequence == 0) {
        return Result_make_failure("exam sequence output missing");
    }

    current = examinations->head;
    while (current != 0) {
        const ExaminationRecord *record = (const ExaminationRecord *)current->data;
        const char *suffix = record->examination_id + strlen(MEDICAL_RECORD_EXAM_ID_PREFIX);
        char *end_pointer = 0;
        long value = 0;

        if (strncmp(
                record->examination_id,
                MEDICAL_RECORD_EXAM_ID_PREFIX,
                strlen(MEDICAL_RECORD_EXAM_ID_PREFIX)
            ) != 0) {
            return Result_make_failure("exam id format invalid");
        }

        errno = 0;
        value = strtol(suffix, &end_pointer, 10);
        if (errno != 0 || end_pointer == suffix || end_pointer == 0 || *end_pointer != '\0') {
            return Result_make_failure("exam id format invalid");
        }

        if ((int)value > max_sequence) {
            max_sequence = (int)value;
        }

        current = current->next;
    }

    *out_sequence = max_sequence + 1;
    return Result_make_success("exam sequence ready");
}

static Result MedicalRecordService_next_examination_sequence(
    MedicalRecordService *service,
    int *out_sequence
) {
    LinkedList examinations;
    LinkedListNode *current = 0;
    int max_sequence = 0;
    Result result;

    if (out_sequence == 0) {
        return Result_make_failure("exam sequence output missing");
    }

    result = MedicalRecordService_load_examinations(service, &examinations);
    if (result.success == 0) {
        return result;
    }

    current = examinations.head;
    while (current != 0) {
        const ExaminationRecord *record = (const ExaminationRecord *)current->data;
        const char *suffix = record->examination_id + strlen(MEDICAL_RECORD_EXAM_ID_PREFIX);
        char *end_pointer = 0;
        long value = 0;

        if (strncmp(
                record->examination_id,
                MEDICAL_RECORD_EXAM_ID_PREFIX,
                strlen(MEDICAL_RECORD_EXAM_ID_PREFIX)
            ) != 0) {
            ExaminationRecordRepository_clear_list(&examinations);
            return Result_make_failure("exam id format invalid");
        }

        errno = 0;
        value = strtol(suffix, &end_pointer, 10);
        if (errno != 0 || end_pointer == suffix || end_pointer == 0 || *end_pointer != '\0') {
            ExaminationRecordRepository_clear_list(&examinations);
            return Result_make_failure("exam id format invalid");
        }

        if ((int)value > max_sequence) {
            max_sequence = (int)value;
        }

        current = current->next;
    }

    ExaminationRecordRepository_clear_list(&examinations);
    *out_sequence = max_sequence + 1;
    return Result_make_success("exam sequence ready");
}

static void MedicalRecordHistory_init(MedicalRecordHistory *history) {
    LinkedList_init(&history->registrations);
    LinkedList_init(&history->visits);
    LinkedList_init(&history->examinations);
    LinkedList_init(&history->admissions);
}

Result MedicalRecordService_init(
    MedicalRecordService *service,
    const char *registration_path,
    const char *visit_path,
    const char *examination_path,
    const char *admission_path
) {
    LinkedList admissions;
    Result result;

    if (service == 0) {
        return Result_make_failure("medical record service missing");
    }

    result = RegistrationRepository_init(&service->registration_repository, registration_path);
    if (result.success == 0) {
        return result;
    }

    result = VisitRecordRepository_init(&service->visit_repository, visit_path);
    if (result.success == 0) {
        return result;
    }

    result = ExaminationRecordRepository_init(
        &service->examination_repository,
        examination_path
    );
    if (result.success == 0) {
        return result;
    }

    result = AdmissionRepository_init(&service->admission_repository, admission_path);
    if (result.success == 0) {
        return result;
    }

    result = RegistrationRepository_ensure_storage(&service->registration_repository);
    if (result.success == 0) {
        return result;
    }

    result = VisitRecordRepository_ensure_storage(&service->visit_repository);
    if (result.success == 0) {
        return result;
    }

    result = ExaminationRecordRepository_ensure_storage(&service->examination_repository);
    if (result.success == 0) {
        return result;
    }

    LinkedList_init(&admissions);
    result = AdmissionRepository_load_all(&service->admission_repository, &admissions);
    if (result.success == 0) {
        return result;
    }

    AdmissionRepository_clear_loaded_list(&admissions);
    return Result_make_success("medical record service ready");
}

void MedicalRecordHistory_clear(MedicalRecordHistory *history) {
    if (history == 0) {
        return;
    }

    RegistrationRepository_clear_list(&history->registrations);
    VisitRecordRepository_clear_list(&history->visits);
    ExaminationRecordRepository_clear_list(&history->examinations);
    AdmissionRepository_clear_loaded_list(&history->admissions);
}

static Result MedicalRecordService_build_visit_record(
    VisitRecord *record,
    const char *visit_id,
    const Registration *registration,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time
) {
    Result result;

    result = MedicalRecordService_validate_text(chief_complaint, "chief complaint", 1);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(diagnosis, "diagnosis", 1);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(advice, "advice", 1);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(visit_time, "visit time", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_flags(need_exam, need_admission, need_medicine);
    if (result.success == 0) {
        return result;
    }

    memset(record, 0, sizeof(*record));
    MedicalRecordService_copy_text(record->visit_id, sizeof(record->visit_id), visit_id);
    MedicalRecordService_copy_text(
        record->registration_id,
        sizeof(record->registration_id),
        registration->registration_id
    );
    MedicalRecordService_copy_text(record->patient_id, sizeof(record->patient_id), registration->patient_id);
    MedicalRecordService_copy_text(record->doctor_id, sizeof(record->doctor_id), registration->doctor_id);
    MedicalRecordService_copy_text(
        record->department_id,
        sizeof(record->department_id),
        registration->department_id
    );
    MedicalRecordService_copy_text(
        record->chief_complaint,
        sizeof(record->chief_complaint),
        chief_complaint
    );
    MedicalRecordService_copy_text(record->diagnosis, sizeof(record->diagnosis), diagnosis);
    MedicalRecordService_copy_text(record->advice, sizeof(record->advice), advice);
    record->need_exam = need_exam;
    record->need_admission = need_admission;
    record->need_medicine = need_medicine;
    MedicalRecordService_copy_text(record->visit_time, sizeof(record->visit_time), visit_time);
    return Result_make_success("visit record built");
}

Result MedicalRecordService_create_visit_record(
    MedicalRecordService *service,
    const char *registration_id,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time,
    VisitRecord *out_record
) {
    LinkedList registrations;
    LinkedList visits;
    Registration *registration = 0;
    VisitRecord *existing = 0;
    VisitRecord *created = 0;
    VisitRecord new_record;
    char generated_visit_id[HIS_DOMAIN_ID_CAPACITY];
    int next_sequence = 0;
    Result result;

    if (service == 0 || out_record == 0) {
        return Result_make_failure("visit create arguments invalid");
    }

    result = MedicalRecordService_validate_text(registration_id, "registration id", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_load_registrations(service, &registrations);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    registration = MedicalRecordService_find_registration(&registrations, registration_id);
    if (registration == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("registration not found");
    }

    if (registration->status == REG_STATUS_CANCELLED) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("registration cancelled");
    }

    existing = MedicalRecordService_find_visit_by_registration(&visits, registration_id);
    if (existing != 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("registration already has visit record");
    }

    result = MedicalRecordService_next_visit_sequence_from_list(&visits, &next_sequence);
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    if (!IdGenerator_format(
            generated_visit_id,
            sizeof(generated_visit_id),
            MEDICAL_RECORD_VISIT_ID_PREFIX,
            next_sequence,
            MEDICAL_RECORD_VISIT_ID_WIDTH)) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("failed to generate visit id");
    }

    result = MedicalRecordService_build_visit_record(
        &new_record,
        generated_visit_id,
        registration,
        chief_complaint,
        diagnosis,
        advice,
        need_exam,
        need_admission,
        need_medicine,
        visit_time
    );
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    created = (VisitRecord *)malloc(sizeof(*created));
    if (created == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("failed to allocate visit record");
    }

    *created = new_record;
    if (!LinkedList_append(&visits, created)) {
        free(created);
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("failed to append visit record");
    }

    if (registration->status == REG_STATUS_PENDING &&
        !Registration_mark_diagnosed(registration, visit_time)) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("failed to mark registration diagnosed");
    }

    result = VisitRecordRepository_save_all(&service->visit_repository, &visits);
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    result = RegistrationRepository_save_all(&service->registration_repository, &registrations);
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    *out_record = new_record;
    VisitRecordRepository_clear_list(&visits);
    RegistrationRepository_clear_list(&registrations);
    return Result_make_success("visit record created");
}

Result MedicalRecordService_update_visit_record(
    MedicalRecordService *service,
    const char *visit_id,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time,
    VisitRecord *out_record
) {
    LinkedList registrations;
    LinkedList visits;
    VisitRecord *existing = 0;
    Registration *registration = 0;
    VisitRecord updated_record;
    Result result;

    if (service == 0 || out_record == 0) {
        return Result_make_failure("visit update arguments invalid");
    }

    result = MedicalRecordService_validate_text(visit_id, "visit id", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_load_registrations(service, &registrations);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    existing = MedicalRecordService_find_visit(&visits, visit_id);
    if (existing == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("visit not found");
    }

    registration = MedicalRecordService_find_registration(&registrations, existing->registration_id);
    if (registration == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("registration not found");
    }

    result = MedicalRecordService_build_visit_record(
        &updated_record,
        visit_id,
        registration,
        chief_complaint,
        diagnosis,
        advice,
        need_exam,
        need_admission,
        need_medicine,
        visit_time
    );
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    *existing = updated_record;
    result = VisitRecordRepository_save_all(&service->visit_repository, &visits);
    VisitRecordRepository_clear_list(&visits);
    RegistrationRepository_clear_list(&registrations);
    if (result.success == 0) {
        return result;
    }

    *out_record = updated_record;
    return Result_make_success("visit record updated");
}

Result MedicalRecordService_delete_visit_record(
    MedicalRecordService *service,
    const char *visit_id
) {
    LinkedList registrations;
    LinkedList visits;
    LinkedList examinations;
    LinkedListNode *previous = 0;
    LinkedListNode *current = 0;
    VisitRecord *target = 0;
    Registration *registration = 0;
    Result result;

    if (service == 0) {
        return Result_make_failure("visit delete arguments invalid");
    }

    result = MedicalRecordService_validate_text(visit_id, "visit id", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_load_registrations(service, &registrations);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    current = visits.head;
    while (current != 0) {
        VisitRecord *record = (VisitRecord *)current->data;

        if (strcmp(record->visit_id, visit_id) == 0) {
            target = record;
            break;
        }

        previous = current;
        current = current->next;
    }

    if (target == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("visit not found");
    }

    LinkedList_init(&examinations);
    result = ExaminationRecordRepository_find_by_visit_id(
        &service->examination_repository,
        visit_id,
        &examinations
    );
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    if (LinkedList_count(&examinations) != 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return Result_make_failure("visit has examination records");
    }

    ExaminationRecordRepository_clear_list(&examinations);
    registration = MedicalRecordService_find_registration(&registrations, target->registration_id);
    if (registration != 0) {
        registration->status = REG_STATUS_PENDING;
        registration->diagnosed_at[0] = '\0';
        registration->cancelled_at[0] = '\0';
    }

    free(MedicalRecordService_remove_node(&visits, previous, current));
    result = VisitRecordRepository_save_all(&service->visit_repository, &visits);
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    result = RegistrationRepository_save_all(&service->registration_repository, &registrations);
    VisitRecordRepository_clear_list(&visits);
    RegistrationRepository_clear_list(&registrations);
    if (result.success == 0) {
        return result;
    }

    return Result_make_success("visit record deleted");
}

Result MedicalRecordService_create_examination_record(
    MedicalRecordService *service,
    const char *visit_id,
    const char *exam_item,
    const char *exam_type,
    const char *requested_at,
    ExaminationRecord *out_record
) {
    LinkedList visits;
    LinkedList examinations;
    VisitRecord *visit = 0;
    ExaminationRecord *created = 0;
    ExaminationRecord new_record;
    int next_sequence = 0;
    Result result;

    if (service == 0 || out_record == 0) {
        return Result_make_failure("exam create arguments invalid");
    }

    result = MedicalRecordService_validate_text(visit_id, "visit id", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(exam_item, "exam item", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(exam_type, "exam type", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(requested_at, "requested at", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_load_examinations(service, &examinations);
    if (result.success == 0) {
        VisitRecordRepository_clear_list(&visits);
        return result;
    }

    visit = MedicalRecordService_find_visit(&visits, visit_id);
    if (visit == 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        VisitRecordRepository_clear_list(&visits);
        return Result_make_failure("visit not found");
    }

    result = MedicalRecordService_next_examination_sequence_from_list(&examinations, &next_sequence);
    if (result.success == 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        VisitRecordRepository_clear_list(&visits);
        return result;
    }

    memset(&new_record, 0, sizeof(new_record));
    if (!IdGenerator_format(
            new_record.examination_id,
            sizeof(new_record.examination_id),
            MEDICAL_RECORD_EXAM_ID_PREFIX,
            next_sequence,
            MEDICAL_RECORD_EXAM_ID_WIDTH)) {
        ExaminationRecordRepository_clear_list(&examinations);
        VisitRecordRepository_clear_list(&visits);
        return Result_make_failure("failed to generate exam id");
    }

    MedicalRecordService_copy_text(new_record.visit_id, sizeof(new_record.visit_id), visit_id);
    MedicalRecordService_copy_text(new_record.patient_id, sizeof(new_record.patient_id), visit->patient_id);
    MedicalRecordService_copy_text(new_record.doctor_id, sizeof(new_record.doctor_id), visit->doctor_id);
    MedicalRecordService_copy_text(new_record.exam_item, sizeof(new_record.exam_item), exam_item);
    MedicalRecordService_copy_text(new_record.exam_type, sizeof(new_record.exam_type), exam_type);
    new_record.status = EXAM_STATUS_PENDING;
    new_record.result[0] = '\0';
    MedicalRecordService_copy_text(
        new_record.requested_at,
        sizeof(new_record.requested_at),
        requested_at
    );
    new_record.completed_at[0] = '\0';

    created = (ExaminationRecord *)malloc(sizeof(*created));
    if (created == 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        VisitRecordRepository_clear_list(&visits);
        return Result_make_failure("failed to allocate exam record");
    }

    *created = new_record;
    if (!LinkedList_append(&examinations, created)) {
        free(created);
        ExaminationRecordRepository_clear_list(&examinations);
        VisitRecordRepository_clear_list(&visits);
        return Result_make_failure("failed to append exam record");
    }

    result = ExaminationRecordRepository_save_all(&service->examination_repository, &examinations);
    ExaminationRecordRepository_clear_list(&examinations);
    VisitRecordRepository_clear_list(&visits);
    if (result.success == 0) {
        return result;
    }

    *out_record = new_record;
    return Result_make_success("exam record created");
}

Result MedicalRecordService_update_examination_record(
    MedicalRecordService *service,
    const char *examination_id,
    ExaminationStatus status,
    const char *result_text,
    const char *completed_at,
    ExaminationRecord *out_record
) {
    LinkedList examinations;
    ExaminationRecord *existing = 0;
    Result result;

    if (service == 0 || out_record == 0) {
        return Result_make_failure("exam update arguments invalid");
    }

    result = MedicalRecordService_validate_text(examination_id, "examination id", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(result_text, "exam result", 1);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_validate_text(completed_at, "completed at", 1);
    if (result.success == 0) {
        return result;
    }

    if (status != EXAM_STATUS_PENDING && status != EXAM_STATUS_COMPLETED) {
        return Result_make_failure("exam status invalid");
    }

    if (status == EXAM_STATUS_PENDING && !MedicalRecordService_is_blank_text(completed_at)) {
        return Result_make_failure("pending exam cannot have completed_at");
    }

    if (status == EXAM_STATUS_COMPLETED && MedicalRecordService_is_blank_text(completed_at)) {
        return Result_make_failure("completed exam missing completed_at");
    }

    if (status == EXAM_STATUS_COMPLETED && MedicalRecordService_is_blank_text(result_text)) {
        return Result_make_failure("completed exam missing result");
    }

    result = MedicalRecordService_load_examinations(service, &examinations);
    if (result.success == 0) {
        return result;
    }

    existing = MedicalRecordService_find_examination(&examinations, examination_id);
    if (existing == 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        return Result_make_failure("exam not found");
    }

    existing->status = status;
    MedicalRecordService_copy_text(existing->result, sizeof(existing->result), result_text);
    MedicalRecordService_copy_text(
        existing->completed_at,
        sizeof(existing->completed_at),
        status == EXAM_STATUS_COMPLETED ? completed_at : ""
    );

    result = ExaminationRecordRepository_save_all(&service->examination_repository, &examinations);
    if (result.success == 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        return result;
    }

    *out_record = *existing;
    ExaminationRecordRepository_clear_list(&examinations);
    return Result_make_success("exam record updated");
}

Result MedicalRecordService_delete_examination_record(
    MedicalRecordService *service,
    const char *examination_id
) {
    LinkedList examinations;
    LinkedListNode *previous = 0;
    LinkedListNode *current = 0;
    Result result;

    if (service == 0) {
        return Result_make_failure("exam delete arguments invalid");
    }

    result = MedicalRecordService_validate_text(examination_id, "examination id", 0);
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_load_examinations(service, &examinations);
    if (result.success == 0) {
        return result;
    }

    current = examinations.head;
    while (current != 0) {
        ExaminationRecord *record = (ExaminationRecord *)current->data;

        if (strcmp(record->examination_id, examination_id) == 0) {
            break;
        }

        previous = current;
        current = current->next;
    }

    if (current == 0) {
        ExaminationRecordRepository_clear_list(&examinations);
        return Result_make_failure("exam not found");
    }

    free(MedicalRecordService_remove_node(&examinations, previous, current));
    result = ExaminationRecordRepository_save_all(&service->examination_repository, &examinations);
    ExaminationRecordRepository_clear_list(&examinations);
    if (result.success == 0) {
        return result;
    }

    return Result_make_success("exam record deleted");
}

Result MedicalRecordService_find_patient_history(
    MedicalRecordService *service,
    const char *patient_id,
    MedicalRecordHistory *out_history
) {
    RegistrationRepositoryFilter filter;
    LinkedList visits;
    LinkedList examinations;
    LinkedList admissions;
    LinkedListNode *current = 0;
    Result result;

    if (service == 0 || out_history == 0) {
        return Result_make_failure("patient history arguments invalid");
    }

    result = MedicalRecordService_validate_text(patient_id, "patient id", 0);
    if (result.success == 0) {
        return result;
    }

    memset(out_history, 0, sizeof(*out_history));
    MedicalRecordHistory_init(out_history);
    RegistrationRepositoryFilter_init(&filter);
    result = RegistrationRepository_find_by_patient_id(
        &service->registration_repository,
        patient_id,
        &filter,
        &out_history->registrations
    );
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    current = visits.head;
    while (current != 0) {
        const VisitRecord *record = (const VisitRecord *)current->data;

        if (strcmp(record->patient_id, patient_id) == 0) {
            result = MedicalRecordService_append_visit_copy(&out_history->visits, record);
            if (result.success == 0) {
                VisitRecordRepository_clear_list(&visits);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    VisitRecordRepository_clear_list(&visits);

    result = MedicalRecordService_load_examinations(service, &examinations);
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    current = examinations.head;
    while (current != 0) {
        const ExaminationRecord *record = (const ExaminationRecord *)current->data;

        if (strcmp(record->patient_id, patient_id) == 0) {
            result = MedicalRecordService_append_examination_copy(
                &out_history->examinations,
                record
            );
            if (result.success == 0) {
                ExaminationRecordRepository_clear_list(&examinations);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    ExaminationRecordRepository_clear_list(&examinations);

    result = MedicalRecordService_load_admissions(service, &admissions);
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    current = admissions.head;
    while (current != 0) {
        const Admission *admission = (const Admission *)current->data;

        if (strcmp(admission->patient_id, patient_id) == 0) {
            result = MedicalRecordService_append_admission_copy(&out_history->admissions, admission);
            if (result.success == 0) {
                AdmissionRepository_clear_loaded_list(&admissions);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    AdmissionRepository_clear_loaded_list(&admissions);

    return Result_make_success("patient history loaded");
}

Result MedicalRecordService_find_records_by_time_range(
    MedicalRecordService *service,
    const char *time_from,
    const char *time_to,
    MedicalRecordHistory *out_history
) {
    LinkedList registrations;
    LinkedList visits;
    LinkedList examinations;
    LinkedList admissions;
    LinkedListNode *current = 0;
    Result result;

    if (service == 0 || out_history == 0) {
        return Result_make_failure("time range query arguments invalid");
    }

    if (!MedicalRecordService_is_blank_text(time_from) &&
        !MedicalRecordService_is_blank_text(time_to) &&
        strcmp(time_from, time_to) > 0) {
        return Result_make_failure("time range invalid");
    }

    memset(out_history, 0, sizeof(*out_history));
    MedicalRecordHistory_init(out_history);

    result = MedicalRecordService_load_registrations(service, &registrations);
    if (result.success == 0) {
        return result;
    }

    current = registrations.head;
    while (current != 0) {
        const Registration *registration = (const Registration *)current->data;

        if (MedicalRecordService_is_in_time_range(
                registration->registered_at,
                time_from,
                time_to
            )) {
            result = MedicalRecordService_append_registration_copy(
                &out_history->registrations,
                registration
            );
            if (result.success == 0) {
                RegistrationRepository_clear_list(&registrations);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    RegistrationRepository_clear_list(&registrations);

    result = MedicalRecordService_load_visits(service, &visits);
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    current = visits.head;
    while (current != 0) {
        const VisitRecord *record = (const VisitRecord *)current->data;

        if (MedicalRecordService_is_in_time_range(record->visit_time, time_from, time_to)) {
            result = MedicalRecordService_append_visit_copy(&out_history->visits, record);
            if (result.success == 0) {
                VisitRecordRepository_clear_list(&visits);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    VisitRecordRepository_clear_list(&visits);

    result = MedicalRecordService_load_examinations(service, &examinations);
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    current = examinations.head;
    while (current != 0) {
        const ExaminationRecord *record = (const ExaminationRecord *)current->data;

        if (MedicalRecordService_is_in_time_range(record->requested_at, time_from, time_to)) {
            result = MedicalRecordService_append_examination_copy(
                &out_history->examinations,
                record
            );
            if (result.success == 0) {
                ExaminationRecordRepository_clear_list(&examinations);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    ExaminationRecordRepository_clear_list(&examinations);

    result = MedicalRecordService_load_admissions(service, &admissions);
    if (result.success == 0) {
        MedicalRecordHistory_clear(out_history);
        return result;
    }

    current = admissions.head;
    while (current != 0) {
        const Admission *admission = (const Admission *)current->data;

        if (MedicalRecordService_is_in_time_range(admission->admitted_at, time_from, time_to)) {
            result = MedicalRecordService_append_admission_copy(&out_history->admissions, admission);
            if (result.success == 0) {
                AdmissionRepository_clear_loaded_list(&admissions);
                MedicalRecordHistory_clear(out_history);
                return result;
            }
        }

        current = current->next;
    }
    AdmissionRepository_clear_loaded_list(&admissions);

    return Result_make_success("time range records loaded");
}
