#ifndef HIS_REPOSITORY_VISIT_RECORD_REPOSITORY_H
#define HIS_REPOSITORY_VISIT_RECORD_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/VisitRecord.h"
#include "repository/TextFileRepository.h"

#define VISIT_RECORD_REPOSITORY_FIELD_COUNT 12

typedef struct VisitRecordRepository {
    TextFileRepository storage;
} VisitRecordRepository;

Result VisitRecordRepository_init(VisitRecordRepository *repository, const char *path);
Result VisitRecordRepository_ensure_storage(const VisitRecordRepository *repository);
Result VisitRecordRepository_append(
    const VisitRecordRepository *repository,
    const VisitRecord *record
);
Result VisitRecordRepository_find_by_visit_id(
    const VisitRecordRepository *repository,
    const char *visit_id,
    VisitRecord *out_record
);
Result VisitRecordRepository_find_by_registration_id(
    const VisitRecordRepository *repository,
    const char *registration_id,
    LinkedList *out_records
);
Result VisitRecordRepository_load_all(
    const VisitRecordRepository *repository,
    LinkedList *out_records
);
Result VisitRecordRepository_save_all(
    const VisitRecordRepository *repository,
    const LinkedList *records
);
void VisitRecordRepository_clear_list(LinkedList *records);

#endif
