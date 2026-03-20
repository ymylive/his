#ifndef HIS_COMMON_RESULT_H
#define HIS_COMMON_RESULT_H

#define RESULT_MESSAGE_CAPACITY 128

typedef struct Result {
    int success;
    char message[RESULT_MESSAGE_CAPACITY];
} Result;

Result Result_make_success(const char *message);
Result Result_make_failure(const char *message);

#endif
