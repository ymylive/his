#include "ui/MenuApplication.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common/IdGenerator.h"
#include "service/DepartmentService.h"

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

static Result MenuApplication_generate_patient_id(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList patients;
    Result result;
    int sequence = 0;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("patient id generation arguments missing");
    }

    LinkedList_init(&patients);
    result = PatientRepository_load_all(&application->patient_service.repository, &patients);
    if (result.success == 0) {
        return result;
    }

    sequence = (int)LinkedList_count(&patients) + 1;
    PatientRepository_clear_loaded_list(&patients);
    if (!IdGenerator_format(buffer, capacity, "PAT", sequence, 4)) {
        return Result_make_failure("failed to generate patient id");
    }

    return Result_make_success("patient id generated");
}

static Result MenuApplication_generate_department_id(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList departments;
    Result result;
    int sequence = 0;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("department id generation arguments missing");
    }

    LinkedList_init(&departments);
    result = DepartmentRepository_load_all(
        &application->doctor_service.department_repository,
        &departments
    );
    if (result.success == 0) {
        return result;
    }

    sequence = (int)LinkedList_count(&departments) + 1;
    DepartmentRepository_clear_list(&departments);
    if (!IdGenerator_format(buffer, capacity, "DEP", sequence, 4)) {
        return Result_make_failure("failed to generate department id");
    }

    return Result_make_success("department id generated");
}

static Result MenuApplication_generate_doctor_id(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList doctors;
    Result result;
    int sequence = 0;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("doctor id generation arguments missing");
    }

    LinkedList_init(&doctors);
    result = DoctorRepository_load_all(&application->doctor_service.doctor_repository, &doctors);
    if (result.success == 0) {
        return result;
    }

    sequence = (int)LinkedList_count(&doctors) + 1;
    DoctorRepository_clear_list(&doctors);
    if (!IdGenerator_format(buffer, capacity, "DOC", sequence, 4)) {
        return Result_make_failure("failed to generate doctor id");
    }

    return Result_make_success("doctor id generated");
}

static Result MenuApplication_generate_medicine_id(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList medicines;
    Result result;
    int sequence = 0;

    if (application == 0 || buffer == 0 || capacity == 0) {
        return Result_make_failure("medicine id generation arguments missing");
    }

    LinkedList_init(&medicines);
    result = MedicineRepository_load_all(&application->pharmacy_service.medicine_repository, &medicines);
    if (result.success == 0) {
        return result;
    }

    sequence = (int)LinkedList_count(&medicines) + 1;
    MedicineRepository_clear_list(&medicines);
    if (!IdGenerator_format(buffer, capacity, "MED", sequence, 4)) {
        return Result_make_failure("failed to generate medicine id");
    }

    return Result_make_success("medicine id generated");
}

