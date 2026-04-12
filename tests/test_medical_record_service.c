/**
 * @file test_medical_record_service.c
 * @brief 病历服务（MedicalRecordService）的单元测试文件
 *
 * 本文件测试病历管理服务的核心功能，包括：
 * - 就诊记录的创建、更新和删除
 * - 删除就诊记录时检查关联检查记录的约束
 * - 检查记录的创建、更新和删除
 * - 按患者查询完整病历（挂号、就诊、检查、入院四类记录）
 * - 按时间范围查询病历记录
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/Admission.h"
#include "domain/ExaminationRecord.h"
#include "domain/Registration.h"
#include "domain/VisitRecord.h"
#include "repository/AdmissionRepository.h"
#include "repository/ExaminationRecordRepository.h"
#include "repository/RegistrationRepository.h"
#include "repository/VisitRecordRepository.h"
#include "service/MedicalRecordService.h"

/**
 * @brief 病历服务测试上下文结构体
 *
 * 封装测试所需的文件路径、仓储和服务对象。
 */
typedef struct MedicalRecordServiceTestContext {
    char registration_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];   /* 挂号记录文件路径 */
    char visit_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];          /* 就诊记录文件路径 */
    char examination_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];    /* 检查记录文件路径 */
    char admission_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];      /* 入院记录文件路径 */
    RegistrationRepository registration_repository;               /* 挂号仓储 */
    VisitRecordRepository visit_repository;                       /* 就诊仓储 */
    ExaminationRecordRepository examination_repository;           /* 检查仓储 */
    AdmissionRepository admission_repository;                     /* 入院仓储 */
    MedicalRecordService service;                                 /* 病历服务 */
} MedicalRecordServiceTestContext;

/**
 * @brief 辅助函数：构建测试用的临时文件路径
 */
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
        "build/test_medical_record_service_data/%s_%ld_%lu_%s",
        test_name,
        (long)time(0),
        (unsigned long)clock(),
        file_name
    );
}

/**
 * @brief 辅助函数：安全地复制字符串
 */
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

/**
 * @brief 辅助函数：构造一个 Registration（挂号记录）对象，状态默认为待诊
 */
static Registration make_registration(
    const char *registration_id,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at
) {
    Registration registration;

    memset(&registration, 0, sizeof(registration));
    copy_text(
        registration.registration_id,
        sizeof(registration.registration_id),
        registration_id
    );
    copy_text(registration.patient_id, sizeof(registration.patient_id), patient_id);
    copy_text(registration.doctor_id, sizeof(registration.doctor_id), doctor_id);
    copy_text(registration.department_id, sizeof(registration.department_id), department_id);
    copy_text(registration.registered_at, sizeof(registration.registered_at), registered_at);
    registration.status = REG_STATUS_PENDING; /* 默认为待诊状态 */
    return registration;
}

/**
 * @brief 辅助函数：构造一个 Admission（入院记录）对象
 */
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

/**
 * @brief 辅助函数：获取链表中指定索引位置的 Registration 指针
 */
static const Registration *registration_at(const LinkedList *list, size_t index) {
    const LinkedListNode *node = 0;
    size_t current_index = 0;

    assert(list != 0);
    node = list->head;
    while (node != 0) {
        if (current_index == index) {
            return (const Registration *)node->data;
        }

        node = node->next;
        current_index++;
    }

    return 0;
}

/**
 * @brief 辅助函数：获取链表中指定索引位置的 VisitRecord 指针
 */
static const VisitRecord *visit_at(const LinkedList *list, size_t index) {
    const LinkedListNode *node = 0;
    size_t current_index = 0;

    assert(list != 0);
    node = list->head;
    while (node != 0) {
        if (current_index == index) {
            return (const VisitRecord *)node->data;
        }

        node = node->next;
        current_index++;
    }

    return 0;
}

/**
 * @brief 辅助函数：获取链表中指定索引位置的 ExaminationRecord 指针
 */
static const ExaminationRecord *examination_at(const LinkedList *list, size_t index) {
    const LinkedListNode *node = 0;
    size_t current_index = 0;

    assert(list != 0);
    node = list->head;
    while (node != 0) {
        if (current_index == index) {
            return (const ExaminationRecord *)node->data;
        }

        node = node->next;
        current_index++;
    }

    return 0;
}

