#include "config.h"

#include <string.h>
#include <stdio.h>

#include "../driver/eeprom.h"
// Глобальный счётчик миллисекунд из main.c
extern volatile uint32_t ms_ticks;
#include "../../device/Include/system_k1921vg015.h"

void config_storage_default(config_storage_t *cfg) //загружаем дефолтный конфиг
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->osdp_addr = 0x01; 
    cfg->osdp_baud = 115200;  
}

bool config_storage_load(config_storage_t *cfg) //читаем конфиг с самого eeprom и проверяем его правильность
{
    if (!cfg) return false;

    config_storage_t tmp;
    // Инициализируем буфер нулями на случай ошибки чтения
    memset(&tmp, 0, sizeof(tmp));
    
    // Первая попытка чтения (может не пройти сразу после инициализации)
    eeprom_read_bytes(CONFIG_EEPROM_BASE, (uint8_t *)&tmp, sizeof(tmp));

    // Ждём завершения асинхронной операции с таймаутом по ms_ticks
    uint32_t start_ms = ms_ticks;
    while (eeprom_is_busy()) {
        if ((ms_ticks - start_ms) > 50u) { // 50 мс таймаут на чтение
            return false;
        }
    }
    if (eeprom_had_error()) {
        return false;
    }
    
    // Если первое чтение вернуло только нули, пробуем ещё раз через небольшую паузу
    bool first_read_all_zero = true;
    for (size_t i = 0; i < sizeof(tmp); i++) {
        if (((uint8_t*)&tmp)[i] != 0) {
            first_read_all_zero = false;
            break;
        }
    }
    
    if (first_read_all_zero) {
        // Задержка для стабилизации шины перед повторной попыткой (5 мс достаточно)
        uint32_t cpu_freq = SystemCoreClock;
        if (cpu_freq == 0) cpu_freq = 16000000;
        uint32_t delay_5ms = (5 * cpu_freq) / 1000; // 5 мс
        for (volatile uint32_t i = 0; i < delay_5ms; ++i) {
            __asm volatile("nop");
        }
        
        // Повторная попытка чтения
        memset(&tmp, 0, sizeof(tmp));
        eeprom_read_bytes(CONFIG_EEPROM_BASE, (uint8_t *)&tmp, sizeof(tmp));

        start_ms = ms_ticks;
        while (eeprom_is_busy()) {
            if ((ms_ticks - start_ms) > 50u) {
                return false;
            }
        }
        if (eeprom_had_error()) {
            return false;
        }
    }

    // Никакой дополнительной валидации (magic/CRC) больше нет — просто копируем
    memcpy(cfg, &tmp, sizeof(tmp));

    return true;
}

void config_storage_save(const config_storage_t *cfg_in)  //загружаем изменённый конфиг
{
    if (!cfg_in) return;

    config_storage_t tmp;
    memcpy(&tmp, cfg_in, sizeof(tmp));

    eeprom_write_bytes(CONFIG_EEPROM_BASE, (const uint8_t *)&tmp, sizeof(tmp));

    // Ожидание завершения записи с таймаутом
    uint32_t start_ms = ms_ticks;
    while (eeprom_is_busy()) {
        if ((ms_ticks - start_ms) > 50u) { // 50 мс на запись конфигурации
            return;
        }
    }
    if (eeprom_had_error()) {
        return;
    }
}

