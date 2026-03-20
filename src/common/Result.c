#include "common/Result.h"

#include <string.h>

static void Result_copy_message(char *destination, const char *message) {
    if (message == 0) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, message, RESULT_MESSAGE_CAPACITY - 1);
    destination[RESULT_MESSAGE_CAPACITY - 1] = '\0';
}

Result Result_make_success(const char *message) {
    Result result;

    result.success = 1;
    Result_copy_message(result.message, message);
    return result;
}

Result Result_make_failure(const char *message) {
    Result result;

    result.success = 0;
    Result_copy_message(result.message, message);
    return result;
}
