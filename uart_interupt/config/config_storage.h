#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

// Базовый адрес хранения конфига во внешней SPI EEPROM
#define CONFIG_EEPROM_BASE 0x0000

// Магическое число для валидации данных в EEPROM ("CFG1")
#define CONFIG_STORAGE_MAGIC 0x43464731u

// Пользовательский конфиг, который хотим хранить:
// - адрес OSDP
// - скорость UART/OSDP
// - произвольные настройки пинов (16 байт под маски/режимы по вашему усмотрению)
typedef struct __attribute__((packed)) {
    uint32_t magic;         // маркер валидности
    uint8_t  osdp_addr;     // адрес OSDP
    uint32_t osdp_baud;     // скорость OSDP/UART
    uint8_t  pin_cfg[16];   // место под конфиг пинов (заполняйте как нужно)
    uint16_t crc;           // простая контрольная сумма
} config_storage_t;

// Заполнить структуру значениями по умолчанию
void config_storage_default(config_storage_t *cfg);

// Прочитать из EEPROM; если невалидно (magic/CRC), вернуть false
bool config_storage_load(config_storage_t *cfg);

// Записать структуру в EEPROM (пересчитает CRC/магик)
void config_storage_save(const config_storage_t *cfg);

#endif // CONFIG_STORAGE_H

