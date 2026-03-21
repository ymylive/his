#include <string.h>

#include "ui/DesktopApp.h"
#include "repository/TextFileRepository.h"

int main(int argc, char **argv) {
    int smoke_mode = 0;
    char user_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char patient_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char department_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char doctor_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char registration_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char visit_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char examination_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char ward_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char bed_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char admission_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char medicine_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    char dispense_record_path[TEXT_FILE_REPOSITORY_PATH_CAPACITY];
    MenuApplicationPaths paths = {
        user_path,
        patient_path,
        department_path,
        doctor_path,
        registration_path,
        visit_path,
        examination_path,
        ward_path,
        bed_path,
        admission_path,
        medicine_path,
        dispense_record_path
    };
    DesktopAppConfig config = {
        &paths,
        1600,
        960,
        "Lightweight HIS Desktop",
        0
    };

    if (argc > 1 && argv[1] != 0 && strcmp(argv[1], "--smoke") == 0) {
        smoke_mode = 1;
    }

    DesktopApp_resolve_data_path("users.txt", user_path, sizeof(user_path));
    DesktopApp_resolve_data_path("patients.txt", patient_path, sizeof(patient_path));
    DesktopApp_resolve_data_path("departments.txt", department_path, sizeof(department_path));
    DesktopApp_resolve_data_path("doctors.txt", doctor_path, sizeof(doctor_path));
    DesktopApp_resolve_data_path("registrations.txt", registration_path, sizeof(registration_path));
    DesktopApp_resolve_data_path("visits.txt", visit_path, sizeof(visit_path));
    DesktopApp_resolve_data_path("examinations.txt", examination_path, sizeof(examination_path));
    DesktopApp_resolve_data_path("wards.txt", ward_path, sizeof(ward_path));
    DesktopApp_resolve_data_path("beds.txt", bed_path, sizeof(bed_path));
    DesktopApp_resolve_data_path("admissions.txt", admission_path, sizeof(admission_path));
    DesktopApp_resolve_data_path("medicines.txt", medicine_path, sizeof(medicine_path));
    DesktopApp_resolve_data_path("dispense_records.txt", dispense_record_path, sizeof(dispense_record_path));

    config.smoke_mode = smoke_mode;
    return DesktopApp_run(&config);
}
