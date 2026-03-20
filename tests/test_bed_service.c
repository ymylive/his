#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/Admission.h"
#include "domain/Bed.h"
#include "domain/Ward.h"
#include "repository/AdmissionRepository.h"
#include "repository/BedRepository.h"
#include "repository/WardRepository.h"
#include "service/BedService.h"

typedef struct BedServiceTestContext {
    char ward_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char bed_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char admission_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    WardRepository ward_repository;
    BedRepository bed_repository;
    AdmissionRepository admission_repository;
    BedService service;
} BedServiceTestContext;

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
        "build/test_bed_service_data/%s_%ld_%lu_%s",
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

static Ward make_ward(
    const char *ward_id,
    const char *name,
    const char *department_id,
    const char *location,
    int capacity,
    int occupied_beds,
    WardStatus status
) {
    Ward ward;

    memset(&ward, 0, sizeof(ward));
    copy_text(ward.ward_id, sizeof(ward.ward_id), ward_id);
    copy_text(ward.name, sizeof(ward.name), name);
    copy_text(ward.department_id, sizeof(ward.department_id), department_id);
    copy_text(ward.location, sizeof(ward.location), location);
    ward.capacity = capacity;
    ward.occupied_beds = occupied_beds;
    ward.status = status;
    return ward;
}

static Bed make_bed(
    const char *bed_id,
    const char *ward_id,
    const char *room_no,
    const char *bed_no,
    BedStatus status,
    const char *current_admission_id,
    const char *occupied_at,
    const char *released_at
) {
    Bed bed;

    memset(&bed, 0, sizeof(bed));
    copy_text(bed.bed_id, sizeof(bed.bed_id), bed_id);
    copy_text(bed.ward_id, sizeof(bed.ward_id), ward_id);
    copy_text(bed.room_no, sizeof(bed.room_no), room_no);
    copy_text(bed.bed_no, sizeof(bed.bed_no), bed_no);
    bed.status = status;
    copy_text(bed.current_admission_id, sizeof(bed.current_admission_id), current_admission_id);
    copy_text(bed.occupied_at, sizeof(bed.occupied_at), occupied_at);
    copy_text(bed.released_at, sizeof(bed.released_at), released_at);
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

static const Bed *bed_at(const LinkedList *beds, size_t index) {
    const LinkedListNode *node = 0;
    size_t current_index = 0;

    assert(beds != 0);
    node = beds->head;
    while (node != 0) {
        if (current_index == index) {
            return (const Bed *)node->data;
        }

        node = node->next;
        current_index++;
    }

    return 0;
}

static void setup_context(BedServiceTestContext *context, const char *test_name) {
    Result result;

    memset(context, 0, sizeof(*context));
    build_test_path(context->ward_path, sizeof(context->ward_path), test_name, "wards.txt");
    build_test_path(context->bed_path, sizeof(context->bed_path), test_name, "beds.txt");
    build_test_path(
        context->admission_path,
        sizeof(context->admission_path),
        test_name,
        "admissions.txt"
    );

    result = WardRepository_init(&context->ward_repository, context->ward_path);
    assert(result.success == 1);
    result = BedRepository_init(&context->bed_repository, context->bed_path);
    assert(result.success == 1);
    result = AdmissionRepository_init(&context->admission_repository, context->admission_path);
    assert(result.success == 1);

    result = BedService_init(
        &context->service,
        context->ward_path,
        context->bed_path,
        context->admission_path
    );
    assert(result.success == 1);
}

static void test_list_wards_and_beds_by_ward(void) {
    BedServiceTestContext context;
    LinkedList wards;
    LinkedList beds;
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 3, 1, WARD_STATUS_ACTIVE);
    Bed first_bed = make_bed("BED0001", "WRD0001", "501", "01", BED_STATUS_AVAILABLE, "", "", "");
    Bed second_bed = make_bed("BED0002", "WRD0001", "501", "02", BED_STATUS_AVAILABLE, "", "", "");
    Result result;

    setup_context(&context, "list");

    assert(WardRepository_append(&context.ward_repository, &ward).success == 1);
    assert(BedRepository_append(&context.bed_repository, &first_bed).success == 1);
    assert(BedRepository_append(&context.bed_repository, &second_bed).success == 1);

    LinkedList_init(&wards);
    result = BedService_list_wards(&context.service, &wards);
    assert(result.success == 1);
    assert(LinkedList_count(&wards) == 1);
    BedService_clear_wards(&wards);

    LinkedList_init(&beds);
    result = BedService_list_beds_by_ward(&context.service, "WRD0001", &beds);
    assert(result.success == 1);
    assert(LinkedList_count(&beds) == 2);
    assert(strcmp(bed_at(&beds, 0)->bed_id, "BED0001") == 0);
    assert(strcmp(bed_at(&beds, 1)->bed_id, "BED0002") == 0);
    BedService_clear_beds(&beds);
}

static void test_find_current_patient_by_bed_id(void) {
    BedServiceTestContext context;
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 2, 1, WARD_STATUS_ACTIVE);
    Bed bed = make_bed(
        "BED0001",
        "WRD0001",
        "501",
        "01",
        BED_STATUS_OCCUPIED,
        "ADM0001",
        "2026-03-20T08:00",
        ""
    );
    Admission admission = make_admission(
        "ADM0001",
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        ADMISSION_STATUS_ACTIVE,
        "",
        "observation"
    );
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    Result result;

    setup_context(&context, "current_patient");

    assert(WardRepository_append(&context.ward_repository, &ward).success == 1);
    assert(BedRepository_append(&context.bed_repository, &bed).success == 1);
    assert(AdmissionRepository_append(&context.admission_repository, &admission).success == 1);

    memset(patient_id, 0, sizeof(patient_id));
    result = BedService_find_current_patient_by_bed_id(
        &context.service,
        "BED0001",
        patient_id,
        sizeof(patient_id)
    );
    assert(result.success == 1);
    assert(strcmp(patient_id, "PAT0001") == 0);
}

static void test_current_patient_query_rejects_available_bed(void) {
    BedServiceTestContext context;
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 2, 0, WARD_STATUS_ACTIVE);
    Bed bed = make_bed("BED0001", "WRD0001", "501", "01", BED_STATUS_AVAILABLE, "", "", "");
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    Result result;

    setup_context(&context, "available_bed");

    assert(WardRepository_append(&context.ward_repository, &ward).success == 1);
    assert(BedRepository_append(&context.bed_repository, &bed).success == 1);

    memset(patient_id, 0, sizeof(patient_id));
    result = BedService_find_current_patient_by_bed_id(
        &context.service,
        "BED0001",
        patient_id,
        sizeof(patient_id)
    );
    assert(result.success == 0);
}

int main(void) {
    test_list_wards_and_beds_by_ward();
    test_find_current_patient_by_bed_id();
    test_current_patient_query_rejects_available_bed();
    return 0;
}
