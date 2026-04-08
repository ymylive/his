/**
 * @file test_patient_repository.c
 * @brief 患者仓储（PatientRepository）的单元测试文件
 *
 * 本文件测试患者数据仓储层的核心功能，包括：
 * - 追加记录和按ID查找
 * - 全量加载（load_all）
 * - 全量重写（save_all）
 * - 重复患者ID的拒绝
 * - 超长字段的加载拒绝
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/LinkedList.h"
#include "domain/Patient.h"
#include "repository/PatientRepository.h"

/**
 * @brief 辅助函数：构建测试用的临时文件路径并删除已存在的文件
 */
static void TestPatientRepository_make_path(
    char *buffer,
    size_t buffer_size,
    const char *test_name
) {
    assert(buffer != 0);
    assert(test_name != 0);

    snprintf(
        buffer,
        buffer_size,
        "build/test_patient_repository_data/%s_%ld.txt",
        test_name,
        (long)time(0)
    );
    remove(buffer); /* 清理旧文件 */
}

/**
 * @brief 辅助函数：构造一个 Patient（患者）对象
 */
static Patient TestPatientRepository_make_patient(
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
 * @param data 链表节点数据（Patient 指针）
 * @param context 期望的患者ID字符串
 * @return 匹配返回1，不匹配返回0
 */
static int TestPatientRepository_has_id(const void *data, const void *context) {
    const Patient *patient = (const Patient *)data;
    const char *expected_id = (const char *)context;

    return strcmp(patient->patient_id, expected_id) == 0;
}

/**
 * @brief 辅助函数：在堆上分配一个 Patient 对象的副本
 */
static Patient *TestPatientRepository_allocate_patient(const Patient *source) {
    Patient *copy = (Patient *)malloc(sizeof(Patient));

    assert(copy != 0);
    *copy = *source;
    return copy;
}

/**
 * @brief 测试追加记录和按ID查找
 *
 * 验证流程：
 * 1. 追加两条患者记录
 * 2. 按ID查找第二条记录，验证所有字段正确
 * 3. 验证文件第一行是正确的表头
 */
static void test_append_and_find_by_id(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientRepository repository;
    Patient expected = TestPatientRepository_make_patient(
        "PAT0002",
        "Bob",
        PATIENT_GENDER_MALE,
        35,
        "13800000001",
        "ID0002",
        "None",
        "Flu",
        1,
        "WardA"
    );
    Patient actual;
    FILE *file = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result;

    TestPatientRepository_make_path(path, sizeof(path), "append_find");
    result = PatientRepository_init(&repository, path);
    assert(result.success == 1);

    /* 追加第一条记录 */
    result = PatientRepository_append(
        &repository,
        &(Patient){
            "PAT0001",
            "Alice",
            PATIENT_GENDER_FEMALE,
            28,
            "13800000000",
            "ID0001",
            "Penicillin",
            "Healthy",
            0,
            "First visit"
        }
    );
    assert(result.success == 1);

    /* 追加第二条记录 */
    result = PatientRepository_append(&repository, &expected);
    assert(result.success == 1);

    /* 按ID查找第二条记录 */
    result = PatientRepository_find_by_id(&repository, "PAT0002", &actual);
    assert(result.success == 1);
    assert(strcmp(actual.patient_id, "PAT0002") == 0);
    assert(strcmp(actual.name, "Bob") == 0);
    assert(actual.gender == PATIENT_GENDER_MALE);
    assert(actual.age == 35);
    assert(strcmp(actual.contact, "13800000001") == 0);
    assert(strcmp(actual.id_card, "ID0002") == 0);
    assert(actual.is_inpatient == 1);
    assert(strcmp(actual.remarks, "WardA") == 0);

    /* 验证文件第一行是正确的表头 */
    file = fopen(path, "r");
    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strcmp(line, PATIENT_REPOSITORY_HEADER "\n") == 0);
    fclose(file);
}

/**
 * @brief 测试全量加载（load_all）
 *
 * 追加两条记录后全量加载，验证记录数量和内容正确。
 */
static void test_load_all_reads_back_records(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientRepository repository;
    LinkedList patients;
    Patient first = TestPatientRepository_make_patient(
        "PAT0101",
        "Carol",
        PATIENT_GENDER_FEMALE,
        41,
        "13800000002",
        "ID0101",
        "Dust",
        "Asthma",
        0,
        "Observe"
    );
    Patient second = TestPatientRepository_make_patient(
        "PAT0102",
        "David",
        PATIENT_GENDER_MALE,
        52,
        "13800000003",
        "ID0102",
        "None",
        "Hypertension",
        1,
        "WardB"
    );
    Patient *loaded = 0;
    Result result;

    TestPatientRepository_make_path(path, sizeof(path), "load_all");
    result = PatientRepository_init(&repository, path);
    assert(result.success == 1);
    assert(PatientRepository_append(&repository, &first).success == 1);
    assert(PatientRepository_append(&repository, &second).success == 1);

    /* 全量加载 */
    LinkedList_init(&patients);
    result = PatientRepository_load_all(&repository, &patients);
    assert(result.success == 1);
    assert(LinkedList_count(&patients) == 2); /* 应有2条记录 */

    /* 在加载结果中查找第二条记录 */
    loaded = (Patient *)LinkedList_find_first(&patients, TestPatientRepository_has_id, "PAT0102");
    assert(loaded != 0);
    assert(strcmp(loaded->name, "David") == 0);
    assert(loaded->age == 52);
    assert(strcmp(loaded->medical_history, "Hypertension") == 0);

    PatientRepository_clear_loaded_list(&patients);
}

