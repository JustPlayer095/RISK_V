#include "../driver/eeprom_spi.h"
#include <stdint.h>
#include <stdio.h>
#include "../device/Include/plic.h"
#include "../device/Include/K1921VG015.h"
#include "../plib/inc/plib015_gpio.h"

static void SPI0_IRQHandler(void);
static void spi0_init(void);
static void gpio_init_spi_pins(void);


#define EEPROM_CS_PORT GPIOB
#define EEPROM_CS_PIN  GPIO_Pin_1


void eeprom_spi_cs_low(void)
{
    GPIO_ClearBits(EEPROM_CS_PORT, EEPROM_CS_PIN);
}

void eeprom_spi_cs_high(void)
{
    GPIO_SetBits(EEPROM_CS_PORT, EEPROM_CS_PIN);
}

void eeprom_spi_init(void)
{
    spi0_init();
}

static void spi0_init(void)
{
    RCU->CGCFGAHB_bit.GPIOBEN = 1;                                                                                                 // Разрешение тактирования порта GPIOB
    RCU->RSTDISAHB_bit.GPIOBEN = 1;                                                                                                // Вывод из состояния сброса порта GPIOB
    RCU->CGCFGAHB_bit.SPI0EN = 1;                                                                                                  // Разрешение тактирования SPI0
    RCU->RSTDISAHB_bit.SPI0EN = 1;                                                                                                 // Вывод из состояния сброса SPI0
    RCU->SPICLKCFG[0].SPICLKCFG_bit.CLKSEL = RCU_SPICLKCFG_CLKSEL_HSE;                                                             // Источник сигнала внешний кварц
    RCU->SPICLKCFG[0].SPICLKCFG_bit.CLKEN = 1;                                                                                     // Разрешение тактирования
    RCU->SPICLKCFG[0].SPICLKCFG_bit.RSTDIS = 1;                                                                                    // Вывод из сброса
    SPI0->CPSR_bit.CPSDVSR = 8;                                                                                                    // Коэффициент деления первого делителя
    SPI0->CR0_bit.SCR = 1;                                                                                                         // Коэффициент деления второго делителя. Результирующий коэффициент SCK/((SCR+1)*CPSDVSR) 16/((1+1)*8)=1МГц
    SPI0->CR0_bit.SPO = 0;                                                                                                         // Полярность сигнала. В режиме ожидания линия в состоянии логического нуля.
    SPI0->CR0_bit.SPH = 0;                                                                                                         // Фаза: выборка на первом фронте (CPHA=0)
    SPI0->CR0_bit.FRF = 0;                                                                                                         // Выбор протокола обмена информацией 0-SPI
    SPI0->CR0_bit.DSS = 7;                                                                                                         // Размер слова данных 8 бит
    SPI0->CR1_bit.MS = 0;                                                                                                          // Режим работы - Мастер
    gpio_init_spi_pins();
    SPI0->IMSC = 0x1;                                                                                                                    // Разрешаем прерывания по переполнению приемного буфера
    // Настраиваем обработчик прерывания для SPI0
    PLIC_SetIrqHandler(Plic_Mach_Target, IsrVect_IRQ_SPI0, SPI0_IRQHandler);
    PLIC_SetPriority(IsrVect_IRQ_SPI0, 0x1);
    PLIC_IntEnable(Plic_Mach_Target, IsrVect_IRQ_SPI0);

    SPI0->CR1_bit.SSE = 1; // Разрешение работы приемопередатчика
}

static void gpio_init_spi_pins(void)
{
    GPIOB->ALTFUNCSET = GPIO_ALTFUNCSET_PIN0_Msk |  // SCK
                        GPIO_ALTFUNCSET_PIN2_Msk |  // MISO (RX)
                        GPIO_ALTFUNCSET_PIN3_Msk;   // MOSI (TX)
    GPIOB->ALTFUNCNUM = (GPIO_ALTFUNCNUM_PIN0_AF1 << GPIO_ALTFUNCNUM_PIN0_Pos) |
                        (GPIO_ALTFUNCNUM_PIN2_AF1 << GPIO_ALTFUNCNUM_PIN2_Pos) |
                        (GPIO_ALTFUNCNUM_PIN3_AF1 << GPIO_ALTFUNCNUM_PIN3_Pos);

    // CS (PB1) как GPIO выход, держим в "1"
    GPIO_Init_TypeDef gpio;
    GPIO_StructInit(&gpio);
    gpio.Out = ENABLE;
    gpio.AltFunc = DISABLE;
    gpio.Pin = EEPROM_CS_PIN;
    GPIO_Init(EEPROM_CS_PORT, &gpio);
    GPIO_SetBits(EEPROM_CS_PORT, EEPROM_CS_PIN);
}

