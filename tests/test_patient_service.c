/**
 * @file test_patient_service.c
 * @brief 患者服务（PatientService）的单元测试文件
 *
 * 本文件测试患者管理服务的核心功能，包括：
 * - 创建患者并通过多种方式查询（按ID、按姓名、按联系方式、按身份证号）
 * - 更新和删除患者记录
 * - 字段校验（年龄、姓名、联系方式、身份证号、住院标记）
 * - 重复联系方式和身份证号的拒绝
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/Patient.h"
#include "service/PatientService.h"

/**
 * @brief 辅助函数：构建测试用的临时文件路径并删除已存在的文件
 */
static void TestPatientService_make_path(
    char *buffer,
    size_t buffer_size,
    const char *test_name
) {
    assert(buffer != 0);
    assert(test_name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_patient_service_data/%s_%ld.txt",
        test_name,
        (long)time(0)
    );
    remove(buffer);
}

/**
 * @brief 辅助函数：构造一个 Patient（患者）对象
 */
static Patient TestPatientService_make_patient(
    const char *patient_id,
    const char *name,
    PatientGender gender,
    int age,
    const char *contact,
    const char *id_card,
    const char *allergy,
    const char *medical_history,
    int is_inpatient,
    const char *remarks
) {
    Patient patient;

    memset(&patient, 0, sizeof(patient));
    strcpy(patient.patient_id, patient_id);
    strcpy(patient.name, name);
    patient.gender = gender;
    patient.age = age;
    strcpy(patient.contact, contact);
    strcpy(patient.id_card, id_card);
    strcpy(patient.allergy, allergy);
    strcpy(patient.medical_history, medical_history);
    patient.is_inpatient = is_inpatient;
    strcpy(patient.remarks, remarks);
    return patient;
}

/**
 * @brief 辅助函数：链表查找谓词 - 判断患者ID是否匹配
 */
static int TestPatientService_has_id(const void *data, const void *context) {
    const Patient *patient = (const Patient *)data;
    const char *patient_id = (const char *)context;

    return strcmp(patient->patient_id, patient_id) == 0;
}

/**
 * @brief 测试创建患者并通过多种方式查询
 *
 * 验证流程：
 * 1. 创建3名患者（其中2名同名为"Alice"）
 * 2. 按ID查询验证详细信息
 * 3. 按姓名查询"Alice"，验证返回2条结果
 * 4. 按联系方式查询验证
 * 5. 按身份证号查询验证
 */
static void test_create_and_query_patient_records(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientService service;
    LinkedList matches;
    Patient patient;
    Result result;

    TestPatientService_make_path(path, sizeof(path), "create_query");
    result = PatientService_init(&service, path);
    assert(result.success == 1);

    /* 创建第1名患者 */
    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT1001",
            "Alice",
            PATIENT_GENDER_FEMALE,
            28,
            "13800000001",
            "110101199001010011",
            "Penicillin",
            "Healthy",
            0,
            "First visit"
        }
    );
    assert(result.success == 1);

    /* 创建第2名患者（同名"Alice"） */
    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT1002",
            "Alice",
            PATIENT_GENDER_FEMALE,
            36,
            "13800000002",
            "110101199202020022",
            "Dust",
            "Asthma",
            1,
            "Ward A"
        }
    );
    assert(result.success == 1);

    /* 创建第3名患者 */
    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT1003",
            "Bob",
            PATIENT_GENDER_MALE,
            41,
            "13800000003",
            "110101198303030033",
            "None",
            "Hypertension",
            0,
            "Follow up"
        }
    );
    assert(result.success == 1);

    /* 按ID查询 */
    result = PatientService_find_patient_by_id(&service, "PAT1002", &patient);
    assert(result.success == 1);
    assert(strcmp(patient.name, "Alice") == 0);
    assert(patient.age == 36);
    assert(strcmp(patient.contact, "13800000002") == 0);
    assert(strcmp(patient.id_card, "110101199202020022") == 0);
    assert(patient.is_inpatient == 1);

    /* 按姓名查询（模糊匹配） */
    LinkedList_init(&matches);
    result = PatientService_find_patients_by_name(&service, "Alice", &matches);
    assert(result.success == 1);
    assert(LinkedList_count(&matches) == 2); /* 两名"Alice" */
    assert(LinkedList_find_first(&matches, TestPatientService_has_id, "PAT1001") != 0);
    assert(LinkedList_find_first(&matches, TestPatientService_has_id, "PAT1002") != 0);
    PatientService_clear_search_results(&matches);

    /* 按联系方式查询 */
    result = PatientService_find_patient_by_contact(&service, "13800000003", &patient);
    assert(result.success == 1);
    assert(strcmp(patient.patient_id, "PAT1003") == 0);

    /* 按身份证号查询 */
    result = PatientService_find_patient_by_id_card(&service, "110101199001010011", &patient);
    assert(result.success == 1);
    assert(strcmp(patient.patient_id, "PAT1001") == 0);
}

