#include "service/BedService.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int BedService_is_blank_text(const char *text) {
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

static Result BedService_validate_required_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (BedService_is_blank_text(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("bed service text valid");
}

static Result BedService_append_bed_copy(LinkedList *out_beds, const Bed *bed) {
    Bed *copy = 0;

    if (out_beds == 0 || bed == 0) {
        return Result_make_failure("bed copy arguments missing");
    }

    copy = (Bed *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate bed result");
    }

    *copy = *bed;
    if (!LinkedList_append(out_beds, copy)) {
        free(copy);
        return Result_make_failure("failed to append bed result");
    }

    return Result_make_success("bed result appended");
}

Result BedService_init(
    BedService *service,
    const char *ward_path,
    const char *bed_path,
    const char *admission_path
) {
    Result result;

    if (service == 0) {
        return Result_make_failure("bed service missing");
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

Result BedService_list_wards(BedService *service, LinkedList *out_wards) {
    if (service == 0 || out_wards == 0) {
        return Result_make_failure("bed service arguments missing");
    }

    return WardRepository_load_all(&service->ward_repository, out_wards);
}

Result BedService_list_beds_by_ward(
    BedService *service,
    const char *ward_id,
    LinkedList *out_beds
) {
    LinkedList loaded_beds;
    const LinkedListNode *current = 0;
    Result result = BedService_validate_required_text(ward_id, "ward id");

    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_beds == 0) {
        return Result_make_failure("bed service arguments missing");
    }

    result = WardRepository_find_by_id(&service->ward_repository, ward_id, &(Ward){0});
    if (result.success == 0) {
        return result;
    }

    LinkedList_init(out_beds);
    result = BedRepository_load_all(&service->bed_repository, &loaded_beds);
    if (result.success == 0) {
        return result;
    }

    current = loaded_beds.head;
    while (current != 0) {
        const Bed *bed = (const Bed *)current->data;

        if (strcmp(bed->ward_id, ward_id) == 0) {
            result = BedService_append_bed_copy(out_beds, bed);
            if (result.success == 0) {
                BedRepository_clear_loaded_list(&loaded_beds);
                BedService_clear_beds(out_beds);
                return result;
            }
        }

        current = current->next;
    }

    BedRepository_clear_loaded_list(&loaded_beds);
    return Result_make_success("beds loaded by ward");
}

Result BedService_find_current_patient_by_bed_id(
    BedService *service,
    const char *bed_id,
    char *out_patient_id,
    size_t out_patient_id_capacity
) {
    Bed bed;
    Admission admission;
    Result result = BedService_validate_required_text(bed_id, "bed id");

    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_patient_id == 0 || out_patient_id_capacity == 0) {
        return Result_make_failure("bed service arguments missing");
    }

    result = BedRepository_find_by_id(&service->bed_repository, bed_id, &bed);
    if (result.success == 0) {
        return result;
    }

    if (bed.status != BED_STATUS_OCCUPIED || bed.current_admission_id[0] == '\0') {
        return Result_make_failure("bed has no current patient");
    }

    result = AdmissionRepository_find_active_by_bed_id(
        &service->admission_repository,
        bed_id,
        &admission
    );
    if (result.success == 0) {
        return result;
    }

    if (strcmp(admission.admission_id, bed.current_admission_id) != 0) {
        return Result_make_failure("bed admission mismatch");
    }

    strncpy(out_patient_id, admission.patient_id, out_patient_id_capacity - 1);
    out_patient_id[out_patient_id_capacity - 1] = '\0';
    return Result_make_success("current patient found");
}

void BedService_clear_wards(LinkedList *wards) {
    WardRepository_clear_loaded_list(wards);
}

void BedService_clear_beds(LinkedList *beds) {
    LinkedList_clear(beds, free);
}