uint8_t eeprom_spi_xfer(uint8_t byte)
{
    // Ждём свободное место в TX FIFO
    while (!(SPI0->SR & SPI_SR_TNF_Msk)) {
    }
    SPI0->DR = byte;

    // Ждём байт в RX FIFO
    while (!(SPI0->SR & SPI_SR_RNE_Msk)) {
    }
    return (uint8_t)SPI0->DR;
}

uint8_t eeprom_spi_read_status(void)
{
    eeprom_spi_cs_low();
    eeprom_spi_xfer(EEPROM_SPI_CMD_RDSR);
    uint8_t sr = eeprom_spi_xfer(0xFF);
    eeprom_spi_cs_high();
    return sr;
}

bool eeprom_spi_wait_ready(uint32_t timeout_cycles)
{
    while (timeout_cycles--) {
        uint8_t sr = eeprom_spi_read_status();
        if ((sr & EEPROM_SPI_STATUS_BUSY) == 0) {
            return true;
        }
    }
    return false;
}

bool eeprom_spi_loopback_test(uint8_t rx_out[3])
{
    const uint8_t tx[3] = {0xAA, 0x55, 0x81};
    uint8_t rx[3] = {0};

    // Очистить возможные данные в RX FIFO
    while (SPI0->SR & SPI_SR_RNE_Msk) {
        (void)SPI0->DR;
    }

    for (uint8_t i = 0; i < 3; ++i) {
        rx[i] = eeprom_spi_xfer(tx[i]);
    }

    if (rx_out) {
        rx_out[0] = rx[0];
        rx_out[1] = rx[1];
        rx_out[2] = rx[2];
    }

    return (rx[0] == tx[0]) && (rx[1] == tx[1]) && (rx[2] == tx[2]);
}

uint8_t eeprom_spi_wren_and_status(void)
{
    // Читаем текущий SR
    uint8_t sr0 = eeprom_spi_read_status();
    // Посылаем WREN
    eeprom_spi_cs_low();
    eeprom_spi_xfer(EEPROM_SPI_CMD_WREN);
    eeprom_spi_cs_high();
    // Читаем SR снова
    return eeprom_spi_read_status();
}

static void eeprom_spi_write_enable(void)
{
    eeprom_spi_cs_low();
    eeprom_spi_xfer(EEPROM_SPI_CMD_WREN);
    eeprom_spi_cs_high();
}

void eeprom_spi_write_bytes(uint16_t addr, const uint8_t* data, size_t len)
{
    if (!data || !len) {
        return;
    }

    while (len) {
        size_t page_off = addr % EEPROM_SPI_PAGE_SIZE;
        size_t chunk = EEPROM_SPI_PAGE_SIZE - page_off;
        if (chunk > len) chunk = len;

        eeprom_spi_write_enable();

        eeprom_spi_cs_low();
        eeprom_spi_xfer(EEPROM_SPI_CMD_WRITE);
        eeprom_spi_xfer(addr >> 8);
        eeprom_spi_xfer(addr & 0xFF);
        for (size_t i = 0; i < chunk; ++i) {
            eeprom_spi_xfer(data[i]);
        }
        eeprom_spi_cs_high();

        if (!eeprom_spi_wait_ready(1000000)) {
            // не дождались — выходим, чтобы не повиснуть
            return;
        }

        addr += (uint16_t)chunk;
        data += chunk;
        len  -= chunk;
    }
}

void eeprom_spi_read_bytes(uint16_t addr, uint8_t* data, size_t len)
{
    if (!data || !len) {
        return;
    }

    eeprom_spi_cs_low();
    eeprom_spi_xfer(EEPROM_SPI_CMD_READ);
    eeprom_spi_xfer(addr >> 8);
    eeprom_spi_xfer(addr & 0xFF);
    for (size_t i = 0; i < len; ++i) {
        data[i] = eeprom_spi_xfer(0xFF);
    }
    eeprom_spi_cs_high();
}

static void SPI0_IRQHandler(void)
{
    GPIOA->DATAOUTTGL = 0xFF00;
    SPI0->ICR = 0x3;
}