/**
 * @brief 辅助函数：获取链表中指定索引位置的 Admission 指针
 */
static const Admission *admission_at(const LinkedList *list, size_t index) {
    const LinkedListNode *node = 0;
    size_t current_index = 0;

    assert(list != 0);
    node = list->head;
    while (node != 0) {
        if (current_index == index) {
            return (const Admission *)node->data;
        }

        node = node->next;
        current_index++;
    }

    return 0;
}

/**
 * @brief 辅助函数：初始化病历服务测试上下文
 */
static void setup_context(MedicalRecordServiceTestContext *context, const char *test_name) {
    Result result;

    memset(context, 0, sizeof(*context));
    build_test_path(
        context->registration_path,
        sizeof(context->registration_path),
        test_name,
        "registrations.txt"
    );
    build_test_path(context->visit_path, sizeof(context->visit_path), test_name, "visits.txt");
    build_test_path(
        context->examination_path,
        sizeof(context->examination_path),
        test_name,
        "examinations.txt"
    );
    build_test_path(
        context->admission_path,
        sizeof(context->admission_path),
        test_name,
        "admissions.txt"
    );

    /* 初始化各仓储 */
    result = RegistrationRepository_init(
        &context->registration_repository,
        context->registration_path
    );
    assert(result.success == 1);
    result = VisitRecordRepository_init(&context->visit_repository, context->visit_path);
    assert(result.success == 1);
    result = ExaminationRecordRepository_init(
        &context->examination_repository,
        context->examination_path
    );
    assert(result.success == 1);
    result = AdmissionRepository_init(&context->admission_repository, context->admission_path);
    assert(result.success == 1);

    /* 初始化病历服务 */
    result = MedicalRecordService_init(
        &context->service,
        context->registration_path,
        context->visit_path,
        context->examination_path,
        context->admission_path
    );
    assert(result.success == 1);
}

/**
 * @brief 辅助函数：在挂号仓储中预埋一条挂号记录
 */
static void seed_registration(
    MedicalRecordServiceTestContext *context,
    const char *registration_id,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at
) {
    Registration registration = make_registration(
        registration_id,
        patient_id,
        doctor_id,
        department_id,
        registered_at
    );

    assert(
        RegistrationRepository_append(&context->registration_repository, &registration).success == 1
    );
}

/**
 * @brief 辅助函数：在入院仓储中预埋一条入院记录
 */
static void seed_admission(
    MedicalRecordServiceTestContext *context,
    const char *admission_id,
    const char *patient_id,
    const char *bed_id,
    const char *admitted_at
) {
    Admission admission = make_admission(
        admission_id,
        patient_id,
        "WAR0001",
        bed_id,
        admitted_at,
        ADMISSION_STATUS_ACTIVE,
        "",
        "Observation"
    );

    assert(AdmissionRepository_append(&context->admission_repository, &admission).success == 1);
}

/**
 * @brief 测试就诊记录的创建、更新和删除
 *
 * 验证流程：
 * 1. 创建就诊记录，验证自动分配的ID和关联的患者ID
 * 2. 验证挂号记录状态自动变为已诊断
 * 3. 更新就诊记录的诊断信息
 * 4. 删除就诊记录后验证查不到
 * 5. 删除就诊记录后挂号状态回退为待诊
 */
