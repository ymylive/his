#ifndef HIS_COMMON_ID_GENERATOR_H
#define HIS_COMMON_ID_GENERATOR_H

#include <stddef.h>

int IdGenerator_format(
    char *buffer,
    size_t capacity,
    const char *prefix,
    int sequence,
    int width
);

#endif
