#include "ws0010_1602.h"
#include "../gpio/gpio_helpers.h"
#include <stddef.h>

// Delays (implemented in vg015/device/source/mtimer.c)
void sleep(uint32_t ms);
void usleep(uint32_t us);

static inline void delay_short(void)
{
    // Small delay for E pulse width / data setup.
    for (volatile uint32_t i = 0; i < 50u; ++i) {
        __asm volatile("nop");
    }
}

static inline void set_pin(uint32_t pin_mask, uint8_t high)
{
    if (high) {
        GPIO_SetBits(WS0010_PORT, pin_mask);
    } else {
        GPIO_ClearBits(WS0010_PORT, pin_mask);
    }
}

static void write4(uint8_t rs, uint8_t nibble)
{
    // RS: 0=command, 1=data
    set_pin(WS0010_PIN_RS, rs ? 1 : 0);

    // Write-only path (R/W=0). Keeping R/W low removes the need for reads.
    set_pin(WS0010_PIN_RW, 0);

    set_pin(WS0010_PIN_DB4, (nibble >> 0) & 1u);
    set_pin(WS0010_PIN_DB5, (nibble >> 1) & 1u);
    set_pin(WS0010_PIN_DB6, (nibble >> 2) & 1u);
    set_pin(WS0010_PIN_DB7, (nibble >> 3) & 1u);

    delay_short();

    // E strobe
    set_pin(WS0010_PIN_E, 1);
    delay_short();
    set_pin(WS0010_PIN_E, 0);

    // Allow internal latch to capture
    delay_short();
}

static void write8(uint8_t rs, uint8_t byte)
{
    write4(rs, (uint8_t)((byte >> 4) & 0x0Fu));
    write4(rs, (uint8_t)(byte & 0x0Fu));
}

static inline void cmd(uint8_t c)
{
    write8(0, c);
}

static inline void data(uint8_t d)
{
    write8(1, d);
}

void ws0010_clear(void)
{
    cmd(0x01);
    // Clear needs a long execution time (order of ms).
    sleep(3);
}

void ws0010_home(void)
{
    cmd(0x02);
    sleep(3);
}

void ws0010_goto(uint8_t row, uint8_t col)
{
    if (col > 15u) col = 15u;
    uint8_t addr = (row ? 0x40u : 0x00u) + col;
    cmd((uint8_t)(0x80u | addr));
    usleep(50);
}

void ws0010_putc(char c)
{
    data((uint8_t)c);
    usleep(50);
}

void ws0010_print(const char* s)
{
    if (!s) return;
    while (*s) {
        ws0010_putc(*s++);
    }
}

static void gpio_init_ws0010(void)
{
    // Enable GPIO port clock/reset for selected port.
    if (WS0010_PORT == GPIOA) {
        RCU->CGCFGAHB_bit.GPIOAEN = 1;
        RCU->RSTDISAHB_bit.GPIOAEN = 1;
    } else if (WS0010_PORT == GPIOB) {
        RCU->CGCFGAHB_bit.GPIOBEN = 1;
        RCU->RSTDISAHB_bit.GPIOBEN = 1;
    } else {
        RCU->CGCFGAHB_bit.GPIOCEN = 1;
        RCU->RSTDISAHB_bit.GPIOCEN = 1;
    }

    gpio_init_output(WS0010_PORT, WS0010_PIN_RS, 0);
    gpio_init_output(WS0010_PORT, WS0010_PIN_RW, 0);
    gpio_init_output(WS0010_PORT, WS0010_PIN_E, 0);
    gpio_init_output(WS0010_PORT, WS0010_PIN_DB4, 0);
    gpio_init_output(WS0010_PORT, WS0010_PIN_DB5, 0);
    gpio_init_output(WS0010_PORT, WS0010_PIN_DB6, 0);
    gpio_init_output(WS0010_PORT, WS0010_PIN_DB7, 0);
}

void ws0010_init(void)
{
    gpio_init_ws0010();

    // Power-up delay. Datasheet mentions BF busy for ~10ms after VDD stable.
    sleep(50);

    // Standard HD44780-style 4-bit init sequence (6800):
    // Send 0x3 nibble 3 times, then 0x2 to enter 4-bit mode.
    write4(0, 0x3);
    sleep(5);
    write4(0, 0x3);
    usleep(150);
    write4(0, 0x3);
    usleep(150);
    write4(0, 0x2); // 4-bit
    usleep(150);

    // Function set: 4-bit, 2 lines, 5x8
    cmd(0x28);
    usleep(100);

    // WS0010: enable internal DCDC power on, character mode.
    // Instruction "Cursor/Display shift/Mode/Pwr" with PWR=1, G/C=0, last bits '11' -> 0x13.
    cmd(0x13);
    usleep(200);

    // Display off
    cmd(0x08);
    usleep(100);

    ws0010_clear();

    // Entry mode: increment, no shift
    cmd(0x06);
    usleep(100);

    // Display on, cursor off, blink off
    cmd(0x0C);
    usleep(100);
}