static void MenuApplication_copy_text(
    char *destination,
    size_t capacity,
    const char *source
) {
    if (destination == 0 || capacity == 0) {
        return;
    }

    if (source == 0) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

static void MenuApplication_clear_patient_session_state(MenuApplication *application) {
    if (application == 0) {
        return;
    }

    application->has_bound_patient_session = 0;
    memset(application->bound_patient_id, 0, sizeof(application->bound_patient_id));
}

static void MenuApplication_clear_authenticated_user_state(MenuApplication *application) {
    if (application == 0) {
        return;
    }

    application->has_authenticated_user = 0;
    memset(&application->authenticated_user, 0, sizeof(application->authenticated_user));
}

static Result MenuApplication_require_patient_session(MenuApplication *application) {
    if (application == 0) {
        return Result_make_failure("menu application missing");
    }

    if (application->has_bound_patient_session == 0 ||
        MenuApplication_is_blank(application->bound_patient_id)) {
        return Result_make_failure("patient session not bound");
    }

    return Result_make_success("patient session ready");
}

static Result MenuApplication_authorize_patient_session(
    MenuApplication *application,
    const char *requested_patient_id
) {
    Result result = MenuApplication_require_patient_session(application);
    if (result.success == 0) {
        return result;
    }

    result = MenuApplication_validate_required_text(requested_patient_id, "patient id");
    if (result.success == 0) {
        return result;
    }

    if (strcmp(application->bound_patient_id, requested_patient_id) != 0) {
        return Result_make_failure("patient session unauthorized");
    }

    return Result_make_success("patient session authorized");
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

static void MenuApplication_fill_visit_handoff(
    const VisitRecord *record,
    MenuApplicationVisitHandoff *out_handoff
) {
    if (record == 0 || out_handoff == 0) {
        return;
    }

    memset(out_handoff, 0, sizeof(*out_handoff));
    MenuApplication_copy_text(out_handoff->visit_id, sizeof(out_handoff->visit_id), record->visit_id);
    MenuApplication_copy_text(
        out_handoff->registration_id,
        sizeof(out_handoff->registration_id),
        record->registration_id
    );
    MenuApplication_copy_text(
        out_handoff->patient_id,
        sizeof(out_handoff->patient_id),
        record->patient_id
    );
    MenuApplication_copy_text(
        out_handoff->doctor_id,
        sizeof(out_handoff->doctor_id),
        record->doctor_id
    );
    MenuApplication_copy_text(
        out_handoff->department_id,
        sizeof(out_handoff->department_id),
        record->department_id
    );
    MenuApplication_copy_text(
        out_handoff->diagnosis,
        sizeof(out_handoff->diagnosis),
        record->diagnosis
    );
    MenuApplication_copy_text(
        out_handoff->advice,
        sizeof(out_handoff->advice),
        record->advice
    );
    out_handoff->need_exam = record->need_exam;
    out_handoff->need_admission = record->need_admission;
    out_handoff->need_medicine = record->need_medicine;
    MenuApplication_copy_text(
        out_handoff->visit_time,
        sizeof(out_handoff->visit_time),
        record->visit_time
    );
}

Result MenuApplication_init(MenuApplication *application, const MenuApplicationPaths *paths) {
    Result result;

    if (application == 0 || paths == 0) {
        return Result_make_failure("menu application arguments missing");
    }

    result = MenuApplication_validate_required_text(paths->user_path, "user path");
    if (result.success == 0) {
        return result;
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

    result = AuthService_init(
        &application->auth_service,
        paths->user_path,
        paths->patient_path
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

    result = PharmacyService_init(
        &application->pharmacy_service,
        paths->medicine_path,
        paths->dispense_record_path
    );
    if (result.success == 1) {
        MenuApplication_clear_authenticated_user_state(application);
        MenuApplication_clear_patient_session_state(application);
    }

    return result;
}

Result MenuApplication_login(
    MenuApplication *application,
    const char *user_id,
    const char *password,
    UserRole required_role
) {
    User user;
    Result result;

    if (application == 0) {
        return Result_make_failure("menu application missing");
    }

    MenuApplication_clear_authenticated_user_state(application);
    MenuApplication_clear_patient_session_state(application);

    result = AuthService_authenticate(
        &application->auth_service,
        user_id,
        password,
        required_role,
        &user
    );
    if (result.success == 0) {
        return result;
    }

    application->authenticated_user = user;
    application->has_authenticated_user = 1;

    if (user.role == USER_ROLE_PATIENT) {
        result = MenuApplication_bind_patient_session(application, user.user_id);
        if (result.success == 0) {
            MenuApplication_clear_authenticated_user_state(application);
            return result;
        }
    }

    return Result_make_success("menu application login ready");
}

void MenuApplication_logout(MenuApplication *application) {
    MenuApplication_clear_authenticated_user_state(application);
    MenuApplication_clear_patient_session_state(application);
}

Result MenuApplication_bind_patient_session(MenuApplication *application, const char *patient_id) {
    Patient patient;
    Result result;

    if (application == 0) {
        return Result_make_failure("menu application missing");
    }

    result = MenuApplication_validate_required_text(patient_id, "patient id");
    if (result.success == 0) {
        return result;
    }

    MenuApplication_clear_patient_session_state(application);
    result = PatientService_find_patient_by_id(
        &application->patient_service,
        patient_id,
        &patient
    );
    if (result.success == 0) {
        return result;
    }

    strncpy(
        application->bound_patient_id,
        patient.patient_id,
        sizeof(application->bound_patient_id) - 1
    );
    application->bound_patient_id[sizeof(application->bound_patient_id) - 1] = '\0';
    application->has_bound_patient_session = 1;
    return Result_make_success("patient session bound");
}

void MenuApplication_reset_patient_session(MenuApplication *application) {
    MenuApplication_clear_patient_session_state(application);
}

Result MenuApplication_add_patient(
    MenuApplication *application,
    const Patient *patient,
    char *buffer,
    size_t capacity
) {
    Patient stored_patient;
    Result result;

    if (application == 0 || patient == 0) {
        return Result_make_failure("patient add arguments missing");
    }

    stored_patient = *patient;
    if (MenuApplication_is_blank(stored_patient.patient_id)) {
        result = MenuApplication_generate_patient_id(
            application,
            stored_patient.patient_id,
            sizeof(stored_patient.patient_id)
        );
        if (result.success == 0) {
            return MenuApplication_write_failure(result.message, buffer, capacity);
        }
    }

    result = PatientService_create_patient(&application->patient_service, &stored_patient);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "患者已添加: %s | %s | 性别=%s | 年龄=%d | 联系方式=%s | 住院=%s",
        stored_patient.patient_id,
        stored_patient.name,
        MenuApplication_gender_label(stored_patient.gender),
        stored_patient.age,
        stored_patient.contact,
        stored_patient.is_inpatient ? "是" : "否"
    );
}

Result MenuApplication_update_patient(
    MenuApplication *application,
    const Patient *patient,
    char *buffer,
    size_t capacity
) {
    Result result = PatientService_update_patient(&application->patient_service, patient);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "患者已更新: %s | %s | 性别=%s | 年龄=%d | 联系方式=%s | 住院=%s",
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

Result MenuApplication_create_self_registration(
    MenuApplication *application,
    const char *doctor_id,
    const char *department_id,
    const char *registered_at,
    Registration *out_registration
) {
    Result result = MenuApplication_require_patient_session(application);

    if (out_registration == 0) {
        return Result_make_failure("self registration output missing");
    }

    if (result.success == 0) {
        return result;
    }

    return RegistrationService_create(
        &application->registration_service,
        application->bound_patient_id,
        doctor_id,
        department_id,
        registered_at,
        out_registration
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

Result MenuApplication_query_registrations_by_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    RegistrationRepositoryFilter filter;
    LinkedList registrations;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;

    LinkedList_init(&registrations);
    RegistrationRepositoryFilter_init(&filter);
    result = RegistrationService_find_by_patient_id(
        &application->registration_service,
        patient_id,
        &filter,
        &registrations
    );
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "患者挂号记录: %s | count=%zu",
        patient_id,
        LinkedList_count(&registrations)
    );
    if (result.success == 0) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    used = strlen(buffer);
    current = registrations.head;
    while (current != 0) {
        const Registration *registration = (const Registration *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | 患者=%s | 医生=%s | 科室=%s | 状态=%s",
            registration->registration_id,
            registration->patient_id,
            registration->doctor_id,
            registration->department_id,
            MenuApplication_registration_status_label(registration->status)
        );
        if (result.success == 0) {
            RegistrationRepository_clear_list(&registrations);
            return result;
        }

        current = current->next;
    }

    RegistrationRepository_clear_list(&registrations);
    return Result_make_success("patient registrations ready");
}

Result MenuApplication_query_pending_registrations_by_doctor(
    MenuApplication *application,
    const char *doctor_id,
    char *buffer,
    size_t capacity
) {
    RegistrationRepositoryFilter filter;
    LinkedList registrations;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;

    LinkedList_init(&registrations);
    RegistrationRepositoryFilter_init(&filter);
    filter.use_status = 1;
    filter.status = REG_STATUS_PENDING;

    result = RegistrationService_find_by_doctor_id(
        &application->registration_service,
        doctor_id,
        &filter,
        &registrations
    );
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "待诊列表: %s | count=%zu",
        doctor_id,
        LinkedList_count(&registrations)
    );
    if (result.success == 0) {
        RegistrationRepository_clear_list(&registrations);
        return result;
    }

    used = strlen(buffer);
    current = registrations.head;
    while (current != 0) {
        const Registration *registration = (const Registration *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | 患者=%s | 科室=%s | 时间=%s",
            registration->registration_id,
            registration->patient_id,
            registration->department_id,
            registration->registered_at
        );
        if (result.success == 0) {
            RegistrationRepository_clear_list(&registrations);
            return result;
        }

        current = current->next;
    }

    RegistrationRepository_clear_list(&registrations);
    return Result_make_success("doctor pending registrations ready");
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
    MenuApplicationVisitHandoff handoff;
    Result result = MenuApplication_create_visit_record_handoff(
        application,
        registration_id,
        chief_complaint,
        diagnosis,
        advice,
        need_exam,
        need_admission,
        need_medicine,
        visit_time,
        &handoff
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "看诊记录已创建: %s | 挂号=%s | 患者=%s | 诊断=%s",
        handoff.visit_id,
        handoff.registration_id,
        handoff.patient_id,
        handoff.diagnosis
    );
}

Result MenuApplication_create_visit_record_handoff(
    MenuApplication *application,
    const char *registration_id,
    const char *chief_complaint,
    const char *diagnosis,
    const char *advice,
    int need_exam,
    int need_admission,
    int need_medicine,
    const char *visit_time,
    MenuApplicationVisitHandoff *out_handoff
) {
    VisitRecord record;
    Result result;

    if (application == 0 || out_handoff == 0) {
        return Result_make_failure("visit handoff arguments missing");
    }

    result = MedicalRecordService_create_visit_record(
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
        return result;
    }

    MenuApplication_fill_visit_handoff(&record, out_handoff);
    return Result_make_success("visit handoff ready");
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

Result MenuApplication_create_examination_record(
    MenuApplication *application,
    const char *visit_id,
    const char *exam_item,
    const char *exam_type,
    const char *requested_at,
    char *buffer,
    size_t capacity
) {
    ExaminationRecord record;
    Result result = MedicalRecordService_create_examination_record(
        &application->medical_record_service,
        visit_id,
        exam_item,
        exam_type,
        requested_at,
        &record
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "检查记录已创建: %s | 就诊=%s | 项目=%s | 类型=%s",
        record.examination_id,
        record.visit_id,
        record.exam_item,
        record.exam_type
    );
}

Result MenuApplication_complete_examination_record(
    MenuApplication *application,
    const char *examination_id,
    const char *result_text,
    const char *completed_at,
    char *buffer,
    size_t capacity
) {
    ExaminationRecord record;
    Result result = MedicalRecordService_update_examination_record(
        &application->medical_record_service,
        examination_id,
        EXAM_STATUS_COMPLETED,
        result_text,
        completed_at,
        &record
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "检查结果已回写: %s | 结果=%s | 完成时间=%s",
        record.examination_id,
        record.result,
        record.completed_at
    );
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

Result MenuApplication_transfer_bed(
    MenuApplication *application,
    const char *admission_id,
    const char *target_bed_id,
    const char *transferred_at,
    char *buffer,
    size_t capacity
) {
    Admission admission;
    Result result = InpatientService_transfer_bed(
        &application->inpatient_service,
        admission_id,
        target_bed_id,
        transferred_at,
        &admission
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "转床成功: %s | 病房=%s | 床位=%s | 时间=%s",
        admission.admission_id,
        admission.ward_id,
        admission.bed_id,
        transferred_at
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
    Medicine stored_medicine;
    Result result;

    if (application == 0 || medicine == 0) {
        return Result_make_failure("medicine add arguments missing");
    }

    stored_medicine = *medicine;
    if (MenuApplication_is_blank(stored_medicine.medicine_id)) {
        result = MenuApplication_generate_medicine_id(
            application,
            stored_medicine.medicine_id,
            sizeof(stored_medicine.medicine_id)
        );
        if (result.success == 0) {
            return MenuApplication_write_failure(result.message, buffer, capacity);
        }
    }

    result = PharmacyService_add_medicine(&application->pharmacy_service, &stored_medicine);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "药品已添加: %s | %s | 单价=%.2f | 库存=%d",
        stored_medicine.medicine_id,
        stored_medicine.name,
        stored_medicine.price,
        stored_medicine.stock
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
    const char *patient_id,
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
    Result result = PharmacyService_dispense_medicine_for_patient(
        &application->pharmacy_service,
        patient_id,
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
        "发药完成: 患者=%s | 处方=%s | 药品=%s | 数量=%d | 当前库存=%d",
        record.patient_id,
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

static Result MenuApplication_query_dispense_history_by_prescription_id(
    MenuApplication *application,
    const char *prescription_id,
    char *buffer,
    size_t capacity
) {
    LinkedList records;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;

    if (application == 0) {
        return Result_make_failure("menu dispense history application missing");
    }

    LinkedList_init(&records);
    result = PharmacyService_find_dispense_records_by_prescription_id(
        &application->pharmacy_service,
        prescription_id,
        &records
    );
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "发药记录(%zu): 处方=%s",
        LinkedList_count(&records),
        prescription_id
    );
    if (result.success == 0) {
        PharmacyService_clear_dispense_record_results(&records);
        return result;
    }

    used = strlen(buffer);
    current = records.head;
    while (current != 0) {
        const DispenseRecord *record = (const DispenseRecord *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | %s | 数量=%d | 发药人=%s | %s",
            record->dispense_id,
            record->medicine_id,
            record->quantity,
            record->pharmacist_id,
            record->dispensed_at
        );
        if (result.success == 0) {
            PharmacyService_clear_dispense_record_results(&records);
            return result;
        }

        current = current->next;
    }

    PharmacyService_clear_dispense_record_results(&records);
    return Result_make_success("dispense history ready");
}

static Result MenuApplication_query_dispense_history_by_patient_id(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    LinkedList records;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;

    if (application == 0) {
        return Result_make_failure("menu dispense history application missing");
    }

    LinkedList_init(&records);
    result = PharmacyService_find_dispense_records_by_patient_id(
        &application->pharmacy_service,
        patient_id,
        &records
    );
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "发药记录(%zu): 患者=%s",
        LinkedList_count(&records),
        patient_id
    );
    if (result.success == 0) {
        PharmacyService_clear_dispense_record_results(&records);
        return result;
    }

    used = strlen(buffer);
    current = records.head;
    while (current != 0) {
        const DispenseRecord *record = (const DispenseRecord *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | 处方=%s | 药品=%s | 数量=%d | 发药人=%s | %s",
            record->dispense_id,
            record->prescription_id,
            record->medicine_id,
            record->quantity,
            record->pharmacist_id,
            record->dispensed_at
        );
        if (result.success == 0) {
            PharmacyService_clear_dispense_record_results(&records);
            return result;
        }

        current = current->next;
    }

    PharmacyService_clear_dispense_record_results(&records);
    return Result_make_success("dispense patient history ready");
}

static void MenuApplication_discard_rest_of_line(FILE *input) {
    int character = 0;

    if (input == 0) {
        return;
    }

    while ((character = fgetc(input)) != EOF) {
        if (character == '\n') {
            return;
        }
    }
}

typedef struct MenuApplicationPromptContext {
    FILE *input;
    FILE *output;
} MenuApplicationPromptContext;

#define MENU_APPLICATION_SELECT_OPTION_MAX 256

typedef struct MenuApplicationSelectionOption {
    char id[HIS_DOMAIN_ID_CAPACITY];
    char label[256];
} MenuApplicationSelectionOption;

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
    if (length > 0 && buffer[length - 1] != '\n' && !feof(context->input)) {
        MenuApplication_discard_rest_of_line(context->input);
        buffer[0] = '\0';
        return Result_make_failure("input too long");
    }

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
    if (end_pointer == buffer || end_pointer == 0 || value < INT_MIN || value > INT_MAX) {
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

static int MenuApplication_char_equal_ignore_case(char left, char right) {
    return tolower((unsigned char)left) == tolower((unsigned char)right);
}

static int MenuApplication_text_contains_ignore_case(const char *text, const char *query) {
    size_t text_index = 0;
    size_t query_length = 0;
    size_t query_index = 0;

    if (query == 0 || query[0] == '\0') {
        return 1;
    }
    if (text == 0) {
        return 0;
    }

    query_length = strlen(query);
    while (text[text_index] != '\0') {
        for (query_index = 0; query_index < query_length; query_index++) {
            if (text[text_index + query_index] == '\0') {
                return 0;
            }
            if (!MenuApplication_char_equal_ignore_case(text[text_index + query_index], query[query_index])) {
                break;
            }
        }
        if (query_index == query_length) {
            return 1;
        }
        text_index++;
    }

    return 0;
}

static int MenuApplication_find_exact_selection(
    const MenuApplicationSelectionOption *options,
    int option_count,
    const char *query
) {
    int index = 0;

    if (options == 0 || option_count <= 0 || query == 0 || query[0] == '\0') {
        return -1;
    }

    for (index = 0; index < option_count; index++) {
        if (strcmp(options[index].id, query) == 0 || strcmp(options[index].label, query) == 0) {
            return index;
        }
    }

    return -1;
}

static Result MenuApplication_prompt_select_option(
    MenuApplicationPromptContext *context,
    const char *title,
    const MenuApplicationSelectionOption *options,
    int option_count,
    char *out_id,
    size_t out_id_capacity
) {
    char query[128];
    int filtered_indices[MENU_APPLICATION_SELECT_OPTION_MAX];
    int filtered_count = 0;
    int selected_number = 0;
    int selected_index = -1;
    int option_index = 0;
    Result result;

    if (context == 0 || options == 0 || option_count <= 0 || out_id == 0 || out_id_capacity == 0) {
        return Result_make_failure("selection arguments invalid");
    }

    result = MenuApplication_prompt_line(
        context,
        title,
        query,
        sizeof(query)
    );
    if (result.success == 0) {
        return result;
    }

    selected_index = MenuApplication_find_exact_selection(options, option_count, query);
    if (selected_index >= 0) {
        MenuApplication_copy_text(out_id, out_id_capacity, options[selected_index].id);
        fprintf(context->output, "已选择: %s\n", options[selected_index].label);
        return Result_make_success("selection chosen");
    }

    for (option_index = 0; option_index < option_count; option_index++) {
        if (MenuApplication_text_contains_ignore_case(options[option_index].label, query) ||
            MenuApplication_text_contains_ignore_case(options[option_index].id, query)) {
            filtered_indices[filtered_count] = option_index;
            filtered_count++;
        }
    }

    if (filtered_count == 0) {
        return Result_make_failure("selection candidates not found");
    }

    fprintf(context->output, "候选列表:\n");
    for (option_index = 0; option_index < filtered_count; option_index++) {
        fprintf(
            context->output,
            "%d. %s\n",
            option_index + 1,
            options[filtered_indices[option_index]].label
        );
    }

    result = MenuApplication_prompt_int(context, "请选择序号: ", &selected_number);
    if (result.success == 0) {
        return result;
    }
    if (selected_number <= 0 || selected_number > filtered_count) {
        return Result_make_failure("selection index invalid");
    }

    selected_index = filtered_indices[selected_number - 1];
    MenuApplication_copy_text(out_id, out_id_capacity, options[selected_index].id);
    fprintf(context->output, "已选择: %s\n", options[selected_index].label);
    return Result_make_success("selection chosen");
}

static Result MenuApplication_prompt_select_patient(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    char *out_patient_id,
    size_t out_patient_id_capacity
) {
    LinkedList patients;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&patients);
    result = PatientRepository_load_all(&application->patient_service.repository, &patients);
    if (result.success == 0) {
        return result;
    }

    current = patients.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Patient *patient = (const Patient *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), patient->patient_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | %s | %d岁 | 联系=%s",
            patient->patient_id,
            patient->name,
            patient->age,
            patient->contact
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_patient_id,
        out_patient_id_capacity
    );
    PatientRepository_clear_loaded_list(&patients);
    return result;
}

static Result MenuApplication_prompt_select_department(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    char *out_department_id,
    size_t out_department_id_capacity
) {
    LinkedList departments;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&departments);
    result = DepartmentRepository_load_all(&application->doctor_service.department_repository, &departments);
    if (result.success == 0) {
        return result;
    }

    current = departments.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Department *department = (const Department *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), department->department_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | %s | %s",
            department->department_id,
            department->name,
            department->location
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_department_id,
        out_department_id_capacity
    );
    DepartmentRepository_clear_list(&departments);
    return result;
}

static Result MenuApplication_prompt_select_doctor(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *department_id_filter,
    char *out_doctor_id,
    size_t out_doctor_id_capacity
) {
    LinkedList doctors;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&doctors);
    if (department_id_filter != 0 && department_id_filter[0] != '\0') {
        result = DoctorService_list_by_department(
            &application->doctor_service,
            department_id_filter,
            &doctors
        );
    } else {
        result = DoctorRepository_load_all(&application->doctor_service.doctor_repository, &doctors);
    }
    if (result.success == 0) {
        return result;
    }

    current = doctors.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Doctor *doctor = (const Doctor *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), doctor->doctor_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | %s | %s | %s",
            doctor->doctor_id,
            doctor->name,
            doctor->title,
            doctor->schedule
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_doctor_id,
        out_doctor_id_capacity
    );
    DoctorService_clear_list(&doctors);
    return result;
}

static Result MenuApplication_prompt_select_registration(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *patient_id_filter,
    const char *doctor_id_filter,
    int status_filter,
    int apply_status_filter,
    char *out_registration_id,
    size_t out_registration_id_capacity
) {
    LinkedList registrations;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&registrations);
    result = RegistrationRepository_load_all(&application->registration_repository, &registrations);
    if (result.success == 0) {
        return result;
    }

    current = registrations.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Registration *registration = (const Registration *)current->data;
        if (patient_id_filter != 0 && patient_id_filter[0] != '\0' &&
            strcmp(registration->patient_id, patient_id_filter) != 0) {
            current = current->next;
            continue;
        }
        if (doctor_id_filter != 0 && doctor_id_filter[0] != '\0' &&
            strcmp(registration->doctor_id, doctor_id_filter) != 0) {
            current = current->next;
            continue;
        }
        if (apply_status_filter != 0 && registration->status != status_filter) {
            current = current->next;
            continue;
        }

        MenuApplication_copy_text(
            options[option_count].id,
            sizeof(options[option_count].id),
            registration->registration_id
        );
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | 患者=%s | 医生=%s | 科室=%s | 时间=%s | 状态=%s",
            registration->registration_id,
            registration->patient_id,
            registration->doctor_id,
            registration->department_id,
            registration->registered_at,
            MenuApplication_registration_status_label(registration->status)
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_registration_id,
        out_registration_id_capacity
    );
    RegistrationRepository_clear_list(&registrations);
    return result;
}