static void test_create_update_and_delete_visit_record(void) {
    MedicalRecordServiceTestContext context;
    VisitRecord created;
    VisitRecord updated;
    Registration registration;
    Result result;

    setup_context(&context, "visit_crud");
    seed_registration(&context, "REG0001", "PAT0001", "DOC0001", "DEP0001", "2026-03-20T08:00");

    /* 创建就诊记录 */
    {
        VisitRecordParams params = {0};
        params.registration_id = "REG0001";
        params.chief_complaint = "Fever";         /* 主诉 */
        params.diagnosis = "Influenza";           /* 诊断 */
        params.advice = "Rest";                   /* 医嘱 */
        params.need_exam = 1;                     /* 需要检查 */
        params.need_admission = 0;                /* 不需要住院 */
        params.need_medicine = 1;                 /* 需要开处方 */
        params.visit_time = "2026-03-20T08:30";
        result = MedicalRecordService_create_visit_record(
            &context.service,
            &params,
            &created
        );
    }
    assert(result.success == 1);
    assert(strcmp(created.visit_id, "VIS0001") == 0);     /* 自动分配的ID */
    assert(strcmp(created.patient_id, "PAT0001") == 0);   /* 从挂号记录继承的患者ID */
    assert(created.need_exam == 1);                        /* 需要检查标记 */

    /* 验证挂号记录状态已自动变为已诊断 */
    result = RegistrationRepository_find_by_registration_id(
        &context.registration_repository,
        "REG0001",
        &registration
    );
    assert(result.success == 1);
    assert(registration.status == REG_STATUS_DIAGNOSED);
    assert(strcmp(registration.diagnosed_at, "2026-03-20T08:30") == 0);

    /* 更新就诊记录 */
    {
        VisitRecordParams params = {0};
        params.chief_complaint = "High fever";         /* 更新主诉 */
        params.diagnosis = "Pneumonia";                /* 更新诊断 */
        params.advice = "Ward observation";            /* 更新医嘱 */
        params.need_exam = 1;
        params.need_admission = 1;                     /* 现在需要住院 */
        params.need_medicine = 1;
        params.visit_time = "2026-03-20T09:00";
        result = MedicalRecordService_update_visit_record(
            &context.service,
            "VIS0001",
            &params,
            &updated
        );
    }
    assert(result.success == 1);
    assert(strcmp(updated.diagnosis, "Pneumonia") == 0);   /* 诊断已更新 */
    assert(updated.need_admission == 1);                   /* 住院标记已更新 */

    /* 从仓储读取验证持久化 */
    result = VisitRecordRepository_find_by_visit_id(
        &context.visit_repository,
        "VIS0001",
        &created
    );
    assert(result.success == 1);
    assert(strcmp(created.chief_complaint, "High fever") == 0);       /* 主诉已更新 */
    assert(strcmp(created.visit_time, "2026-03-20T09:00") == 0);     /* 时间已更新 */

    /* 删除就诊记录 */
    result = MedicalRecordService_delete_visit_record(&context.service, "VIS0001");
    assert(result.success == 1);

    /* 验证删除后查不到 */
    assert(
        VisitRecordRepository_find_by_visit_id(&context.visit_repository, "VIS0001", &created)
            .success == 0
    );

    /* 验证挂号状态回退为待诊 */
    result = RegistrationRepository_find_by_registration_id(
        &context.registration_repository,
        "REG0001",
        &registration
    );
    assert(result.success == 1);
    assert(registration.status == REG_STATUS_PENDING);        /* 状态回退 */
    assert(strcmp(registration.diagnosed_at, "") == 0);        /* 诊断时间清空 */
}

/**
 * @brief 测试存在关联检查记录时拒绝删除就诊记录
 *
 * 当就诊记录下存在检查记录时，不允许删除就诊记录。
 */
static void test_delete_visit_rejects_when_examination_exists(void) {
    MedicalRecordServiceTestContext context;
    VisitRecord visit;
    ExaminationRecord examination;
    Result result;

    setup_context(&context, "visit_delete_guard");
    seed_registration(&context, "REG0001", "PAT0001", "DOC0001", "DEP0001", "2026-03-20T08:00");

    /* 创建就诊记录 */
    {
        VisitRecordParams params = {0};
        params.registration_id = "REG0001";
        params.chief_complaint = "Cough";
        params.diagnosis = "Bronchitis";
        params.advice = "Chest exam";
        params.need_exam = 1;
        params.need_admission = 0;
        params.need_medicine = 0;
        params.visit_time = "2026-03-20T08:20";
        result = MedicalRecordService_create_visit_record(
            &context.service,
            &params,
            &visit
        );
    }
    assert(result.success == 1);

    /* 为该就诊记录创建一条检查记录 */
    result = MedicalRecordService_create_examination_record(
        &context.service,
        visit.visit_id,
        "Chest CT",
        "Imaging",
        500.00,
        "2026-03-20T08:40",
        &examination
    );
    assert(result.success == 1);

    /* 存在关联检查记录时，删除就诊记录应失败 */
    result = MedicalRecordService_delete_visit_record(&context.service, visit.visit_id);
    assert(result.success == 0);
}

/**
 * @brief 测试检查记录的创建、更新和删除
 *
 * 验证流程：
 * 1. 创建检查记录，状态默认为待检查
 * 2. 更新为已完成但缺少结果应失败
 * 3. 更新为已完成并提供结果应成功
 * 4. 删除检查记录后查不到
 */
