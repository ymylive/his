/**
 * @file test_domain.c
 * @brief 领域模型（Domain）的单元测试文件
 *
 * 本文件测试各领域实体的基本行为和校验逻辑，包括：
 * - 挂号记录（Registration）的状态流转：待诊 -> 已诊断/已取消
 * - 处方（Prescription）的数量边界校验
 * - 各领域实体（科室、医生、患者、就诊记录、检查记录、处方、发药记录、药品）的存在性和字段验证
 */

#include <assert.h>

#include "domain/Department.h"
#include "domain/DispenseRecord.h"
#include "domain/Doctor.h"
#include "domain/ExaminationRecord.h"
#include "domain/Medicine.h"
#include "domain/Patient.h"
#include "domain/Prescription.h"
#include "domain/Registration.h"
#include "domain/VisitRecord.h"

/**
 * @brief 测试挂号记录的状态流转逻辑
 *
 * 验证规则：
 * 1. 待诊（PENDING）状态可标记为已诊断（DIAGNOSED）
 * 2. 已诊断状态不能再取消
 * 3. 待诊状态可取消（CANCELLED）
 * 4. 已取消状态不能再标记为已诊断
 */
static void test_registration_status_flow(void) {
    /* 创建一个待诊状态的挂号记录 */
    Registration pending = {
        "REG0001",
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "2026-03-20",
        REG_STATUS_PENDING,
        "",
        ""
    };
    /* 创建另一个待诊状态的挂号记录（用于测试取消流程） */
    Registration cancelled = {
        "REG0002",
        "PAT0002",
        "DOC0002",
        "DEP0001",
        "2026-03-20",
        REG_STATUS_PENDING,
        "",
        ""
    };

    /* 待诊 -> 已诊断 */
    assert(pending.status == REG_STATUS_PENDING);
    assert(Registration_mark_diagnosed(&pending, "2026-03-20T09:10") == 1);  /* 标记诊断成功 */
    assert(pending.status == REG_STATUS_DIAGNOSED);
    assert(pending.diagnosed_at[0] == '2');  /* 诊断时间已设置 */

    /* 已诊断状态不允许取消 */
    assert(Registration_cancel(&pending, "2026-03-20T09:20") == 0);
    assert(pending.status == REG_STATUS_DIAGNOSED);   /* 状态未变 */
    assert(pending.cancelled_at[0] == '\0');           /* 取消时间未设置 */

    /* 待诊 -> 已取消 */
    assert(Registration_cancel(&cancelled, "2026-03-20T09:30") == 1);  /* 取消成功 */
    assert(cancelled.status == REG_STATUS_CANCELLED);
    assert(cancelled.cancelled_at[0] == '2');  /* 取消时间已设置 */

    /* 已取消状态不允许标记为已诊断 */
    assert(Registration_mark_diagnosed(&cancelled, "2026-03-20T09:40") == 0);
    assert(cancelled.status == REG_STATUS_CANCELLED);  /* 状态未变 */
}

/**
 * @brief 测试处方数量的边界校验
 *
 * 验证规则：
 * - 数量为1且库存为5：有效
 * - 数量为0：无效（数量必须大于0）
 * - 数量为-1：无效（数量不能为负）
 * - 数量为6但库存为5：无效（超过库存）
 */
static void test_prescription_quantity_boundary(void) {
    assert(Prescription_has_valid_quantity(1, 5) == 1);   /* 正常数量 */
    assert(Prescription_has_valid_quantity(0, 5) == 0);   /* 零数量无效 */
    assert(Prescription_has_valid_quantity(-1, 5) == 0);  /* 负数量无效 */
    assert(Prescription_has_valid_quantity(6, 5) == 0);   /* 超过库存无效 */
}

/**
 * @brief 测试各领域实体的存在性和字段校验
 *
 * 创建各种领域对象，验证：
 * - 各字段可正常赋值和读取
 * - 患者年龄校验（0-150有效，151无效）
 * - 药品库存校验
 */
static void test_domain_entities_exist(void) {
    /* 科室实体 */
    Department department = {"DEP0001", "Cardiology", "Floor2", "Heart care"};
    /* 医生实体 */
    Doctor doctor = {
        "DOC0001",
        "Dr.Alice",
        "Chief",
        "DEP0001",
        "Mon-Fri",
        DOCTOR_STATUS_ACTIVE
    };
    /* 患者实体 */
    Patient patient = {
        "PAT0001",
        "Bob",
        PATIENT_GENDER_MALE,
        36,
        "13800000000",
        "ID1234567890",
        "Penicillin",
        "Diabetic history",
        0,
        "None"
    };
    /* 就诊记录实体 */
    VisitRecord visit = {
        "VIS0001",
        "REG0001",
        "PAT0001",
        "DOC0001",
        "DEP0001",
        "Headache",
        "Migraine",
        "Plenty of rest",
        1,       /* need_exam: 需要检查 */
        0,       /* need_admission: 不需要住院 */
        1,       /* need_prescription: 需要开处方 */
        "2026-03-20T09:00"
    };
    /* 检查记录实体 */
    ExaminationRecord exam = {
        "EXM0001",
        "VIS0001",
        "PAT0001",
        "DOC0001",
        "CT scan",
        "CT",
        EXAM_STATUS_COMPLETED,
        "Normal",
        "2026-03-20T09:30",
        "2026-03-20T10:00"
    };
    /* 处方实体 */
    Prescription prescription = {
        "PRE0001",
        "VIS0001",
        "MED0001",
        2,
        "twice daily"
    };
    /* 发药记录实体 */
    DispenseRecord dispense = {
        "DSP0001",
        "PAT0001",
        "PRE0001",
        "MED0001",
        2,
        "PHA0001",
        "2026-03-20T10:30",
        DISPENSE_STATUS_COMPLETED
    };
    /* 药品实体 */
    Medicine medicine = {
        "MED0001",
        "Aspirin",
        12.5,
        20,
        "DEP0001",
        5
    };

    /* 验证科室ID首字符 */
    assert(department.department_id[0] == 'D');

    /* 验证医生状态 */
    assert(doctor.status == DOCTOR_STATUS_ACTIVE);

    /* 验证患者年龄校验 */
    assert(Patient_is_valid_age(patient.age) == 1);  /* 36岁有效 */
    assert(Patient_is_valid_age(151) == 0);           /* 151岁无效 */

    /* 验证患者身份证首字符 */
    assert(patient.id_card[0] == 'I');

    /* 验证就诊记录关联的挂号ID */
    assert(visit.registration_id[0] == 'R');
    assert(visit.need_exam == 1);       /* 需要检查 */
    assert(visit.need_admission == 0);  /* 不需要住院 */

    /* 验证检查记录 */
    assert(exam.exam_type[0] == 'C');
    assert(exam.status == EXAM_STATUS_COMPLETED);

    /* 验证处方数量 */
    assert(prescription.quantity == 2);

    /* 验证发药记录状态 */
    assert(dispense.status == DISPENSE_STATUS_COMPLETED);

    /* 验证药品库存有效性 */
    assert(Medicine_has_valid_inventory(&medicine) == 1);
}

/**
 * @brief 测试主函数，依次运行所有领域模型测试用例
 */
int main(void) {
    test_registration_status_flow();
    test_prescription_quantity_boundary();
    test_domain_entities_exist();
    return 0;
}
