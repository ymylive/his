#ifndef HIS_REPOSITORY_MEDICINE_REPOSITORY_H
#define HIS_REPOSITORY_MEDICINE_REPOSITORY_H

#include "common/LinkedList.h"
#include "common/Result.h"
#include "domain/Medicine.h"
#include "repository/TextFileRepository.h"

typedef struct MedicineRepository {
    TextFileRepository storage;
} MedicineRepository;

Result MedicineRepository_init(MedicineRepository *repository, const char *path);
Result MedicineRepository_load_all(
    MedicineRepository *repository,
    LinkedList *out_medicines
);
Result MedicineRepository_find_by_medicine_id(
    MedicineRepository *repository,
    const char *medicine_id,
    Medicine *out_medicine
);
Result MedicineRepository_save(
    MedicineRepository *repository,
    const Medicine *medicine
);
Result MedicineRepository_save_all(
    MedicineRepository *repository,
    const LinkedList *medicines
);
void MedicineRepository_clear_list(LinkedList *medicines);

#endif
