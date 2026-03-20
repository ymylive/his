#ifndef HIS_DOMAIN_BED_H
#define HIS_DOMAIN_BED_H

#include "domain/DomainTypes.h"

typedef enum BedStatus {
    BED_STATUS_AVAILABLE = 0,
    BED_STATUS_OCCUPIED = 1,
    BED_STATUS_MAINTENANCE = 2
} BedStatus;

typedef struct Bed {
    char bed_id[HIS_DOMAIN_ID_CAPACITY];
    char ward_id[HIS_DOMAIN_ID_CAPACITY];
    char room_no[HIS_DOMAIN_TEXT_CAPACITY];
    char bed_no[HIS_DOMAIN_TEXT_CAPACITY];
    BedStatus status;
    char current_admission_id[HIS_DOMAIN_ID_CAPACITY];
    char occupied_at[HIS_DOMAIN_TIME_CAPACITY];
    char released_at[HIS_DOMAIN_TIME_CAPACITY];
} Bed;

int Bed_is_assignable(const Bed *bed);
int Bed_assign(Bed *bed, const char *admission_id, const char *occupied_at);
int Bed_release(Bed *bed, const char *released_at);
int Bed_mark_maintenance(Bed *bed);
int Bed_mark_available(Bed *bed);

#endif
