#ifndef HIS_DOMAIN_INPATIENT_ORDER_H
#define HIS_DOMAIN_INPATIENT_ORDER_H

#include "domain/DomainTypes.h"

typedef enum InpatientOrderStatus {
    INPATIENT_ORDER_STATUS_PENDING = 0,
    INPATIENT_ORDER_STATUS_EXECUTED = 1,
    INPATIENT_ORDER_STATUS_CANCELLED = 2
} InpatientOrderStatus;

typedef struct InpatientOrder {
    char order_id[HIS_DOMAIN_ID_CAPACITY];
    char admission_id[HIS_DOMAIN_ID_CAPACITY];
    char order_type[HIS_DOMAIN_NAME_CAPACITY];
    char content[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    char ordered_at[HIS_DOMAIN_TIME_CAPACITY];
    InpatientOrderStatus status;
    char executed_at[HIS_DOMAIN_TIME_CAPACITY];
    char cancelled_at[HIS_DOMAIN_TIME_CAPACITY];
} InpatientOrder;

int InpatientOrder_mark_executed(InpatientOrder *order, const char *executed_at);
int InpatientOrder_cancel(InpatientOrder *order, const char *cancelled_at);

#endif
