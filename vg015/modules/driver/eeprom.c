#include "../driver/eeprom.h"
#include <stdint.h>
#include "../../device/Include/K1921VG015.h"
#include "../../device/Include/plic.h"
#include "../../plib/inc/plib015_gpio.h"

// Глобальная частота системного тактирования (задаётся в system_k1921vg015.c)
extern uint32_t SystemCoreClock;

// Таймаут ожидания завершения внутренней записи EEPROM в SPI‑ветке (мс).
// Можно переопределить через -DEEPROM_SPI_TIMEOUT_MS=... при сборке.
#ifndef EEPROM_SPI_TIMEOUT_MS
#define EEPROM_SPI_TIMEOUT_MS 20u
#endif

#if !defined(EEPROM_USE_SPI) && !defined(EEPROM_USE_I2C)
#define EEPROM_USE_SPI
#endif

#if defined(SPI_USE) && !defined(EEPROM_USE_SPI)
#define EEPROM_USE_SPI
#endif

#if defined(I2C_USE) && !defined(EEPROM_USE_I2C)
#define EEPROM_USE_I2C
#endif



#if defined(EEPROM_USE_SPI)
#define CS_PORT GPIOB
#define CS_PIN  GPIO_Pin_1
static void SPI0_IRQHandler(void);
static void spi0_init(void);
static void gpio_init_spi_pins(void);

// Простейший статус для SPI-ветки (чтобы config.c мог вызывать eeprom_is_busy/eeprom_had_error)
static volatile bool g_eeprom_spi_busy  = false;
static volatile bool g_eeprom_spi_error = false;

bool eeprom_is_busy(void)
{
    return g_eeprom_spi_busy;
}

bool eeprom_had_error(void)
{
    return g_eeprom_spi_error;
}

void eeprom_spi_cs_low(void)
{
    GPIO_ClearBits(CS_PORT, CS_PIN);
}

void eeprom_spi_cs_high(void)
{
    GPIO_SetBits(CS_PORT, CS_PIN);
}

void eeprom_init(void)
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
    gpio.Pin = CS_PIN;
    GPIO_Init(CS_PORT, &gpio);
    GPIO_SetBits(CS_PORT, CS_PIN);
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
    eeprom_spi_xfer(SPI_CMD_RDSR);
    uint8_t sr = eeprom_spi_xfer(0xFF);
    eeprom_spi_cs_high();
    return sr;
}

bool eeprom_spi_wait_ready(uint32_t timeout_ms)
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
        uint8_t sr = eeprom_spi_read_status();
        if ((sr & SPI_STATUS_BUSY) == 0u) {
            return true;
        }
    }
    return false;
}


static void eeprom_spi_write_enable(void)
{
    eeprom_spi_cs_low();
    eeprom_spi_xfer(SPI_CMD_WREN);
    eeprom_spi_cs_high();
}

void eeprom_write_bytes(uint16_t addr, const uint8_t* data, size_t len)
{
    if (!data || !len) {
        return;
    }
    if (g_eeprom_spi_busy) {
        return;
    }

    g_eeprom_spi_error = false;
    g_eeprom_spi_busy  = true;

    while (len) {
        size_t page_off = addr % EEPROM_PAGE_SIZE;
        size_t chunk = EEPROM_PAGE_SIZE - page_off;
        if (chunk > len) {
            chunk = len;
        }

        eeprom_spi_write_enable();

        eeprom_spi_cs_low();
        eeprom_spi_xfer(SPI_CMD_WRITE);
        eeprom_spi_xfer(addr >> 8);
        eeprom_spi_xfer(addr & 0xFF);
        for (size_t i = 0; i < chunk; ++i) {
            eeprom_spi_xfer(data[i]);
        }
        eeprom_spi_cs_high();

        if (!eeprom_spi_wait_ready(EEPROM_SPI_TIMEOUT_MS)) {
            // не дождались — выходим, чтобы не повиснуть
            g_eeprom_spi_error = true;
            g_eeprom_spi_busy  = false;
            return;
        }

        addr += (uint16_t)chunk;
        data += chunk;
        len  -= chunk;
    }

    g_eeprom_spi_busy = false;
}

