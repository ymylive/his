/**
 * @file PharmacyService.c
 * @brief 药房服务模块实现
 *
 * 实现药品管理和发药业务功能，包括药品添加、补货、发药、库存查询、
 * 低库存药品查找以及发药记录查询。发药操作采用先扣减库存、再追加记录
 * 的方式，并在记录追加失败时回滚库存变更。
 */

#include "service/PharmacyService.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/IdGenerator.h"
#include "repository/RepositoryUtils.h"

/**
 * @brief 发药记录计数上下文结构体
 *
 * 用于在逐行遍历回调中计数发药记录行数，以生成下一个发药ID。
 */
typedef struct PharmacyServiceDispenseCountContext {
    int count;  /* 当前已有的发药记录数量 */
} PharmacyServiceDispenseCountContext;

/**
 * @brief 判断文本是否为空白（NULL、空串或全为空白字符）
 *
 * @param text  待检查的字符串
 * @return int  1=空白，0=非空白
 */
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

/**
 * @brief 校验必填文本字段
 *
 * 检查文本是否为空白以及是否包含保留字符。
 *
 * @param text        待校验的文本
 * @param field_name  字段名称（用于错误消息）
 * @return Result     校验结果
 */
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

/**
 * @brief 校验可选文本字段
 *
 * 仅检查是否包含保留字符，允许空字符串。
 *
 * @param text        待校验的文本
 * @param field_name  字段名称（用于错误消息）
 * @return Result     校验结果
 */
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

/**
 * @brief 综合校验药品信息的合法性
 *
 * 依次校验药品ID、名称、科室ID中的保留字符以及库存有效性。
 *
 * @param medicine  指向待校验的药品结构体
 * @return Result   校验结果
 */
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

    /* 库存和低库存阈值必须非负 */
    if (!Medicine_has_valid_inventory(medicine)) {
        return Result_make_failure("medicine inventory invalid");
    }

    return Result_make_success("medicine valid");
}

/**
 * @brief 校验数量参数的合法性
 *
 * @param quantity    待校验的数量
 * @param field_name  字段名称（用于错误消息）
 * @return Result     校验结果
 */
static Result PharmacyService_validate_quantity(int quantity, const char *field_name) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (quantity <= 0) {
        snprintf(message, sizeof(message), "%s invalid", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("quantity valid");
}

/**
 * @brief 从药品仓库加载所有药品数据
 *
 * @param service    指向药房服务结构体
 * @param medicines  输出参数，药品链表
 * @return Result    操作结果
 */
static Result PharmacyService_load_medicines(
    PharmacyService *service,
    LinkedList *medicines
) {
    if (service == 0 || medicines == 0) {
        return Result_make_failure("pharmacy service arguments missing");
    }

    return MedicineRepository_load_all(&service->medicine_repository, medicines);
}

/**
 * @brief 在已加载的药品链表中按ID查找药品
 *
 * @param medicines    药品链表
 * @param medicine_id  待查找的药品ID
 * @return Medicine*   找到时返回指针，未找到返回 NULL
 */
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

/**
 * @brief 将药品信息的副本追加到结果链表
 *
 * @param out_medicines  输出结果链表
 * @param medicine       待复制的源药品信息
 * @return Result        操作结果
 */
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

/**
 * @brief 逐行计数回调：统计发药记录行数
 *
 * 作为 TextFileRepository_for_each_data_line 的回调函数，
 * 每遍历一行有效数据，计数器加1。
 *
 * @param line     数据行文本（未使用）
 * @param context  计数上下文指针
 * @return Result  操作结果
 */
static Result PharmacyService_count_dispense_line(const char *line, void *context) {
    PharmacyServiceDispenseCountContext *count_context =
        (PharmacyServiceDispenseCountContext *)context;

    (void)line;  /* 显式忽略未使用的参数 */
    if (count_context == 0) {
        return Result_make_failure("dispense count context missing");
    }

    count_context->count++;
    return Result_make_success("dispense counted");
}

/**
 * @brief 生成下一个发药记录ID
 *
 * 通过统计已有发药记录行数来确定下一个序列号，
 * 生成格式为 "DSP000001" 的发药ID。
 *
 * @param service      指向药房服务结构体
 * @param buffer       输出缓冲区
 * @param buffer_size  缓冲区容量
 * @return Result      操作结果
 */
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

    /* 确保发药记录存储文件已就绪 */
    result = DispenseRecordRepository_ensure_storage(&service->dispense_record_repository);
    if (result.success == 0) {
        return result;
    }

    /* 统计已有记录行数 */
    context.count = 0;
    result = TextFileRepository_for_each_data_line(
        &service->dispense_record_repository.storage,
        PharmacyService_count_dispense_line,
        &context
    );
    if (result.success == 0) {
        return result;
    }

    /* 生成格式化的发药ID */
    if (!IdGenerator_format(buffer, buffer_size, "DSP", context.count + 1, 6)) {
        return Result_make_failure("failed to generate dispense id");
    }

    return Result_make_success("dispense id generated");
}

