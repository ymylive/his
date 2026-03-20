#ifndef HIS_REPOSITORY_WARD_REPOSITORY_H
#define HIS_REPOSITORY_WARD_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Ward.h"
#include "repository/TextFileRepository.h"

#define WARD_REPOSITORY_HEADER \
    "ward_id|name|department_id|location|capacity|occupied_beds|status"
#define WARD_REPOSITORY_FIELD_COUNT 7

typedef struct WardRepository {
    TextFileRepository storage;
} WardRepository;

Result WardRepository_init(WardRepository *repository, const char *path);
Result WardRepository_append(const WardRepository *repository, const Ward *ward);
Result WardRepository_find_by_id(
    const WardRepository *repository,
    const char *ward_id,
    Ward *out_ward
);
Result WardRepository_load_all(
    const WardRepository *repository,
    LinkedList *out_wards
);
Result WardRepository_save_all(
    const WardRepository *repository,
    const LinkedList *wards
);
void WardRepository_clear_loaded_list(LinkedList *wards);

#endif