static Result MenuApplication_prompt_select_visit(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *patient_id_filter,
    const char *doctor_id_filter,
    char *out_visit_id,
    size_t out_visit_id_capacity
) {
    LinkedList visits;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&visits);
    result = VisitRecordRepository_load_all(
        &application->medical_record_service.visit_repository,
        &visits
    );
    if (result.success == 0) {
        return result;
    }

    current = visits.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const VisitRecord *visit = (const VisitRecord *)current->data;
        if (patient_id_filter != 0 && patient_id_filter[0] != '\0' &&
            strcmp(visit->patient_id, patient_id_filter) != 0) {
            current = current->next;
            continue;
        }
        if (doctor_id_filter != 0 && doctor_id_filter[0] != '\0' &&
            strcmp(visit->doctor_id, doctor_id_filter) != 0) {
            current = current->next;
            continue;
        }

        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), visit->visit_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | 挂号=%s | 患者=%s | 诊断=%s | 时间=%s",
            visit->visit_id,
            visit->registration_id,
            visit->patient_id,
            visit->diagnosis,
            visit->visit_time
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_visit_id,
        out_visit_id_capacity
    );
    VisitRecordRepository_clear_list(&visits);
    return result;
}

static Result MenuApplication_prompt_select_examination(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *doctor_id_filter,
    int pending_only,
    char *out_examination_id,
    size_t out_examination_id_capacity
) {
    LinkedList records;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&records);
    result = ExaminationRecordRepository_load_all(
        &application->medical_record_service.examination_repository,
        &records
    );
    if (result.success == 0) {
        return result;
    }

    current = records.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const ExaminationRecord *record = (const ExaminationRecord *)current->data;
        if (doctor_id_filter != 0 && doctor_id_filter[0] != '\0' &&
            strcmp(record->doctor_id, doctor_id_filter) != 0) {
            current = current->next;
            continue;
        }
        if (pending_only != 0 && record->status != EXAM_STATUS_PENDING) {
            current = current->next;
            continue;
        }

        MenuApplication_copy_text(
            options[option_count].id,
            sizeof(options[option_count].id),
            record->examination_id
        );
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | 就诊=%s | 患者=%s | 项目=%s | 状态=%s",
            record->examination_id,
            record->visit_id,
            record->patient_id,
            record->exam_item,
            record->status == EXAM_STATUS_COMPLETED ? "已完成" : "待完成"
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_examination_id,
        out_examination_id_capacity
    );
    ExaminationRecordRepository_clear_list(&records);
    return result;
}