/**
 * @brief 构建发药记录结构体
 *
 * 生成发药ID并填充各字段，初始状态设为已完成。
 *
 * @param service         指向药房服务结构体
 * @param patient_id      患者ID（可为 NULL）
 * @param prescription_id 处方ID
 * @param medicine_id     药品ID
 * @param quantity        发药数量
 * @param pharmacist_id   药剂师ID
 * @param dispensed_at    发药时间
 * @param record          输出参数，待填充的发药记录
 * @return Result         操作结果
 */
static Result PharmacyService_build_dispense_record(
    PharmacyService *service,
    const char *patient_id,
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

    /* 生成发药记录ID */
    result = PharmacyService_generate_dispense_id(
        service,
        record->dispense_id,
        sizeof(record->dispense_id)
    );
    if (result.success == 0) {
        return result;
    }

    /* 清零并填充各字段 */
    memset(record->prescription_id, 0, sizeof(record->prescription_id));
    memset(record->patient_id, 0, sizeof(record->patient_id));
    memset(record->medicine_id, 0, sizeof(record->medicine_id));
    memset(record->pharmacist_id, 0, sizeof(record->pharmacist_id));
    memset(record->dispensed_at, 0, sizeof(record->dispensed_at));

    if (patient_id != 0) {
        strncpy(record->patient_id, patient_id, sizeof(record->patient_id) - 1);
    }
    strncpy(record->prescription_id, prescription_id, sizeof(record->prescription_id) - 1);
    strncpy(record->medicine_id, medicine_id, sizeof(record->medicine_id) - 1);
    strncpy(record->pharmacist_id, pharmacist_id, sizeof(record->pharmacist_id) - 1);
    strncpy(record->dispensed_at, dispensed_at, sizeof(record->dispensed_at) - 1);
    record->quantity = quantity;
    record->status = DISPENSE_STATUS_COMPLETED;  /* 发药记录创建即完成 */

    return Result_make_success("dispense record built");
}

/**
 * @brief 初始化药房服务
 *
 * 分别初始化药品仓库和发药记录仓库。
 *
 * @param service              指向待初始化的药房服务结构体
 * @param medicine_path        药品数据文件路径
 * @param dispense_record_path 发药记录数据文件路径
 * @return Result              操作结果
 */
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

    /* 确保发药记录存储文件已就绪 */
    return DispenseRecordRepository_ensure_storage(&service->dispense_record_repository);
}

/**
 * @brief 添加新药品
 *
 * 流程：校验药品信息 -> 加载已有药品列表 -> 检查药品ID唯一性 -> 保存新药品。
 *
 * @param service   指向药房服务结构体
 * @param medicine  指向待添加的药品信息
 * @return Result   操作结果
 */
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

    /* 加载已有药品列表 */
    result = PharmacyService_load_medicines(service, &medicines);
    if (result.success == 0) {
        return result;
    }

    /* 检查药品ID是否已存在 */
    stored_medicine = PharmacyService_find_loaded_medicine(&medicines, medicine->medicine_id);
    MedicineRepository_clear_list(&medicines);
    if (stored_medicine != 0) {
        return Result_make_failure("medicine id already exists");
    }

    /* ID唯一性校验通过，保存新药品 */
    return MedicineRepository_save(&service->medicine_repository, medicine);
}