void eeprom_read_bytes(uint16_t addr, uint8_t* data, size_t len)
{
    if (!data || !len) {
        return;
    }

    if (g_eeprom_spi_busy) {
        return;
    }

    g_eeprom_spi_error = false;
    g_eeprom_spi_busy  = true;

    eeprom_spi_cs_low();
    eeprom_spi_xfer(SPI_CMD_READ);
    eeprom_spi_xfer(addr >> 8);
    eeprom_spi_xfer(addr & 0xFF);
    for (size_t i = 0; i < len; ++i) {
        data[i] = eeprom_spi_xfer(0xFF);
    }
    eeprom_spi_cs_high();

    g_eeprom_spi_busy = false;
}

static void SPI0_IRQHandler(void)
{
    SPI0->ICR = 0x3;
}
#elif defined(EEPROM_USE_I2C)

#define EEPROM_I2C_ACK_POLL_RETRIES 1000U
#define EEPROM_TIMEOUT_MS 50
// State machine для EEPROM
typedef enum {
    EEPROM_STATE_IDLE,
    EEPROM_STATE_WRITE_ADDR_H,
    EEPROM_STATE_WRITE_ADDR_L,
    EEPROM_STATE_WRITE_DATA,
    EEPROM_STATE_WRITE_WAIT,
    EEPROM_STATE_READ_ADDR_H,
    EEPROM_STATE_READ_ADDR_L,
    EEPROM_STATE_READ_RESTART,
    EEPROM_STATE_READ_DATA,
    EEPROM_STATE_ACK_POLL
} eeprom_state_t;

// Структура описания состояния транзакции 
typedef struct {
    eeprom_state_t state;                                   
    uint16_t addr;
    const uint8_t *write_data;
    uint8_t *read_data;
    size_t len;
    size_t idx;
    bool write_op;  // true для записи, false для чтения
    bool error;
    volatile bool busy;
} eeprom_transaction_t;

static eeprom_transaction_t transaction = {0};
bool eeprom_is_busy(void)
{
    return transaction.busy;
}

bool eeprom_had_error(void)
{
    return transaction.error;
}

void I2C_IRQHandler(void);

// Простая функция задержки для таймаутов (циклы процессора)
static void delay_cycles(uint32_t cycles)
{
    for (volatile uint32_t i = 0; i < cycles; ++i) {
        __asm volatile("nop");
    }
}

// Ожидание сброса бита 
static bool i2c_wait_for_bit(volatile uint32_t *reg, uint32_t bit_mask, uint32_t expected_value, uint32_t timeout_cycles)
{
    uint32_t cycles = 0;
    while (cycles < timeout_cycles) {
        uint32_t current_value = (*reg & bit_mask) ? 1 : 0;
        if (current_value == expected_value) {
            return true;
        }
        // Минимальная пауза между проверками
        delay_cycles(10);
        cycles += 10;
    }
    return false;
}

// Инициализация GPIO для I2C
static void I2C_GPIO_Init(void)
{
    /* Подключение I2C: SCL - PC12, SDA - PC13 */
    RCU->CGCFGAHB_bit.GPIOCEN = 1;                                             // Разрешение тактирования порта GPIOC
    RCU->RSTDISAHB_bit.GPIOCEN = 1;                                            // Вывод из состояния сброса порта GPIOC
    GPIOC->ALTFUNCSET = GPIO_ALTFUNCSET_PIN12_Msk | GPIO_ALTFUNCSET_PIN13_Msk; // Переводим пины в режим альтернативной функции
    // Выбор номера альтернативной функции
    GPIOC->ALTFUNCNUM_bit.PIN12 = 1;
    GPIOC->ALTFUNCNUM_bit.PIN13 = 1;
    GPIOC->OUTMODE_bit.PIN12 = 1; // Open Drain
    GPIOC->OUTMODE_bit.PIN13 = 1; // Open Drain
    GPIOC->PULLMODE |= 0x3000;    // PullUp
}

