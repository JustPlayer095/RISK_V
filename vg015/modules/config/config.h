#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

// Базовый адрес хранения конфига во внешней EEPROM
#define CONFIG_EEPROM_BASE 0x0000

// Пользовательский конфиг, который хотим хранить:
// - адрес OSDP
// - скорость UART/OSDP
// - произвольные настройки пинов 
typedef struct __attribute__((packed)) {
    uint8_t  osdp_addr;     // адрес OSDP
    uint32_t osdp_baud;     // скорость OSDP/UART
    uint8_t  pin_cfg[16];   // место под конфиг пинов 
} config_storage_t;

// Заполнить структуру значениями по умолчанию
void config_storage_default(config_storage_t *cfg);

// Прочитать из EEPROM; если чтение не удалось, вернуть false
bool config_storage_load(config_storage_t *cfg);

// Записать структуру в EEPROM
void config_storage_save(const config_storage_t *cfg);

#endif // CONFIG_STORAGE_H