static void test_create_update_and_delete_examination_record(void) {
    MedicalRecordServiceTestContext context;
    VisitRecord visit;
    ExaminationRecord examination;
    Result result;

    setup_context(&context, "exam_crud");
    seed_registration(&context, "REG0001", "PAT0001", "DOC0001", "DEP0001", "2026-03-20T08:00");

    /* 先创建就诊记录 */
    {
        VisitRecordParams params = {0};
        params.registration_id = "REG0001";
        params.chief_complaint = "Headache";
        params.diagnosis = "Migraine";
        params.advice = "Observe";
        params.need_exam = 1;
        params.need_admission = 0;
        params.need_medicine = 0;
        params.visit_time = "2026-03-20T08:15";
        result = MedicalRecordService_create_visit_record(
            &context.service,
            &params,
            &visit
        );
    }
    assert(result.success == 1);

    /* 创建检查记录 */
    result = MedicalRecordService_create_examination_record(
        &context.service,
        visit.visit_id,
        "Blood test",
        "Lab",
        30.00,
        "2026-03-20T08:25",
        &examination
    );
    assert(result.success == 1);
    assert(strcmp(examination.examination_id, "EXM0001") == 0);  /* 自动分配的ID */
    assert(examination.status == EXAM_STATUS_PENDING);           /* 默认为待检查 */

    /* 标记为已完成但结果为空，应失败 */
    result = MedicalRecordService_update_examination_record(
        &context.service,
        examination.examination_id,
        EXAM_STATUS_COMPLETED,
        "",                  /* 结果为空 */
        "2026-03-20T09:10",
        &examination
    );
    assert(result.success == 0);

    /* 标记为已完成并提供结果，应成功 */
    result = MedicalRecordService_update_examination_record(
        &context.service,
        examination.examination_id,
        EXAM_STATUS_COMPLETED,
        "Normal",
        "2026-03-20T09:10",
        &examination
    );
    assert(result.success == 1);
    assert(examination.status == EXAM_STATUS_COMPLETED);       /* 状态已更新 */
    assert(strcmp(examination.result, "Normal") == 0);          /* 结果已设置 */

    /* 删除检查记录 */
    result = MedicalRecordService_delete_examination_record(
        &context.service,
        examination.examination_id
    );
    assert(result.success == 1);

    /* 验证删除后查不到 */
    assert(
        ExaminationRecordRepository_find_by_examination_id(
            &context.examination_repository,
            "EXM0001",
            &examination
        ).success == 0
    );
}

/**
 * @brief 测试按患者查询完整病历（四类记录）
 *
 * 预埋两个患者的数据，查询 PAT0001 的完整病历，
 * 验证返回的挂号、就诊、检查、入院各类记录数量和ID正确，
 * 且不包含其他患者的记录。
 */
static void test_history_query_collects_four_record_types(void) {
    MedicalRecordServiceTestContext context;
    MedicalRecordHistory history;
    VisitRecord visit;
    ExaminationRecord examination;
    Result result;

    setup_context(&context, "patient_history");

    /* 预埋两个患者的挂号和入院记录 */
    seed_registration(&context, "REG0001", "PAT0001", "DOC0001", "DEP0001", "2026-03-20T08:00");
    seed_registration(&context, "REG0002", "PAT0002", "DOC0002", "DEP0002", "2026-03-20T09:00");
    seed_admission(&context, "ADM0001", "PAT0001", "BED0001", "2026-03-20T10:00");
    seed_admission(&context, "ADM0002", "PAT0002", "BED0002", "2026-03-21T10:00");

    /* 为 PAT0001 创建就诊记录和检查记录 */
    {
        VisitRecordParams params = {0};
        params.registration_id = "REG0001";
        params.chief_complaint = "Pain";
        params.diagnosis = "Appendicitis";
        params.advice = "Admit";
        params.need_exam = 1;
        params.need_admission = 1;
        params.need_medicine = 0;
        params.visit_time = "2026-03-20T08:30";
        result = MedicalRecordService_create_visit_record(
            &context.service,
            &params,
            &visit
        );
    }
    assert(result.success == 1);

    result = MedicalRecordService_create_examination_record(
        &context.service,
        visit.visit_id,
        "Ultrasound",
        "Imaging",
        200.00,
        "2026-03-20T08:45",
        &examination
    );
    assert(result.success == 1);

    /* 查询 PAT0001 的完整病历 */
    result = MedicalRecordService_find_patient_history(&context.service, "PAT0001", &history);
    assert(result.success == 1);
    assert(LinkedList_count(&history.registrations) == 1);   /* 1条挂号记录 */
    assert(LinkedList_count(&history.visits) == 1);          /* 1条就诊记录 */
    assert(LinkedList_count(&history.examinations) == 1);    /* 1条检查记录 */
    assert(LinkedList_count(&history.admissions) == 1);      /* 1条入院记录 */

    /* 验证各记录的ID正确 */
    assert(strcmp(registration_at(&history.registrations, 0)->registration_id, "REG0001") == 0);
    assert(strcmp(visit_at(&history.visits, 0)->visit_id, visit.visit_id) == 0);
    assert(
        strcmp(
            examination_at(&history.examinations, 0)->examination_id,
            examination.examination_id
        ) == 0
    );
    assert(strcmp(admission_at(&history.admissions, 0)->admission_id, "ADM0001") == 0);
    MedicalRecordHistory_clear(&history);
}

