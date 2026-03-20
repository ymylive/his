#include "service/DoctorService.h"

#include <ctype.h>
#include <string.h>

static int DoctorService_has_non_empty_text(const char *text) {
    const unsigned char *current = (const unsigned char *)text;

    if (current == 0) {
        return 0;
    }

    while (*current != '\0') {
        if (isspace(*current) == 0) {
            return 1;
        }

        current++;
    }

    return 0;
}

static Result DoctorService_validate(const Doctor *doctor) {
    if (doctor == 0) {
        return Result_make_failure("doctor missing");
    }

    if (!DoctorService_has_non_empty_text(doctor->doctor_id)) {
        return Result_make_failure("doctor id missing");
    }

    if (!DoctorService_has_non_empty_text(doctor->name)) {
        return Result_make_failure("doctor name missing");
    }

    if (!DoctorService_has_non_empty_text(doctor->title)) {
        return Result_make_failure("doctor title missing");
    }

    if (!DoctorService_has_non_empty_text(doctor->department_id)) {
        return Result_make_failure("doctor department missing");
    }

    if (!DoctorService_has_non_empty_text(doctor->schedule)) {
        return Result_make_failure("doctor schedule missing");
    }

    if (doctor->status != DOCTOR_STATUS_INACTIVE &&
        doctor->status != DOCTOR_STATUS_ACTIVE) {
        return Result_make_failure("doctor status invalid");
    }

    return Result_make_success("doctor valid");
}

static Doctor *DoctorService_find_in_list(
    LinkedList *doctors,
    const char *doctor_id
) {
    LinkedListNode *current = 0;

    if (doctors == 0 || doctor_id == 0) {
        return 0;
    }

    current = doctors->head;
    while (current != 0) {
        Doctor *doctor = (Doctor *)current->data;

        if (doctor != 0 && strcmp(doctor->doctor_id, doctor_id) == 0) {
            return doctor;
        }

        current = current->next;
    }

    return 0;
}

static Result DoctorService_ensure_department_exists(
    DoctorService *service,
    const char *department_id
) {
    Department department;

    if (service == 0) {
        return Result_make_failure("doctor service missing");
    }

    if (!DoctorService_has_non_empty_text(department_id)) {
        return Result_make_failure("doctor department missing");
    }

    if (DepartmentRepository_find_by_department_id(
            &service->department_repository,
            department_id,
            &department
        ).success == 0) {
        return Result_make_failure("doctor department not found");
    }

    return Result_make_success("doctor department exists");
}

Result DoctorService_init(
    DoctorService *service,
    const char *doctor_path,
    const char *department_path
) {
    Result result;

    if (service == 0) {
        return Result_make_failure("doctor service missing");
    }

    result = DoctorRepository_init(&service->doctor_repository, doctor_path);
    if (result.success == 0) {
        return result;
    }

    return DepartmentRepository_init(
        &service->department_repository,
        department_path
    );
}

Result DoctorService_add(DoctorService *service, const Doctor *doctor) {
    LinkedList doctors;
    Doctor *existing = 0;
    Result result = DoctorService_validate(doctor);

    if (result.success == 0) {
        return result;
    }

    if (service == 0) {
        return Result_make_failure("doctor service missing");
    }

    result = DoctorService_ensure_department_exists(
        service,
        doctor->department_id
    );
    if (result.success == 0) {
        return result;
    }

    result = DoctorRepository_load_all(&service->doctor_repository, &doctors);
    if (result.success == 0) {
        return result;
    }

    existing = DoctorService_find_in_list(&doctors, doctor->doctor_id);
    DoctorRepository_clear_list(&doctors);
    if (existing != 0) {
        return Result_make_failure("doctor already exists");
    }

    return DoctorRepository_save(&service->doctor_repository, doctor);
}

Result DoctorService_get_by_id(
    DoctorService *service,
    const char *doctor_id,
    Doctor *out_doctor
) {
    if (service == 0) {
        return Result_make_failure("doctor service missing");
    }

    if (!DoctorService_has_non_empty_text(doctor_id) || out_doctor == 0) {
        return Result_make_failure("doctor query arguments missing");
    }

    return DoctorRepository_find_by_doctor_id(
        &service->doctor_repository,
        doctor_id,
        out_doctor
    );
}

Result DoctorService_list_by_department(
    DoctorService *service,
    const char *department_id,
    LinkedList *out_doctors
) {
    Result result;

    if (service == 0) {
        return Result_make_failure("doctor service missing");
    }

    if (out_doctors == 0) {
        return Result_make_failure("doctor output list missing");
    }

    result = DoctorService_ensure_department_exists(service, department_id);
    if (result.success == 0) {
        return result;
    }

    return DoctorRepository_find_by_department_id(
        &service->doctor_repository,
        department_id,
        out_doctors
    );
}

void DoctorService_clear_list(LinkedList *doctors) {
    DoctorRepository_clear_list(doctors);
}
