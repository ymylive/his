#include "service/InpatientService.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/IdGenerator.h"
#include "repository/RepositoryUtils.h"

#define INPATIENT_SERVICE_ID_PREFIX "ADM"
#define INPATIENT_SERVICE_ID_WIDTH 4

static int InpatientService_is_blank_text(const char *text) {
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

static Result InpatientService_validate_required_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (InpatientService_is_blank_text(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("inpatient text valid");
}

static Result InpatientService_load_patients(
    InpatientService *service,
    LinkedList *out_patients
) {
    return PatientRepository_load_all(&service->patient_repository, out_patients);
}

static Result InpatientService_load_wards(InpatientService *service, LinkedList *out_wards) {
    return WardRepository_load_all(&service->ward_repository, out_wards);
}

static Result InpatientService_load_beds(InpatientService *service, LinkedList *out_beds) {
    return BedRepository_load_all(&service->bed_repository, out_beds);
}

static Result InpatientService_load_admissions(
    InpatientService *service,
    LinkedList *out_admissions
) {
    return AdmissionRepository_load_all(&service->admission_repository, out_admissions);
}

static Patient *InpatientService_find_patient(LinkedList *patients, const char *patient_id) {
    LinkedListNode *current = 0;

    if (patients == 0 || patient_id == 0) {
        return 0;
    }

    current = patients->head;
    while (current != 0) {
        Patient *patient = (Patient *)current->data;

        if (strcmp(patient->patient_id, patient_id) == 0) {
            return patient;
        }

        current = current->next;
    }

    return 0;
}

static Ward *InpatientService_find_ward(LinkedList *wards, const char *ward_id) {
    LinkedListNode *current = 0;

    if (wards == 0 || ward_id == 0) {
        return 0;
    }

    current = wards->head;
    while (current != 0) {
        Ward *ward = (Ward *)current->data;

        if (strcmp(ward->ward_id, ward_id) == 0) {
            return ward;
        }

        current = current->next;
    }

    return 0;
}

static Bed *InpatientService_find_bed(LinkedList *beds, const char *bed_id) {
    LinkedListNode *current = 0;

    if (beds == 0 || bed_id == 0) {
        return 0;
    }

    current = beds->head;
    while (current != 0) {
        Bed *bed = (Bed *)current->data;

        if (strcmp(bed->bed_id, bed_id) == 0) {
            return bed;
        }

        current = current->next;
    }

    return 0;
}

static Admission *InpatientService_find_admission(
    LinkedList *admissions,
    const char *admission_id
) {
    LinkedListNode *current = 0;

    if (admissions == 0 || admission_id == 0) {
        return 0;
    }

    current = admissions->head;
    while (current != 0) {
        Admission *admission = (Admission *)current->data;

        if (strcmp(admission->admission_id, admission_id) == 0) {
            return admission;
        }

        current = current->next;
    }

    return 0;
}

static int InpatientService_has_active_admission_for_patient(
    const LinkedList *admissions,
    const char *patient_id
) {
    const LinkedListNode *current = 0;

    if (admissions == 0 || patient_id == 0) {
        return 0;
    }

    current = admissions->head;
    while (current != 0) {
        const Admission *admission = (const Admission *)current->data;

        if (admission->status == ADMISSION_STATUS_ACTIVE &&
            strcmp(admission->patient_id, patient_id) == 0) {
            return 1;
        }

        current = current->next;
    }

    return 0;
}

static int InpatientService_has_active_admission_for_bed(
    const LinkedList *admissions,
    const char *bed_id
) {
    const LinkedListNode *current = 0;

    if (admissions == 0 || bed_id == 0) {
        return 0;
    }

    current = admissions->head;
    while (current != 0) {
        const Admission *admission = (const Admission *)current->data;

        if (admission->status == ADMISSION_STATUS_ACTIVE &&
            strcmp(admission->bed_id, bed_id) == 0) {
            return 1;
        }

        current = current->next;
    }

    return 0;
}

static Result InpatientService_next_admission_sequence(
    InpatientService *service,
    int *out_sequence
) {
    LinkedList admissions;
    LinkedListNode *current = 0;
    int max_sequence = 0;
    Result result;

    if (out_sequence == 0) {
        return Result_make_failure("admission sequence output missing");
    }

    result = InpatientService_load_admissions(service, &admissions);
    if (result.success == 0) {
        return result;
    }

    current = admissions.head;
    while (current != 0) {
        const Admission *admission = (const Admission *)current->data;
        const char *suffix = admission->admission_id + strlen(INPATIENT_SERVICE_ID_PREFIX);
        char *end_pointer = 0;
        long value = 0;

        if (strncmp(
                admission->admission_id,
                INPATIENT_SERVICE_ID_PREFIX,
                strlen(INPATIENT_SERVICE_ID_PREFIX)
            ) != 0) {
            AdmissionRepository_clear_loaded_list(&admissions);
            return Result_make_failure("admission id format invalid");
        }

        value = strtol(suffix, &end_pointer, 10);
        if (end_pointer == suffix || end_pointer == 0 || *end_pointer != '\0' || value < 0) {
            AdmissionRepository_clear_loaded_list(&admissions);
            return Result_make_failure("admission id format invalid");
        }

        if ((int)value > max_sequence) {
            max_sequence = (int)value;
        }

        current = current->next;
    }

    AdmissionRepository_clear_loaded_list(&admissions);
    *out_sequence = max_sequence + 1;
    return Result_make_success("admission sequence ready");
}

static Result InpatientService_save_all(
    InpatientService *service,
    LinkedList *patients,
    LinkedList *wards,
    LinkedList *beds,
    LinkedList *admissions
) {
    Result result;

    result = PatientRepository_save_all(&service->patient_repository, patients);
    if (result.success == 0) {
        return result;
    }

    result = WardRepository_save_all(&service->ward_repository, wards);
    if (result.success == 0) {
        return result;
    }

    result = BedRepository_save_all(&service->bed_repository, beds);
    if (result.success == 0) {
        return result;
    }

    return AdmissionRepository_save_all(&service->admission_repository, admissions);
}

static void InpatientService_clear_all(
    LinkedList *patients,
    LinkedList *wards,
    LinkedList *beds,
    LinkedList *admissions
) {
    PatientRepository_clear_loaded_list(patients);
    WardRepository_clear_loaded_list(wards);
    BedRepository_clear_loaded_list(beds);
    AdmissionRepository_clear_loaded_list(admissions);
}

Result InpatientService_init(
    InpatientService *service,
    const char *patient_path,
    const char *ward_path,
    const char *bed_path,
    const char *admission_path
) {
    Result result;

    if (service == 0) {
        return Result_make_failure("inpatient service missing");
    }

    result = PatientRepository_init(&service->patient_repository, patient_path);
    if (result.success == 0) {
        return result;
    }

    result = WardRepository_init(&service->ward_repository, ward_path);
    if (result.success == 0) {
        return result;
    }

    result = BedRepository_init(&service->bed_repository, bed_path);
    if (result.success == 0) {
        return result;
    }

    return AdmissionRepository_init(&service->admission_repository, admission_path);
}

Result InpatientService_admit_patient(
    InpatientService *service,
    const char *patient_id,
    const char *ward_id,
    const char *bed_id,
    const char *admitted_at,
    const char *summary,
    Admission *out_admission
) {
    LinkedList patients;
    LinkedList wards;
    LinkedList beds;
    LinkedList admissions;
    Patient *patient = 0;
    Ward *ward = 0;
    Bed *bed = 0;
    Admission *admission = 0;
    int next_sequence = 0;
    Result result = InpatientService_validate_required_text(patient_id, "patient id");

    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(ward_id, "ward id");
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(bed_id, "bed id");
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(admitted_at, "admitted at");
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(summary, "admission summary");
    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_admission == 0) {
        return Result_make_failure("inpatient service arguments missing");
    }

    result = InpatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_load_wards(service, &wards);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        return result;
    }

    result = InpatientService_load_beds(service, &beds);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        WardRepository_clear_loaded_list(&wards);
        return result;
    }

    result = InpatientService_load_admissions(service, &admissions);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        WardRepository_clear_loaded_list(&wards);
        BedRepository_clear_loaded_list(&beds);
        return result;
    }

    patient = InpatientService_find_patient(&patients, patient_id);
    ward = InpatientService_find_ward(&wards, ward_id);
    bed = InpatientService_find_bed(&beds, bed_id);
    if (patient == 0 || ward == 0 || bed == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("inpatient related entity not found");
    }

    if (patient->is_inpatient != 0 || InpatientService_has_active_admission_for_patient(&admissions, patient_id)) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("patient already admitted");
    }

    if (ward->status != WARD_STATUS_ACTIVE || !Ward_has_capacity(ward)) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("ward has no capacity");
    }

    if (strcmp(bed->ward_id, ward_id) != 0 || !Bed_is_assignable(bed) ||
        InpatientService_has_active_admission_for_bed(&admissions, bed_id)) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("bed not assignable");
    }

    result = InpatientService_next_admission_sequence(service, &next_sequence);
    if (result.success == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return result;
    }

    admission = (Admission *)malloc(sizeof(*admission));
    if (admission == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("failed to allocate admission");
    }

    memset(admission, 0, sizeof(*admission));
    if (!IdGenerator_format(
            admission->admission_id,
            sizeof(admission->admission_id),
            INPATIENT_SERVICE_ID_PREFIX,
            next_sequence,
            INPATIENT_SERVICE_ID_WIDTH
        )) {
        free(admission);
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("failed to generate admission id");
    }

    strncpy(admission->patient_id, patient_id, sizeof(admission->patient_id) - 1);
    strncpy(admission->ward_id, ward_id, sizeof(admission->ward_id) - 1);
    strncpy(admission->bed_id, bed_id, sizeof(admission->bed_id) - 1);
    strncpy(admission->admitted_at, admitted_at, sizeof(admission->admitted_at) - 1);
    strncpy(admission->summary, summary, sizeof(admission->summary) - 1);
    admission->status = ADMISSION_STATUS_ACTIVE;

    patient->is_inpatient = 1;
    if (!Ward_occupy_bed(ward) || !Bed_assign(bed, admission->admission_id, admitted_at)) {
        free(admission);
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("failed to assign inpatient resources");
    }

    if (!LinkedList_append(&admissions, admission)) {
        free(admission);
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("failed to append admission");
    }

    result = InpatientService_save_all(service, &patients, &wards, &beds, &admissions);
    if (result.success == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return result;
    }

    *out_admission = *admission;
    InpatientService_clear_all(&patients, &wards, &beds, &admissions);
    return Result_make_success("patient admitted");
}

