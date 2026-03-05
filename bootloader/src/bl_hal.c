#include "../include/bl_hal.h"

#include "../../vg015/device/include/K1921VG015.h"

#define BL_UART_BAUDRATE               ((uint32_t)115200u)
#ifdef HSECLK_VAL
#define BL_UART_CLOCK_HZ               ((uint32_t)HSECLK_VAL)
#else
#define BL_UART_CLOCK_HZ               ((uint32_t)16000000u)
#endif
#define BL_FLASH_WAIT_ERASE_LOOPS      ((uint32_t)2000000u)
#define BL_FLASH_WAIT_WRITE_LOOPS      ((uint32_t)200000u)
#define BL_FLASH_ERASE_SIZE_BYTES      ((uint32_t)4096u)
#define BL_UART_TIMEOUT_LOOPS_PER_MS   ((uint32_t)2000u)
#define BL_UPDATE_BTN_PORT             (GPIOC)
#define BL_UPDATE_BTN_MASK             ((uint32_t)1u << 0)
#define BL_UPDATE_LED_PORT             (GPIOA)
#define BL_UPDATE_LED_MASK             (((uint32_t)1u << 12) | ((uint32_t)1u << 13) | ((uint32_t)1u << 14) | ((uint32_t)1u << 15))

/* Ожидает снятия флага busy у flash до готовности или исчерпания таймаута. */
static bool bl_flash_wait_ready(uint32_t loops) {
    while (loops > 0u) {
        if ((FLASH->STAT & FLASH_STAT_BUSY_Msk) == 0u) {
            return true;
        }
        --loops;
    }
    return false;
}

/* Преобразует абсолютный flash-адрес в смещение для контроллера flash. */
static uint32_t bl_flash_offs(uint32_t abs_addr) {
    return (abs_addr - MEM_FLASH_BASE);
}

/* Стирает одну страницу flash, содержащую указанный абсолютный адрес. */
static bool bl_flash_erase_page(uint32_t abs_addr) {
    FLASH->ADDR = bl_flash_offs(abs_addr);
    FLASH->CMD = ((uint32_t)FLASH_CMD_KEY_Access << FLASH_CMD_KEY_Pos) | FLASH_CMD_ERSEC_Msk;
    return bl_flash_wait_ready(BL_FLASH_WAIT_ERASE_LOOPS);
}

/* Программирует один 16-байтный блок flash через регистры данных контроллера. */
static bool bl_flash_write16(uint32_t abs_addr, const uint8_t* data, uint32_t len) {
    uint8_t tmp[16];
    uint32_t word0;
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;
    uint32_t i;

    for (i = 0u; i < 16u; ++i) {
        tmp[i] = 0xFFu;
    }
    for (i = 0u; i < len; ++i) {
        tmp[i] = data[i];
    }

    word0 = ((uint32_t)tmp[0]) | ((uint32_t)tmp[1] << 8) | ((uint32_t)tmp[2] << 16) | ((uint32_t)tmp[3] << 24);
    word1 = ((uint32_t)tmp[4]) | ((uint32_t)tmp[5] << 8) | ((uint32_t)tmp[6] << 16) | ((uint32_t)tmp[7] << 24);
    word2 = ((uint32_t)tmp[8]) | ((uint32_t)tmp[9] << 8) | ((uint32_t)tmp[10] << 16) | ((uint32_t)tmp[11] << 24);
    word3 = ((uint32_t)tmp[12]) | ((uint32_t)tmp[13] << 8) | ((uint32_t)tmp[14] << 16) | ((uint32_t)tmp[15] << 24);

    FLASH->DATA[0].DATA = word0;
    FLASH->DATA[1].DATA = word1;
    FLASH->DATA[2].DATA = word2;
    FLASH->DATA[3].DATA = word3;
    FLASH->ADDR = bl_flash_offs(abs_addr);
    FLASH->CMD = ((uint32_t)FLASH_CMD_KEY_Access << FLASH_CMD_KEY_Pos) | FLASH_CMD_WR_Msk;
    return bl_flash_wait_ready(BL_FLASH_WAIT_WRITE_LOOPS);
}

