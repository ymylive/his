#ifndef HIS_DOMAIN_DEPARTMENT_H
#define HIS_DOMAIN_DEPARTMENT_H

#include "domain/DomainTypes.h"

typedef struct Department {
    char department_id[HIS_DOMAIN_ID_CAPACITY];
    char name[HIS_DOMAIN_NAME_CAPACITY];
    char location[HIS_DOMAIN_LOCATION_CAPACITY];
    char description[HIS_DOMAIN_TEXT_CAPACITY];
} Department;

#endif
