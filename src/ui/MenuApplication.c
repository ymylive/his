#include "ui/MenuApplication.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int MenuApplication_is_blank(const char *text) {
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

static Result MenuApplication_validate_required_text(
    const char *text,
    const char *field_name
) {
    char message[RESULT_MESSAGE_CAPACITY];

    if (MenuApplication_is_blank(text)) {
        snprintf(message, sizeof(message), "%s missing", field_name);
        return Result_make_failure(message);
    }

    return Result_make_success("text valid");
}

static Result MenuApplication_write_text(
    char *buffer,
    size_t capacity,
    const char *format,
    ...
) {
    va_list arguments;
    int written = 0;

    if (buffer == 0 || capacity == 0 || format == 0) {
        return Result_make_failure("output buffer invalid");
    }

    va_start(arguments, format);
    written = vsnprintf(buffer, capacity, format, arguments);
    va_end(arguments);

    if (written < 0 || (size_t)written >= capacity) {
        return Result_make_failure("output buffer too small");
    }

    return Result_make_success("output ready");
}

static Result MenuApplication_append_text(
    char *buffer,
    size_t capacity,
    size_t *used,
    const char *format,
    ...
) {
    va_list arguments;
    int written = 0;

    if (buffer == 0 || capacity == 0 || used == 0 || format == 0 || *used >= capacity) {
        return Result_make_failure("output append invalid");
    }

    va_start(arguments, format);
    written = vsnprintf(buffer + *used, capacity - *used, format, arguments);
    va_end(arguments);

    if (written < 0 || (size_t)written >= capacity - *used) {
        return Result_make_failure("output buffer too small");
    }

    *used += (size_t)written;
    return Result_make_success("output appended");
}

static Result MenuApplication_write_failure(
    const char *message,
    char *buffer,
    size_t capacity
) {
    if (buffer != 0 && capacity > 0 && message != 0) {
        (void)MenuApplication_write_text(buffer, capacity, "操作失败: %s", message);
    }

    if (message == 0) {
        return Result_make_failure("operation failed");
    }

    return Result_make_failure(message);
}

static const char *MenuApplication_gender_label(PatientGender gender) {
    switch (gender) {
        case PATIENT_GENDER_MALE:
            return "男";
        case PATIENT_GENDER_FEMALE:
            return "女";
        default:
            return "未知";
    }
}

static const char *MenuApplication_registration_status_label(RegistrationStatus status) {
    switch (status) {
        case REG_STATUS_PENDING:
            return "待诊";
        case REG_STATUS_DIAGNOSED:
            return "已接诊";
        case REG_STATUS_CANCELLED:
            return "已取消";
        default:
            return "未知";
    }
}

static const char *MenuApplication_bed_status_label(BedStatus status) {
    switch (status) {
        case BED_STATUS_AVAILABLE:
            return "空闲";
        case BED_STATUS_OCCUPIED:
            return "占用";
        case BED_STATUS_MAINTENANCE:
            return "维护";
        default:
            return "未知";
    }
}

static const char *MenuApplication_ward_status_label(WardStatus status) {
    switch (status) {
        case WARD_STATUS_ACTIVE:
            return "启用";
        case WARD_STATUS_CLOSED:
            return "停用";
        default:
            return "未知";
    }
}

static const char *MenuApplication_admission_status_label(AdmissionStatus status) {
    switch (status) {
        case ADMISSION_STATUS_ACTIVE:
            return "住院中";
        case ADMISSION_STATUS_DISCHARGED:
            return "已出院";
        default:
            return "未知";
    }
}

