#ifndef HIS_DOMAIN_MEDICINE_H
#define HIS_DOMAIN_MEDICINE_H

#include "domain/DomainTypes.h"

typedef struct Medicine {
    char medicine_id[HIS_DOMAIN_ID_CAPACITY];
    char name[HIS_DOMAIN_NAME_CAPACITY];
    double price;
    int stock;
    char department_id[HIS_DOMAIN_ID_CAPACITY];
    int low_stock_threshold;
} Medicine;

static inline int Medicine_has_valid_inventory(const Medicine *medicine) {
    return medicine != 0 &&
        medicine->price >= 0.0 &&
        medicine->stock >= 0 &&
        medicine->low_stock_threshold >= 0;
}

#endif
