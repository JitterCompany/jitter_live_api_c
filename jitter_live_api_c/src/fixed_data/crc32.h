#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

uint32_t crc32b(const uint8_t *message, const size_t sizeof_message,
        const uint32_t prev_crc);

#endif

