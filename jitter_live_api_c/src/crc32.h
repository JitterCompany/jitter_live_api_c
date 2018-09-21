#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

uint32_t crc32b(uint8_t *message, size_t sizeof_message, uint32_t prev_crc);

#endif