Result MenuApplication_init(MenuApplication *application, const MenuApplicationPaths *paths) {
    Result result;

    if (application == 0 || paths == 0) {
        return Result_make_failure("menu application arguments missing");
    }

    result = MenuApplication_validate_required_text(paths->patient_path, "patient path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->department_path, "department path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->doctor_path, "doctor path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->registration_path, "registration path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->visit_path, "visit path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->examination_path, "examination path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->ward_path, "ward path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->bed_path, "bed path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->admission_path, "admission path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(paths->medicine_path, "medicine path");
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_validate_required_text(
        paths->dispense_record_path,
        "dispense record path"
    );
    if (result.success == 0) {
        return result;
    }

    result = PatientService_init(&application->patient_service, paths->patient_path);
    if (result.success == 0) {
        return result;
    }

    result = DoctorService_init(
        &application->doctor_service,
        paths->doctor_path,
        paths->department_path
    );
    if (result.success == 0) {
        return result;
    }

    result = RegistrationRepository_init(
        &application->registration_repository,
        paths->registration_path
    );
    if (result.success == 0) {
        return result;
    }

    result = RegistrationService_init(
        &application->registration_service,
        &application->registration_repository,
        &application->patient_service.repository,
        &application->doctor_service.doctor_repository,
        &application->doctor_service.department_repository
    );
    if (result.success == 0) {
        return result;
    }

    result = MedicalRecordService_init(
        &application->medical_record_service,
        paths->registration_path,
        paths->visit_path,
        paths->examination_path,
        paths->admission_path
    );
    if (result.success == 0) {
        return result;
    }

    result = InpatientService_init(
        &application->inpatient_service,
        paths->patient_path,
        paths->ward_path,
        paths->bed_path,
        paths->admission_path
    );
    if (result.success == 0) {
        return result;
    }

    result = BedService_init(
        &application->bed_service,
        paths->ward_path,
        paths->bed_path,
        paths->admission_path
    );
    if (result.success == 0) {
        return result;
    }

    return PharmacyService_init(
        &application->pharmacy_service,
        paths->medicine_path,
        paths->dispense_record_path
    );
}

Result MenuApplication_add_patient(
    MenuApplication *application,
    const Patient *patient,
    char *buffer,
    size_t capacity
) {
    Result result = PatientService_create_patient(&application->patient_service, patient);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "患者已添加: %s | %s | 性别=%s | 年龄=%d | 联系方式=%s | 住院=%s",
        patient->patient_id,
        patient->name,
        MenuApplication_gender_label(patient->gender),
        patient->age,
        patient->contact,
        patient->is_inpatient ? "是" : "否"
    );
}

Result MenuApplication_query_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    Patient patient;
    Result result = PatientService_find_patient_by_id(
        &application->patient_service,
        patient_id,
        &patient
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "患者信息: %s | %s | 性别=%s | 年龄=%d | 联系方式=%s | 身份证=%s | 住院=%s",
        patient.patient_id,
        patient.name,
        MenuApplication_gender_label(patient.gender),
        patient.age,
        patient.contact,
        patient.id_card,
        patient.is_inpatient ? "是" : "否"
    );
}

Result MenuApplication_create_registration(
    MenuApplication *application,
    const char *patient_id,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    char *buffer,
    size_t capacity
) {
    Registration registration;
    Result result = RegistrationService_create(
        &application->registration_service,
        patient_id,
        doctor_id,
        department_id,
        registered_at,
        &registration
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "挂号已创建: %s | 患者=%s | 医生=%s | 科室=%s | 时间=%s | 状态=%s",
        registration.registration_id,
        registration.patient_id,
        registration.doctor_id,
        registration.department_id,
        registration.registered_at,
        MenuApplication_registration_status_label(registration.status)
    );
}

Result MenuApplication_query_registration(
    MenuApplication *application,
    const char *registration_id,
    char *buffer,
    size_t capacity
) {
    Registration registration;
    Result result = RegistrationService_find_by_registration_id(
        &application->registration_service,
        registration_id,
        &registration
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "挂号信息: %s | 患者=%s | 医生=%s | 科室=%s | 状态=%s | 挂号时间=%s",
        registration.registration_id,
        registration.patient_id,
        registration.doctor_id,
        registration.department_id,
        MenuApplication_registration_status_label(registration.status),
        registration.registered_at
    );
}