// Инициализация аппаратного I2C контроллера
static void I2C_Master_Init(void)
{
    uint32_t freq_calc;

    I2C_GPIO_Init();

    RCU->CGCFGAPB_bit.I2CEN = 1;
    RCU->RSTDISAPB_bit.I2CEN = 1;

    freq_calc = SystemCoreClock / (4 * I2C_FREQ);
    I2C->CTL1_bit.SCLFRQ = freq_calc & 0x7F;
    I2C->CTL3_bit.SCLFRQ = freq_calc >> 7;

    // Включаем модуль и разрешаем прерывание
    I2C->CTL1_bit.ENABLE = 1;
    I2C->CTL0_bit.INTEN = 1;

    // Настраиваем обработчик прерывания для I2C
    PLIC_SetIrqHandler(Plic_Mach_Target, IsrVect_IRQ_I2C, I2C_IRQHandler);
    PLIC_SetPriority(IsrVect_IRQ_I2C, 0x1);
    PLIC_IntEnable(Plic_Mach_Target, IsrVect_IRQ_I2C);
}

// ACK-поллинг для ожидания готовности EEPROM
static bool eeprom_ack_poll(void)
{
    uint32_t attempts = EEPROM_I2C_ACK_POLL_RETRIES;
    
    while (attempts--) {
        // Убеждаемся, что предыдущая транзакция завершена
        if (transaction.busy) {
            transaction.busy = false;
            transaction.state = EEPROM_STATE_IDLE;
            transaction.error = false;
        }
        
        // Инициализируем транзакцию для ACK-поллинга
        transaction.state = EEPROM_STATE_ACK_POLL;
        transaction.busy = true;
        transaction.error = false;
        
        // Проверяем, что шина свободна перед START
        uint32_t cpu_freq = SystemCoreClock;
        
        // Генерируем START
        I2C->CTL0_bit.START = 1;
        
        // Дальше всё делает I2C_IRQHandler: если адрес принят (ACK),
        // он перейдёт в состояние IDLE и сбросит busy.
        if (!transaction.error && !transaction.busy && transaction.state == EEPROM_STATE_IDLE) {
            return true;  // Получен ACK - EEPROM готов
        }
        
        // Сбрасываем состояние перед следующей попыткой
        transaction.busy = false;
        transaction.state = EEPROM_STATE_IDLE;
        transaction.error = false;
        
        // Задержка перед следующей попыткой (~50 мкс между попытками)
        // cpu_freq уже объявлена выше в цикле
        uint32_t delay_50us = cpu_freq / 20000;
        delay_cycles(delay_50us);
    }
    
    // Сбрасываем состояние при таймауте
    transaction.state = EEPROM_STATE_IDLE;
    transaction.busy = false;
    return false;  // Таймаут
}

// --- Вспомогательные обработчики режимов I2C ---

static void i2c_handle_stdone(void)
{
    // START сформирован (первичный START)
    if (transaction.state == EEPROM_STATE_WRITE_ADDR_H) {
        I2C->SDA = (I2C_ADDR << 1) | 0;
        I2C->CTL0_bit.CLRST = 1;
    } else if (transaction.state == EEPROM_STATE_READ_ADDR_H) {
        I2C->SDA = (I2C_ADDR << 1) | 0;
        I2C->CTL0_bit.CLRST = 1;
    } else if (transaction.state == EEPROM_STATE_ACK_POLL) {
        I2C->SDA = (I2C_ADDR << 1) | 0;
        I2C->CTL0_bit.CLRST = 1;
    } else {
        if (transaction.busy) {
            if (transaction.write_op) {
                transaction.state = EEPROM_STATE_WRITE_ADDR_H;
                I2C->SDA = (I2C_ADDR << 1) | 0;
            } else {
                transaction.state = EEPROM_STATE_READ_ADDR_H;
                I2C->SDA = (I2C_ADDR << 1) | 0;
            }
            I2C->CTL0_bit.CLRST = 1;
        } else {
            I2C->CTL0_bit.CLRST = 1;
        }
    }
}

static void i2c_handle_rsdone(void)
{
    // Повторный START сформирован
    if (transaction.state == EEPROM_STATE_READ_RESTART) {
        I2C->SDA = (I2C_ADDR << 1) | 1;
        I2C->CTL0_bit.CLRST = 1;
    } else {
        I2C->CTL0_bit.CLRST = 1;
    }
}

