#ifndef HIS_DOMAIN_WARD_H
#define HIS_DOMAIN_WARD_H

#include "domain/DomainTypes.h"

typedef enum WardStatus {
    WARD_STATUS_ACTIVE = 0,
    WARD_STATUS_CLOSED = 1
} WardStatus;

typedef struct Ward {
    char ward_id[HIS_DOMAIN_ID_CAPACITY];
    char name[HIS_DOMAIN_NAME_CAPACITY];
    char department_id[HIS_DOMAIN_ID_CAPACITY];
    char location[HIS_DOMAIN_LOCATION_CAPACITY];
    int capacity;
    int occupied_beds;
    WardStatus status;
} Ward;

int Ward_has_capacity(const Ward *ward);
int Ward_occupy_bed(Ward *ward);
int Ward_release_bed(Ward *ward);
int Ward_close(Ward *ward);
int Ward_open(Ward *ward);

#endif
