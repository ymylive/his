#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/DispenseRecord.h"
#include "domain/Medicine.h"
#include "repository/DispenseRecordRepository.h"
#include "service/PharmacyService.h"

static void TestPharmacyService_make_path(
    char *buffer,
    size_t buffer_size,
    const char *test_name,
    const char *kind
) {
    static long sequence = 1;

    assert(buffer != 0);
    assert(test_name != 0);
    assert(kind != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_pharmacy_service_data/%s_%s_%ld_%ld.txt",
        test_name,
        kind,
        (long)time(0),
        sequence++
    );
    remove(buffer);
}

static Medicine TestPharmacyService_make_medicine(
    const char *medicine_id,
    const char *name,
    double price,
    int stock,
    const char *department_id,
    int low_stock_threshold
) {
    Medicine medicine;

    memset(&medicine, 0, sizeof(medicine));
    strcpy(medicine.medicine_id, medicine_id);
    strcpy(medicine.name, name);
    medicine.price = price;
    medicine.stock = stock;
    strcpy(medicine.department_id, department_id);
    medicine.low_stock_threshold = low_stock_threshold;
    return medicine;
}

static int TestPharmacyService_has_medicine_id(const void *data, const void *context) {
    const Medicine *medicine = (const Medicine *)data;
    const char *medicine_id = (const char *)context;

    return strcmp(medicine->medicine_id, medicine_id) == 0;
}

static void test_add_restock_and_query_inventory(void) {
    char medicine_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char record_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PharmacyService service;
    Medicine medicine = TestPharmacyService_make_medicine(
        "MED1001",
        "Amoxicillin",
        12.50,
        10,
        "DEP01",
        5
    );
    int stock = 0;
    Result result;

    TestPharmacyService_make_path(medicine_path, sizeof(medicine_path), "inventory", "medicine");
    TestPharmacyService_make_path(record_path, sizeof(record_path), "inventory", "record");

    result = PharmacyService_init(&service, medicine_path, record_path);
    assert(result.success == 1);

    result = PharmacyService_add_medicine(&service, &medicine);
    assert(result.success == 1);

    result = PharmacyService_get_stock(&service, "MED1001", &stock);
    assert(result.success == 1);
    assert(stock == 10);

    result = PharmacyService_restock_medicine(&service, "MED1001", 15);
    assert(result.success == 1);

    result = PharmacyService_get_stock(&service, "MED1001", &stock);
    assert(result.success == 1);
    assert(stock == 25);
}

static void test_dispense_updates_inventory_and_writes_record(void) {
    char medicine_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char record_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PharmacyService service;
    DispenseRecord record;
    LinkedList records;
    DispenseRecord *stored_record = 0;
    int stock = 0;
    Result result;

    TestPharmacyService_make_path(medicine_path, sizeof(medicine_path), "dispense", "medicine");
    TestPharmacyService_make_path(record_path, sizeof(record_path), "dispense", "record");

    result = PharmacyService_init(&service, medicine_path, record_path);
    assert(result.success == 1);

    result = PharmacyService_add_medicine(
        &service,
        &(Medicine){
            "MED2001",
            "Ibuprofen",
            18.00,
            20,
            "DEP02",
            5
        }
    );
    assert(result.success == 1);

    memset(&record, 0, sizeof(record));
    result = PharmacyService_dispense_medicine_for_patient(
        &service,
        "PAT2001",
        "RX2001",
        "MED2001",
        6,
        "PHA2001",
        "2026-03-20T10:30:00",
        &record
    );
    assert(result.success == 1);
    assert(record.dispense_id[0] != '\0');
    assert(strcmp(record.patient_id, "PAT2001") == 0);
    assert(strcmp(record.prescription_id, "RX2001") == 0);
    assert(strcmp(record.medicine_id, "MED2001") == 0);
    assert(record.quantity == 6);
    assert(strcmp(record.pharmacist_id, "PHA2001") == 0);
    assert(strcmp(record.dispensed_at, "2026-03-20T10:30:00") == 0);
    assert(record.status == DISPENSE_STATUS_COMPLETED);

    result = PharmacyService_get_stock(&service, "MED2001", &stock);
    assert(result.success == 1);
    assert(stock == 14);

    LinkedList_init(&records);
    result = PharmacyService_find_dispense_records_by_patient_id(&service, "PAT2001", &records);
    assert(result.success == 1);
    assert(LinkedList_count(&records) == 1);

    stored_record = (DispenseRecord *)records.head->data;
    assert(stored_record != 0);
    assert(strcmp(stored_record->dispense_id, record.dispense_id) == 0);
    assert(strcmp(stored_record->patient_id, "PAT2001") == 0);
    assert(stored_record->quantity == 6);
    DispenseRecordRepository_clear_list(&records);
}