Result MenuApplication_cancel_registration(
    MenuApplication *application,
    const char *registration_id,
    const char *cancelled_at,
    char *buffer,
    size_t capacity
) {
    Registration registration;
    Result result = RegistrationService_cancel(
        &application->registration_service,
        registration_id,
        cancelled_at,
        &registration
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "挂号已取消: %s | 患者=%s | 时间=%s",
        registration.registration_id,
        registration.patient_id,
        registration.cancelled_at
    );
}

Result MenuApplication_create_visit_record(
    MenuApplication *application,
    const char *registration_id,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time,
    char *buffer,
    size_t capacity
) {
    VisitRecord record;
    Result result = MedicalRecordService_create_visit_record(
        &application->medical_record_service,
        registration_id,
        chief_complaint,
        diagnosis,
        advice,
        need_exam,
        need_admission,
        need_medicine,
        visit_time,
        &record
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "看诊记录已创建: %s | 挂号=%s | 患者=%s | 诊断=%s",
        record.visit_id,
        record.registration_id,
        record.patient_id,
        record.diagnosis
    );
}

Result MenuApplication_query_patient_history(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    MedicalRecordHistory history;
    Result result = MedicalRecordService_find_patient_history(
        &application->medical_record_service,
        patient_id,
        &history
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "患者历史: %s | registrations=%zu | visits=%zu | examinations=%zu | admissions=%zu",
        patient_id,
        LinkedList_count(&history.registrations),
        LinkedList_count(&history.visits),
        LinkedList_count(&history.examinations),
        LinkedList_count(&history.admissions)
    );
    MedicalRecordHistory_clear(&history);
    return result;
}

Result MenuApplication_list_wards(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList wards;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result = BedService_list_wards(&application->bed_service, &wards);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "病房列表(%zu)",
        LinkedList_count(&wards)
    );
    if (result.success == 0) {
        BedService_clear_wards(&wards);
        return result;
    }

    used = strlen(buffer);
    current = wards.head;
    while (current != 0) {
        const Ward *ward = (const Ward *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | %s | 科室=%s | 位置=%s | 床位=%d/%d | 状态=%s",
            ward->ward_id,
            ward->name,
            ward->department_id,
            ward->location,
            ward->occupied_beds,
            ward->capacity,
            MenuApplication_ward_status_label(ward->status)
        );
        if (result.success == 0) {
            BedService_clear_wards(&wards);
            return result;
        }

        current = current->next;
    }

    BedService_clear_wards(&wards);
    return Result_make_success("ward list ready");
}

Result MenuApplication_list_beds_by_ward(
    MenuApplication *application,
    const char *ward_id,
    char *buffer,
    size_t capacity
) {
    LinkedList beds;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result = BedService_list_beds_by_ward(
        &application->bed_service,
        ward_id,
        &beds
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "床位列表(%s/%zu)",
        ward_id,
        LinkedList_count(&beds)
    );
    if (result.success == 0) {
        BedService_clear_beds(&beds);
        return result;
    }

    used = strlen(buffer);
    current = beds.head;
    while (current != 0) {
        const Bed *bed = (const Bed *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | 房间=%s | 床号=%s | 状态=%s",
            bed->bed_id,
            bed->room_no,
            bed->bed_no,
            MenuApplication_bed_status_label(bed->status)
        );
        if (result.success == 0) {
            BedService_clear_beds(&beds);
            return result;
        }

        current = current->next;
    }

    BedService_clear_beds(&beds);
    return Result_make_success("bed list ready");
}

Result MenuApplication_admit_patient(
    MenuApplication *application,
    const char *patient_id,
    const char *ward_id,
    const char *bed_id,
    const char *admitted_at,
    const char *summary,
    char *buffer,
    size_t capacity
) {
    Admission admission;
    Result result = InpatientService_admit_patient(
        &application->inpatient_service,
        patient_id,
        ward_id,
        bed_id,
        admitted_at,
        summary,
        &admission
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "住院办理成功: %s | 患者=%s | 病房=%s | 床位=%s | 时间=%s",
        admission.admission_id,
        admission.patient_id,
        admission.ward_id,
        admission.bed_id,
        admission.admitted_at
    );
}

