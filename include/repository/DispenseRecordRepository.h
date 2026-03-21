#ifndef HIS_REPOSITORY_DISPENSE_RECORD_REPOSITORY_H
#define HIS_REPOSITORY_DISPENSE_RECORD_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/DispenseRecord.h"
#include "repository/TextFileRepository.h"

#define DISPENSE_RECORD_REPOSITORY_FIELD_COUNT 8

typedef struct DispenseRecordRepository {
    TextFileRepository storage;
} DispenseRecordRepository;

Result DispenseRecordRepository_init(DispenseRecordRepository *repository, const char *path);
Result DispenseRecordRepository_ensure_storage(const DispenseRecordRepository *repository);
Result DispenseRecordRepository_append(
    const DispenseRecordRepository *repository,
    const DispenseRecord *record
);
Result DispenseRecordRepository_find_by_dispense_id(
    const DispenseRecordRepository *repository,
    const char *dispense_id,
    DispenseRecord *out_record
);
Result DispenseRecordRepository_find_by_prescription_id(
    const DispenseRecordRepository *repository,
    const char *prescription_id,
    LinkedList *out_records
);
Result DispenseRecordRepository_find_by_patient_id(
    const DispenseRecordRepository *repository,
    const char *patient_id,
    LinkedList *out_records
);
Result DispenseRecordRepository_load_all(
    const DispenseRecordRepository *repository,
    LinkedList *out_records
);
Result DispenseRecordRepository_save_all(
    const DispenseRecordRepository *repository,
    const LinkedList *records
);
void DispenseRecordRepository_clear_list(LinkedList *records);

#endif
