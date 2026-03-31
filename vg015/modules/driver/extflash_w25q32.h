#ifndef EXTFLASH_W25Q32_H
#define EXTFLASH_W25Q32_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Базовый адрес слота с будущей прошивкой в W25Q32.
// Должно совпадать с разметкой bootloader'а.
#define EXTFLASH_FW_SLOT_BASE   0x00010000u
#define EXTFLASH_FW_SLOT_SIZE   0x00300000u

// Инициализация SPI0 + CS = PB1.
void extflash_init_spi0_cs_pb1(void);

bool extflash_read(uint32_t addr, uint8_t *dst, size_t len);

// Пишет len байт по адресу addr (выполняет page program 256B по частям).
// Требует, чтобы соответствующая область была заранее стерта.
bool extflash_write(uint32_t addr, const uint8_t *src, size_t len);

// Стирание 4КБ секторами (W25Q32SECTOR_SIZE = 4096).
bool extflash_erase_range_4k(uint32_t addr, uint32_t len);

#endif /* EXTFLASH_W25Q32_H */