static void i2c_handle_mtadpa(void)
{
    // Адрес отправлен, получен ACK 
    if (transaction.state == EEPROM_STATE_WRITE_ADDR_H) {
        I2C->SDA = (uint8_t)(transaction.addr >> 8);
        I2C->CTL0_bit.CLRST = 1;
        transaction.state = EEPROM_STATE_WRITE_ADDR_L;
    } else if (transaction.state == EEPROM_STATE_READ_ADDR_H) {
        I2C->SDA = (uint8_t)(transaction.addr >> 8);
        I2C->CTL0_bit.CLRST = 1;
        transaction.state = EEPROM_STATE_READ_ADDR_L;
    } else if (transaction.state == EEPROM_STATE_ACK_POLL) {
        I2C->CTL0_bit.STOP = 1;
        I2C->CTL0_bit.CLRST = 1;
        transaction.state = EEPROM_STATE_IDLE;
        transaction.busy = false;
    } else {
        I2C->CTL0_bit.CLRST = 1;
    }
}

static void i2c_handle_mtdapa(void)
{
    // Байт данных отправлен, получен ACK
    if (transaction.state == EEPROM_STATE_WRITE_ADDR_L) {
        I2C->SDA = (uint8_t)(transaction.addr & 0xFF);
        I2C->CTL0_bit.CLRST = 1;
        transaction.state = EEPROM_STATE_WRITE_DATA;
    } else if (transaction.state == EEPROM_STATE_WRITE_DATA) {
        if (transaction.idx < transaction.len) {
            uint8_t byte_to_send = transaction.write_data[transaction.idx++];
            I2C->SDA = byte_to_send;
            I2C->CTL0_bit.CLRST = 1;
        } else {
            I2C->CTL0_bit.STOP = 1;
            I2C->CTL0_bit.CLRST = 1;
            transaction.state = EEPROM_STATE_WRITE_WAIT;
        }
    } else if (transaction.state == EEPROM_STATE_READ_ADDR_L) {
        I2C->SDA = (uint8_t)(transaction.addr & 0xFF);
        I2C->CTL0_bit.CLRST = 1;
        transaction.state = EEPROM_STATE_READ_RESTART;
        I2C->CTL0_bit.START = 1;
    } else if (transaction.state == EEPROM_STATE_READ_RESTART) {
        I2C->CTL0_bit.CLRST = 1;
    } else {
        I2C->CTL0_bit.CLRST = 1;
    }
}

static void i2c_handle_mradpa(void)
{
    // Адрес для чтения отправлен, получен ACK
    if (transaction.state == EEPROM_STATE_READ_RESTART) {
        transaction.state = EEPROM_STATE_READ_DATA;
        transaction.idx = 0;
        if (transaction.len > 1) {
            I2C->CTL0_bit.ACK = 0;
        } else {
            I2C->CTL0_bit.ACK = 1;
        }
        I2C->CTL0_bit.CLRST = 1;
    } else {
        I2C->CTL0_bit.CLRST = 1;
    }
}

static void i2c_handle_mrdapa(void)
{
    // Байт данных принят, отправлен ACK
    if (transaction.state == EEPROM_STATE_READ_DATA) {
        uint8_t byte = I2C->SDA;
        if (transaction.read_data && transaction.idx < transaction.len) {
            transaction.read_data[transaction.idx++] = byte;
        }
        if (transaction.idx >= transaction.len) {
            I2C->CTL0_bit.STOP = 1;
            I2C->CTL0_bit.CLRST = 1;
            transaction.state = EEPROM_STATE_IDLE;
            transaction.busy = false;
        } else if (transaction.idx >= transaction.len - 1) {
            I2C->CTL0_bit.ACK = 1;
            I2C->CTL0_bit.CLRST = 1;
        } else {
            I2C->CTL0_bit.ACK = 0;
            I2C->CTL0_bit.CLRST = 1;
        }
    } else {
        I2C->CTL0_bit.CLRST = 1;
    }
}

static void i2c_handle_mrdana(void)
{
    // Последний байт принят, отправлен NACK
    if (transaction.state == EEPROM_STATE_READ_DATA) {
        uint8_t byte = I2C->SDA;
        if (transaction.read_data && transaction.idx < transaction.len) {
            transaction.read_data[transaction.idx++] = byte;
        }
        I2C->CTL0_bit.STOP = 1;
        I2C->CTL0_bit.CLRST = 1;
        transaction.state = EEPROM_STATE_IDLE;
        transaction.busy = false;
    } else {
        I2C->CTL0_bit.CLRST = 1;
    }
}

