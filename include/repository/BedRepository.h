#ifndef HIS_REPOSITORY_BED_REPOSITORY_H
#define HIS_REPOSITORY_BED_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Bed.h"
#include "repository/TextFileRepository.h"

#define BED_REPOSITORY_HEADER \
    "bed_id|ward_id|room_no|bed_no|status|current_admission_id|occupied_at|released_at"
#define BED_REPOSITORY_FIELD_COUNT 8

typedef struct BedRepository {
    TextFileRepository storage;
} BedRepository;

Result BedRepository_init(BedRepository *repository, const char *path);
Result BedRepository_append(const BedRepository *repository, const Bed *bed);
Result BedRepository_find_by_id(
    const BedRepository *repository,
    const char *bed_id,
    Bed *out_bed
);
Result BedRepository_load_all(
    const BedRepository *repository,
    LinkedList *out_beds
);
Result BedRepository_save_all(
    const BedRepository *repository,
    const LinkedList *beds
);
void BedRepository_clear_loaded_list(LinkedList *beds);

#endif
