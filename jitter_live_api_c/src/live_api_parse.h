#ifndef LIVE_API_PARSE_H
#define LIVE_API_PARSE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    const char *username;
    const char *password;
    const char *random;
} RegisterMessage;

bool live_api_parse_register(RegisterMessage *result,
        uint8_t *buffer, size_t sizeof_buffer);

#endif