static void i2c_handle_addr_nack(void)
{
    // NACK на адрес при записи/чтении
    I2C->CTL0_bit.STOP = 1;
    I2C->CTL0_bit.CLRST = 1;
    transaction.error = true;
    transaction.busy = false;
    transaction.state = EEPROM_STATE_IDLE;
}

static void i2c_handle_data_nack(void)
{
    // NACK на данные при записи
    I2C->CTL0_bit.STOP = 1;
    I2C->CTL0_bit.CLRST = 1;
    transaction.error = true;
    transaction.busy = false;
    transaction.state = EEPROM_STATE_IDLE;
}

// Обработчик прерываний I2C
void I2C_IRQHandler(void)
{
    uint8_t mode = I2C->ST_bit.MODE;
    
    switch (mode) {
        case I2C_ST_MODE_STDONE:
            i2c_handle_stdone();
            return;
            
        case I2C_ST_MODE_RSDONE:
            i2c_handle_rsdone();
            return;

        case I2C_ST_MODE_MTADPA:
            i2c_handle_mtadpa();
            return;
            
        case I2C_ST_MODE_MTDAPA:
            i2c_handle_mtdapa();
            return;
            
        case I2C_ST_MODE_MRADPA:
            i2c_handle_mradpa();
            return;
            
        case I2C_ST_MODE_MRDAPA:
            i2c_handle_mrdapa();
            return;
            
        case I2C_ST_MODE_MRDANA:
            i2c_handle_mrdana();
            return;
            
        case I2C_ST_MODE_MTADNA:  // NACK на адрес при записи
        case I2C_ST_MODE_MRADNA:  // NACK на адрес при чтении
            i2c_handle_addr_nack();
            return;
            
        case I2C_ST_MODE_MTDANA:  // NACK на данные при записи
            i2c_handle_data_nack();
            return;
            
        case I2C_ST_MODE_IDLE:  // Шина в состоянии IDLE (после STOP)
            I2C->CTL0_bit.CLRST = 1;
            return;
            
        default:
            I2C->CTL0_bit.CLRST = 1;
            return;
    }
}

// Запись данных в EEPROM (асинхронно, одна страница за вызов)
void eeprom_write_bytes(uint16_t addr, const uint8_t* data, size_t len)
{
    if (!data || !len) {
        return;
    }

    // Не поддерживаем конкурентные транзакции: если кто-то уже пишет/читает — выходим.
    if (transaction.busy) {
        return;
    }

    // Разбивка на страницу: драйвер гарантированно отправляет не больше одной страницы за раз.
    size_t page_off = addr % EEPROM_PAGE_SIZE;
    size_t chunk = EEPROM_PAGE_SIZE - page_off;
    if (chunk > len) chunk = len;

    // Инициализация транзакции
    transaction.state     = EEPROM_STATE_WRITE_ADDR_H;
    transaction.addr      = addr;
    transaction.write_data= data;
    transaction.read_data = NULL;
    transaction.len       = chunk;
    transaction.idx       = 0;
    transaction.write_op  = true;
    transaction.error     = false;
    transaction.busy      = true;

    // Старт: всё остальное сделает I2C_IRQHandler
    I2C->CTL0_bit.START = 1;
}

// Чтение данных из EEPROM (асинхронно)
void eeprom_read_bytes(uint16_t addr, uint8_t* data, size_t len)
{
    if (!data || !len) {
        return;
    }

    if (transaction.busy) {
        return;
    }

    // Инициализация транзакции
    transaction.state     = EEPROM_STATE_READ_ADDR_H;
    transaction.addr      = addr;
    transaction.write_data= NULL;
    transaction.read_data = data;
    transaction.len       = len;
    transaction.idx       = 0;
    transaction.write_op  = false;
    transaction.error     = false;
    transaction.busy      = true;

    // Генерируем START — дальше работает I2C_IRQHandler
    I2C->CTL0_bit.START = 1;
}

// Инициализация EEPROM
void eeprom_init(void)
{
    // Инициализируем аппаратный I2C контроллер
    I2C_Master_Init();
    
    // Инициализируем состояние транзакции
    transaction.state = EEPROM_STATE_IDLE;
    transaction.busy = false;
    transaction.error = false;
}
#endif