static void test_low_stock_alerts_and_insufficient_stock_rejected(void) {
    char medicine_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char record_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PharmacyService service;
    LinkedList low_stock_medicines;
    LinkedList failed_records;
    Result result;

    TestPharmacyService_make_path(medicine_path, sizeof(medicine_path), "low_stock", "medicine");
    TestPharmacyService_make_path(record_path, sizeof(record_path), "low_stock", "record");

    result = PharmacyService_init(&service, medicine_path, record_path);
    assert(result.success == 1);

    result = PharmacyService_add_medicine(
        &service,
        &(Medicine){
            "MED3001",
            "VitaminC",
            6.80,
            4,
            "DEP03",
            5
        }
    );
    assert(result.success == 1);

    result = PharmacyService_add_medicine(
        &service,
        &(Medicine){
            "MED3002",
            "Paracetamol",
            9.90,
            12,
            "DEP03",
            2
        }
    );
    assert(result.success == 1);

    LinkedList_init(&low_stock_medicines);
    result = PharmacyService_find_low_stock_medicines(&service, &low_stock_medicines);
    assert(result.success == 1);
    assert(LinkedList_count(&low_stock_medicines) == 1);
    assert(
        LinkedList_find_first(
            &low_stock_medicines,
            TestPharmacyService_has_medicine_id,
            "MED3001"
        ) != 0
    );
    PharmacyService_clear_medicine_results(&low_stock_medicines);

    result = PharmacyService_dispense_medicine(
        &service,
        "RX3001",
        "MED3002",
        11,
        "PHA3001",
        "2026-03-20T11:00:00",
        0
    );
    assert(result.success == 1);

    LinkedList_init(&low_stock_medicines);
    result = PharmacyService_find_low_stock_medicines(&service, &low_stock_medicines);
    assert(result.success == 1);
    assert(LinkedList_count(&low_stock_medicines) == 2);
    assert(
        LinkedList_find_first(
            &low_stock_medicines,
            TestPharmacyService_has_medicine_id,
            "MED3001"
        ) != 0
    );
    assert(
        LinkedList_find_first(
            &low_stock_medicines,
            TestPharmacyService_has_medicine_id,
            "MED3002"
        ) != 0
    );
    PharmacyService_clear_medicine_results(&low_stock_medicines);

    result = PharmacyService_dispense_medicine(
        &service,
        "RX3002",
        "MED3001",
        10,
        "PHA3001",
        "2026-03-20T11:10:00",
        0
    );
    assert(result.success == 0);

    LinkedList_init(&failed_records);
    result = DispenseRecordRepository_find_by_prescription_id(
        &service.dispense_record_repository,
        "RX3002",
        &failed_records
    );
    assert(result.success == 1);
    assert(LinkedList_count(&failed_records) == 0);
    DispenseRecordRepository_clear_list(&failed_records);
}

static void test_query_dispense_history_by_patient_id(void) {
    char medicine_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char record_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PharmacyService service;
    LinkedList records;
    DispenseRecord *stored_record = 0;
    Result result;

    TestPharmacyService_make_path(medicine_path, sizeof(medicine_path), "history", "medicine");
    TestPharmacyService_make_path(record_path, sizeof(record_path), "history", "record");

    result = PharmacyService_init(&service, medicine_path, record_path);
    assert(result.success == 1);

    result = PharmacyService_add_medicine(
        &service,
        &(Medicine){
            "MED9001",
            "HistoryMed",
            10.00,
            10,
            "DEP09",
            1
        }
    );
    assert(result.success == 1);

    result = PharmacyService_dispense_medicine_for_patient(
        &service,
        "PAT9001",
        "RX9001",
        "MED9001",
        2,
        "PHA9001",
        "2026-03-20T19:00:00",
        0
    );
    assert(result.success == 1);

    result = PharmacyService_dispense_medicine_for_patient(
        &service,
        "PAT9002",
        "RX9002",
        "MED9001",
        1,
        "PHA9001",
        "2026-03-20T19:10:00",
        0
    );
    assert(result.success == 1);

    LinkedList_init(&records);
    result = PharmacyService_find_dispense_records_by_patient_id(&service, "PAT9001", &records);
    assert(result.success == 1);
    assert(LinkedList_count(&records) == 1);

    stored_record = (DispenseRecord *)records.head->data;
    assert(stored_record != 0);
    assert(strcmp(stored_record->patient_id, "PAT9001") == 0);
    assert(strcmp(stored_record->prescription_id, "RX9001") == 0);
    assert(strcmp(stored_record->medicine_id, "MED9001") == 0);
    assert(stored_record->quantity == 2);
    assert(strcmp(stored_record->pharmacist_id, "PHA9001") == 0);
    assert(strcmp(stored_record->dispensed_at, "2026-03-20T19:00:00") == 0);
    assert(stored_record->status == DISPENSE_STATUS_COMPLETED);

    PharmacyService_clear_dispense_record_results(&records);
}

int main(void) {
    test_add_restock_and_query_inventory();
    test_dispense_updates_inventory_and_writes_record();
    test_low_stock_alerts_and_insufficient_stock_rejected();
    test_query_dispense_history_by_patient_id();
    return 0;
}
