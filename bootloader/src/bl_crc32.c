#include "../include/bl_crc32.h"
#include "../../vg015/device/include/K1921VG015.h"

/* CRC-32 (poly 0x04C11DB7), non-reflected, init=0xFFFFFFFF, xorout=0xFFFFFFFF. */
static void bl_crc32_hw_prepare(void) {
    /* Enable CRC0 clock and release peripheral reset. */
    RCU->CGCFGAHB_bit.CRC0EN = 1u;
    RCU->RSTDISAHB_bit.CRC0EN = 1u;

    /* Configure the same CRC profile as the previous software implementation. */
    CRC0->POL = 0x04C11DB7u;
    CRC0->INIT = 0xFFFFFFFFu;
    CRC0->CR =
        ((uint32_t)CRC_CR_MODE_StandartCRC << CRC_CR_MODE_Pos) |
        ((uint32_t)CRC_CR_XOROUT_Enable << CRC_CR_XOROUT_Pos) |
        ((uint32_t)CRC_CR_POLYSIZE_POL32 << CRC_CR_POLYSIZE_Pos) |
        ((uint32_t)CRC_CR_REV_IN_Disable << CRC_CR_REV_IN_Pos) |
        ((uint32_t)CRC_CR_REV_OUT_Disable << CRC_CR_REV_OUT_Pos);

    /* Reload INIT value into CRC calculation path. */
    CRC0->CR |= CRC_CR_RESET_Msk;
}

uint32_t bl_crc32_calc(const uint8_t* data, uint32_t len) {
    uint32_t i;

    if (data == 0) {
        return 0u;
    }

    bl_crc32_hw_prepare();

    for (i = 0u; i < len; ++i) {
        CRC0->DR8 = data[i];
    }

    return CRC0->POST;
}
