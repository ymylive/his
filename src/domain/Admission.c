#include "domain/Admission.h"

#include <string.h>

static void Admission_copy_time(char *destination, const char *source) {
    if (destination == 0) {
        return;
    }

    if (source == 0) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, HIS_DOMAIN_TIME_CAPACITY - 1);
    destination[HIS_DOMAIN_TIME_CAPACITY - 1] = '\0';
}

int Admission_is_active(const Admission *admission) {
    return admission != 0 && admission->status == ADMISSION_STATUS_ACTIVE;
}

int Admission_discharge(Admission *admission, const char *discharged_at) {
    if (!Admission_is_active(admission) || discharged_at == 0 || discharged_at[0] == '\0') {
        return 0;
    }

    admission->status = ADMISSION_STATUS_DISCHARGED;
    Admission_copy_time(admission->discharged_at, discharged_at);
    return 1;
}