static Result MenuApplication_prompt_select_ward(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    char *out_ward_id,
    size_t out_ward_id_capacity
) {
    LinkedList wards;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&wards);
    result = WardRepository_load_all(&application->inpatient_service.ward_repository, &wards);
    if (result.success == 0) {
        return result;
    }

    current = wards.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Ward *ward = (const Ward *)current->data;
        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), ward->ward_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | %s | %s | 已占用=%d/%d",
            ward->ward_id,
            ward->name,
            ward->location,
            ward->occupied_beds,
            ward->capacity
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_ward_id,
        out_ward_id_capacity
    );
    WardRepository_clear_loaded_list(&wards);
    return result;
}

static Result MenuApplication_prompt_select_bed(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *ward_id_filter,
    int available_only,
    int occupied_only,
    char *out_bed_id,
    size_t out_bed_id_capacity
) {
    LinkedList beds;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&beds);
    result = BedRepository_load_all(&application->inpatient_service.bed_repository, &beds);
    if (result.success == 0) {
        return result;
    }

    current = beds.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Bed *bed = (const Bed *)current->data;
        if (ward_id_filter != 0 && ward_id_filter[0] != '\0' &&
            strcmp(bed->ward_id, ward_id_filter) != 0) {
            current = current->next;
            continue;
        }
        if (available_only != 0 && bed->status != BED_STATUS_AVAILABLE) {
            current = current->next;
            continue;
        }
        if (occupied_only != 0 && bed->status != BED_STATUS_OCCUPIED) {
            current = current->next;
            continue;
        }

        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), bed->bed_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | 病区=%s | 房间=%s | 床位=%s | 状态=%s",
            bed->bed_id,
            bed->ward_id,
            bed->room_no,
            bed->bed_no,
            MenuApplication_bed_status_label(bed->status)
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_bed_id,
        out_bed_id_capacity
    );
    BedRepository_clear_loaded_list(&beds);
    return result;
}

