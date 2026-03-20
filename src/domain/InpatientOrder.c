#include "domain/InpatientOrder.h"

#include <string.h>

static void InpatientOrder_copy_time(char *destination, const char *source) {
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

int InpatientOrder_mark_executed(InpatientOrder *order, const char *executed_at) {
    if (order == 0 || executed_at == 0 || executed_at[0] == '\0' ||
        order->status != INPATIENT_ORDER_STATUS_PENDING) {
        return 0;
    }

    order->status = INPATIENT_ORDER_STATUS_EXECUTED;
    InpatientOrder_copy_time(order->executed_at, executed_at);
    order->cancelled_at[0] = '\0';
    return 1;
}

int InpatientOrder_cancel(InpatientOrder *order, const char *cancelled_at) {
    if (order == 0 || cancelled_at == 0 || cancelled_at[0] == '\0' ||
        order->status != INPATIENT_ORDER_STATUS_PENDING) {
        return 0;
    }

    order->status = INPATIENT_ORDER_STATUS_CANCELLED;
    order->executed_at[0] = '\0';
    InpatientOrder_copy_time(order->cancelled_at, cancelled_at);
    return 1;
}
