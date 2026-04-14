#include "extflash_w25q32.h"

#include <stdint.h>
#include "../../../device/Include/K1921VG015.h"
#include "../../../plib/inc/plib015_gpio.h"

// Команды W25Q32
#define W25Q32_CMD_READ_DATA      0x03u
#define W25Q32_CMD_PAGE_PROGRAM   0x02u
#define W25Q32_CMD_READ_STATUS1   0x05u
#define W25Q32_CMD_WRITE_ENABLE   0x06u
#define W25Q32_CMD_SECTOR_ERASE   0x20u  // 4КБ сектор

// Bits регистра STATUS1
#define W25Q32_SR1_BUSY           0x01u

// Параметры W25Q32
#define W25Q32_PAGE_SIZE          256u
#define W25Q32_SECTOR_SIZE        4096u

// Используем те же пины SPI0, что и в текущем драйвере EEPROM по SPI:
// SCK=PB0, MISO=PB2, MOSI=PB3, CS=PB1
#define EXTFLASH_CS_PORT GPIOB
#define EXTFLASH_CS_PIN  GPIO_Pin_1

static void spi0_init_for_extflash(void);
static void gpio_init_spi_pins(void);
static void extflash_cs_low(void);
static void extflash_cs_high(void);
static uint8_t extflash_spi_xfer(uint8_t byte);
static bool extflash_wait_ready(uint32_t timeout_ms);
static void extflash_write_enable(void);

void extflash_init_spi0_cs_pb1(void)
{
	spi0_init_for_extflash();
}

static void spi0_init_for_extflash(void)
{
	// Питание тактированием GPIOB и SPI0
	RCU->CGCFGAHB_bit.GPIOBEN = 1;
	RCU->RSTDISAHB_bit.GPIOBEN = 1;
	RCU->CGCFGAHB_bit.SPI0EN = 1;
	RCU->RSTDISAHB_bit.SPI0EN = 1;

	RCU->SPICLKCFG[0].SPICLKCFG_bit.CLKSEL = RCU_SPICLKCFG_CLKSEL_HSE;
	RCU->SPICLKCFG[0].SPICLKCFG_bit.CLKEN = 1;
	RCU->SPICLKCFG[0].SPICLKCFG_bit.RSTDIS = 1;

	// Частота SCK ~ 1 МГц (как и в eeprom.c)
	SPI0->CPSR_bit.CPSDVSR = 8;
	SPI0->CR0_bit.SCR = 1;
	SPI0->CR0_bit.SPO = 0;
	SPI0->CR0_bit.SPH = 0;
	SPI0->CR0_bit.FRF = 0;
	SPI0->CR0_bit.DSS = 7; // 8 бит
	SPI0->CR1_bit.MS = 0;

	gpio_init_spi_pins();

	// Без прерываний: работаем по polling
	SPI0->IMSC = 0;
	SPI0->CR1_bit.SSE = 1;
}

static void gpio_init_spi_pins(void)
{
	GPIOB->ALTFUNCSET = GPIO_ALTFUNCSET_PIN0_Msk |
	                    GPIO_ALTFUNCSET_PIN2_Msk |
	                    GPIO_ALTFUNCSET_PIN3_Msk;
	GPIOB->ALTFUNCNUM = (GPIO_ALTFUNCNUM_PIN0_AF1 << GPIO_ALTFUNCNUM_PIN0_Pos) |
	                    (GPIO_ALTFUNCNUM_PIN2_AF1 << GPIO_ALTFUNCNUM_PIN2_Pos) |
	                    (GPIO_ALTFUNCNUM_PIN3_AF1 << GPIO_ALTFUNCNUM_PIN3_Pos);

	// CS как GPIO выход, по умолчанию в 1.
	GPIO_Init_TypeDef gpio;
	GPIO_StructInit(&gpio);
	gpio.Out = ENABLE;
	gpio.AltFunc = DISABLE;
	gpio.Pin = EXTFLASH_CS_PIN;
	GPIO_Init(EXTFLASH_CS_PORT, &gpio);
	GPIO_SetBits(EXTFLASH_CS_PORT, EXTFLASH_CS_PIN);
}

