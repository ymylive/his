#ifndef HIS_REPOSITORY_INPATIENT_ORDER_REPOSITORY_H
#define HIS_REPOSITORY_INPATIENT_ORDER_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/InpatientOrder.h"
#include "repository/TextFileRepository.h"

#define INPATIENT_ORDER_REPOSITORY_HEADER \
    "order_id|admission_id|order_type|content|ordered_at|status|executed_at|cancelled_at"
#define INPATIENT_ORDER_REPOSITORY_FIELD_COUNT 8

typedef struct InpatientOrderRepository {
    TextFileRepository storage;
} InpatientOrderRepository;

Result InpatientOrderRepository_init(InpatientOrderRepository *repository, const char *path);
Result InpatientOrderRepository_append(
    const InpatientOrderRepository *repository,
    const InpatientOrder *order
);
Result InpatientOrderRepository_find_by_id(
    const InpatientOrderRepository *repository,
    const char *order_id,
    InpatientOrder *out_order
);
Result InpatientOrderRepository_load_all(
    const InpatientOrderRepository *repository,
    LinkedList *out_orders
);
Result InpatientOrderRepository_save_all(
    const InpatientOrderRepository *repository,
    const LinkedList *orders
);
void InpatientOrderRepository_clear_loaded_list(LinkedList *orders);

#endif
