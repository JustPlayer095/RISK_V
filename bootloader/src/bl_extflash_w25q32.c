#include "../include/bl_extflash_w25q32.h"

#include <stdint.h>
#include "../../vg015/device/include/K1921VG015.h"
#include "../../vg015/plib/inc/plib015_gpio.h"

// Команды W25Q32
#define W25Q32_CMD_READ_DATA      0x03u
#define W25Q32_CMD_READ_STATUS1   0x05u
#define W25Q32_CMD_RELEASE_PD     0xABu

// STATUS1 BUSY bit
#define W25Q32_SR1_BUSY          0x01u

// Пины/CS (как в приложении: SPI0 SCK=PB0, MISO=PB2, MOSI=PB3, CS=PB1)
#define EXTFLASH_CS_PORT GPIOB
#define EXTFLASH_CS_PIN  GPIO_Pin_1

static void spi0_init_for_extflash(void);
static void gpio_init_spi_pins(void);
static void extflash_cs_low(void);
static void extflash_cs_high(void);
static uint8_t extflash_spi_xfer(uint8_t byte);
static void extflash_release_power_down(void);
static void extflash_spi_flush_rx(void);
static void extflash_short_delay(void);

void bl_extflash_init_spi0_cs_pb1(void)
{
	spi0_init_for_extflash();
	extflash_release_power_down();
}

static void spi0_init_for_extflash(void)
{
	// Тактирование GPIOB и SPI0
	RCU->CGCFGAHB_bit.GPIOBEN = 1;
	RCU->RSTDISAHB_bit.GPIOBEN = 1;
	RCU->CGCFGAHB_bit.SPI0EN = 1;
	RCU->RSTDISAHB_bit.SPI0EN = 1;

	/* В bootloader выбираем HSI, чтобы не зависеть от CPE/инициализации HSE. */
	RCU->SPICLKCFG[0].SPICLKCFG_bit.CLKSEL = RCU_SPICLKCFG_CLKSEL_HSI;
	RCU->SPICLKCFG[0].SPICLKCFG_bit.CLKEN = 1;
	RCU->SPICLKCFG[0].SPICLKCFG_bit.RSTDIS = 1;

	// Более высокая скорость SPI для ускорения копирования.
	SPI0->CPSR_bit.CPSDVSR = 2;
	SPI0->CR0_bit.SCR = 0;

	// CPOL=0, CPHA=0
	SPI0->CR0_bit.SPO = 0;
	SPI0->CR0_bit.SPH = 0;
	SPI0->CR0_bit.FRF = 0;
	SPI0->CR0_bit.DSS = 7; // 8 бит
	SPI0->CR1_bit.MS = 0;

	gpio_init_spi_pins();

	// Разрешение работы приемопередатчика
	SPI0->IMSC = 0;
	SPI0->CR1_bit.SSE = 1;
}

static void gpio_init_spi_pins(void)
{
	// SCK/MISO/MOSI в альтернативную функцию AF1.
	GPIOB->ALTFUNCSET = GPIO_ALTFUNCSET_PIN0_Msk |
	                    GPIO_ALTFUNCSET_PIN2_Msk |
	                    GPIO_ALTFUNCSET_PIN3_Msk;
	GPIOB->ALTFUNCNUM = (GPIO_ALTFUNCNUM_PIN0_AF1 << GPIO_ALTFUNCNUM_PIN0_Pos) |
	                    (GPIO_ALTFUNCNUM_PIN2_AF1 << GPIO_ALTFUNCNUM_PIN2_Pos) |
	                    (GPIO_ALTFUNCNUM_PIN3_AF1 << GPIO_ALTFUNCNUM_PIN3_Pos);

	// CS (PB1) как обычный GPIO‑выход, push‑pull, по умолчанию = 1.
	GPIOB->ALTFUNCCLR = (1u << 1);                 // убрать альтернативную функцию для PB1
	GPIOB->OUTMODE_bit.PIN1 = GPIO_OUTMODE_PIN1_PP;
	GPIOB->OUTENSET = (1u << 1);
	GPIOB->DATAOUTSET = (1u << 1);
}

static void extflash_cs_low(void)
{
	GPIO_ClearBits(EXTFLASH_CS_PORT, EXTFLASH_CS_PIN);
	extflash_short_delay();
}

static void extflash_cs_high(void)
{
	extflash_short_delay();
	GPIO_SetBits(EXTFLASH_CS_PORT, EXTFLASH_CS_PIN);
	extflash_short_delay();
}

static uint8_t extflash_spi_xfer(uint8_t byte)
{
	while (!(SPI0->SR & SPI_SR_TNF_Msk)) {
	}
	SPI0->DR = byte;
	while (!(SPI0->SR & SPI_SR_RNE_Msk)) {
	}
	while (SPI0->SR & SPI_SR_BSY_Msk) {
	}
	return (uint8_t)SPI0->DR;
}

static void extflash_release_power_down(void)
{
	/* Некоторые чипы после reset могут быть в deep-power-down.
	 * Команда 0xAB + короткая пауза выводит flash в normal read mode. */
	extflash_cs_low();
	extflash_spi_xfer(W25Q32_CMD_RELEASE_PD);
	extflash_spi_xfer(0x00u);
	extflash_spi_xfer(0x00u);
	extflash_spi_xfer(0x00u);
	(void)extflash_spi_xfer(0x00u);
	extflash_cs_high();

	for (volatile uint32_t d = 0u; d < 2000u; ++d) {
		__asm__ volatile ("nop");
	}
}

static void extflash_spi_flush_rx(void)
{
	while (SPI0->SR & SPI_SR_RNE_Msk) {
		(void)SPI0->DR;
	}
}

static void extflash_short_delay(void)
{
	for (volatile uint32_t d = 0u; d < 8u; ++d) {
		__asm__ volatile ("nop");
	}
}

bool bl_extflash_read(uint32_t addr, uint8_t *dst, size_t len)
{
	if (!dst || len == 0u) {
		return false;
	}

	extflash_spi_flush_rx();
	extflash_cs_low();
	extflash_spi_xfer(W25Q32_CMD_READ_DATA);
	extflash_spi_xfer((uint8_t)(addr >> 16));
	extflash_spi_xfer((uint8_t)(addr >> 8));
	extflash_spi_xfer((uint8_t)(addr));
	extflash_short_delay();

	for (size_t i = 0; i < len; ++i) {
		dst[i] = extflash_spi_xfer(0xFFu);
	}

	extflash_cs_high();
	return true;
}

