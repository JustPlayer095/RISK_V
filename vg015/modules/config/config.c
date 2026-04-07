#include "config.h"

#include <string.h>

#if (CONFIG_STORAGE == CONFIG_STORAGE_EEPROM)
#include "../driver/eeprom/eeprom.h"
#include "../timebase/timebase.h"
#elif (CONFIG_STORAGE == CONFIG_STORAGE_W25Q32)
#include "../driver/w25q32/extflash_w25q32.h"
#else
#error "Unsupported CONFIG_STORAGE value"
#endif

typedef struct __attribute__((packed)) {
    uint32_t seq;
    config_storage_t cfg;
} config_flash_record_t;

static const uint8_t default_osdp_cap[16u * 3u] = {
    0x01, 0x01, 0x04,
    0x02, 0x01, 0x04,
    0x03, 0x00, 0x00,
    0x04, 0x01, 0x01,
    0x05, 0x00, 0x00,
    0x06, 0x00, 0x00,
    0x07, 0x00, 0x00,
    0x08, 0x01, 0x00,
    0x09, 0x00, 0x00,
    0x0A, 0x00, 0x01,
    0x0B, 0x00, 0x01,
    0x0C, 0x00, 0x00,
    0x0D, 0x00, 0x00,
    0x0E, 0x00, 0x00,
    0x0F, 0x00, 0x00,
    0x10, 0x01, 0x00
};

static const uint8_t default_osdp_pdid[12u] = {
    'P', 'R', 'S',
    1u,
    1u,
    0x01u, 0x00u, 0x00u, 0x00u,
    1u, 1u, 1u
};

static bool config_is_valid_pdid(const uint8_t pdid[12u])
{
    if (!pdid) {
        return false;
    }

    // Vendor code должен быть печатным ASCII.
    for (uint32_t i = 0u; i < 3u; ++i) {
        if (pdid[i] < 0x20u || pdid[i] > 0x7Eu) {
            return false;
        }
    }

    return true;
}

static bool config_is_valid_cap(const uint8_t cap[16u * 3u])
{
    if (!cap) {
        return false;
    }

    // Ожидаем стандартный формат 16 triplets c function-code 0x01..0x10.
    for (uint32_t n = 0u; n < 16u; ++n) {
        if (cap[n * 3u] != (uint8_t)(n + 1u)) {
            return false;
        }
    }

    return true;
}

static void config_apply_defaults_if_needed(config_storage_t *cfg)
{
    if (!cfg) {
        return;
    }

    if (cfg->osdp_addr == 0u || cfg->osdp_addr == 0xFFu) {
        cfg->osdp_addr = 0x01u;
    }
    if (cfg->osdp_baud < 9600u || cfg->osdp_baud > 115200u) {
        cfg->osdp_baud = 115200u;
    }

    if (!config_is_valid_pdid(cfg->osdp_pdid)) {
        memcpy(cfg->osdp_pdid, default_osdp_pdid, sizeof(cfg->osdp_pdid));
    }

    if (!config_is_valid_cap(cfg->osdp_cap)) {
        memcpy(cfg->osdp_cap, default_osdp_cap, sizeof(cfg->osdp_cap));
    }

}

static bool config_read_record(config_flash_record_t *rec)
{
    if (!rec) {
        return false;
    }

#if (CONFIG_STORAGE == CONFIG_STORAGE_EEPROM)
    eeprom_read_bytes(CONFIG_EEPROM_BASE, (uint8_t *)rec, sizeof(*rec));

    {
        uint32_t start_ms = ms_ticks;
        while (eeprom_is_busy()) {
            if ((ms_ticks - start_ms) > 50u) {
                return false;
            }
        }
    }

    if (eeprom_had_error()) {
        return false;
    }
#else
    extflash_init_spi0_cs_pb1();
    if (!extflash_read(CONFIG_EXTFLASH_BASE, (uint8_t *)rec, sizeof(*rec))) {
        return false;
    }
#endif

    // Пустая память после erase обычно заполнена 0xFF.
    if (rec->seq == 0xFFFFFFFFu) {
        return false;
    }

    return true;
}

void config_storage_default(config_storage_t *cfg) //загружаем дефолтный конфиг
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->osdp_addr = 0x01; 
    cfg->osdp_baud = 115200;  
    memcpy(cfg->osdp_pdid, default_osdp_pdid, sizeof(cfg->osdp_pdid));
    memcpy(cfg->osdp_cap, default_osdp_cap, sizeof(cfg->osdp_cap));
}

bool config_storage_load(config_storage_t *cfg) //читаем конфиг и проверяем его правильность
{
    if (!cfg) return false;

    config_flash_record_t rec;
    config_storage_t before_normalize;
    memset(&rec, 0, sizeof(rec));
    if (!config_read_record(&rec)) {
        return false;
    }

    memcpy(cfg, &rec.cfg, sizeof(*cfg));
    before_normalize = *cfg;
    config_apply_defaults_if_needed(cfg);

    // Миграция старой записи: если после нормализации данные изменились, сохраняем обратно.
    if (memcmp(&before_normalize, cfg, sizeof(*cfg)) != 0) {
        config_storage_save(cfg);
    }

    return true;
}

void config_storage_save(const config_storage_t *cfg_in)  //загружаем изменённый конфиг
{
    if (!cfg_in) return;

    config_flash_record_t old_rec;
    config_flash_record_t new_rec;

    memset(&new_rec, 0xFF, sizeof(new_rec));
    memcpy(&new_rec.cfg, cfg_in, sizeof(new_rec.cfg));

    if (config_read_record(&old_rec)) {
        new_rec.seq = old_rec.seq + 1u;
    } else {
        new_rec.seq = 1u;
    }

#if (CONFIG_STORAGE == CONFIG_STORAGE_EEPROM)
    eeprom_write_bytes(CONFIG_EEPROM_BASE, (const uint8_t *)&new_rec, sizeof(new_rec));

    {
        uint32_t start_ms = ms_ticks;
        while (eeprom_is_busy()) {
            if ((ms_ticks - start_ms) > 50u) {
                return;
            }
        }
    }

    if (eeprom_had_error()) {
        return;
    }
#else
    extflash_init_spi0_cs_pb1();
    if (!extflash_erase_range_4k(CONFIG_EXTFLASH_BASE, CONFIG_EXTFLASH_AREA_SIZE)) {
        return;
    }
    if (!extflash_write(CONFIG_EXTFLASH_BASE, (const uint8_t *)&new_rec, sizeof(new_rec))) {
        return;
    }
#endif
}

void config_storage_get_seq(uint32_t *seq){
    if (!seq) return;
    
}