static Result MenuApplication_prompt_select_admission(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *patient_id_filter,
    int active_only,
    char *out_admission_id,
    size_t out_admission_id_capacity
) {
    LinkedList admissions;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&admissions);
    result = AdmissionRepository_load_all(&application->inpatient_service.admission_repository, &admissions);
    if (result.success == 0) {
        return result;
    }

    current = admissions.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Admission *admission = (const Admission *)current->data;
        if (patient_id_filter != 0 && patient_id_filter[0] != '\0' &&
            strcmp(admission->patient_id, patient_id_filter) != 0) {
            current = current->next;
            continue;
        }
        if (active_only != 0 && admission->status != ADMISSION_STATUS_ACTIVE) {
            current = current->next;
            continue;
        }

        MenuApplication_copy_text(
            options[option_count].id,
            sizeof(options[option_count].id),
            admission->admission_id
        );
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | 患者=%s | 病区=%s | 床位=%s | 状态=%s",
            admission->admission_id,
            admission->patient_id,
            admission->ward_id,
            admission->bed_id,
            MenuApplication_admission_status_label(admission->status)
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_admission_id,
        out_admission_id_capacity
    );
    AdmissionRepository_clear_loaded_list(&admissions);
    return result;
}

static Result MenuApplication_prompt_select_medicine(
    MenuApplication *application,
    MenuApplicationPromptContext *context,
    const char *prompt,
    const char *department_id_filter,
    char *out_medicine_id,
    size_t out_medicine_id_capacity
) {
    LinkedList medicines;
    const LinkedListNode *current = 0;
    MenuApplicationSelectionOption options[MENU_APPLICATION_SELECT_OPTION_MAX];
    int option_count = 0;
    Result result;

    LinkedList_init(&medicines);
    result = MedicineRepository_load_all(&application->pharmacy_service.medicine_repository, &medicines);
    if (result.success == 0) {
        return result;
    }

    current = medicines.head;
    while (current != 0 && option_count < MENU_APPLICATION_SELECT_OPTION_MAX) {
        const Medicine *medicine = (const Medicine *)current->data;
        if (department_id_filter != 0 && department_id_filter[0] != '\0' &&
            strcmp(medicine->department_id, department_id_filter) != 0) {
            current = current->next;
            continue;
        }

        MenuApplication_copy_text(options[option_count].id, sizeof(options[option_count].id), medicine->medicine_id);
        snprintf(
            options[option_count].label,
            sizeof(options[option_count].label),
            "%s | %s | 库存=%d | 阈值=%d",
            medicine->medicine_id,
            medicine->name,
            medicine->stock,
            medicine->low_stock_threshold
        );
        option_count++;
        current = current->next;
    }

    result = MenuApplication_prompt_select_option(
        context,
        prompt,
        options,
        option_count,
        out_medicine_id,
        out_medicine_id_capacity
    );
    MedicineRepository_clear_list(&medicines);
    return result;
}

