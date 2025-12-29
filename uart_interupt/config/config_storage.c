#include "config_storage.h"

#include <string.h>

#include "../driver/eeprom_spi.h"
#include "../osdp/ccitt_crc16.h"

static uint16_t config_storage_calc_crc(const config_storage_t *cfg)
{
    // CRC считается по всему, кроме поля crc
    return ccitt_crc16_calc(OSDP_INIT_CRC16, (const uint8_t *)cfg, (uint16_t)(sizeof(*cfg) - sizeof(cfg->crc)));
}


void config_storage_default(config_storage_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->osdp_addr = 0x01;     // пример: брать из флеша/заводской, можно заменить
    cfg->osdp_baud = 115200;   // дефолтная скорость
}

bool config_storage_load(config_storage_t *cfg)
{
    if (!cfg) return false;

    config_storage_t tmp;
    eeprom_spi_read_bytes(CONFIG_EEPROM_BASE, (uint8_t *)&tmp, sizeof(tmp));

    if (tmp.magic != CONFIG_STORAGE_MAGIC) {
        return false;
    }
    uint16_t crc = config_storage_calc_crc(&tmp);
    if (crc != tmp.crc) {
        return false;
    }

    memcpy(cfg, &tmp, sizeof(tmp));

    return true;
}

void config_storage_save(const config_storage_t *cfg_in)
{
    if (!cfg_in) return;

    config_storage_t tmp;
    memcpy(&tmp, cfg_in, sizeof(tmp));
    tmp.magic = CONFIG_STORAGE_MAGIC;
    tmp.crc = config_storage_calc_crc(&tmp);

    eeprom_spi_write_bytes(CONFIG_EEPROM_BASE, (const uint8_t *)&tmp, sizeof(tmp));
}

