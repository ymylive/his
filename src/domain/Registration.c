#include "domain/Registration.h"

#include <string.h>

static void Registration_copy_time(char *destination, const char *value) {
    if (destination == 0) {
        return;
    }

    if (value == 0) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, value, HIS_DOMAIN_TIME_CAPACITY - 1);
    destination[HIS_DOMAIN_TIME_CAPACITY - 1] = '\0';
}

int Registration_mark_diagnosed(Registration *registration, const char *diagnosed_at) {
    if (registration == 0 || diagnosed_at == 0 || registration->status != REG_STATUS_PENDING) {
        return 0;
    }

    registration->status = REG_STATUS_DIAGNOSED;
    Registration_copy_time(registration->diagnosed_at, diagnosed_at);
    return 1;
}

int Registration_cancel(Registration *registration, const char *cancelled_at) {
    if (registration == 0 || cancelled_at == 0 || registration->status != REG_STATUS_PENDING) {
        return 0;
    }

    registration->status = REG_STATUS_CANCELLED;
    Registration_copy_time(registration->cancelled_at, cancelled_at);
    return 1;
}