static Result MenuApplication_prompt_patient_form(
    MenuApplicationPromptContext *context,
    Patient *out_patient,
    int require_patient_id
) {
    int gender = 0;
    int age = 0;
    int is_inpatient = 0;
    Result result;

    if (out_patient == 0) {
        return Result_make_failure("patient form missing");
    }

    memset(out_patient, 0, sizeof(*out_patient));
    if (require_patient_id != 0) {
        result = MenuApplication_prompt_line(
            context,
            "患者编号: ",
            out_patient->patient_id,
            sizeof(out_patient->patient_id)
        );
        if (result.success == 0) {
            return result;
        }
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
    Medicine *out_medicine,
    int require_medicine_id
) {
    char price_buffer[64];
    char *end_pointer = 0;
    int stock = 0;
    int threshold = 0;
    Result result;

    if (out_medicine == 0) {
        return Result_make_failure("medicine form missing");
    }

    memset(out_medicine, 0, sizeof(*out_medicine));
    if (require_medicine_id != 0) {
        result = MenuApplication_prompt_line(
            context,
            "药品编号: ",
            out_medicine->medicine_id,
            sizeof(out_medicine->medicine_id)
        );
        if (result.success == 0) {
            return result;
        }
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
    out_medicine->price = strtod(price_buffer, &end_pointer);
    if (end_pointer == price_buffer || end_pointer == 0) {
        return Result_make_failure("medicine price invalid");
    }
    while (*end_pointer != '\0') {
        if (!isspace((unsigned char)*end_pointer)) {
            return Result_make_failure("medicine price invalid");
        }

        end_pointer++;
    }
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

static Result MenuApplication_prompt_department_form(
    MenuApplicationPromptContext *context,
    Department *out_department,
    int require_department_id
) {
    if (out_department == 0) {
        return Result_make_failure("department form missing");
    }

    memset(out_department, 0, sizeof(*out_department));
    if (require_department_id != 0) {
        if (MenuApplication_prompt_line(
                context,
                "科室编号: ",
                out_department->department_id,
                sizeof(out_department->department_id)
            ).success == 0) {
            return Result_make_failure("input ended");
        }
    }
    if (MenuApplication_prompt_line(
            context,
            "科室名称: ",
            out_department->name,
            sizeof(out_department->name)
        ).success == 0) {
        return Result_make_failure("input ended");
    }
    if (MenuApplication_prompt_line(
            context,
            "科室位置: ",
            out_department->location,
            sizeof(out_department->location)
        ).success == 0) {
        return Result_make_failure("input ended");
    }
    return MenuApplication_prompt_line(
        context,
        "科室描述: ",
        out_department->description,
        sizeof(out_department->description)
    );
}

static Result MenuApplication_prompt_doctor_form(
    MenuApplicationPromptContext *context,
    Doctor *out_doctor,
    int require_doctor_id
) {
    int status = 0;
    Result result;

    if (out_doctor == 0) {
        return Result_make_failure("doctor form missing");
    }

    memset(out_doctor, 0, sizeof(*out_doctor));
    if (require_doctor_id != 0) {
        result = MenuApplication_prompt_line(
            context,
            "医生工号: ",
            out_doctor->doctor_id,
            sizeof(out_doctor->doctor_id)
        );
        if (result.success == 0) {
            return result;
        }
    }
    result = MenuApplication_prompt_line(
        context,
        "姓名: ",
        out_doctor->name,
        sizeof(out_doctor->name)
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_prompt_line(
        context,
        "职称: ",
        out_doctor->title,
        sizeof(out_doctor->title)
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_prompt_line(
        context,
        "所属科室编号: ",
        out_doctor->department_id,
        sizeof(out_doctor->department_id)
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_prompt_line(
        context,
        "出诊安排: ",
        out_doctor->schedule,
        sizeof(out_doctor->schedule)
    );
    if (result.success == 0) {
        return result;
    }
    result = MenuApplication_prompt_int(context, "状态(0停用/1启用): ", &status);
    if (result.success == 0) {
        return result;
    }
    out_doctor->status = (DoctorStatus)status;
    return Result_make_success("doctor form ready");
}

Result MenuApplication_delete_patient(
    MenuApplication *application,
    const char *patient_id,
    char *buffer,
    size_t capacity
) {
    Result result = PatientService_delete_patient(&application->patient_service, patient_id);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(buffer, capacity, "患者已删除: %s", patient_id);
}

Result MenuApplication_list_departments(
    MenuApplication *application,
    char *buffer,
    size_t capacity
) {
    LinkedList departments;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result;

    if (application == 0) {
        return Result_make_failure("menu application missing");
    }

    result = DepartmentRepository_load_all(
        &application->doctor_service.department_repository,
        &departments
    );
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "科室列表(%zu)",
        LinkedList_count(&departments)
    );
    if (result.success == 0) {
        DepartmentRepository_clear_list(&departments);
        return result;
    }

    used = strlen(buffer);
    current = departments.head;
    while (current != 0) {
        const Department *department = (const Department *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | %s | 位置=%s | 描述=%s",
            department->department_id,
            department->name,
            department->location,
            department->description
        );
        if (result.success == 0) {
            DepartmentRepository_clear_list(&departments);
            return result;
        }

        current = current->next;
    }

    DepartmentRepository_clear_list(&departments);
    return Result_make_success("department list ready");
}

Result MenuApplication_add_department(
    MenuApplication *application,
    const Department *department,
    char *buffer,
    size_t capacity
) {
    DepartmentService department_service;
    Department stored_department;
    Result result;

    if (application == 0 || department == 0) {
        return Result_make_failure("department add arguments missing");
    }

    stored_department = *department;
    if (MenuApplication_is_blank(stored_department.department_id)) {
        result = MenuApplication_generate_department_id(
            application,
            stored_department.department_id,
            sizeof(stored_department.department_id)
        );
        if (result.success == 0) {
            return MenuApplication_write_failure(result.message, buffer, capacity);
        }
    }

    department_service.repository = application->doctor_service.department_repository;
    result = DepartmentService_add(&department_service, &stored_department);
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "科室已添加: %s | %s",
        stored_department.department_id,
        stored_department.name
    );
}

Result MenuApplication_update_department(
    MenuApplication *application,
    const Department *department,
    char *buffer,
    size_t capacity
) {
    DepartmentService department_service;
    Result result;

    if (application == 0 || department == 0) {
        return Result_make_failure("department update arguments missing");
    }

    department_service.repository = application->doctor_service.department_repository;
    result = DepartmentService_update(&department_service, department);
    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "科室已更新: %s | %s",
        department->department_id,
        department->name
    );
}

Result MenuApplication_add_doctor(
    MenuApplication *application,
    const Doctor *doctor,
    char *buffer,
    size_t capacity
) {
    Doctor stored_doctor;
    Result result;

    if (application == 0 || doctor == 0) {
        return Result_make_failure("doctor add arguments missing");
    }

    stored_doctor = *doctor;
    if (MenuApplication_is_blank(stored_doctor.doctor_id)) {
        result = MenuApplication_generate_doctor_id(
            application,
            stored_doctor.doctor_id,
            sizeof(stored_doctor.doctor_id)
        );
        if (result.success == 0) {
            return MenuApplication_write_failure(result.message, buffer, capacity);
        }
    }

    result = DoctorService_add(&application->doctor_service, &stored_doctor);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "医生已添加: %s | %s | 科室=%s",
        stored_doctor.doctor_id,
        stored_doctor.name,
        stored_doctor.department_id
    );
}

Result MenuApplication_query_doctor(
    MenuApplication *application,
    const char *doctor_id,
    char *buffer,
    size_t capacity
) {
    Doctor doctor;
    Result result = DoctorService_get_by_id(&application->doctor_service, doctor_id, &doctor);

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "医生信息: %s | %s | 职称=%s | 科室=%s | 排班=%s",
        doctor.doctor_id,
        doctor.name,
        doctor.title,
        doctor.department_id,
        doctor.schedule
    );
}

Result MenuApplication_list_doctors_by_department(
    MenuApplication *application,
    const char *department_id,
    char *buffer,
    size_t capacity
) {
    LinkedList doctors;
    const LinkedListNode *current = 0;
    size_t used = 0;
    Result result = DoctorService_list_by_department(
        &application->doctor_service,
        department_id,
        &doctors
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "科室医生列表: %s | count=%zu",
        department_id,
        LinkedList_count(&doctors)
    );
    if (result.success == 0) {
        DoctorService_clear_list(&doctors);
        return result;
    }

    used = strlen(buffer);
    current = doctors.head;
    while (current != 0) {
        const Doctor *doctor = (const Doctor *)current->data;

        result = MenuApplication_append_text(
            buffer,
            capacity,
            &used,
            "\n%s | %s | 职称=%s | 排班=%s",
            doctor->doctor_id,
            doctor->name,
            doctor->title,
            doctor->schedule
        );
        if (result.success == 0) {
            DoctorService_clear_list(&doctors);
            return result;
        }

        current = current->next;
    }

    DoctorService_clear_list(&doctors);
    return Result_make_success("department doctor list ready");
}

Result MenuApplication_query_records_by_time_range(
    MenuApplication *application,
    const char *time_from,
    const char *time_to,
    char *buffer,
    size_t capacity
) {
    MedicalRecordHistory history;
    Result result = MedicalRecordService_find_records_by_time_range(
        &application->medical_record_service,
        time_from,
        time_to,
        &history
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    result = MenuApplication_write_text(
        buffer,
        capacity,
        "时间范围记录: %s ~ %s | registrations=%zu | visits=%zu | examinations=%zu | admissions=%zu",
        time_from,
        time_to,
        LinkedList_count(&history.registrations),
        LinkedList_count(&history.visits),
        LinkedList_count(&history.examinations),
        LinkedList_count(&history.admissions)
    );
    MedicalRecordHistory_clear(&history);
    return result;
}

Result MenuApplication_query_medicine_detail(
    MenuApplication *application,
    const char *medicine_id,
    char *buffer,
    size_t capacity,
    int include_instruction_note
) {
    Medicine medicine;
    Result result = MedicineRepository_find_by_medicine_id(
        &application->pharmacy_service.medicine_repository,
        medicine_id,
        &medicine
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    if (include_instruction_note != 0) {
        return MenuApplication_write_text(
            buffer,
            capacity,
            "药品信息: %s | %s | 单价=%.2f | 科室=%s | 当前系统未维护用法说明",
            medicine.medicine_id,
            medicine.name,
            medicine.price,
            medicine.department_id
        );
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "药品信息: %s | %s | 单价=%.2f | 库存=%d | 科室=%s",
        medicine.medicine_id,
        medicine.name,
        medicine.price,
        medicine.stock,
        medicine.department_id
    );
}

Result MenuApplication_discharge_check(
    MenuApplication *application,
    const char *admission_id,
    char *buffer,
    size_t capacity
) {
    Admission admission;
    Result result = InpatientService_find_by_id(
        &application->inpatient_service,
        admission_id,
        &admission
    );

    if (result.success == 0) {
        return MenuApplication_write_failure(result.message, buffer, capacity);
    }

    if (admission.status != ADMISSION_STATUS_ACTIVE) {
        return MenuApplication_write_failure("admission not active", buffer, capacity);
    }

    return MenuApplication_write_text(
        buffer,
        capacity,
        "出院前检查: %s | 患者=%s | 病房=%s | 床位=%s | 可办理出院",
        admission.admission_id,
        admission.patient_id,
        admission.ward_id,
        admission.bed_id
    );
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
    char text_value[HIS_DOMAIN_TEXT_CAPACITY];
    char time_value[HIS_DOMAIN_TIME_CAPACITY];
    char long_text[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    Patient patient;
    Doctor doctor;
    Department department;
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
    memset(text_value, 0, sizeof(text_value));
    memset(time_value, 0, sizeof(time_value));
    memset(long_text, 0, sizeof(long_text));

    switch (action) {
        case MENU_ACTION_ADMIN_PATIENT_MANAGEMENT:
            result = MenuApplication_prompt_line(
                &context,
                "1.添加 2.修改 3.删除 4.查询\n请选择: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            if (strcmp(first_id, "1") == 0) {
                result = MenuApplication_prompt_patient_form(&context, &patient, 0);
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_add_patient(
                    application,
                    &patient,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "2") == 0) {
                result = MenuApplication_prompt_patient_form(&context, &patient, 1);
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_update_patient(
                    application,
                    &patient,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "3") == 0) {
                result = MenuApplication_prompt_select_patient(
                    application,
                    &context,
                    "患者搜索关键字/编号(回车列出全部): ",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_delete_patient(
                    application,
                    second_id,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "4") == 0) {
                result = MenuApplication_prompt_select_patient(
                    application,
                    &context,
                    "患者搜索关键字/编号(回车列出全部): ",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_query_patient(
                    application,
                    second_id,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else {
                return Result_make_failure("invalid admin patient action");
            }
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_ADMIN_DOCTOR_DEPARTMENT:
            result = MenuApplication_prompt_line(
                &context,
                "1.新增科室 2.修改科室 3.添加医生 4.查询医生 5.按科室查看医生\n请选择: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            if (strcmp(first_id, "1") == 0 || strcmp(first_id, "2") == 0) {
                result = MenuApplication_prompt_department_form(&context, &department, strcmp(first_id, "2") == 0 ? 1 : 0);
                if (result.success == 0) {
                    return result;
                }
                result = strcmp(first_id, "1") == 0
                    ? MenuApplication_add_department(
                        application,
                        &department,
                        output_buffer,
                        sizeof(output_buffer)
                    )
                    : MenuApplication_update_department(
                        application,
                        &department,
                        output_buffer,
                        sizeof(output_buffer)
                    );
            } else if (strcmp(first_id, "3") == 0) {
                result = MenuApplication_prompt_doctor_form(&context, &doctor, 0);
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_add_doctor(
                    application,
                    &doctor,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "4") == 0) {
                result = MenuApplication_prompt_select_doctor(
                    application,
                    &context,
                    "医生搜索关键字/工号(回车列出全部): ",
                    "",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_query_doctor(
                    application,
                    second_id,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "5") == 0) {
                result = MenuApplication_prompt_select_department(
                    application,
                    &context,
                    "科室搜索关键字/编号(回车列出全部): ",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_list_doctors_by_department(
                    application,
                    second_id,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else {
                return Result_make_failure("invalid admin doctor department action");
            }
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_ADMIN_MEDICAL_RECORDS:
            result = MenuApplication_prompt_line(
                &context,
                "起始时间: ",
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "结束时间: ",
                text_value,
                sizeof(text_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_records_by_time_range(
                application,
                time_value,
                text_value,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_ADMIN_WARD_BED_OVERVIEW:
            result = MenuApplication_list_wards(
                application,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            memset(output_buffer, 0, sizeof(output_buffer));
            result = MenuApplication_prompt_select_ward(
                application,
                &context,
                "病区搜索关键字/编号(回车列出全部): ",
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

        case MENU_ACTION_ADMIN_MEDICINE_OVERVIEW:
            result = MenuApplication_prompt_line(
                &context,
                "1.药品详情 2.低库存提醒\n请选择: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            if (strcmp(first_id, "1") == 0) {
                result = MenuApplication_prompt_select_medicine(
                    application,
                    &context,
                    "药品搜索关键字/编号(回车列出全部): ",
                    "",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_query_medicine_detail(
                    application,
                    second_id,
                    output_buffer,
                    sizeof(output_buffer),
                    0
                );
            } else if (strcmp(first_id, "2") == 0) {
                result = MenuApplication_find_low_stock_medicines(
                    application,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else {
                return Result_make_failure("invalid admin medicine action");
            }
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_PATIENT_BASIC_INFO:
            result = MenuApplication_require_patient_session(application);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_patient(
                application,
                application->bound_patient_id,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_PATIENT_QUERY_REGISTRATION:
            result = MenuApplication_require_patient_session(application);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_registrations_by_patient(
                application,
                application->bound_patient_id,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_DOCTOR_PENDING_LIST:
            if (application->has_authenticated_user != 0 && application->authenticated_user.role == USER_ROLE_DOCTOR) {
                MenuApplication_copy_text(first_id, sizeof(first_id), application->authenticated_user.user_id);
            } else {
                result = MenuApplication_prompt_select_doctor(
                    application,
                    &context,
                    "医生搜索关键字/工号(回车列出全部): ",
                    "",
                    first_id,
                    sizeof(first_id)
                );
                if (result.success == 0) {
                    return result;
                }
            }
            result = MenuApplication_query_pending_registrations_by_doctor(
                application,
                first_id,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_DOCTOR_QUERY_PATIENT_HISTORY:
            result = MenuApplication_prompt_select_patient(
                application,
                &context,
                "患者搜索关键字/编号(回车列出全部): ",
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

        case MENU_ACTION_PATIENT_QUERY_VISITS:
        case MENU_ACTION_PATIENT_QUERY_EXAMS:
        case MENU_ACTION_PATIENT_QUERY_ADMISSIONS:
            result = MenuApplication_require_patient_session(application);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_patient_history(
                application,
                application->bound_patient_id,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_PATIENT_QUERY_DISPENSE:
            result = MenuApplication_require_patient_session(application);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_dispense_history_by_patient_id(
                application,
                application->bound_patient_id,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_PATIENT_QUERY_MEDICINE_USAGE:
            result = MenuApplication_require_patient_session(application);
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_select_medicine(
                application,
                &context,
                "药品搜索关键字/编号(回车列出全部): ",
                "",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_query_medicine_detail(
                application,
                first_id,
                output_buffer,
                sizeof(output_buffer),
                1
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_DOCTOR_VISIT_RECORD:
            result = MenuApplication_prompt_select_registration(
                application,
                &context,
                "待接诊挂号搜索关键字/编号(回车列出全部): ",
                "",
                application->has_authenticated_user != 0 ? application->authenticated_user.user_id : "",
                REG_STATUS_PENDING,
                1,
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "主诉: ",
                text_value,
                sizeof(text_value)
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
                text_value,
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

        case MENU_ACTION_DOCTOR_EXAM_RECORD:
            result = MenuApplication_prompt_line(
                &context,
                "1. 新增检查  2. 回写结果\n请选择: ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            if (strcmp(first_id, "1") == 0) {
                result = MenuApplication_prompt_select_visit(
                    application,
                    &context,
                    "就诊搜索关键字/编号(回车列出全部): ",
                    "",
                    application->has_authenticated_user != 0 ? application->authenticated_user.user_id : "",
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_prompt_line(
                    &context,
                    "检查项目: ",
                    text_value,
                    sizeof(text_value)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_prompt_line(
                    &context,
                    "检查类型: ",
                    third_id,
                    sizeof(third_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_prompt_line(
                    &context,
                    "申请时间: ",
                    time_value,
                    sizeof(time_value)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_create_examination_record(
                    application,
                    second_id,
                    text_value,
                    third_id,
                    time_value,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else if (strcmp(first_id, "2") == 0) {
                result = MenuApplication_prompt_select_examination(
                    application,
                    &context,
                    "待回写检查搜索关键字/编号(回车列出全部): ",
                    application->has_authenticated_user != 0 ? application->authenticated_user.user_id : "",
                    1,
                    second_id,
                    sizeof(second_id)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_prompt_line(
                    &context,
                    "检查结果: ",
                    long_text,
                    sizeof(long_text)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_prompt_line(
                    &context,
                    "完成时间: ",
                    time_value,
                    sizeof(time_value)
                );
                if (result.success == 0) {
                    return result;
                }
                result = MenuApplication_complete_examination_record(
                    application,
                    second_id,
                    long_text,
                    time_value,
                    output_buffer,
                    sizeof(output_buffer)
                );
            } else {
                return Result_make_failure("invalid exam action");
            }
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_INPATIENT_QUERY_BED:
            result = MenuApplication_list_wards(
                application,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            memset(output_buffer, 0, sizeof(output_buffer));
            result = MenuApplication_prompt_select_ward(
                application,
                &context,
                "病区搜索关键字/编号(回车列出全部): ",
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

        case MENU_ACTION_INPATIENT_LIST_WARDS:
            result = MenuApplication_list_wards(
                application,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_INPATIENT_LIST_BEDS:
            result = MenuApplication_prompt_select_ward(
                application,
                &context,
                "病区搜索关键字/编号(回车列出全部): ",
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
            result = MenuApplication_prompt_select_patient(
                application,
                &context,
                "患者搜索关键字/编号(回车列出全部): ",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_select_ward(
                application,
                &context,
                "病区搜索关键字/编号(回车列出全部): ",
                second_id,
                sizeof(second_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_select_bed(
                application,
                &context,
                "床位搜索关键字/编号(回车列出全部): ",
                second_id,
                1,
                0,
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
            result = MenuApplication_prompt_select_admission(
                application,
                &context,
                "住院记录搜索关键字/编号(回车列出全部): ",
                "",
                1,
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
            result = MenuApplication_prompt_select_patient(
                application,
                &context,
                "患者搜索关键字/编号(回车列出全部): ",
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

        case MENU_ACTION_INPATIENT_TRANSFER_BED:
            result = MenuApplication_prompt_select_admission(
                application,
                &context,
                "在院住院记录搜索关键字/编号(回车列出全部): ",
                "",
                1,
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_select_bed(
                application,
                &context,
                "目标床位搜索关键字/编号(回车列出全部): ",
                "",
                1,
                0,
                second_id,
                sizeof(second_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_line(
                &context,
                "转床时间: ",
                time_value,
                sizeof(time_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_transfer_bed(
                application,
                first_id,
                second_id,
                time_value,
                output_buffer,
                sizeof(output_buffer)
            );
            if (output_buffer[0] != '\0') {
                fprintf(output, "%s\n", output_buffer);
            }
            return result;

        case MENU_ACTION_INPATIENT_DISCHARGE_CHECK:
            result = MenuApplication_prompt_select_admission(
                application,
                &context,
                "在院住院记录搜索关键字/编号(回车列出全部): ",
                "",
                1,
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_discharge_check(
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
            result = MenuApplication_prompt_medicine_form(&context, &medicine, 0);
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
            result = MenuApplication_prompt_select_medicine(
                application,
                &context,
                "药品搜索关键字/编号(回车列出全部): ",
                "",
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
            result = MenuApplication_prompt_select_patient(
                application,
                &context,
                "患者搜索关键字/编号(回车列出全部): ",
                text_value,
                sizeof(text_value)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_select_visit(
                application,
                &context,
                "处方/就诊搜索关键字/编号(回车列出全部): ",
                text_value,
                "",
                first_id,
                sizeof(first_id)
            );
            if (result.success == 0) {
                return result;
            }
            result = MenuApplication_prompt_select_medicine(
                application,
                &context,
                "药品搜索关键字/编号(回车列出全部): ",
                "",
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
            if (application->has_authenticated_user != 0 && application->authenticated_user.role == USER_ROLE_PHARMACY) {
                MenuApplication_copy_text(third_id, sizeof(third_id), application->authenticated_user.user_id);
            } else {
                result = MenuApplication_prompt_line(
                    &context,
                    "药师工号: ",
                    third_id,
                    sizeof(third_id)
                );
                if (result.success == 0) {
                    return result;
                }
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
                text_value,
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
            result = MenuApplication_prompt_select_medicine(
                application,
                &context,
                "药品搜索关键字/编号(回车列出全部): ",
                "",
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
