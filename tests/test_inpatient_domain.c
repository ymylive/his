#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/Admission.h"
#include "domain/Bed.h"
#include "domain/InpatientOrder.h"
#include "domain/Ward.h"
#include "repository/AdmissionRepository.h"
#include "repository/BedRepository.h"
#include "repository/InpatientOrderRepository.h"
#include "repository/WardRepository.h"

static void make_test_path(char *buffer, size_t buffer_size, const char *file_name) {
    assert(buffer != 0);
    assert(file_name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_inpatient_domain_data/%s_%ld.txt",
        file_name,
        (long)time(0)
    );
    remove(buffer);
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
    copy_text(
        bed.current_admission_id,
        sizeof(bed.current_admission_id),
        current_admission_id
    );
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

static InpatientOrder make_order(
    const char *order_id,
    const char *admission_id,
    const char *order_type,
    const char *content,
    const char *ordered_at,
    InpatientOrderStatus status,
    const char *executed_at,
    const char *cancelled_at
) {
    InpatientOrder order;

    memset(&order, 0, sizeof(order));
    copy_text(order.order_id, sizeof(order.order_id), order_id);
    copy_text(order.admission_id, sizeof(order.admission_id), admission_id);
    copy_text(order.order_type, sizeof(order.order_type), order_type);
    copy_text(order.content, sizeof(order.content), content);
    copy_text(order.ordered_at, sizeof(order.ordered_at), ordered_at);
    order.status = status;
    copy_text(order.executed_at, sizeof(order.executed_at), executed_at);
    copy_text(order.cancelled_at, sizeof(order.cancelled_at), cancelled_at);
    return order;
}

static Admission *find_admission_in_list(LinkedList *list, const char *admission_id) {
    LinkedListNode *current = 0;

    assert(list != 0);
    assert(admission_id != 0);

    current = list->head;
    while (current != 0) {
        Admission *admission = (Admission *)current->data;
        if (strcmp(admission->admission_id, admission_id) == 0) {
            return admission;
        }

        current = current->next;
    }

    return 0;
}

static void test_ward_and_bed_status_flow(void) {
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 2, 0, WARD_STATUS_ACTIVE);
    Bed bed = make_bed("BED0001", "WRD0001", "501", "01", BED_STATUS_AVAILABLE, "", "", "");

    assert(Ward_has_capacity(&ward) == 1);
    assert(Ward_occupy_bed(&ward) == 1);
    assert(ward.occupied_beds == 1);
    assert(Ward_occupy_bed(&ward) == 1);
    assert(ward.occupied_beds == 2);
    assert(Ward_has_capacity(&ward) == 0);
    assert(Ward_occupy_bed(&ward) == 0);
    assert(Ward_release_bed(&ward) == 1);
    assert(ward.occupied_beds == 1);

    assert(Bed_assign(&bed, "ADM0001", "2026-03-20T08:00") == 1);
    assert(bed.status == BED_STATUS_OCCUPIED);
    assert(strcmp(bed.current_admission_id, "ADM0001") == 0);
    assert(strcmp(bed.occupied_at, "2026-03-20T08:00") == 0);
    assert(Bed_assign(&bed, "ADM0002", "2026-03-20T09:00") == 0);
    assert(Bed_release(&bed, "2026-03-21T10:00") == 1);
    assert(bed.status == BED_STATUS_AVAILABLE);
    assert(bed.current_admission_id[0] == '\0');
    assert(strcmp(bed.released_at, "2026-03-21T10:00") == 0);
    assert(Bed_release(&bed, "2026-03-22T10:00") == 0);
}

static void test_admission_and_order_status_flow(void) {
    Admission admission = make_admission(
        "ADM0001",
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        ADMISSION_STATUS_ACTIVE,
        "",
        "initial admission"
    );
    InpatientOrder pending = make_order(
        "ORD0001",
        "ADM0001",
        "NURSING",
        "Monitor blood pressure",
        "2026-03-20T08:10",
        INPATIENT_ORDER_STATUS_PENDING,
        "",
        ""
    );
    InpatientOrder cancelled = make_order(
        "ORD0002",
        "ADM0001",
        "DIET",
        "Low sodium diet",
        "2026-03-20T08:20",
        INPATIENT_ORDER_STATUS_PENDING,
        "",
        ""
    );

    assert(Admission_discharge(&admission, "2026-03-23T10:00") == 1);
    assert(admission.status == ADMISSION_STATUS_DISCHARGED);
    assert(strcmp(admission.discharged_at, "2026-03-23T10:00") == 0);
    assert(Admission_discharge(&admission, "2026-03-24T10:00") == 0);

    assert(InpatientOrder_mark_executed(&pending, "2026-03-20T09:00") == 1);
    assert(pending.status == INPATIENT_ORDER_STATUS_EXECUTED);
    assert(strcmp(pending.executed_at, "2026-03-20T09:00") == 0);
    assert(InpatientOrder_cancel(&pending, "2026-03-20T09:10") == 0);

    assert(InpatientOrder_cancel(&cancelled, "2026-03-20T08:30") == 1);
    assert(cancelled.status == INPATIENT_ORDER_STATUS_CANCELLED);
    assert(strcmp(cancelled.cancelled_at, "2026-03-20T08:30") == 0);
    assert(InpatientOrder_mark_executed(&cancelled, "2026-03-20T08:40") == 0);
}