/**
 * @brief 测试按时间范围查询病历记录
 *
 * 验证时间范围过滤：
 * - 挂号时间在范围外的不返回
 * - 就诊时间在范围内的返回
 * - 检查时间在范围内的返回
 * - 入院时间在范围外的不返回
 */
static void test_time_range_query_filters_occurrence_time(void) {
    MedicalRecordServiceTestContext context;
    MedicalRecordHistory history;
    VisitRecord visit;
    ExaminationRecord examination;
    Result result;

    setup_context(&context, "time_range");

    /* 预埋不同时间的挂号和入院记录 */
    seed_registration(&context, "REG0001", "PAT0001", "DOC0001", "DEP0001", "2026-03-20T08:00");
    seed_registration(&context, "REG0002", "PAT0002", "DOC0002", "DEP0002", "2026-03-25T08:00");
    seed_admission(&context, "ADM0001", "PAT0001", "BED0001", "2026-03-23T08:00");
    seed_admission(&context, "ADM0002", "PAT0002", "BED0002", "2026-03-26T08:00");

    /* 就诊时间为 3/21，检查时间为 3/22（均在查询范围内） */
    {
        VisitRecordParams params = {0};
        params.registration_id = "REG0001";
        params.chief_complaint = "Nausea";
        params.diagnosis = "Gastritis";
        params.advice = "Diet";
        params.need_exam = 1;
        params.need_admission = 0;
        params.need_medicine = 1;
        params.visit_time = "2026-03-21T09:00";
        result = MedicalRecordService_create_visit_record(
            &context.service,
            &params,
            &visit
        );
    }
    assert(result.success == 1);

    result = MedicalRecordService_create_examination_record(
        &context.service,
        visit.visit_id,
        "Gastroscopy",
        "Endoscopy",
        300.00,
        "2026-03-22T11:00",
        &examination
    );
    assert(result.success == 1);

    /* 查询时间范围：3/21 ~ 3/22 */
    result = MedicalRecordService_find_records_by_time_range(
        &context.service,
        "2026-03-21T00:00",
        "2026-03-22T23:59",
        &history
    );
    assert(result.success == 1);
    assert(LinkedList_count(&history.registrations) == 0);   /* 挂号时间在范围外 */
    assert(LinkedList_count(&history.visits) == 1);          /* 就诊时间在范围内 */
    assert(LinkedList_count(&history.examinations) == 1);    /* 检查时间在范围内 */
    assert(LinkedList_count(&history.admissions) == 0);      /* 入院时间在范围外 */

    /* 验证返回记录的ID正确 */
    assert(strcmp(visit_at(&history.visits, 0)->visit_id, visit.visit_id) == 0);
    assert(
        strcmp(
            examination_at(&history.examinations, 0)->examination_id,
            examination.examination_id
        ) == 0
    );
    MedicalRecordHistory_clear(&history);
}

/**
 * @brief 测试主函数，依次运行所有病历服务测试用例
 */
int main(void) {
    test_create_update_and_delete_visit_record();
    test_delete_visit_rejects_when_examination_exists();
    test_create_update_and_delete_examination_record();
    test_history_query_collects_four_record_types();
    test_time_range_query_filters_occurrence_time();
    return 0;
}
