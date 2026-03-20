#ifndef HIS_REPOSITORY_EXAMINATION_RECORD_REPOSITORY_H
#define HIS_REPOSITORY_EXAMINATION_RECORD_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/ExaminationRecord.h"
#include "repository/TextFileRepository.h"

#define EXAMINATION_RECORD_REPOSITORY_FIELD_COUNT 10

typedef struct ExaminationRecordRepository {
    TextFileRepository storage;
} ExaminationRecordRepository;

Result ExaminationRecordRepository_init(
    ExaminationRecordRepository *repository,
    const char *path
);
Result ExaminationRecordRepository_ensure_storage(
    const ExaminationRecordRepository *repository
);
Result ExaminationRecordRepository_append(
    const ExaminationRecordRepository *repository,
    const ExaminationRecord *record
);
Result ExaminationRecordRepository_find_by_examination_id(
    const ExaminationRecordRepository *repository,
    const char *examination_id,
    ExaminationRecord *out_record
);
Result ExaminationRecordRepository_find_by_visit_id(
    const ExaminationRecordRepository *repository,
    const char *visit_id,
    LinkedList *out_records
);
Result ExaminationRecordRepository_save_all(
    const ExaminationRecordRepository *repository,
    const LinkedList *records
);
void ExaminationRecordRepository_clear_list(LinkedList *records);

#endif