static void test_repository_roundtrip_and_guards(void) {
    char ward_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char bed_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char admission_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char order_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    WardRepository ward_repository;
    BedRepository bed_repository;
    AdmissionRepository admission_repository;
    InpatientOrderRepository order_repository;
    Ward ward = make_ward("WRD0001", "Ward A", "DEP0001", "Floor 5", 10, 0, WARD_STATUS_ACTIVE);
    Bed first_bed = make_bed("BED0001", "WRD0001", "501", "01", BED_STATUS_AVAILABLE, "", "", "");
    Bed second_bed = make_bed("BED0002", "WRD0001", "501", "02", BED_STATUS_AVAILABLE, "", "", "");
    Admission first_admission = make_admission(
        "ADM0001",
        "PAT0001",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:00",
        ADMISSION_STATUS_ACTIVE,
        "",
        "admit for observation"
    );
    Admission same_bed_conflict = make_admission(
        "ADM0002",
        "PAT0002",
        "WRD0001",
        "BED0001",
        "2026-03-20T08:10",
        ADMISSION_STATUS_ACTIVE,
        "",
        "bed conflict"
    );
    Admission same_patient_conflict = make_admission(
        "ADM0003",
        "PAT0001",
        "WRD0001",
        "BED0002",
        "2026-03-20T08:20",
        ADMISSION_STATUS_ACTIVE,
        "",
        "patient conflict"
    );
    Admission readmit = make_admission(
        "ADM0004",
        "PAT0001",
        "WRD0001",
        "BED0002",
        "2026-03-24T08:00",
        ADMISSION_STATUS_ACTIVE,
        "",
        "readmission"
    );
    InpatientOrder order = make_order(
        "ORD0001",
        "ADM0001",
        "MEDICATION",
        "Aspirin 100mg",
        "2026-03-20T08:30",
        INPATIENT_ORDER_STATUS_PENDING,
        "",
        ""
    );
    Ward loaded_ward;
    Bed loaded_bed;
    Admission loaded_admission;
    Admission active_admission;
    InpatientOrder loaded_order;
    LinkedList admissions;
    Result result;

    make_test_path(ward_path, sizeof(ward_path), "wards");
    make_test_path(bed_path, sizeof(bed_path), "beds");
    make_test_path(admission_path, sizeof(admission_path), "admissions");
    make_test_path(order_path, sizeof(order_path), "orders");

    result = WardRepository_init(&ward_repository, ward_path);
    assert(result.success == 1);
    result = BedRepository_init(&bed_repository, bed_path);
    assert(result.success == 1);
    result = AdmissionRepository_init(&admission_repository, admission_path);
    assert(result.success == 1);
    result = InpatientOrderRepository_init(&order_repository, order_path);
    assert(result.success == 1);

    assert(WardRepository_append(&ward_repository, &ward).success == 1);
    assert(BedRepository_append(&bed_repository, &first_bed).success == 1);
    assert(BedRepository_append(&bed_repository, &second_bed).success == 1);
    assert(AdmissionRepository_append(&admission_repository, &first_admission).success == 1);
    assert(InpatientOrderRepository_append(&order_repository, &order).success == 1);

    result = WardRepository_find_by_id(&ward_repository, "WRD0001", &loaded_ward);
    assert(result.success == 1);
    assert(strcmp(loaded_ward.name, "Ward A") == 0);

    result = BedRepository_find_by_id(&bed_repository, "BED0001", &loaded_bed);
    assert(result.success == 1);
    assert(loaded_bed.status == BED_STATUS_AVAILABLE);

    result = AdmissionRepository_find_active_by_patient_id(
        &admission_repository,
        "PAT0001",
        &active_admission
    );
    assert(result.success == 1);
    assert(strcmp(active_admission.admission_id, "ADM0001") == 0);

    result = AdmissionRepository_find_active_by_bed_id(
        &admission_repository,
        "BED0001",
        &active_admission
    );
    assert(result.success == 1);
    assert(strcmp(active_admission.patient_id, "PAT0001") == 0);

    assert(AdmissionRepository_append(&admission_repository, &same_bed_conflict).success == 0);
    assert(AdmissionRepository_append(&admission_repository, &same_patient_conflict).success == 0);

    LinkedList_init(&admissions);
    result = AdmissionRepository_load_all(&admission_repository, &admissions);
    assert(result.success == 1);
    assert(find_admission_in_list(&admissions, "ADM0001") != 0);
    assert(Admission_discharge(find_admission_in_list(&admissions, "ADM0001"), "2026-03-23T10:00") == 1);
    assert(AdmissionRepository_save_all(&admission_repository, &admissions).success == 1);
    AdmissionRepository_clear_loaded_list(&admissions);

    result = AdmissionRepository_find_by_id(&admission_repository, "ADM0001", &loaded_admission);
    assert(result.success == 1);
    assert(loaded_admission.status == ADMISSION_STATUS_DISCHARGED);
    assert(strcmp(loaded_admission.discharged_at, "2026-03-23T10:00") == 0);

    assert(AdmissionRepository_append(&admission_repository, &readmit).success == 1);

    result = InpatientOrderRepository_find_by_id(&order_repository, "ORD0001", &loaded_order);
    assert(result.success == 1);
    assert(strcmp(loaded_order.content, "Aspirin 100mg") == 0);
    assert(loaded_order.status == INPATIENT_ORDER_STATUS_PENDING);
}

int main(void) {
    test_ward_and_bed_status_flow();
    test_admission_and_order_status_flow();
    test_repository_roundtrip_and_guards();
    return 0;
}