/* Читает один 16-байтный блок напрямую из отображенной flash-памяти. */
static bool bl_flash_read_block16(uint32_t abs_addr, uint8_t* out_block) {
    const volatile uint8_t* flash_ptr;
    uint32_t i;

    if (out_block == 0) {
        return false;
    }

    flash_ptr = (const volatile uint8_t*)(uintptr_t)abs_addr;
    for (i = 0u; i < 16u; ++i) {
        out_block[i] = flash_ptr[i];
    }
    return true;
}

/* Инициализирует UART, вход кнопки обновления и статусные светодиоды. */
void bl_hal_init(void) {
    uint32_t ibrd;
    uint32_t fbrd;

    /* UART4 pins: PA8 RX, PA9 TX */
    RCU->CGCFGAHB_bit.GPIOAEN = 1;
    RCU->RSTDISAHB_bit.GPIOAEN = 1;
    GPIOA->ALTFUNCNUM_bit.PIN8 = 1;
    GPIOA->ALTFUNCNUM_bit.PIN9 = 1;
    GPIOA->ALTFUNCSET = GPIO_ALTFUNCSET_PIN8_Msk | GPIO_ALTFUNCSET_PIN9_Msk;

    RCU->CGCFGAPB_bit.UART4EN = 1;
    RCU->RSTDISAPB_bit.UART4EN = 1;
    RCU->UARTCLKCFG[4].UARTCLKCFG_bit.CLKSEL = RCU_UARTCLKCFG_CLKSEL_HSE;
    RCU->UARTCLKCFG[4].UARTCLKCFG_bit.DIVEN = 0;
    RCU->UARTCLKCFG[4].UARTCLKCFG_bit.RSTDIS = 1;
    RCU->UARTCLKCFG[4].UARTCLKCFG_bit.CLKEN = 1;

    ibrd = BL_UART_CLOCK_HZ / (16u * BL_UART_BAUDRATE);
    fbrd = ((BL_UART_CLOCK_HZ - (ibrd * 16u * BL_UART_BAUDRATE)) << 6) / (16u * BL_UART_BAUDRATE);
    UART4->IBRD = ibrd;
    UART4->FBRD = fbrd;
    UART4->LCRH = UART_LCRH_FEN_Msk | (UART_LCRH_WLEN_8bit << UART_LCRH_WLEN_Pos);
    UART4->IFLS = 0;
    UART4->CR = UART_CR_RXE_Msk | UART_CR_TXE_Msk | UART_CR_UARTEN_Msk;

    /* Update button: PC0 with pull-up, active low. */
    RCU->CGCFGAHB_bit.GPIOCEN = 1;
    RCU->RSTDISAHB_bit.GPIOCEN = 1;
    BL_UPDATE_BTN_PORT->PULLMODE_bit.PIN0 = 1;
    BL_UPDATE_BTN_PORT->ALTFUNCCLR = BL_UPDATE_BTN_MASK;
    BL_UPDATE_BTN_PORT->OUTENCLR = BL_UPDATE_BTN_MASK;

    /* Update LEDs: PA12..PA15 as GPIO outputs, default off. */
    BL_UPDATE_LED_PORT->ALTFUNCCLR = BL_UPDATE_LED_MASK;
    BL_UPDATE_LED_PORT->OUTMODE_bit.PIN12 = GPIO_OUTMODE_PIN12_PP;
    BL_UPDATE_LED_PORT->OUTMODE_bit.PIN13 = GPIO_OUTMODE_PIN13_PP;
    BL_UPDATE_LED_PORT->OUTMODE_bit.PIN14 = GPIO_OUTMODE_PIN14_PP;
    BL_UPDATE_LED_PORT->OUTMODE_bit.PIN15 = GPIO_OUTMODE_PIN15_PP;
    BL_UPDATE_LED_PORT->OUTENSET = BL_UPDATE_LED_MASK;
    BL_UPDATE_LED_PORT->DATAOUTCLR = BL_UPDATE_LED_MASK;
}