/**
 * @brief 药品补货（增加库存）
 *
 * 流程：校验参数 -> 加载药品列表 -> 查找目标药品 ->
 * 增加库存数量 -> 保存全量数据。
 *
 * @param service      指向药房服务结构体
 * @param medicine_id  药品ID
 * @param quantity     补货数量（必须大于0）
 * @return Result      操作结果
 */
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

    /* 加载所有药品数据 */
    result = PharmacyService_load_medicines(service, &medicines);
    if (result.success == 0) {
        return result;
    }

    /* 查找目标药品 */
    medicine = PharmacyService_find_loaded_medicine(&medicines, medicine_id);
    if (medicine == 0) {
        MedicineRepository_clear_list(&medicines);
        return Result_make_failure("medicine not found");
    }

    /* 增加库存并保存全量数据 */
    medicine->stock += quantity;
    result = MedicineRepository_save_all(&service->medicine_repository, &medicines);
    MedicineRepository_clear_list(&medicines);
    return result;
}

/**
 * @brief 发药操作的内部实现
 *
 * 统一处理普通发药和指定患者发药两种场景。
 * 流程：校验所有参数 -> 加载药品数据 -> 查找目标药品 ->
 * 检查库存是否充足 -> 构建发药记录 -> 扣减库存 -> 保存药品数据 ->
 * 追加发药记录 -> 若追加失败则回滚库存。
 *
 * @param service            指向药房服务结构体
 * @param patient_id         患者ID（普通发药时可为空串）
 * @param require_patient_id 是否要求患者ID非空（1=必填，0=可选）
 * @param prescription_id   处方ID
 * @param medicine_id       药品ID
 * @param quantity           发药数量
 * @param pharmacist_id     药剂师ID
 * @param dispensed_at       发药时间
 * @param out_record         输出参数，发药成功时存放发药记录
 * @return Result            操作结果
 */
