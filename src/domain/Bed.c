#include "domain/Bed.h"

#include <string.h>

static void Bed_copy_text(char *destination, size_t capacity, const char *source) {
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

int Bed_is_assignable(const Bed *bed) {
    if (bed == 0 || bed->status != BED_STATUS_AVAILABLE) {
        return 0;
    }

    return bed->current_admission_id[0] == '\0';
}

int Bed_assign(Bed *bed, const char *admission_id, const char *occupied_at) {
    if (bed == 0 || admission_id == 0 || admission_id[0] == '\0' ||
        occupied_at == 0 || occupied_at[0] == '\0' || !Bed_is_assignable(bed)) {
        return 0;
    }

    bed->status = BED_STATUS_OCCUPIED;
    Bed_copy_text(bed->current_admission_id, sizeof(bed->current_admission_id), admission_id);
    Bed_copy_text(bed->occupied_at, sizeof(bed->occupied_at), occupied_at);
    bed->released_at[0] = '\0';
    return 1;
}

int Bed_release(Bed *bed, const char *released_at) {
    if (bed == 0 || released_at == 0 || released_at[0] == '\0' ||
        bed->status != BED_STATUS_OCCUPIED) {
        return 0;
    }

    bed->status = BED_STATUS_AVAILABLE;
    bed->current_admission_id[0] = '\0';
    bed->occupied_at[0] = '\0';
    Bed_copy_text(bed->released_at, sizeof(bed->released_at), released_at);
    return 1;
}

int Bed_mark_maintenance(Bed *bed) {
    if (bed == 0 || bed->status == BED_STATUS_OCCUPIED) {
        return 0;
    }

    bed->status = BED_STATUS_MAINTENANCE;
    bed->current_admission_id[0] = '\0';
    bed->occupied_at[0] = '\0';
    bed->released_at[0] = '\0';
    return 1;
}

int Bed_mark_available(Bed *bed) {
    if (bed == 0 || bed->status != BED_STATUS_MAINTENANCE) {
        return 0;
    }

    bed->status = BED_STATUS_AVAILABLE;
    return 1;
}