Result InpatientService_discharge_patient(
    InpatientService *service,
    const char *admission_id,
    const char *discharged_at,
    const char *summary,
    Admission *out_admission
) {
    LinkedList patients;
    LinkedList wards;
    LinkedList beds;
    LinkedList admissions;
    Admission *admission = 0;
    Patient *patient = 0;
    Ward *ward = 0;
    Bed *bed = 0;
    Result result = InpatientService_validate_required_text(admission_id, "admission id");

    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(discharged_at, "discharged at");
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(summary, "discharge summary");
    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_admission == 0) {
        return Result_make_failure("inpatient service arguments missing");
    }

    result = InpatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_load_wards(service, &wards);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        return result;
    }

    result = InpatientService_load_beds(service, &beds);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        WardRepository_clear_loaded_list(&wards);
        return result;
    }

    result = InpatientService_load_admissions(service, &admissions);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        WardRepository_clear_loaded_list(&wards);
        BedRepository_clear_loaded_list(&beds);
        return result;
    }

    admission = InpatientService_find_admission(&admissions, admission_id);
    if (admission == 0 || admission->status != ADMISSION_STATUS_ACTIVE) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("active admission not found");
    }

    patient = InpatientService_find_patient(&patients, admission->patient_id);
    ward = InpatientService_find_ward(&wards, admission->ward_id);
    bed = InpatientService_find_bed(&beds, admission->bed_id);
    if (patient == 0 || ward == 0 || bed == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("inpatient related entity not found");
    }

    if (patient->is_inpatient == 0 || bed->status != BED_STATUS_OCCUPIED) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("inpatient state invalid");
    }

    patient->is_inpatient = 0;
    strncpy(admission->summary, summary, sizeof(admission->summary) - 1);
    admission->summary[sizeof(admission->summary) - 1] = '\0';
    if (!Admission_discharge(admission, discharged_at) ||
        !Bed_release(bed, discharged_at) ||
        !Ward_release_bed(ward)) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("failed to discharge patient");
    }

    result = InpatientService_save_all(service, &patients, &wards, &beds, &admissions);
    if (result.success == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return result;
    }

    *out_admission = *admission;
    InpatientService_clear_all(&patients, &wards, &beds, &admissions);
    return Result_make_success("patient discharged");
}