Result MenuApplication_discharge_patient(
    MenuApplication *application,
    const char *admission_id,
    const char *discharged_at,
    const char *summary,
    char *buffer,
    size_t capacity
) {
    Admission admission;
    Result result = InpatientService_discharge_patient(
        &application->inpatient_service,
        admission_id,
        discharged_at,
        summary,
        &admission
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "出院办理成功: %s | 患者=%s | 状态=%s | 时间=%s",
        admission.admission_id,
        admission.patient_id,
        MenuApplication_admission_status_label(admission.status),
        admission.discharged_at
    );
}

Result MenuApplication_query_active_admission_by_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    Admission admission;
    Result result = InpatientService_find_active_by_patient_id(
        &application->inpatient_service,
        patient_id,
        &admission
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "当前住院: %s | 患者=%s | 病房=%s | 床位=%s | 状态=%s",
        admission.admission_id,
        admission.patient_id,
        admission.ward_id,
        admission.bed_id,
        MenuApplication_admission_status_label(admission.status)
    );
}

Result MenuApplication_query_current_patient_by_bed(
    MenuApplication *application,
    const char *bed_id,
    char *buffer,
    size_t capacity
) {
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    Result result = BedService_find_current_patient_by_bed_id(
        &application->bed_service,
        bed_id,
        patient_id,
        sizeof(patient_id)
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "当前入住患者: 床位=%s | 患者=%s",
        bed_id,
        patient_id
    );
}

Result MenuApplication_add_medicine(
    MenuApplication *application,
    const Medicine *medicine,
    char *buffer,
    size_t capacity
) {
    Result result = PharmacyService_add_medicine(&application->pharmacy_service, medicine);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "药品已添加: %s | %s | 单价=%.2f | 库存=%d",
        medicine->medicine_id,
        medicine->name,
        medicine->price,
        medicine->stock
    );
}

Result MenuApplication_restock_medicine(
    MenuApplication *application,
    const char *medicine_id,
    int quantity,
    char *buffer,
    size_t capacity
) {
    int stock = 0;
    Result result = PharmacyService_restock_medicine(
        &application->pharmacy_service,
        medicine_id,
        quantity
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = PharmacyService_get_stock(&application->pharmacy_service, medicine_id, &stock);
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "药品已入库: %s | 本次=%d | 当前库存=%d",
        medicine_id,
        quantity,
        stock
    );
}

Result MenuApplication_dispense_medicine(
    MenuApplication *application,
    const char *prescription_id,
    const char *medicine_id,
    int quantity,
    const char *pharmacist_id,
    const char *dispensed_at,
    char *buffer,
    size_t capacity
) {
    DispenseRecord record;
    int stock = 0;
    Result result = PharmacyService_dispense_medicine(
        &application->pharmacy_service,
        prescription_id,
        medicine_id,
        quantity,
        pharmacist_id,
        dispensed_at,
        &record
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = PharmacyService_get_stock(&application->pharmacy_service, medicine_id, &stock);
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "发药完成: 处方=%s | 药品=%s | 数量=%d | 当前库存=%d",
        record.prescription_id,
        record.medicine_id,
        record.quantity,
        stock
    );
}

Result MenuApplication_query_medicine_stock(
    MenuApplication *application,
    const char *medicine_id,
    char *buffer,
    size_t capacity
) {
    int stock = 0;
    Result result = PharmacyService_get_stock(
        &application->pharmacy_service,
        medicine_id,
        &stock
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "药品库存: %s | %d",
        medicine_id,
        stock
    );
}