/* Отправляет один байт через UART загрузчика  */
void bl_hal_uart_putc(uint8_t byte) {
    while (UART4->FR_bit.TXFE == 0u) {
    }
    UART4->DR = byte;
}

/* Ожидает, пока передатчик UART и FIFO перейдут в состояние idle. */
void bl_hal_uart_wait_tx_idle(void) {
    uint32_t loops = 200000u;
    while (loops > 0u) {
        if ((UART4->FR_bit.BUSY == 0u) && (UART4->FR_bit.TXFE != 0u)) {
            break;
        }
        --loops;
    }
}

/* Принимает фиксированное число байт с простым таймаутом по циклам. */
bool bl_hal_uart_get(uint8_t* dst, uint32_t len, uint32_t timeout_ms) {
    uint32_t loops;
    uint32_t left = len;
    uint8_t* wr = dst;

    if (dst == 0) {
        return false;
    }

    loops = timeout_ms * BL_UART_TIMEOUT_LOOPS_PER_MS;
    while (left > 0u && loops > 0u) {
        if (UART4->FR_bit.RXFE == 0u) {
            *wr++ = (uint8_t)UART4->DR;
            --left;
            loops = timeout_ms * BL_UART_TIMEOUT_LOOPS_PER_MS;
            continue;
        }
        --loops;
    }
    return (left == 0u);
}

/* Читает состояние кнопки обновления (активный низкий уровень). */
bool bl_hal_is_update_button_pressed(void) {
    return ((BL_UPDATE_BTN_PORT->DATA & BL_UPDATE_BTN_MASK) == 0u);
}

/* Включает или выключает группу светодиодов режима обновления. */
void bl_hal_set_update_mode_leds(bool on) {
    if (on) {
        BL_UPDATE_LED_PORT->DATAOUTSET = BL_UPDATE_LED_MASK;
    } else {
        BL_UPDATE_LED_PORT->DATAOUTCLR = BL_UPDATE_LED_MASK;
    }
}

/* Стирает все страницы flash, пересекающиеся с заданным диапазоном адресов. */
bool bl_hal_flash_erase_range(uint32_t abs_addr, uint32_t size_bytes) {
    uint32_t start;
    uint32_t end;
    uint32_t addr;

    if (size_bytes == 0u) {
        return true;
    }

    start = abs_addr & ~(BL_FLASH_ERASE_SIZE_BYTES - 1u);
    end = (abs_addr + size_bytes + BL_FLASH_ERASE_SIZE_BYTES - 1u) & ~(BL_FLASH_ERASE_SIZE_BYTES - 1u);

    for (addr = start; addr < end; addr += BL_FLASH_ERASE_SIZE_BYTES) {
        if (!bl_flash_erase_page(addr)) {
            return false;
        }
    }
    return true;
}

/* Записывает данные произвольной длины во flash через 16-байтные RMW-блоки. */
bool bl_hal_flash_write(uint32_t abs_addr, const uint8_t* data, uint32_t len) {
    uint32_t addr = abs_addr;
    const uint8_t* src = data;
    uint32_t left = len;
    uint32_t page_addr;
    uint32_t page_off;
    uint32_t chunk;
    uint32_t i;
    uint8_t page_buf[16];

    if (data == 0) {
        return false;
    }
    if (len == 0u) {
        return true;
    }

    while (left > 0u) {
        page_addr = addr & ~(uint32_t)0x0Fu;
        page_off = addr & (uint32_t)0x0Fu;
        chunk = 16u - page_off;
        if (chunk > left) {
            chunk = left;
        }

        if (!bl_flash_read_block16(page_addr, page_buf)) {
            return false;
        }
        for (i = 0u; i < chunk; ++i) {
            page_buf[page_off + i] = src[i];
        }

        if (!bl_flash_write16(page_addr, page_buf, 16u)) {
            return false;
        }
        addr += chunk;
        src += chunk;
        left -= chunk;
    }
    return true;
}
