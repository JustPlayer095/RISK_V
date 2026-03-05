#include "../include/bl_image.h"
#include "../include/bl_jump.h"
#include "../include/bl_config.h"
#include "../include/bl_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* Внутренний маркер: заголовок не получен, продолжаем ждать в цикле обновления. */
#define BL_INTERNAL_NO_HEADER          (-1)

/* Отправляет хосту один байт статуса протокола. */
static void bl_proto_send(uint8_t code) {
    bl_hal_uart_putc(code);
}

/* Получает заголовок приложения фиксированного размера с таймаутом. */
static bool bl_proto_receive_header(uint32_t timeout_ms, bl_app_header_t* out_hdr) {
    return bl_hal_uart_get((uint8_t*)out_hdr, (uint32_t)sizeof(*out_hdr), timeout_ms);
}

/* Стирает flash и записывает payload и заголовок, полученные от хоста. */
static int bl_flash_program_image(const bl_app_header_t* hdr) {
    uint8_t buf[256];
    uint32_t left;
    uint32_t wr_addr;
    uint32_t chunk;
    uint32_t image_total;

    image_total = hdr->image_size + (uint32_t)sizeof(bl_app_header_t);
    if (!bl_hal_flash_erase_range(APP_HEADER_ADDR, image_total)) {
        return (int)BL_PROTO_ERR_WAIT_ERASE_PAGE;
    }
    /* Сообщаем хосту готовность только после стирания flash. */
    bl_proto_send(BL_PROTO_REPLY_ACK);

    left = hdr->image_size;
    wr_addr = APP_PAYLOAD_ADDR;
    while (left > 0u) {
        chunk = (left > (uint32_t)sizeof(buf)) ? (uint32_t)sizeof(buf) : left;
        if (!bl_hal_uart_get(buf, chunk, BL_UPDATE_WAIT_TIMEOUT_MS)) {
            return (int)BL_PROTO_ERR_RECEIVE;
        }
        if (!bl_hal_flash_write(wr_addr, buf, chunk)) {
            return (int)BL_PROTO_ERR_WAIT_WRITE_PAGE;
        }
        wr_addr += chunk;
        left -= chunk;
        bl_proto_send(BL_PROTO_REPLY_ACK);
    }

    if (!bl_hal_flash_write(APP_HEADER_ADDR, (const uint8_t*)hdr, (uint32_t)sizeof(*hdr))) {
        return (int)BL_PROTO_ERR_WAIT_WRITE_PAGE;
    }

    return 0;
}

/* Возвращает ненулевое значение, когда нажата кнопка обновления. */
static int bl_update_requested(void) {
    return bl_hal_is_update_button_pressed() ? 1 : 0;
}

/* Принимает одну транзакцию образа и проверяет записанные данные. */
static int bl_receive_and_program(void) {
    bl_app_header_t hdr;
    int rc;

    if (!bl_proto_receive_header(BL_UPDATE_WAIT_TIMEOUT_MS, &hdr)) {
        return BL_INTERNAL_NO_HEADER;
    }

    if (!bl_image_header_is_valid(&hdr)) {
        return (int)BL_PROTO_ERR_TOO_BIG;
    }

    rc = bl_flash_program_image(&hdr);
    if (rc != 0) {
        return rc;
    }

    if (!bl_image_is_valid() || bl_image_get_size() != hdr.image_size) {
        return (int)BL_PROTO_ERR_CRC32;
    }

    return 0;
}

/* Выполняет цикл протокола обновления до успешной записи и перехода в приложение. */
static void bl_enter_update_mode(void) {
    int rc;
    uint32_t i;

    bl_hal_set_update_mode_leds(true);
    while (1) {
        bl_proto_send(BL_PROTO_REPLY_WAITING);
        rc = bl_receive_and_program();
        if (rc == BL_INTERNAL_NO_HEADER) {
            continue;
        }
        if (rc == 0) {
            /* Повторяем финальный ACK, чтобы хост успел его прочитать до перехода. */
            for (i = 0u; i < 4u; ++i) {
                bl_proto_send(BL_PROTO_REPLY_ACK);
                bl_hal_uart_wait_tx_idle();
            }
            for (i = 0u; i < 200000u; ++i) {
                __asm__ volatile ("nop");
            }
            bl_hal_set_update_mode_leds(false);
            bl_jump_to_app(APP_ENTRY_ADDR);
        }
        bl_proto_send((uint8_t)rc);
    }
}

/* Точка входа загрузчика: выбор между режимом обновления и запуском приложения. */
int main(void) {
    bl_hal_init();

    if (bl_update_requested()) {
        bl_enter_update_mode();
    }

    if (bl_image_is_valid()) {
        bl_hal_set_update_mode_leds(false);
        bl_jump_to_app(APP_ENTRY_ADDR);
    }

    bl_enter_update_mode();
    return 0; /* недостижимо */
}
