#include <string.h>

#include "ui/DesktopApp.h"

int main(int argc, char **argv) {
    int smoke_mode = 0;
    MenuApplicationPaths paths = {
        "data/users.txt",
        "data/patients.txt",
        "data/departments.txt",
        "data/doctors.txt",
        "data/registrations.txt",
        "data/visits.txt",
        "data/examinations.txt",
        "data/wards.txt",
        "data/beds.txt",
        "data/admissions.txt",
        "data/medicines.txt",
        "data/dispense_records.txt"
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

    config.smoke_mode = smoke_mode;
    return DesktopApp_run(&config);
}