Result InpatientService_transfer_bed(
    InpatientService *service,
    const char *admission_id,
    const char *target_bed_id,
    const char *transferred_at,
    Admission *out_admission
) {
    LinkedList patients;
    LinkedList wards;
    LinkedList beds;
    LinkedList admissions;
    Admission *admission = 0;
    Bed *current_bed = 0;
    Bed *target_bed = 0;
    Ward *current_ward = 0;
    Ward *target_ward = 0;
    Result result = InpatientService_validate_required_text(admission_id, "admission id");

    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(target_bed_id, "target bed id");
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_validate_required_text(transferred_at, "transferred at");
    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_admission == 0) {
        return Result_make_failure("inpatient service arguments missing");
    }

    result = InpatientService_load_patients(service, &patients);
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_load_wards(service, &wards);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        return result;
    }

    result = InpatientService_load_beds(service, &beds);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        WardRepository_clear_loaded_list(&wards);
        return result;
    }

    result = InpatientService_load_admissions(service, &admissions);
    if (result.success == 0) {
        PatientRepository_clear_loaded_list(&patients);
        WardRepository_clear_loaded_list(&wards);
        BedRepository_clear_loaded_list(&beds);
        return result;
    }

    admission = InpatientService_find_admission(&admissions, admission_id);
    target_bed = InpatientService_find_bed(&beds, target_bed_id);
    if (admission == 0 || admission->status != ADMISSION_STATUS_ACTIVE || target_bed == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("transfer target not found");
    }

    current_bed = InpatientService_find_bed(&beds, admission->bed_id);
    current_ward = InpatientService_find_ward(&wards, admission->ward_id);
    target_ward = InpatientService_find_ward(&wards, target_bed->ward_id);
    if (current_bed == 0 || current_ward == 0 || target_ward == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("transfer related entity not found");
    }

    if (strcmp(current_bed->bed_id, target_bed->bed_id) == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("target bed unchanged");
    }

    if (current_bed->status != BED_STATUS_OCCUPIED || !Bed_is_assignable(target_bed) ||
        InpatientService_has_active_admission_for_bed(&admissions, target_bed_id)) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("target bed not assignable");
    }

    if (!Bed_release(current_bed, transferred_at) ||
        !Bed_assign(target_bed, admission_id, transferred_at)) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return Result_make_failure("failed to transfer bed");
    }

    if (strcmp(current_ward->ward_id, target_ward->ward_id) != 0) {
        if (!Ward_release_bed(current_ward) || !Ward_occupy_bed(target_ward)) {
            InpatientService_clear_all(&patients, &wards, &beds, &admissions);
            return Result_make_failure("failed to transfer ward occupancy");
        }

        strncpy(admission->ward_id, target_ward->ward_id, sizeof(admission->ward_id) - 1);
        admission->ward_id[sizeof(admission->ward_id) - 1] = '\0';
    }

    strncpy(admission->bed_id, target_bed_id, sizeof(admission->bed_id) - 1);
    admission->bed_id[sizeof(admission->bed_id) - 1] = '\0';

    result = InpatientService_save_all(service, &patients, &wards, &beds, &admissions);
    if (result.success == 0) {
        InpatientService_clear_all(&patients, &wards, &beds, &admissions);
        return result;
    }

    *out_admission = *admission;
    InpatientService_clear_all(&patients, &wards, &beds, &admissions);
    return Result_make_success("bed transferred");
}

Result InpatientService_find_by_id(
    InpatientService *service,
    const char *admission_id,
    Admission *out_admission
) {
    if (service == 0 || out_admission == 0) {
        return Result_make_failure("inpatient service arguments missing");
    }

    return AdmissionRepository_find_by_id(
        &service->admission_repository,
        admission_id,
        out_admission
    );
}

Result InpatientService_find_active_by_patient_id(
    InpatientService *service,
    const char *patient_id,
    Admission *out_admission
) {
    if (service == 0 || out_admission == 0) {
        return Result_make_failure("inpatient service arguments missing");
    }

    return AdmissionRepository_find_active_by_patient_id(
        &service->admission_repository,
        patient_id,
        out_admission
    );
}