/**
 * @brief 测试更新和删除患者记录
 *
 * 验证流程：
 * 1. 创建患者后更新信息
 * 2. 验证更新后的字段正确
 * 3. 验证旧联系方式查不到（已被替换）
 * 4. 删除患者后按ID查不到
 */
static void test_update_and_delete_patient_record(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientService service;
    Patient updated = TestPatientService_make_patient(
        "PAT2001",
        "Carol Updated",
        PATIENT_GENDER_FEMALE,
        52,
        "13800000022",
        "110101197404040044",
        "Pollen",
        "Diabetes",
        1,
        "Ward B"
    );
    Patient loaded;
    Result result;

    TestPatientService_make_path(path, sizeof(path), "update_delete");
    result = PatientService_init(&service, path);
    assert(result.success == 1);

    /* 创建患者 */
    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT2001",
            "Carol",
            PATIENT_GENDER_FEMALE,
            51,
            "13800000021",
            "110101197303030033",
            "Pollen",
            "None",
            0,
            "Observe"
        }
    );
    assert(result.success == 1);

    /* 更新患者信息 */
    result = PatientService_update_patient(&service, &updated);
    assert(result.success == 1);

    /* 验证更新后的字段 */
    result = PatientService_find_patient_by_id(&service, "PAT2001", &loaded);
    assert(result.success == 1);
    assert(strcmp(loaded.name, "Carol Updated") == 0);     /* 姓名已更新 */
    assert(loaded.age == 52);                               /* 年龄已更新 */
    assert(strcmp(loaded.contact, "13800000022") == 0);     /* 联系方式已更新 */
    assert(strcmp(loaded.id_card, "110101197404040044") == 0); /* 身份证号已更新 */
    assert(loaded.is_inpatient == 1);                       /* 住院标记已更新 */

    /* 旧联系方式查不到 */
    result = PatientService_find_patient_by_contact(&service, "13800000021", &loaded);
    assert(result.success == 0);

    /* 删除患者 */
    result = PatientService_delete_patient(&service, "PAT2001");
    assert(result.success == 1);

    /* 删除后按ID查不到 */
    result = PatientService_find_patient_by_id(&service, "PAT2001", &loaded);
    assert(result.success == 0);
}

/**
 * @brief 测试各种无效患者字段被拒绝
 *
 * 验证以下字段校验：
 * 1. 年龄超过150应失败
 * 2. 姓名为空应失败
 * 3. 联系方式格式无效（含字母）应失败
 * 4. 身份证号为空应失败
 * 5. 身份证号格式无效应失败
 * 6. 住院标记不是0或1应失败
 */
