
#include "crc32.h"

/* Basic CRC-32 calculation with some optimization but no table lookup.
 * The the byte reversal is avoided by shifting the crc reg right instead of
 * left and by using a reversed 32-bit word to represent the polynomial.
*/
uint32_t crc32b(const uint8_t *message, const size_t sizeof_message,
        const uint32_t prev_crc)
{

   //crc = 0xFFFFFFFF;
   uint32_t crc = ~prev_crc;

    for(size_t i=0;i<sizeof_message;i++) {
      const uint32_t byte = message[i];            // Get next byte.
      crc = crc ^ byte;
      for (int j = 7; j >= 0; j--) {    // Do eight times.
         const uint32_t mask = -(crc & 1);
         crc = (crc >> 1) ^ (0xEDB88320 & mask);
      }
   }
   return ~crc;
}
