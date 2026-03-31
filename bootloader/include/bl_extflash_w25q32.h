#ifndef BL_EXTFLASH_W25Q32_H
#define BL_EXTFLASH_W25Q32_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Slot с готовым образом приложения в внешней W25Q32.
#define BL_EXTFLASH_FW_SLOT_BASE 0x00010000u
#define BL_EXTFLASH_FW_SLOT_SIZE 0x00300000u

// Инициализация SPI0 + CS=PB1.
void bl_extflash_init_spi0_cs_pb1(void);

// Чтение байтов из внешней flash по 24-битному адресу.
bool bl_extflash_read(uint32_t addr, uint8_t *dst, size_t len);

#endif /* BL_EXTFLASH_W25Q32_H */