Result MenuApplication_find_low_stock_medicines(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList medicines;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result = PharmacyService_find_low_stock_medicines(
        &application->pharmacy_service,
        &medicines
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "低库存药品(%zu)",
        LinkedList_count(&medicines)
    );
    if (result.success == 0) {
        PharmacyService_clear_medicine_results(&medicines);
        return result;
    }

    used = strlen(buffer);
    current = medicines.head;
    while (current != 0) {
        const Medicine *medicine = (const Medicine *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | %s | 库存=%d | 阈值=%d",
            medicine->medicine_id,
            medicine->name,
            medicine->stock,
            medicine->low_stock_threshold
        );
        if (result.success == 0) {
            PharmacyService_clear_medicine_results(&medicines);
            return result;
        }

        current = current->next;
    }

    PharmacyService_clear_medicine_results(&medicines);
    return Result_make_success("low stock ready");
}

typedef struct MenuApplicationPromptContext {
    FILE *input;
    FILE *output;
} MenuApplicationPromptContext;

static Result MenuApplication_prompt_line(
    MenuApplicationPromptContext *context,
    const char *prompt,
    char *buffer,
    size_t capacity
) {
    size_t length = 0;

    if (context == 0 || context->input == 0 || context->output == 0) {
        return Result_make_failure("prompt context invalid");
    }

    if (buffer == 0 || capacity == 0) {
        return Result_make_failure("prompt buffer invalid");
    }

    fputs(prompt, context->output);
    fflush(context->output);
    if (fgets(buffer, (int)capacity, context->input) == 0) {
        return Result_make_failure("input ended");
    }

    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    }

    return Result_make_success("line read");
}

static Result MenuApplication_prompt_int(
    MenuApplicationPromptContext *context,
    const char *prompt,
    int *out_value
) {
    char buffer[64];
    char *end_pointer = 0;
    long value = 0;
    Result result = MenuApplication_prompt_line(context, prompt, buffer, sizeof(buffer));

    if (result.success == 0) {
        return result;
    }

    value = strtol(buffer, &end_pointer, 10);
    if (end_pointer == buffer || end_pointer == 0) {
        return Result_make_failure("numeric input invalid");
    }

    while (*end_pointer != '\0') {
        if (!isspace((unsigned char)*end_pointer)) {
            return Result_make_failure("numeric input invalid");
        }

        end_pointer++;
    }

    *out_value = (int)value;
    return Result_make_success("number parsed");
}

