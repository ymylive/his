#include "common/IdGenerator.h"

#include <stdio.h>

int IdGenerator_format(
    char *buffer,
    size_t capacity,
    const char *prefix,
    int sequence,
    int width
) {
    int written = 0;

    if (buffer == 0 || capacity == 0 || prefix == 0 || sequence < 0 || width < 0) {
        return 0;
    }

    written = snprintf(buffer, capacity, "%s%0*d", prefix, width, sequence);
    if (written < 0) {
        buffer[0] = '\0';
        return 0;
    }

    if ((size_t)written >= capacity) {
        buffer[0] = '\0';
        return 0;
    }

    return 1;
}