/**
 * @brief 测试全量重写（save_all）
 *
 * 先追加一条记录，再用 save_all 写入另一条记录，
 * 验证文件内容被完全替换（旧记录被覆盖）。
 */
static void test_save_all_rewrites_table(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientRepository repository;
    LinkedList patients;
    Patient first = TestPatientRepository_make_patient(
        "PAT0201",
        "Eva",
        PATIENT_GENDER_FEMALE,
        30,
        "13800000004",
        "ID0201",
        "Seafood",
        "None",
        0,
        "Follow up"
    );
    Patient second = TestPatientRepository_make_patient(
        "PAT0202",
        "Frank",
        PATIENT_GENDER_MALE,
        46,
        "13800000005",
        "ID0202",
        "None",
        "Diabetes",
        1,
        "WardC"
    );
    FILE *file = 0;
    char line[TEXT_FILE_REPOSITORY_LINE_CAPACITY];
    Result result;

    TestPatientRepository_make_path(path, sizeof(path), "save_all");
    result = PatientRepository_init(&repository, path);
    assert(result.success == 1);
    assert(PatientRepository_append(&repository, &first).success == 1); /* 先追加旧记录 */

    /* 用 save_all 全量重写为新记录 */
    LinkedList_init(&patients);
    assert(LinkedList_append(&patients, TestPatientRepository_allocate_patient(&second)) == 1);

    result = PatientRepository_save_all(&repository, &patients);
    assert(result.success == 1);

    /* 逐行读取文件，验证内容已被完全替换 */
    file = fopen(path, "r");
    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strcmp(line, PATIENT_REPOSITORY_HEADER "\n") == 0);     /* 第一行是表头 */
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strcmp(line, "PAT0202|Frank|1|46|13800000005|ID0202|None|Diabetes|1|WardC\n") == 0); /* 新数据 */
    assert(fgets(line, sizeof(line), file) == 0);                  /* 没有更多行（旧数据被覆盖） */
    fclose(file);

    PatientRepository_clear_loaded_list(&patients);
}

/**
 * @brief 测试重复患者ID被拒绝
 *
 * 追加一条记录后，使用相同ID再次追加应失败，
 * 且原记录不被修改。
 */
static void test_duplicate_patient_id_is_rejected(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientRepository repository;
    LinkedList patients;
    Patient *loaded = 0;
    Result result;

    TestPatientRepository_make_path(path, sizeof(path), "duplicate");
    result = PatientRepository_init(&repository, path);
    assert(result.success == 1);

    /* 追加第一条记录 */
    result = PatientRepository_append(
        &repository,
        &(Patient){
            "PAT0301",
            "Grace",
            PATIENT_GENDER_FEMALE,
            33,
            "13800000006",
            "ID0301",
            "Pollen",
            "None",
            0,
            "Stable"
        }
    );
    assert(result.success == 1);

    /* 使用相同ID追加应失败 */
    result = PatientRepository_append(
        &repository,
        &(Patient){
            "PAT0301",
            "Grace Updated",
            PATIENT_GENDER_FEMALE,
            34,
            "13800009999",
            "ID0301X",
            "Pollen",
            "Cold",
            1,
            "Changed"
        }
    );
    assert(result.success == 0);

    /* 验证原记录未被修改 */
    LinkedList_init(&patients);
    result = PatientRepository_load_all(&repository, &patients);
    assert(result.success == 1);
    assert(LinkedList_count(&patients) == 1); /* 仍然只有1条记录 */

    loaded = (Patient *)LinkedList_find_first(&patients, TestPatientRepository_has_id, "PAT0301");
    assert(loaded != 0);
    assert(strcmp(loaded->name, "Grace") == 0); /* 姓名未变 */
    assert(loaded->age == 33);                  /* 年龄未变 */

    PatientRepository_clear_loaded_list(&patients);
}

/**
 * @brief 测试加载包含超长字段的持久化数据时被拒绝
 *
 * 手动写入一条姓名超过字段容量的记录，加载时应报错。
 */
static void test_load_all_rejects_overlong_persisted_field(void) {
    char path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    PatientRepository repository;
    LinkedList patients;
    char overlong_name[HIS_DOMAIN_NAME_CAPACITY + 32]; /* 超过容量的姓名 */
    FILE *file = 0;
    Result result;

    /* 生成超长姓名字符串 */
    memset(overlong_name, 'N', sizeof(overlong_name) - 1);
    overlong_name[sizeof(overlong_name) - 1] = '\0';

    TestPatientRepository_make_path(path, sizeof(path), "reject_overlong");
    result = PatientRepository_init(&repository, path);
    assert(result.success == 1);

    /* 手动写入包含超长字段的数据 */
    file = fopen(path, "w");
    assert(file != 0);
    assert(fputs(PATIENT_REPOSITORY_HEADER "\n", file) != EOF);
    assert(
        fprintf(
            file,
            "PAT0401|%s|1|40|13800000007|ID0401|None|History|0|Remark\n",
            overlong_name
        ) > 0
    );
    fclose(file);

    /* 加载时应失败 */
    LinkedList_init(&patients);
    result = PatientRepository_load_all(&repository, &patients);
    assert(result.success == 0);
}

/**
 * @brief 测试主函数，依次运行所有患者仓储测试用例
 */
int main(void) {
    test_append_and_find_by_id();
    test_load_all_reads_back_records();
    test_save_all_rewrites_table();
    test_duplicate_patient_id_is_rejected();
    test_load_all_rejects_overlong_persisted_field();
    return 0;
}
