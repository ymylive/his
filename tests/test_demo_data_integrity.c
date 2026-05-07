#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/LinkedList.h"
#include "domain/Admission.h"
#include "domain/Doctor.h"
#include "domain/InpatientOrder.h"
#include "domain/NursingRecord.h"
#include "domain/Patient.h"
#include "domain/Prescription.h"
#include "domain/User.h"
#include "domain/VisitRecord.h"
#include "repository/AdmissionRepository.h"
#include "repository/DepartmentRepository.h"
#include "repository/DispenseRecordRepository.h"
#include "repository/DoctorRepository.h"
#include "repository/ExaminationRecordRepository.h"
#include "repository/InpatientOrderRepository.h"
#include "repository/MedicineRepository.h"
#include "repository/NursingRecordRepository.h"
#include "repository/PatientRepository.h"
#include "repository/PrescriptionRepository.h"
#include "repository/RegistrationRepository.h"
#include "repository/TextFileRepository.h"
#include "repository/UserRepository.h"
#include "repository/VisitRecordRepository.h"
#include "repository/WardRepository.h"
#include "repository/BedRepository.h"
#include "ui/DemoData.h"

static const char *resolve_demo_data_path(const char *file_name) {
    static char path[512];
    FILE *file = 0;

#ifdef HIS_TEST_SOURCE_DIR
    snprintf(path, sizeof(path), "%s/data/%s", HIS_TEST_SOURCE_DIR, file_name);
    file = fopen(path, "r");
    if (file != 0) {
        fclose(file);
        return path;
    }
#endif

    snprintf(path, sizeof(path), "../data/%s", file_name);
    file = fopen(path, "r");
    if (file != 0) {
        fclose(file);
        return path;
    }

    snprintf(path, sizeof(path), "data/%s", file_name);
    return path;
}

static const char *resolve_demo_seed_path(const char *file_name) {
    static char path[512];
    FILE *file = 0;

#ifdef HIS_TEST_SOURCE_DIR
    snprintf(path, sizeof(path), "%s/data/demo_seed/%s", HIS_TEST_SOURCE_DIR, file_name);
    file = fopen(path, "r");
    if (file != 0) {
        fclose(file);
        return path;
    }
#endif

    snprintf(path, sizeof(path), "../data/demo_seed/%s", file_name);
    file = fopen(path, "r");
    if (file != 0) {
        fclose(file);
        return path;
    }

    snprintf(path, sizeof(path), "data/demo_seed/%s", file_name);
    return path;
}