static Result PharmacyService_dispense_medicine_internal(
    PharmacyService *service,
    const char *patient_id,
    int require_patient_id,
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

    /* 根据场景校验患者ID */
    if (require_patient_id) {
        result = PharmacyService_validate_required_text(patient_id, "patient id");
        if (result.success == 0) {
            return result;
        }
    } else if (patient_id != 0 && patient_id[0] != '\0') {
        result = PharmacyService_validate_optional_text(patient_id, "patient id");
        if (result.success == 0) {
            return result;
        }
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

    /* 加载所有药品数据 */
    result = PharmacyService_load_medicines(service, &medicines);
    if (result.success == 0) {
        return result;
    }

    /* 查找目标药品 */
    medicine = PharmacyService_find_loaded_medicine(&medicines, medicine_id);
    if (medicine == 0) {
        MedicineRepository_clear_list(&medicines);
        return Result_make_failure("medicine not found");
    }

    /* 检查库存是否充足 */
    if (medicine->stock < quantity) {
        MedicineRepository_clear_list(&medicines);
        return Result_make_failure("medicine stock insufficient");
    }

    /* 构建发药记录 */
    memset(&record, 0, sizeof(record));
    result = PharmacyService_build_dispense_record(
        service,
        patient_id,
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

    /* 扣减库存并保存药品数据 */
    original_stock = medicine->stock;
    medicine->stock -= quantity;
    result = MedicineRepository_save_all(&service->medicine_repository, &medicines);
    if (result.success == 0) {
        MedicineRepository_clear_list(&medicines);
        return result;
    }

    /* 追加发药记录到存储文件 */
    result = DispenseRecordRepository_append(&service->dispense_record_repository, &record);
    if (result.success == 0) {
        /* 追加失败，回滚库存变更 */
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

/**
 * @brief 发药（不指定患者ID）
 *
 * @param service         指向药房服务结构体
 * @param prescription_id 处方ID
 * @param medicine_id     药品ID
 * @param quantity        发药数量
 * @param pharmacist_id   药剂师ID
 * @param dispensed_at    发药时间
 * @param out_record      输出参数，发药成功时存放发药记录
 * @return Result         操作结果
 */
Result PharmacyService_dispense_medicine(
    PharmacyService *service,
    const char *prescription_id,
    const char *medicine_id,
    int quantity,
    const char *pharmacist_id,
    const char *dispensed_at,
    DispenseRecord *out_record
) {
    return PharmacyService_dispense_medicine_internal(
        service,
        "",          /* 不指定患者ID */
        0,           /* 患者ID非必填 */
        prescription_id,
        medicine_id,
        quantity,
        pharmacist_id,
        dispensed_at,
        out_record
    );
}

/**
 * @brief 为指定患者发药
 *
 * @param service         指向药房服务结构体
 * @param patient_id      患者ID（必填）
 * @param prescription_id 处方ID
 * @param medicine_id     药品ID
 * @param quantity        发药数量
 * @param pharmacist_id   药剂师ID
 * @param dispensed_at    发药时间
 * @param out_record      输出参数，发药成功时存放发药记录
 * @return Result         操作结果
 */
Result PharmacyService_dispense_medicine_for_patient(
    PharmacyService *service,
    const char *patient_id,
    const char *prescription_id,
    const char *medicine_id,
    int quantity,
    const char *pharmacist_id,
    const char *dispensed_at,
    DispenseRecord *out_record
) {
    return PharmacyService_dispense_medicine_internal(
        service,
        patient_id,
        1,           /* 患者ID必填 */
        prescription_id,
        medicine_id,
        quantity,
        pharmacist_id,
        dispensed_at,
        out_record
    );
}

/**
 * @brief 查询药品当前库存
 *
 * @param service      指向药房服务结构体
 * @param medicine_id  药品ID
 * @param out_stock    输出参数，查询成功时存放库存数量
 * @return Result      操作结果
 */
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

    /* 从仓库中按ID查找药品 */
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

/**
 * @brief 查找低库存药品
 *
 * 加载所有药品数据，筛选库存量不超过低库存阈值的药品。
 *
 * @param service        指向药房服务结构体
 * @param out_medicines  输出参数，低库存药品链表
 * @return Result        操作结果
 */
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

    /* 遍历所有药品，筛选库存 <= 低库存阈值的药品 */
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

/**
 * @brief 清理药品查询结果链表
 *
 * 释放链表中所有动态分配的 Medicine 节点。
 *
 * @param medicines  指向待清理的药品链表
 */
void PharmacyService_clear_medicine_results(LinkedList *medicines) {
    LinkedList_clear(medicines, free);
}

/**
 * @brief 根据处方ID查找发药记录
 *
 * @param service         指向药房服务结构体
 * @param prescription_id 处方ID
 * @param out_records     输出参数，发药记录链表（必须为空链表）
 * @return Result         操作结果
 */
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

    /* 输出链表必须为空，防止混入旧数据 */
    if (LinkedList_count(out_records) != 0) {
        return Result_make_failure("dispense history output must be empty");
    }

    /* 委托仓库层按处方ID查找 */
    return DispenseRecordRepository_find_by_prescription_id(
        &service->dispense_record_repository,
        prescription_id,
        out_records
    );
}

/**
 * @brief 根据患者ID查找发药记录
 *
 * @param service      指向药房服务结构体
 * @param patient_id   患者ID
 * @param out_records  输出参数，发药记录链表（必须为空链表）
 * @return Result      操作结果
 */
Result PharmacyService_find_dispense_records_by_patient_id(
    PharmacyService *service,
    const char *patient_id,
    LinkedList *out_records
) {
    Result result = PharmacyService_validate_required_text(patient_id, "patient id");

    if (result.success == 0) {
        return result;
    }

    if (service == 0 || out_records == 0) {
        return Result_make_failure("dispense history query arguments missing");
    }

    /* 输出链表必须为空，防止混入旧数据 */
    if (LinkedList_count(out_records) != 0) {
        return Result_make_failure("dispense history output must be empty");
    }

    /* 委托仓库层按患者ID查找 */
    return DispenseRecordRepository_find_by_patient_id(
        &service->dispense_record_repository,
        patient_id,
        out_records
    );
}

/**
 * @brief 清理发药记录查询结果链表
 *
 * 释放链表中所有动态分配的 DispenseRecord 节点。
 *
 * @param records  指向待清理的发药记录链表
 */
void PharmacyService_clear_dispense_record_results(LinkedList *records) {
    DispenseRecordRepository_clear_list(records);
}
