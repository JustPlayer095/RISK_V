#include "bl_crc32.h"

/* CRC-32 (poly 0x04C11DB7), non-reflected, init=0xFFFFFFFF, xorout=0xFFFFFFFF. */
uint32_t bl_crc32_calc(const uint8_t* data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i;
    uint32_t bit;

    if (data == 0) {
        return 0u;
    }

    for (i = 0u; i < len; ++i) {
        crc ^= ((uint32_t)data[i] << 24);
        for (bit = 0u; bit < 8u; ++bit) {
            if ((crc & 0x80000000u) != 0u) {
                crc = (crc << 1) ^ 0x04C11DB7u;
            } else {
                crc <<= 1;
            }
        }
    }

    return (crc ^ 0xFFFFFFFFu);
}