static void extflash_cs_low(void)
{
	GPIO_ClearBits(EXTFLASH_CS_PORT, EXTFLASH_CS_PIN);
}

static void extflash_cs_high(void)
{
	GPIO_SetBits(EXTFLASH_CS_PORT, EXTFLASH_CS_PIN);
}

static uint8_t extflash_spi_xfer(uint8_t byte)
{
	while (!(SPI0->SR & SPI_SR_TNF_Msk)) {
	}
	SPI0->DR = byte;

	while (!(SPI0->SR & SPI_SR_RNE_Msk)) {
	}
	return (uint8_t)SPI0->DR;
}

static void extflash_write_enable(void)
{
	extflash_cs_low();
	extflash_spi_xfer(W25Q32_CMD_WRITE_ENABLE);
	extflash_cs_high();
}

static bool extflash_wait_ready(uint32_t timeout_ms)
{
	extern uint32_t SystemCoreClock;
	uint32_t cpu_hz = SystemCoreClock;

	const uint32_t cycles_per_iter = 30u;
	uint32_t total_cycles = (cpu_hz / 1000u) * timeout_ms;
	uint32_t iters = total_cycles / cycles_per_iter;
	if (iters == 0u) {
		iters = 1u;
	}

	while (iters--) {
		extflash_cs_low();
		extflash_spi_xfer(W25Q32_CMD_READ_STATUS1);
		uint8_t sr = extflash_spi_xfer(0xFFu);
		extflash_cs_high();
		if ((sr & W25Q32_SR1_BUSY) == 0u) {
			return true;
		}
	}
	return false;
}

bool extflash_read(uint32_t addr, uint8_t *dst, size_t len)
{
	if (!dst || len == 0u) {
		return false;
	}

	extflash_cs_low();
	extflash_spi_xfer(W25Q32_CMD_READ_DATA);
	extflash_spi_xfer((uint8_t)(addr >> 16));
	extflash_spi_xfer((uint8_t)(addr >> 8));
	extflash_spi_xfer((uint8_t)(addr));

	for (size_t i = 0; i < len; ++i) {
		dst[i] = extflash_spi_xfer(0xFFu);
	}

	extflash_cs_high();
	return true;
}

bool extflash_erase_range_4k(uint32_t addr, uint32_t len)
{
	uint32_t start = addr & ~(W25Q32_SECTOR_SIZE - 1u);
	uint32_t end = (addr + len + W25Q32_SECTOR_SIZE - 1u) & ~(W25Q32_SECTOR_SIZE - 1u);

	for (uint32_t a = start; a < end; a += W25Q32_SECTOR_SIZE) {
		extflash_write_enable();
		extflash_cs_low();
		extflash_spi_xfer(W25Q32_CMD_SECTOR_ERASE);
		extflash_spi_xfer((uint8_t)(a >> 16));
		extflash_spi_xfer((uint8_t)(a >> 8));
		extflash_spi_xfer((uint8_t)(a));
		extflash_cs_high();

		if (!extflash_wait_ready(300u)) {
			return false;
		}
	}
	return true;
}

bool extflash_write(uint32_t addr, const uint8_t *src, size_t len)
{
	if (!src || len == 0u) {
		return false;
	}

	while (len > 0u) {
		uint32_t page_off = addr % W25Q32_PAGE_SIZE;
		uint32_t chunk = W25Q32_PAGE_SIZE - page_off;
		if (chunk > len) {
			chunk = (uint32_t)len;
		}

		extflash_write_enable();
		extflash_cs_low();
		extflash_spi_xfer(W25Q32_CMD_PAGE_PROGRAM);
		extflash_spi_xfer((uint8_t)(addr >> 16));
		extflash_spi_xfer((uint8_t)(addr >> 8));
		extflash_spi_xfer((uint8_t)(addr));
		for (uint32_t i = 0; i < chunk; ++i) {
			extflash_spi_xfer(src[i]);
		}
		extflash_cs_high();

		if (!extflash_wait_ready(10u)) {
			return false;
		}

		addr += chunk;
		src += chunk;
		len -= chunk;
	}
	return true;
}

