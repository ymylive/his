#ifndef HIS_REPOSITORY_USER_REPOSITORY_H
#define HIS_REPOSITORY_USER_REPOSITORY_H

#include "common/Result.h"
#include "domain/User.h"
#include "repository/TextFileRepository.h"

#define USER_REPOSITORY_HEADER "user_id|password_hash|role"
#define USER_REPOSITORY_FIELD_COUNT 3

typedef struct UserRepository {
    TextFileRepository file_repository;
} UserRepository;

Result UserRepository_init(UserRepository *repository, const char *path);
Result UserRepository_append(const UserRepository *repository, const User *user);
Result UserRepository_find_by_user_id(
    const UserRepository *repository,
    const char *user_id,
    User *out_user
);

#endif