static void test_validation_rejects_invalid_patient_fields(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientService service;
    Patient invalid = TestPatientService_make_patient(
        "PAT3001",
        "Valid Name",
        PATIENT_GENDER_MALE,
        20,
        "13800000031",
        "110101199505050055",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    Result result;

    TestPatientService_make_path(path, sizeof(path), "validation");
    result = PatientService_init(&service, path);
    assert(result.success == 1);

    /* 年龄151超过上限 */
    invalid.age = 151;
    result = PatientService_create_patient(&service, &invalid);
    assert(result.success == 0);

    /* 姓名为空 */
    invalid = TestPatientService_make_patient(
        "PAT3002",
        "",
        PATIENT_GENDER_MALE,
        20,
        "13800000032",
        "110101199606060066",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    result = PatientService_create_patient(&service, &invalid);
    assert(result.success == 0);

    /* 联系方式格式无效（含字母） */
    invalid = TestPatientService_make_patient(
        "PAT3003",
        "Derek",
        PATIENT_GENDER_MALE,
        20,
        "13AB",
        "110101199707070077",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    result = PatientService_create_patient(&service, &invalid);
    assert(result.success == 0);

    /* 身份证号为空 */
    invalid = TestPatientService_make_patient(
        "PAT3004",
        "Emma",
        PATIENT_GENDER_FEMALE,
        20,
        "13800000034",
        "",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    result = PatientService_create_patient(&service, &invalid);
    assert(result.success == 0);

    /* 身份证号格式无效 */
    invalid = TestPatientService_make_patient(
        "PAT3005",
        "Fiona",
        PATIENT_GENDER_FEMALE,
        20,
        "13800000035",
        "invalid-card",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    result = PatientService_create_patient(&service, &invalid);
    assert(result.success == 0);

    /* 住院标记不是0或1（值为2） */
    invalid = TestPatientService_make_patient(
        "PAT3006",
        "Gavin",
        PATIENT_GENDER_MALE,
        20,
        "13800000036",
        "110101199808080088",
        "None",
        "Healthy",
        2,
        "Stable"
    );
    result = PatientService_create_patient(&service, &invalid);
    assert(result.success == 0);
}

/**
 * @brief 测试重复联系方式和身份证号被拒绝
 *
 * 验证唯一性约束：
 * 1. 不同患者不能使用相同的联系方式
 * 2. 不同患者不能使用相同的身份证号
 * 3. 更新时也不能将联系方式或身份证号改为已存在的值
 */
static void test_duplicate_contact_and_id_card_are_rejected(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientService service;
    Patient second = TestPatientService_make_patient(
        "PAT4002",
        "Irene",
        PATIENT_GENDER_FEMALE,
        44,
        "13800000042",
        "110101198909090099",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    Result result;

    TestPatientService_make_path(path, sizeof(path), "duplicates");
    result = PatientService_init(&service, path);
    assert(result.success == 1);

    /* 创建第一名患者 */
    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT4001",
            "Henry",
            PATIENT_GENDER_MALE,
            43,
            "13800000041",
            "110101198808080088",
            "None",
            "Healthy",
            0,
            "Stable"
        }
    );
    assert(result.success == 1);

    /* 使用相同联系方式创建第二名患者应失败 */
    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT4003",
            "Jack",
            PATIENT_GENDER_MALE,
            45,
            "13800000041",              /* 与PAT4001相同的联系方式 */
            "110101199010100100",
            "None",
            "Healthy",
            0,
            "Stable"
        }
    );
    assert(result.success == 0);

    /* 使用相同身份证号创建第二名患者应失败 */
    result = PatientService_create_patient(
        &service,
        &(Patient){
            "PAT4004",
            "Kate",
            PATIENT_GENDER_FEMALE,
            46,
            "13800000044",
            "110101198808080088",        /* 与PAT4001相同的身份证号 */
            "None",
            "Healthy",
            0,
            "Stable"
        }
    );
    assert(result.success == 0);

    /* 正常创建第二名患者 */
    result = PatientService_create_patient(&service, &second);
    assert(result.success == 1);

    /* 更新时将联系方式改为已存在的值应失败 */
    second.age = 45;
    strcpy(second.contact, "13800000041"); /* 改为PAT4001的联系方式 */
    result = PatientService_update_patient(&service, &second);
    assert(result.success == 0);

    /* 更新时将身份证号改为已存在的值应失败 */
    second = TestPatientService_make_patient(
        "PAT4002",
        "Irene",
        PATIENT_GENDER_FEMALE,
        44,
        "13800000042",
        "110101198909090099",
        "None",
        "Healthy",
        0,
        "Stable"
    );
    strcpy(second.id_card, "110101198808080088"); /* 改为PAT4001的身份证号 */
    result = PatientService_update_patient(&service, &second);
    assert(result.success == 0);
}

/**
 * @brief 测试主函数，依次运行所有患者服务测试用例
 */
int main(void) {
    test_create_and_query_patient_records();
    test_update_and_delete_patient_record();
    test_validation_rejects_invalid_patient_fields();
    test_duplicate_contact_and_id_card_are_rejected();
    return 0;
}
