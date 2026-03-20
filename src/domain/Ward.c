#include "domain/Ward.h"

int Ward_has_capacity(const Ward *ward) {
    if (ward == 0 || ward->status != WARD_STATUS_ACTIVE) {
        return 0;
    }

    if (ward->capacity <= 0 || ward->occupied_beds < 0) {
        return 0;
    }

    return ward->occupied_beds < ward->capacity;
}

int Ward_occupy_bed(Ward *ward) {
    if (ward == 0 || !Ward_has_capacity(ward)) {
        return 0;
    }

    ward->occupied_beds++;
    return 1;
}

int Ward_release_bed(Ward *ward) {
    if (ward == 0 || ward->occupied_beds <= 0) {
        return 0;
    }

    ward->occupied_beds--;
    return 1;
}

int Ward_close(Ward *ward) {
    if (ward == 0 || ward->status != WARD_STATUS_ACTIVE || ward->occupied_beds != 0) {
        return 0;
    }

    ward->status = WARD_STATUS_CLOSED;
    return 1;
}

int Ward_open(Ward *ward) {
    if (ward == 0 || ward->status != WARD_STATUS_CLOSED) {
        return 0;
    }

    ward->status = WARD_STATUS_ACTIVE;
    return 1;
}
