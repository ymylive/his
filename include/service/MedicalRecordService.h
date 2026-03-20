#ifndef HIS_SERVICE_MEDICAL_RECORD_SERVICE_H
#define HIS_SERVICE_MEDICAL_RECORD_SERVICE_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Admission.h"
#include "domain/ExaminationRecord.h"
#include "domain/VisitRecord.h"
#include "repository/AdmissionRepository.h"
#include "repository/ExaminationRecordRepository.h"
#include "repository/RegistrationRepository.h"
#include "repository/VisitRecordRepository.h"

typedef struct MedicalRecordHistory {
    LinkedList registrations;
    LinkedList visits;
    LinkedList examinations;
    LinkedList admissions;
} MedicalRecordHistory;

typedef struct MedicalRecordService {
    RegistrationRepository registration_repository;
    VisitRecordRepository visit_repository;
    ExaminationRecordRepository examination_repository;
    AdmissionRepository admission_repository;
} MedicalRecordService;

Result MedicalRecordService_init(
    MedicalRecordService *service,
    const char *registration_path,
    const char *visit_path,
    const char *examination_path,
    const char *admission_path
);
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
);
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
);
Result MedicalRecordService_delete_visit_record(
    MedicalRecordService *service,
    const char *visit_id
);
Result MedicalRecordService_create_examination_record(
    MedicalRecordService *service,
    const char *visit_id,
    const char *exam_item,
    const char *exam_type,
    const char *requested_at,
    ExaminationRecord *out_record
);
Result MedicalRecordService_update_examination_record(
    MedicalRecordService *service,
    const char *examination_id,
    ExaminationStatus status,
    const char *result,
    const char *completed_at,
    ExaminationRecord *out_record
);
Result MedicalRecordService_delete_examination_record(
    MedicalRecordService *service,
    const char *examination_id
);
Result MedicalRecordService_find_patient_history(
    MedicalRecordService *service,
    const char *patient_id,
    MedicalRecordHistory *out_history
);
Result MedicalRecordService_find_records_by_time_range(
    MedicalRecordService *service,
    const char *time_from,
    const char *time_to,
    MedicalRecordHistory *out_history
);
void MedicalRecordHistory_clear(MedicalRecordHistory *history);

#endif
