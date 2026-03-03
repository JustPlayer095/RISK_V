#include "../include/bl_hal.h"

#include "../../vg015/device/include/K1921VG015.h"

#define BL_UART_BAUDRATE               ((uint32_t)460800u)
#define BL_UART_CLOCK_HZ               ((uint32_t)50000000u)
#define BL_FLASH_WAIT_ERASE_LOOPS      ((uint32_t)2000000u)
#define BL_FLASH_WAIT_WRITE_LOOPS      ((uint32_t)200000u)
#define BL_UART_TIMEOUT_LOOPS_PER_MS   ((uint32_t)2000u)

static bool bl_flash_wait_ready(uint32_t loops) {
    while (loops > 0u) {
        if ((FLASH->STAT & FLASH_STAT_BUSY_Msk) == 0u) {
            return true;
        }
        --loops;
    }
    return false;
}

static uint32_t bl_flash_offs(uint32_t abs_addr) {
    return (abs_addr - MEM_FLASH_BASE);
}

static bool bl_flash_erase_page(uint32_t abs_addr) {
    FLASH->ADDR = bl_flash_offs(abs_addr);
    FLASH->CMD = ((uint32_t)FLASH_CMD_KEY_Access << FLASH_CMD_KEY_Pos) | FLASH_CMD_ERSEC_Msk;
    return bl_flash_wait_ready(BL_FLASH_WAIT_ERASE_LOOPS);
}

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

void bl_hal_init(void) {
    uint32_t ibrd;
    uint32_t fbrd;

    /* UART0 pins: PA0 RX, PA1 TX */
    RCU->CGCFGAHB_bit.GPIOAEN = 1;
    RCU->RSTDISAHB_bit.GPIOAEN = 1;
    GPIOA->PULLMODE &= ~(GPIO_PULLMODE_PIN0_Msk | GPIO_PULLMODE_PIN1_Msk);
    GPIOA->ALTFUNCNUM_bit.PIN0 = 1;
    GPIOA->ALTFUNCNUM_bit.PIN1 = 1;
    GPIOA->ALTFUNCSET_bit.PIN0 = 1;
    GPIOA->ALTFUNCSET_bit.PIN1 = 1;

    RCU->CGCFGAPB_bit.UART0EN = 1;
    RCU->RSTDISAPB_bit.UART0EN = 1;
    RCU->UARTCLKCFG[0].UARTCLKCFG = RCU_UARTCLKCFG_CLKEN_Msk | (RCU_UARTCLKCFG_CLKSEL_PLL0 << RCU_UARTCLKCFG_CLKSEL_Pos);
    RCU->UARTCLKCFG[0].UARTCLKCFG_bit.RSTDIS = 1;

    ibrd = BL_UART_CLOCK_HZ / (16u * BL_UART_BAUDRATE);
    fbrd = ((BL_UART_CLOCK_HZ - (ibrd * 16u * BL_UART_BAUDRATE)) << 6) / (16u * BL_UART_BAUDRATE);
    UART0->IBRD = ibrd;
    UART0->FBRD = fbrd;
    UART0->LCRH = UART_LCRH_FEN_Msk | (UART_LCRH_WLEN_8bit << UART_LCRH_WLEN_Pos);
    UART0->CR = UART_CR_RXE_Msk | UART_CR_TXE_Msk | UART_CR_UARTEN_Msk;
}

void bl_hal_uart_putc(uint8_t byte) {
    while (UART0->FR_bit.TXFE == 0u) {
    }
    UART0->DR = byte;
}

bool bl_hal_uart_get(uint8_t* dst, uint32_t len, uint32_t timeout_ms) {
    uint32_t loops;
    uint32_t left = len;
    uint8_t* wr = dst;

    if (dst == 0) {
        return false;
    }

    loops = timeout_ms * BL_UART_TIMEOUT_LOOPS_PER_MS;
    while (left > 0u && loops > 0u) {
        if (UART0->FR_bit.RXFE == 0u) {
            *wr++ = (uint8_t)UART0->DR;
            --left;
            loops = timeout_ms * BL_UART_TIMEOUT_LOOPS_PER_MS;
            continue;
        }
        --loops;
    }
    return (left == 0u);
}

bool bl_hal_flash_erase_range(uint32_t abs_addr, uint32_t size_bytes) {
    uint32_t start;
    uint32_t end;
    uint32_t addr;

    if (size_bytes == 0u) {
        return true;
    }

    start = abs_addr & ~(MEM_FLASH_PAGE_SIZE - 1u);
    end = (abs_addr + size_bytes + MEM_FLASH_PAGE_SIZE - 1u) & ~(MEM_FLASH_PAGE_SIZE - 1u);

    for (addr = start; addr < end; addr += MEM_FLASH_PAGE_SIZE) {
        if (!bl_flash_erase_page(addr)) {
            return false;
        }
    }
    return true;
}

bool bl_hal_flash_write(uint32_t abs_addr, const uint8_t* data, uint32_t len) {
    uint32_t addr = abs_addr;
    const uint8_t* src = data;
    uint32_t left = len;
    uint32_t chunk;

    if (data == 0) {
        return false;
    }
    if (len == 0u) {
        return true;
    }

    while (left > 0u) {
        chunk = (left >= 16u) ? 16u : left;
        if (!bl_flash_write16(addr, src, chunk)) {
            return false;
        }
        addr += chunk;
        src += chunk;
        left -= chunk;
    }
    return true;
}