static Result MenuApplication_prompt_patient_form(
    MenuApplicationPromptContext *context,
    Patient *out_patient
) {
    int gender = 0;
    int age = 0;
    int is_inpatient = 0;
    Result result;

    if (out_patient == 0) {
        return Result_make_failure("patient form missing");
    }

    memset(out_patient, 0, sizeof(*out_patient));
    result = MenuApplication_prompt_line(
        context,
        "患者编号: ",
        out_patient->patient_id,
        sizeof(out_patient->patient_id)
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_prompt_line(
        context,
        "姓名: ",
        out_patient->name,
        sizeof(out_patient->name)
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_prompt_int(context, "性别(0未知/1男/2女): ", &gender);
    if (result.success == 0) {
        return result;
    }
    out_patient->gender = (PatientGender)gender;
    result = MenuApplication_prompt_int(context, "年龄: ", &age);
    if (result.success == 0) {
        return result;
    }
    out_patient->age = age;
    result = MenuApplication_prompt_line(
        context,
        "联系方式: ",
        out_patient->contact,
        sizeof(out_patient->contact)
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_prompt_line(
        context,
        "身份证号: ",
        out_patient->id_card,
        sizeof(out_patient->id_card)
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_prompt_line(
        context,
        "过敏史: ",
        out_patient->allergy,
        sizeof(out_patient->allergy)
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_prompt_line(
        context,
        "病史: ",
        out_patient->medical_history,
        sizeof(out_patient->medical_history)
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_prompt_int(context, "是否住院(0否/1是): ", &is_inpatient);
    if (result.success == 0) {
        return result;
    }
    out_patient->is_inpatient = is_inpatient;
    return MenuApplication_prompt_line(
        context,
        "备注: ",
        out_patient->remarks,
        sizeof(out_patient->remarks)
    );
}

static Result MenuApplication_prompt_medicine_form(
    MenuApplicationPromptContext *context,
    Medicine *out_medicine
) {
    char price_buffer[64];
    int stock = 0;
    int threshold = 0;
    Result result;

    if (out_medicine == 0) {
        return Result_make_failure("medicine form missing");
    }

    memset(out_medicine, 0, sizeof(*out_medicine));
    result = MenuApplication_prompt_line(
        context,
        "药品编号: ",
        out_medicine->medicine_id,
        sizeof(out_medicine->medicine_id)
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_prompt_line(
        context,
        "药品名称: ",
        out_medicine->name,
        sizeof(out_medicine->name)
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_prompt_line(context, "单价: ", price_buffer, sizeof(price_buffer));
    if (result.success == 0) {
        return result;
    }
    out_medicine->price = strtod(price_buffer, 0);
    result = MenuApplication_prompt_int(context, "库存: ", &stock);
    if (result.success == 0) {
        return result;
    }
    out_medicine->stock = stock;
    result = MenuApplication_prompt_line(
        context,
        "所属科室编号: ",
        out_medicine->department_id,
        sizeof(out_medicine->department_id)
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_prompt_int(context, "库存预警阈值: ", &threshold);
    if (result.success == 0) {
        return result;
    }
    out_medicine->low_stock_threshold = threshold;
    return Result_make_success("medicine form ready");
}

Result MenuApplication_execute_action(
    MenuApplication *application,
    MenuAction action,
    FILE *input,
    FILE *output
) {
    MenuApplicationPromptContext context;
    char output_buffer[2048];
    char first_id[HIS_DOMAIN_ID_CAPACITY];
    char second_id[HIS_DOMAIN_ID_CAPACITY];
    char third_id[HIS_DOMAIN_TEXT_CAPACITY];
    char time_value[HIS_DOMAIN_TIME_CAPACITY];
    char long_text[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    Patient patient;
    Medicine medicine;
    int flag_one = 0;
    int flag_two = 0;
    int flag_three = 0;
    Result result;

    if (application == 0 || input == 0 || output == 0) {
        return Result_make_failure("menu action arguments missing");
    }

    context.input = input;
    context.output = output;
    memset(output_buffer, 0, sizeof(output_buffer));
    memset(first_id, 0, sizeof(first_id));
    memset(second_id, 0, sizeof(second_id));
    memset(third_id, 0, sizeof(third_id));
    memset(time_value, 0, sizeof(time_value));
    memset(long_text, 0, sizeof(long_text));

    switch (action) {
        case MENU_ACTION_CLERK_ADD_PATIENT:
            result = MenuApplication_prompt_patient_form(&context, &patient);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_add_patient(
                application,
                &patient,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_CLERK_QUERY_PATIENT:
        case MENU_ACTION_PATIENT_BASIC_INFO:
            result = MenuApplication_prompt_line(
                &context,
                "患者编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_patient(
                application,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_CLERK_CREATE_REGISTRATION:
            result = MenuApplication_prompt_line(
                &context,
                "患者编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "医生工号: ",
                second_id,
                sizeof(second_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "科室编号: ",
                third_id,
                sizeof(third_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "挂号时间: ",
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_create_registration(
                application,
                first_id,
                second_id,
                third_id,
                time_value,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_CLERK_QUERY_REGISTRATION:
            result = MenuApplication_prompt_line(
                &context,
                "1. 查询挂号  2. 取消挂号\n请选择: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            if (strcmp(first_id, "1") == 0) {
                result = MenuApplication_prompt_line(
                    &context,
                    "挂号编号: ",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_query_registration(
                    application,
                    second_id,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "2") == 0) {
                result = MenuApplication_prompt_line(
                    &context,
                    "挂号编号: ",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_prompt_line(
                    &context,
                    "取消时间: ",
                    time_value,
                    sizeof(time_value)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_cancel_registration(
                    application,
                    second_id,
                    time_value,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else {
                return Result_make_failure("invalid registration action");
            }
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_DOCTOR_QUERY_PATIENT_HISTORY:
        case MENU_ACTION_PATIENT_QUERY_VISITS:
        case MENU_ACTION_PATIENT_QUERY_EXAMS:
        case MENU_ACTION_PATIENT_QUERY_ADMISSIONS:
            result = MenuApplication_prompt_line(
                &context,
                "患者编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_patient_history(
                application,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_DOCTOR_VISIT_RECORD:
            result = MenuApplication_prompt_line(
                &context,
                "挂号编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "主诉: ",
                second_id,
                sizeof(second_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "诊断结果: ",
                third_id,
                sizeof(third_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "医生建议: ",
                long_text,
                sizeof(long_text)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_int(&context, "是否需要检查(0/1): ", &flag_one);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_int(&context, "是否需要住院(0/1): ", &flag_two);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_int(&context, "是否需要用药(0/1): ", &flag_three);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "就诊时间: ",
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_create_visit_record(
                application,
                first_id,
                second_id,
                third_id,
                long_text,
                flag_one,
                flag_two,
                flag_three,
                time_value,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_INPATIENT_QUERY_BED:
        case MENU_ACTION_WARD_LIST_WARDS:
            result = MenuApplication_list_wards(
                application,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_WARD_LIST_BEDS:
            result = MenuApplication_prompt_line(
                &context,
                "病房编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_list_beds_by_ward(
                application,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_INPATIENT_ADMIT:
            result = MenuApplication_prompt_line(
                &context,
                "患者编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "病房编号: ",
                second_id,
                sizeof(second_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "床位编号: ",
                third_id,
                sizeof(third_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "入院时间: ",
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "住院摘要: ",
                long_text,
                sizeof(long_text)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_admit_patient(
                application,
                first_id,
                second_id,
                third_id,
                time_value,
                long_text,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_INPATIENT_DISCHARGE:
            result = MenuApplication_prompt_line(
                &context,
                "住院编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "出院时间: ",
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "出院小结: ",
                long_text,
                sizeof(long_text)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_discharge_patient(
                application,
                first_id,
                time_value,
                long_text,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_INPATIENT_QUERY_RECORD:
            result = MenuApplication_prompt_line(
                &context,
                "患者编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_active_admission_by_patient(
                application,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_WARD_QUERY_INPATIENT:
            result = MenuApplication_prompt_line(
                &context,
                "床位编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_current_patient_by_bed(
                application,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_PHARMACY_ADD_MEDICINE:
            result = MenuApplication_prompt_medicine_form(&context, &medicine);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_add_medicine(
                application,
                &medicine,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_PHARMACY_RESTOCK:
            result = MenuApplication_prompt_line(
                &context,
                "药品编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_int(&context, "入库数量: ", &flag_one);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_restock_medicine(
                application,
                first_id,
                flag_one,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_PHARMACY_DISPENSE:
            result = MenuApplication_prompt_line(
                &context,
                "处方编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "药品编号: ",
                second_id,
                sizeof(second_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_int(&context, "发药数量: ", &flag_one);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "药师工号: ",
                third_id,
                sizeof(third_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "发药时间: ",
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_dispense_medicine(
                application,
                first_id,
                second_id,
                flag_one,
                third_id,
                time_value,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_PHARMACY_QUERY_STOCK:
        case MENU_ACTION_DOCTOR_PRESCRIPTION_STOCK:
            result = MenuApplication_prompt_line(
                &context,
                "药品编号: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_medicine_stock(
                application,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_PHARMACY_LOW_STOCK:
            result = MenuApplication_find_low_stock_medicines(
                application,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        default:
            fprintf(output, "当前动作尚未落地: %s\n", MenuController_action_label(action));
            return Result_make_failure("action not implemented yet");
    }
}
