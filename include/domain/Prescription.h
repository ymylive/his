#ifndef HIS_DOMAIN_PRESCRIPTION_H
#define HIS_DOMAIN_PRESCRIPTION_H

#include "domain/DomainTypes.h"

typedef struct Prescription {
    char prescription_id[HIS_DOMAIN_ID_CAPACITY];
    char visit_id[HIS_DOMAIN_ID_CAPACITY];
    char medicine_id[HIS_DOMAIN_ID_CAPACITY];
    int quantity;
    char usage[HIS_DOMAIN_TEXT_CAPACITY];
} Prescription;

static inline int Prescription_has_valid_quantity(int quantity, int available_stock) {
    return quantity > 0 && available_stock >= 0 && quantity <= available_stock;
}

#endif
