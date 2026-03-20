#ifndef HIS_DOMAIN_PATIENT_H
#define HIS_DOMAIN_PATIENT_H

#include "domain/DomainTypes.h"

typedef enum PatientGender {
    PATIENT_GENDER_UNKNOWN = 0,
    PATIENT_GENDER_MALE = 1,
    PATIENT_GENDER_FEMALE = 2
} PatientGender;

typedef struct Patient {
    char patient_id[HIS_DOMAIN_ID_CAPACITY];
    char name[HIS_DOMAIN_NAME_CAPACITY];
    PatientGender gender;
    int age;
    char contact[HIS_DOMAIN_CONTACT_CAPACITY];
    char id_card[HIS_DOMAIN_CONTACT_CAPACITY];
    char allergy[HIS_DOMAIN_TEXT_CAPACITY];
    char medical_history[HIS_DOMAIN_LARGE_TEXT_CAPACITY];
    int is_inpatient;
    char remarks[HIS_DOMAIN_TEXT_CAPACITY];
} Patient;

static inline int Patient_is_valid_age(int age) {
    return age >= 0 && age <= 150;
}

#endif
