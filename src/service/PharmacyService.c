#include "service/PharmacyService.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/IdGenerator.h"
#include "repository/RepositoryUtils.h"

typedef struct PharmacyServiceDispenseCountContext {
    int count;
} PharmacyServiceDispenseCountContext;

static int PharmacyService_is_blank_text(const char *text) {
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

static Result PharmacyService_validate_required_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (PharmacyService_is_blank_text(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("pharmacy text valid");
}

static Result PharmacyService_validate_optional_text(const char *text, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (text == 0) {
        return Result_make_failure("optional text missing");
    }

    if (!RepositoryUtils_is_safe_field_text(text)) {
        snprintf(message, sizeof(message), "%s contains reserved characters", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("pharmacy optional text valid");
}

static Result PharmacyService_validate_medicine(const Medicine *medicine) {
    Result result;

    if (medicine == 0) {
        return Result_make_failure("medicine missing");
    }

    result = PharmacyService_validate_required_text(medicine->medicine_id, "medicine id");
    if (result.success == 0) {
        return result;
    }

    result = PharmacyService_validate_required_text(medicine->name, "medicine name");
    if (result.success == 0) {
        return result;
    }

    result = PharmacyService_validate_optional_text(medicine->department_id, "department id");
    if (result.success == 0) {
        return result;
    }

    if (!Medicine_has_valid_inventory(medicine)) {
        return Result_make_failure("medicine inventory invalid");
    }

    return Result_make_success("medicine valid");
}

static Result PharmacyService_validate_quantity(int quantity, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (quantity <= 0) {
        snprintf(message, sizeof(message), "%s invalid", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("quantity valid");
}

static Result PharmacyService_load_medicines(
    PharmacyService *service,
    LinkedList *medicines
) {
    if (service == 0 || medicines == 0) {
        return Result_make_failure("pharmacy service arguments missing");
    }

    return MedicineRepository_load_all(&service->medicine_repository, medicines);
}

static Medicine *PharmacyService_find_loaded_medicine(
    LinkedList *medicines,
    const char *medicine_id
) {
    LinkedListNode *current = 0;

    if (medicines == 0 || medicine_id == 0) {
        return 0;
    }

    current = medicines->head;
    while (current != 0) {
        Medicine *medicine = (Medicine *)current->data;

        if (strcmp(medicine->medicine_id, medicine_id) == 0) {
            return medicine;
        }

        current = current->next;
    }

    return 0;
}

static Result PharmacyService_append_medicine_copy(
    LinkedList *out_medicines,
    const Medicine *medicine
) {
    Medicine *copy = 0;

    if (out_medicines == 0 || medicine == 0) {
        return Result_make_failure("medicine result arguments missing");
    }

    copy = (Medicine *)malloc(sizeof(*copy));
    if (copy == 0) {
        return Result_make_failure("failed to allocate medicine result");
    }

    *copy = *medicine;
    if (!LinkedList_append(out_medicines, copy)) {
        free(copy);
        return Result_make_failure("failed to append medicine result");
    }

    return Result_make_success("medicine result appended");
}

static Result PharmacyService_count_dispense_line(const char *line, void *context) {
    PharmacyServiceDispenseCountContext *count_context =
        (PharmacyServiceDispenseCountContext *)context;

    (void)line;
    if (count_context == 0) {
        return Result_make_failure("dispense count context missing");
    }

    count_context->count++;
    return Result_make_success("dispense counted");
}

static Result PharmacyService_generate_dispense_id(
    PharmacyService *service,
    char *buffer,
    size_t buffer_size
) {
    PharmacyServiceDispenseCountContext context;
    Result result;

    if (service == 0 || buffer == 0 || buffer_size == 0) {
        return Result_make_failure("dispense id arguments missing");
    }

    result = DispenseRecordRepository_ensure_storage(&service->dispense_record_repository);
    if (result.success == 0) {
        return result;
    }

    context.count = 0;
    result = TextFileRepository_for_each_data_line(
        &service->dispense_record_repository.storage,
        PharmacyService_count_dispense_line,
        &context
    );
    if (result.success == 0) {
        return result;
    }

    if (!IdGenerator_format(buffer, buffer_size, "DSP", context.count + 1, 6)) {
        return Result_make_failure("failed to generate dispense id");
    }

    return Result_make_success("dispense id generated");
}

static Result PharmacyService_build_dispense_record(
    PharmacyService *service,
    const char *prescription_id,
    const char *medicine_id,
    int quantity,
    const char *pharmacist_id,
    const char *dispensed_at,
    DispenseRecord *record
) {
    Result result;

    if (record == 0) {
        return Result_make_failure("dispense record output missing");
    }

    result = PharmacyService_generate_dispense_id(
        service,
        record->dispense_id,
        sizeof(record->dispense_id)
    );
    if (result.success == 0) {
        return result;
    }

    memset(record->prescription_id, 0, sizeof(record->prescription_id));
    memset(record->medicine_id, 0, sizeof(record->medicine_id));
    memset(record->pharmacist_id, 0, sizeof(record->pharmacist_id));
    memset(record->dispensed_at, 0, sizeof(record->dispensed_at));

    strncpy(record->prescription_id, prescription_id, sizeof(record->prescription_id) - 1);
    strncpy(record->medicine_id, medicine_id, sizeof(record->medicine_id) - 1);
    strncpy(record->pharmacist_id, pharmacist_id, sizeof(record->pharmacist_id) - 1);
    strncpy(record->dispensed_at, dispensed_at, sizeof(record->dispensed_at) - 1);
    record->quantity = quantity;
    record->status = DISPENSE_STATUS_COMPLETED;

    return Result_make_success("dispense record built");
}

Result PharmacyService_init(
    PharmacyService *service,
    const char *medicine_path,
    const char *dispense_record_path
) {
    Result result;

    if (service == 0) {
        return Result_make_failure("pharmacy service missing");
    }

    result = MedicineRepository_init(&service->medicine_repository, medicine_path);
    if (result.success == 0) {
        return result;
    }

    result = DispenseRecordRepository_init(
        &service->dispense_record_repository,
        dispense_record_path
    );
    if (result.success == 0) {
        return result;
    }

    return DispenseRecordRepository_ensure_storage(&service->dispense_record_repository);
}

Result PharmacyService_add_medicine(
    PharmacyService *service,
    const Medicine *medicine
) {
    LinkedList medicines;
    Medicine *stored_medicine = 0;
    Result result = PharmacyService_validate_medicine(medicine);

    if (result.success == 0) {
        return result;
    }

    result = PharmacyService_load_medicines(service, &medicines);
    if (result.success == 0) {
        return result;
    }

    stored_medicine = PharmacyService_find_loaded_medicine(&medicines, medicine->medicine_id);
    MedicineRepository_clear_list(&medicines);
    if (stored_medicine != 0) {
        return Result_make_failure("medicine id already exists");
    }

    return MedicineRepository_save(&service->medicine_repository, medicine);
}

Result PharmacyService_restock_medicine(
    PharmacyService *service,
    const char *medicine_id,
    int quantity
) {
    LinkedList medicines;
    Medicine *medicine = 0;
    Result result = PharmacyService_validate_required_text(medicine_id, "medicine id");

    if (result.success == 0) {
        return result;
    }

    result = PharmacyService_validate_quantity(quantity, "restock quantity");
    if (result.success == 0) {
        return result;
    }

    result = PharmacyService_load_medicines(service, &medicines);
    if (result.success == 0) {
        return result;
    }

    medicine = PharmacyService_find_loaded_medicine(&medicines, medicine_id);
    if (medicine == 0) {
        MedicineRepository_clear_list(&medicines);
        return Result_make_failure("medicine not found");
    }

    medicine->stock += quantity;
    result = MedicineRepository_save_all(&service->medicine_repository, &medicines);
    MedicineRepository_clear_list(&medicines);
    return result;
}

Result PharmacyService_dispense_medicine(
    PharmacyService *service,
    const char *prescription_id,
    const char *medicine_id,
    int quantity,
    const char *pharmacist_id,
    const char *dispensed_at,
    DispenseRecord *out_record
) {
    LinkedList medicines;
    Medicine *medicine = 0;
    DispenseRecord record;
    int original_stock = 0;
    Result result = PharmacyService_validate_required_text(prescription_id, "prescription id");

    if (result.success == 0) {
        return result;
    }

    result = PharmacyService_validate_required_text(medicine_id, "medicine id");
    if (result.success == 0) {
        return result;
    }

    result = PharmacyService_validate_required_text(pharmacist_id, "pharmacist id");
    if (result.success == 0) {
        return result;
    }

    result = PharmacyService_validate_required_text(dispensed_at, "dispensed at");
    if (result.success == 0) {
        return result;
    }

    result = PharmacyService_validate_quantity(quantity, "dispense quantity");
    if (result.success == 0) {
        return result;
    }

    result = PharmacyService_load_medicines(service, &medicines);
    if (result.success == 0) {
        return result;
    }

    medicine = PharmacyService_find_loaded_medicine(&medicines, medicine_id);
    if (medicine == 0) {
        MedicineRepository_clear_list(&medicines);
        return Result_make_failure("medicine not found");
    }

    if (medicine->stock < quantity) {
        MedicineRepository_clear_list(&medicines);
        return Result_make_failure("medicine stock insufficient");
    }

    memset(&record, 0, sizeof(record));
    result = PharmacyService_build_dispense_record(
        service,
        prescription_id,
        medicine_id,
        quantity,
        pharmacist_id,
        dispensed_at,
        &record
    );
    if (result.success == 0) {
        MedicineRepository_clear_list(&medicines);
        return result;
    }

    original_stock = medicine->stock;
    medicine->stock -= quantity;
    result = MedicineRepository_save_all(&service->medicine_repository, &medicines);
    if (result.success == 0) {
        MedicineRepository_clear_list(&medicines);
        return result;
    }

    result = DispenseRecordRepository_append(&service->dispense_record_repository, &record);
    if (result.success == 0) {
        medicine->stock = original_stock;
        MedicineRepository_save_all(&service->medicine_repository, &medicines);
        MedicineRepository_clear_list(&medicines);
        return result;
    }

    MedicineRepository_clear_list(&medicines);
    if (out_record != 0) {
        *out_record = record;
    }

    return Result_make_success("medicine dispensed");
}

Result PharmacyService_get_stock(
    PharmacyService *service,
    const char *medicine_id,
    int *out_stock
) {
    Medicine medicine;
    Result result = PharmacyService_validate_required_text(medicine_id, "medicine id");

    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_stock == 0) {
        return Result_make_failure("stock query arguments missing");
    }

    result = MedicineRepository_find_by_medicine_id(
        &service->medicine_repository,
        medicine_id,
        &medicine
    );
    if (result.success == 0) {
        return result;
    }

    *out_stock = medicine.stock;
    return Result_make_success("medicine stock loaded");
}

Result PharmacyService_find_low_stock_medicines(
    PharmacyService *service,
    LinkedList *out_medicines
) {
    LinkedList medicines;
    LinkedListNode *current = 0;
    Result result;

    if (out_medicines == 0) {
        return Result_make_failure("low stock output missing");
    }

    LinkedList_init(out_medicines);
    result = PharmacyService_load_medicines(service, &medicines);
    if (result.success == 0) {
        return result;
    }

    current = medicines.head;
    while (current != 0) {
        const Medicine *medicine = (const Medicine *)current->data;

        if (medicine->stock <= medicine->low_stock_threshold) {
            result = PharmacyService_append_medicine_copy(out_medicines, medicine);
            if (result.success == 0) {
                MedicineRepository_clear_list(&medicines);
                PharmacyService_clear_medicine_results(out_medicines);
                return result;
            }
        }

        current = current->next;
    }

    MedicineRepository_clear_list(&medicines);
    return Result_make_success("low stock medicines loaded");
}

void PharmacyService_clear_medicine_results(LinkedList *medicines) {
    LinkedList_clear(medicines, free);
}

Result PharmacyService_find_dispense_records_by_prescription_id(
    PharmacyService *service,
    const char *prescription_id,
    LinkedList *out_records
) {
    Result result = PharmacyService_validate_required_text(prescription_id, "prescription id");

    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_records == 0) {
        return Result_make_failure("dispense history query arguments missing");
    }

    if (LinkedList_count(out_records) != 0) {
        return Result_make_failure("dispense history output must be empty");
    }

    return DispenseRecordRepository_find_by_prescription_id(
        &service->dispense_record_repository,
        prescription_id,
        out_records
    );
}

void PharmacyService_clear_dispense_record_results(LinkedList *records) {
    DispenseRecordRepository_clear_list(records);
}