static void copy_file_contents(const char *source_path, const char *destination_path) {
    FILE *source = fopen(source_path, "rb");
    FILE *destination = fopen(destination_path, "wb");
    char buffer[4096];
    size_t read_bytes = 0;

    assert(source != 0);
    assert(destination != 0);

    while ((read_bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        assert(fwrite(buffer, 1, read_bytes, destination) == read_bytes);
    }

    fclose(source);
    fclose(destination);
}

static void test_demo_accounts_exist(void) {
    UserRepository repository;
    User user;
    Result result = UserRepository_init(&repository, resolve_demo_data_path("users.txt"));

    assert(result.success == 1);
    assert(UserRepository_find_by_user_id(&repository, "ADM0001", &user).success == 1);
    assert(user.role == USER_ROLE_ADMIN);
    assert(UserRepository_find_by_user_id(&repository, "DOC0001", &user).success == 1);
    assert(user.role == USER_ROLE_DOCTOR);
    assert(UserRepository_find_by_user_id(&repository, "INP0001", &user).success == 1);
    assert(user.role == USER_ROLE_INPATIENT_MANAGER);
    assert(UserRepository_find_by_user_id(&repository, "PHA0001", &user).success == 1);
    assert(user.role == USER_ROLE_PHARMACY);
    assert(UserRepository_find_by_user_id(&repository, "linyue", &user).success == 1);
    assert(user.role == USER_ROLE_PATIENT);
    assert(strcmp(user.patient_id, "PAT0001") == 0);
}

static void test_demo_domain_data_has_answering_scale(void) {
    PatientRepository patient_repository;
    DepartmentRepository department_repository;
    DoctorRepository doctor_repository;
    RegistrationRepository registration_repository;
    VisitRecordRepository visit_repository;
    ExaminationRecordRepository examination_repository;
    WardRepository ward_repository;
    BedRepository bed_repository;
    AdmissionRepository admission_repository;
    MedicineRepository medicine_repository;
    DispenseRecordRepository dispense_repository;
    LinkedList patients;
    LinkedList departments;
    LinkedList doctors;
    LinkedList registrations;
    LinkedList visits;
    LinkedList examinations;
    LinkedList wards;
    LinkedList beds;
    LinkedList admissions;
    LinkedList medicines;
    LinkedList dispenses;
    Result result;

    LinkedList_init(&patients);
    LinkedList_init(&departments);
    LinkedList_init(&doctors);
    LinkedList_init(&registrations);
    LinkedList_init(&visits);
    LinkedList_init(&examinations);
    LinkedList_init(&wards);
    LinkedList_init(&beds);
    LinkedList_init(&admissions);
    LinkedList_init(&medicines);
    LinkedList_init(&dispenses);

    assert(PatientRepository_init(&patient_repository, resolve_demo_data_path("patients.txt")).success == 1);
    assert(DepartmentRepository_init(&department_repository, resolve_demo_data_path("departments.txt")).success == 1);
    assert(DoctorRepository_init(&doctor_repository, resolve_demo_data_path("doctors.txt")).success == 1);
    assert(RegistrationRepository_init(&registration_repository, resolve_demo_data_path("registrations.txt")).success == 1);
    assert(VisitRecordRepository_init(&visit_repository, resolve_demo_data_path("visits.txt")).success == 1);
    assert(ExaminationRecordRepository_init(&examination_repository, resolve_demo_data_path("examinations.txt")).success == 1);
    assert(WardRepository_init(&ward_repository, resolve_demo_data_path("wards.txt")).success == 1);
    assert(BedRepository_init(&bed_repository, resolve_demo_data_path("beds.txt")).success == 1);
    assert(AdmissionRepository_init(&admission_repository, resolve_demo_data_path("admissions.txt")).success == 1);
    assert(MedicineRepository_init(&medicine_repository, resolve_demo_data_path("medicines.txt")).success == 1);
    assert(DispenseRecordRepository_init(&dispense_repository, resolve_demo_data_path("dispense_records.txt")).success == 1);

    result = PatientRepository_load_all(&patient_repository, &patients);
    assert(result.success == 1);
    result = DepartmentRepository_load_all(&department_repository, &departments);
    assert(result.success == 1);
    result = DoctorRepository_load_all(&doctor_repository, &doctors);
    assert(result.success == 1);
    result = RegistrationRepository_load_all(&registration_repository, &registrations);
    assert(result.success == 1);
    result = VisitRecordRepository_load_all(&visit_repository, &visits);
    assert(result.success == 1);
    result = ExaminationRecordRepository_load_all(&examination_repository, &examinations);
    assert(result.success == 1);
    result = WardRepository_load_all(&ward_repository, &wards);
    assert(result.success == 1);
    result = BedRepository_load_all(&bed_repository, &beds);
    assert(result.success == 1);
    result = AdmissionRepository_load_all(&admission_repository, &admissions);
    assert(result.success == 1);
    result = MedicineRepository_load_all(&medicine_repository, &medicines);
    assert(result.success == 1);
    result = DispenseRecordRepository_load_all(&dispense_repository, &dispenses);
    assert(result.success == 1);

    assert(LinkedList_count(&patients) >= 20);
    assert(LinkedList_count(&departments) >= 6);
    assert(LinkedList_count(&doctors) >= 10);
    assert(LinkedList_count(&registrations) >= 20);
    assert(LinkedList_count(&visits) >= 14);
    assert(LinkedList_count(&examinations) >= 9);
    assert(LinkedList_count(&wards) >= 5);
    assert(LinkedList_count(&beds) >= 18);
    assert(LinkedList_count(&admissions) >= 5);
    assert(LinkedList_count(&medicines) >= 10);
    assert(LinkedList_count(&dispenses) >= 8);

    PatientRepository_clear_loaded_list(&patients);
    DepartmentRepository_clear_list(&departments);
    DoctorRepository_clear_list(&doctors);
    RegistrationRepository_clear_list(&registrations);
    VisitRecordRepository_clear_list(&visits);
    ExaminationRecordRepository_clear_list(&examinations);
    WardRepository_clear_loaded_list(&wards);
    BedRepository_clear_loaded_list(&beds);
    AdmissionRepository_clear_loaded_list(&admissions);
    MedicineRepository_clear_list(&medicines);
    DispenseRecordRepository_clear_list(&dispenses);
}

static void test_demo_seed_files_exist_and_reset_runtime_files(void) {
    MenuApplicationPaths paths;
    char runtime_users[256];
    char runtime_patients[256];
    char runtime_departments[256];
    char runtime_doctors[256];
    char runtime_registrations[256];
    char runtime_visits[256];
    char runtime_examinations[256];
    char runtime_wards[256];
    char runtime_beds[256];
    char runtime_admissions[256];
    char runtime_medicines[256];
    char runtime_dispenses[256];
    char runtime_prescriptions[256];
    char runtime_inpatient_orders[256];
    char runtime_nursing_records[256];
    char runtime_round_records[256];
    char buffer[256];
    FILE *file = 0;
    char line[256];
    Result result;

#ifdef _WIN32
    system("mkdir build\\demo_seed >NUL 2>NUL");
#else
    system("mkdir -p build/demo_seed");
#endif

    assert(fopen(resolve_demo_seed_path("users.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("patients.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("departments.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("doctors.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("registrations.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("visits.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("examinations.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("wards.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("beds.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("admissions.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("medicines.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("dispense_records.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("nursing_records.txt"), "r") != 0);
    assert(fopen(resolve_demo_seed_path("round_records.txt"), "r") != 0);

    snprintf(runtime_users, sizeof(runtime_users), "build/users.txt");
    snprintf(runtime_patients, sizeof(runtime_patients), "build/patients.txt");
    snprintf(runtime_departments, sizeof(runtime_departments), "build/departments.txt");
    snprintf(runtime_doctors, sizeof(runtime_doctors), "build/doctors.txt");
    snprintf(runtime_registrations, sizeof(runtime_registrations), "build/registrations.txt");
    snprintf(runtime_visits, sizeof(runtime_visits), "build/visits.txt");
    snprintf(runtime_examinations, sizeof(runtime_examinations), "build/examinations.txt");
    snprintf(runtime_wards, sizeof(runtime_wards), "build/wards.txt");
    snprintf(runtime_beds, sizeof(runtime_beds), "build/beds.txt");
    snprintf(runtime_admissions, sizeof(runtime_admissions), "build/admissions.txt");
    snprintf(runtime_medicines, sizeof(runtime_medicines), "build/medicines.txt");
    snprintf(runtime_dispenses, sizeof(runtime_dispenses), "build/dispense_records.txt");
    snprintf(runtime_prescriptions, sizeof(runtime_prescriptions), "build/prescriptions.txt");
    snprintf(runtime_inpatient_orders, sizeof(runtime_inpatient_orders), "build/inpatient_orders.txt");
    snprintf(runtime_nursing_records, sizeof(runtime_nursing_records), "build/nursing_records.txt");
    snprintf(runtime_round_records, sizeof(runtime_round_records), "build/round_records.txt");

    copy_file_contents(resolve_demo_seed_path("users.txt"), "build/demo_seed/users.txt");
    copy_file_contents(resolve_demo_seed_path("patients.txt"), "build/demo_seed/patients.txt");
    copy_file_contents(resolve_demo_seed_path("departments.txt"), "build/demo_seed/departments.txt");
    copy_file_contents(resolve_demo_seed_path("doctors.txt"), "build/demo_seed/doctors.txt");
    copy_file_contents(resolve_demo_seed_path("registrations.txt"), "build/demo_seed/registrations.txt");
    copy_file_contents(resolve_demo_seed_path("visits.txt"), "build/demo_seed/visits.txt");
    copy_file_contents(resolve_demo_seed_path("examinations.txt"), "build/demo_seed/examinations.txt");
    copy_file_contents(resolve_demo_seed_path("wards.txt"), "build/demo_seed/wards.txt");
    copy_file_contents(resolve_demo_seed_path("beds.txt"), "build/demo_seed/beds.txt");
    copy_file_contents(resolve_demo_seed_path("admissions.txt"), "build/demo_seed/admissions.txt");
    copy_file_contents(resolve_demo_seed_path("medicines.txt"), "build/demo_seed/medicines.txt");
    copy_file_contents(resolve_demo_seed_path("dispense_records.txt"), "build/demo_seed/dispense_records.txt");
    copy_file_contents(resolve_demo_seed_path("prescriptions.txt"), "build/demo_seed/prescriptions.txt");
    copy_file_contents(resolve_demo_seed_path("inpatient_orders.txt"), "build/demo_seed/inpatient_orders.txt");
    copy_file_contents(resolve_demo_seed_path("nursing_records.txt"), "build/demo_seed/nursing_records.txt");
    copy_file_contents(resolve_demo_seed_path("round_records.txt"), "build/demo_seed/round_records.txt");

    file = fopen(runtime_users, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_patients, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_departments, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_doctors, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_registrations, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_visits, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_examinations, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_wards, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_beds, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_admissions, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_medicines, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_dispenses, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_prescriptions, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_inpatient_orders, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    file = fopen(runtime_nursing_records, "w");
    assert(file != 0);
    fputs("corrupted\n", file);
    fclose(file);

    paths.user_path = runtime_users;
    paths.patient_path = runtime_patients;
    paths.department_path = runtime_departments;
    paths.doctor_path = runtime_doctors;
    paths.registration_path = runtime_registrations;
    paths.visit_path = runtime_visits;
    paths.examination_path = runtime_examinations;
    paths.ward_path = runtime_wards;
    paths.bed_path = runtime_beds;
    paths.admission_path = runtime_admissions;
    paths.medicine_path = runtime_medicines;
    paths.dispense_record_path = runtime_dispenses;
    paths.prescription_path = runtime_prescriptions;
    paths.inpatient_order_path = runtime_inpatient_orders;
    paths.nursing_record_path = runtime_nursing_records;
    paths.round_record_path = runtime_round_records;

    result = DemoData_reset(&paths, buffer, sizeof(buffer));
    assert(result.success == 1);
    assert(strstr(buffer, "演示数据已重置") != 0);

    file = fopen(runtime_patients, "r");
    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strstr(line, "patient_id|name|gender") != 0);
    fclose(file);

    file = fopen(runtime_registrations, "r");
    assert(file != 0);
    assert(fgets(line, sizeof(line), file) != 0);
    assert(strstr(line, "registration_id|patient_id|doctor_id") != 0);
    fclose(file);
}

/* ========================================================================== */
/* 数据自洽性回归测试：固化"全角色可登录 + 外键完整"质量基线                  */
/* ========================================================================== */

/**
 * @brief 用户记录的精简快照
 *
 * UserRepository 当前未对外暴露 load_all，因此通过
 * TextFileRepository_for_each_data_line 自行收集每条用户记录中
 * 与外键校验相关的字段（user_id、role、patient_id）。
 */
typedef struct UserSnapshot {
    char user_id[HIS_DOMAIN_ID_CAPACITY];
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    int role;
} UserSnapshot;

/**
 * @brief 行处理回调：将 users.txt 中的一行解析成 UserSnapshot 并追加到链表
 */
static Result collect_user_snapshot_line_handler(const char *line, void *context) {
    LinkedList *list = (LinkedList *)context;
    UserSnapshot *snapshot = 0;
    const char *cursor = line;
    const char *separator = 0;
    size_t length = 0;
    int field_index = 0;

    if (line == 0 || list == 0) {
        return Result_make_failure("invalid arguments for user snapshot collector");
    }

    /* 跳过空行（保险，虽然 for_each_data_line 已过滤空行） */
    if (line[0] == '\0') {
        return Result_make_success("empty line skipped");
    }

    snapshot = (UserSnapshot *)malloc(sizeof(UserSnapshot));
    if (snapshot == 0) {
        return Result_make_failure("snapshot alloc failed");
    }
    snapshot->user_id[0] = '\0';
    snapshot->patient_id[0] = '\0';
    snapshot->role = 0;

    while (cursor != 0 && field_index <= 3) {
        separator = strchr(cursor, '|');
        length = (separator != 0) ? (size_t)(separator - cursor) : strlen(cursor);

        if (field_index == 0) {
            size_t copy_len = length < (HIS_DOMAIN_ID_CAPACITY - 1)
                ? length : (HIS_DOMAIN_ID_CAPACITY - 1);
            memcpy(snapshot->user_id, cursor, copy_len);
            snapshot->user_id[copy_len] = '\0';
        } else if (field_index == 2) {
            char role_buffer[16];
            size_t copy_len = length < (sizeof(role_buffer) - 1)
                ? length : (sizeof(role_buffer) - 1);
            memcpy(role_buffer, cursor, copy_len);
            role_buffer[copy_len] = '\0';
            snapshot->role = atoi(role_buffer);
        } else if (field_index == 3) {
            size_t copy_len = length < (HIS_DOMAIN_ID_CAPACITY - 1)
                ? length : (HIS_DOMAIN_ID_CAPACITY - 1);
            memcpy(snapshot->patient_id, cursor, copy_len);
            snapshot->patient_id[copy_len] = '\0';
        }

        if (separator == 0) {
            break;
        }
        cursor = separator + 1;
        field_index++;
    }

    if (LinkedList_append(list, snapshot) == 0) {
        free(snapshot);
        return Result_make_failure("snapshot append failed");
    }

    return Result_make_success("snapshot collected");
}

static int doctor_id_matches(const void *data, const void *context) {
    const Doctor *doctor = (const Doctor *)data;
    const char *target_id = (const char *)context;
    if (doctor == 0 || target_id == 0) {
        return 0;
    }
    return strcmp(doctor->doctor_id, target_id) == 0;
}

static int patient_id_matches(const void *data, const void *context) {
    const Patient *patient = (const Patient *)data;
    const char *target_id = (const char *)context;
    if (patient == 0 || target_id == 0) {
        return 0;
    }
    return strcmp(patient->patient_id, target_id) == 0;
}

static int admission_id_matches(const void *data, const void *context) {
    const Admission *admission = (const Admission *)data;
    const char *target_id = (const char *)context;
    if (admission == 0 || target_id == 0) {
        return 0;
    }
    return strcmp(admission->admission_id, target_id) == 0;
}

static int visit_id_matches(const void *data, const void *context) {
    const VisitRecord *visit = (const VisitRecord *)data;
    const char *target_id = (const char *)context;
    if (visit == 0 || target_id == 0) {
        return 0;
    }
    return strcmp(visit->visit_id, target_id) == 0;
}

/**
 * @brief T1：每个 doctor 都有一个 role=2 且 user_id == doctor_id 的登录账号
 */
static void test_users_doctor_account_coverage(void) {
    DoctorRepository doctor_repository;
    UserRepository user_repository;
    LinkedList doctors;
    User user;
    LinkedListNode *node = 0;
    int missing_count = 0;

    LinkedList_init(&doctors);

    assert(DoctorRepository_init(&doctor_repository, resolve_demo_data_path("doctors.txt")).success == 1);
    assert(UserRepository_init(&user_repository, resolve_demo_data_path("users.txt")).success == 1);
    assert(DoctorRepository_load_all(&doctor_repository, &doctors).success == 1);

    node = doctors.head;
    while (node != 0) {
        const Doctor *doctor = (const Doctor *)node->data;
        Result lookup_result = UserRepository_find_by_user_id(
            &user_repository,
            doctor->doctor_id,
            &user
        );

        if (lookup_result.success == 0 || user.role != USER_ROLE_DOCTOR) {
            printf("[T1] missing doctor login account for doctor_id=%s\n", doctor->doctor_id);
            missing_count++;
        }

        node = node->next;
    }

    DoctorRepository_clear_list(&doctors);

    assert(missing_count == 0);
}

/**
 * @brief T2：每个用户的 linked_id 都指向真实实体
 *
 * - role=2（医生）：user_id 必须存在于 doctors.txt 中的 doctor_id
 * - role=1（患者）：user.patient_id 必须存在于 patients.txt 中的 patient_id
 */
static void test_user_linked_id_referential_integrity(void) {
    UserRepository user_repository;
    DoctorRepository doctor_repository;
    PatientRepository patient_repository;
    LinkedList user_snapshots;
    LinkedList doctors;
    LinkedList patients;
    LinkedListNode *node = 0;
    int dangling_count = 0;

    LinkedList_init(&user_snapshots);
    LinkedList_init(&doctors);
    LinkedList_init(&patients);

    assert(UserRepository_init(&user_repository, resolve_demo_data_path("users.txt")).success == 1);
    assert(DoctorRepository_init(&doctor_repository, resolve_demo_data_path("doctors.txt")).success == 1);
    assert(PatientRepository_init(&patient_repository, resolve_demo_data_path("patients.txt")).success == 1);

    assert(TextFileRepository_for_each_data_line(
        &user_repository.file_repository,
        collect_user_snapshot_line_handler,
        &user_snapshots
    ).success == 1);
    assert(DoctorRepository_load_all(&doctor_repository, &doctors).success == 1);
    assert(PatientRepository_load_all(&patient_repository, &patients).success == 1);

    node = user_snapshots.head;
    while (node != 0) {
        const UserSnapshot *snapshot = (const UserSnapshot *)node->data;

        if (snapshot->role == USER_ROLE_DOCTOR) {
            void *match = LinkedList_find_first(
                &doctors,
                doctor_id_matches,
                snapshot->user_id
            );
            if (match == 0) {
                printf("[T2] doctor user '%s' has no matching doctor record\n",
                    snapshot->user_id);
                dangling_count++;
            }
        } else if (snapshot->role == USER_ROLE_PATIENT) {
            if (snapshot->patient_id[0] == '\0') {
                printf("[T2] patient user '%s' has empty patient_id\n", snapshot->user_id);
                dangling_count++;
            } else {
                void *match = LinkedList_find_first(
                    &patients,
                    patient_id_matches,
                    snapshot->patient_id
                );
                if (match == 0) {
                    printf("[T2] patient user '%s' linked to missing patient '%s'\n",
                        snapshot->user_id, snapshot->patient_id);
                    dangling_count++;
                }
            }
        }

        node = node->next;
    }

    LinkedList_clear(&user_snapshots, free);
    DoctorRepository_clear_list(&doctors);
    PatientRepository_clear_loaded_list(&patients);

    assert(dangling_count == 0);
}

/**
 * @brief T3：每条 prescription 的 doctor_id 字段非空且存在于 doctors.txt
 *
 * Prescription 领域模型已新增 doctor_id 字段（紧邻 visit_id 之后），
 * 直接校验 prescription.doctor_id 非空且能在 doctors.txt 中找到对应记录。
 */
static void test_prescription_doctor_referential_integrity(void) {
    PrescriptionRepository prescription_repository;
    DoctorRepository doctor_repository;
    LinkedList prescriptions;
    LinkedList doctors;
    LinkedListNode *node = 0;
    int broken_count = 0;

    LinkedList_init(&prescriptions);
    LinkedList_init(&doctors);

    assert(PrescriptionRepository_init(&prescription_repository,
        resolve_demo_data_path("prescriptions.txt")).success == 1);
    assert(DoctorRepository_init(&doctor_repository,
        resolve_demo_data_path("doctors.txt")).success == 1);

    assert(PrescriptionRepository_load_all(&prescription_repository, &prescriptions).success == 1);
    assert(DoctorRepository_load_all(&doctor_repository, &doctors).success == 1);

    node = prescriptions.head;
    while (node != 0) {
        const Prescription *prescription = (const Prescription *)node->data;

        if (prescription->doctor_id[0] == '\0') {
            printf("[T3] prescription %s has empty doctor_id\n",
                prescription->prescription_id);
            broken_count++;
        } else {
            void *match = LinkedList_find_first(
                &doctors,
                doctor_id_matches,
                prescription->doctor_id
            );
            if (match == 0) {
                printf("[T3] prescription %s references unknown doctor_id %s\n",
                    prescription->prescription_id, prescription->doctor_id);
                broken_count++;
            }
        }

        node = node->next;
    }

    PrescriptionRepository_clear_list(&prescriptions);
    DoctorRepository_clear_list(&doctors);

    assert(broken_count == 0);
}

/**
 * @brief T4：每条 prescription 的 visit_id 在 visits.txt 中存在
 */
static void test_prescription_visit_referential_integrity(void) {
    PrescriptionRepository prescription_repository;
    VisitRecordRepository visit_repository;
    LinkedList prescriptions;
    LinkedList visits;
    LinkedListNode *node = 0;
    int dangling_count = 0;

    LinkedList_init(&prescriptions);
    LinkedList_init(&visits);

    assert(PrescriptionRepository_init(&prescription_repository,
        resolve_demo_data_path("prescriptions.txt")).success == 1);
    assert(VisitRecordRepository_init(&visit_repository,
        resolve_demo_data_path("visits.txt")).success == 1);

    assert(PrescriptionRepository_load_all(&prescription_repository, &prescriptions).success == 1);
    assert(VisitRecordRepository_load_all(&visit_repository, &visits).success == 1);

    node = prescriptions.head;
    while (node != 0) {
        const Prescription *prescription = (const Prescription *)node->data;

        if (prescription->visit_id[0] == '\0') {
            printf("[T4] prescription %s has empty visit_id\n", prescription->prescription_id);
            dangling_count++;
        } else {
            void *match = LinkedList_find_first(
                &visits,
                visit_id_matches,
                prescription->visit_id
            );
            if (match == 0) {
                printf("[T4] prescription %s -> visit_id %s not found in visits.txt\n",
                    prescription->prescription_id, prescription->visit_id);
                dangling_count++;
            }
        }

        node = node->next;
    }

    PrescriptionRepository_clear_list(&prescriptions);
    VisitRecordRepository_clear_list(&visits);

    assert(dangling_count == 0);
}

/**
 * @brief T5：每条护理记录的 admission_id 必须在 admissions.txt 中存在
 */
static void test_nursing_record_admission_referential_integrity(void) {
    NursingRecordRepository nursing_repository;
    AdmissionRepository admission_repository;
    LinkedList nursing_records;
    LinkedList admissions;
    LinkedListNode *node = 0;
    int dangling_count = 0;

    LinkedList_init(&nursing_records);
    LinkedList_init(&admissions);

    assert(NursingRecordRepository_init(&nursing_repository,
        resolve_demo_data_path("nursing_records.txt")).success == 1);
    assert(AdmissionRepository_init(&admission_repository,
        resolve_demo_data_path("admissions.txt")).success == 1);

    assert(NursingRecordRepository_load_all(&nursing_repository, &nursing_records).success == 1);
    assert(AdmissionRepository_load_all(&admission_repository, &admissions).success == 1);

    node = nursing_records.head;
    while (node != 0) {
        const NursingRecord *record = (const NursingRecord *)node->data;

        if (record->admission_id[0] == '\0') {
            printf("[T5] nursing record %s has empty admission_id\n", record->nursing_id);
            dangling_count++;
        } else {
            void *match = LinkedList_find_first(
                &admissions,
                admission_id_matches,
                record->admission_id
            );
            if (match == 0) {
                printf("[T5] nursing record %s -> admission_id %s not found\n",
                    record->nursing_id, record->admission_id);
                dangling_count++;
            }
        }

        node = node->next;
    }

    NursingRecordRepository_clear_list(&nursing_records);
    AdmissionRepository_clear_loaded_list(&admissions);

    assert(dangling_count == 0);
}

/**
 * @brief T6：每条住院医嘱的 admission_id 必须在 admissions.txt 中存在
 */
static void test_inpatient_order_admission_referential_integrity(void) {
    InpatientOrderRepository order_repository;
    AdmissionRepository admission_repository;
    LinkedList orders;
    LinkedList admissions;
    LinkedListNode *node = 0;
    int dangling_count = 0;

    LinkedList_init(&orders);
    LinkedList_init(&admissions);

    assert(InpatientOrderRepository_init(&order_repository,
        resolve_demo_data_path("inpatient_orders.txt")).success == 1);
    assert(AdmissionRepository_init(&admission_repository,
        resolve_demo_data_path("admissions.txt")).success == 1);

    assert(InpatientOrderRepository_load_all(&order_repository, &orders).success == 1);
    assert(AdmissionRepository_load_all(&admission_repository, &admissions).success == 1);

    node = orders.head;
    while (node != 0) {
        const InpatientOrder *order = (const InpatientOrder *)node->data;

        if (order->admission_id[0] == '\0') {
            printf("[T6] inpatient order %s has empty admission_id\n", order->order_id);
            dangling_count++;
        } else {
            void *match = LinkedList_find_first(
                &admissions,
                admission_id_matches,
                order->admission_id
            );
            if (match == 0) {
                printf("[T6] inpatient order %s -> admission_id %s not found\n",
                    order->order_id, order->admission_id);
                dangling_count++;
            }
        }

        node = node->next;
    }

    InpatientOrderRepository_clear_loaded_list(&orders);
    AdmissionRepository_clear_loaded_list(&admissions);

    assert(dangling_count == 0);
}

int main(void) {
    test_demo_accounts_exist();
    test_demo_domain_data_has_answering_scale();
    test_demo_seed_files_exist_and_reset_runtime_files();
    test_users_doctor_account_coverage();
    test_user_linked_id_referential_integrity();
    test_prescription_doctor_referential_integrity();
    test_prescription_visit_referential_integrity();
    test_nursing_record_admission_referential_integrity();
    test_inpatient_order_admission_referential_integrity();
    return 0;
}
