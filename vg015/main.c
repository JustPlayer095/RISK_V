//-- Includes ------------------------------------------------------------------
#include "device/Include/K1921VG015.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "device/Include/system_k1921vg015.h"
#include "device/Include/retarget.h"
#include "device/Include/plic.h"
#include "plib/inc/plib015_gpio.h"
#include "plib/inc/plib015_tmr32.h"
#include "modules/osdp/osdp.h"
#include "modules/adcsar/adcsar.h"
#include "modules/driver/eeprom.h"
#include "modules/config/config.h"
#include "modules/gpio/gpio_helpers.h"

//-- Defines -------------------------------------------------------------------
#define UART4_BAUD  115200


// Состояния LED на PA4-PA7
static volatile uint8_t led_state[4] = {0, 0, 0, 0}; // состояние светодиодов PA4-PA7 (0=выкл,1=вкл)

// Время последнего события для антидребезга кнопок PA0-PA3
static volatile uint32_t g_btn_last_ms[4] = {0, 0, 0, 0};

// Предыдущее состояние кнопок PA0-PA3 (1=отпущена, 0=нажата)
static volatile uint8_t g_btn_last_state[4] = {1, 1, 1, 1};

static const uint32_t g_debounce_ms = 50; // время антидребезга в мс
// Таймер 1 мс на TMR32: 
volatile uint32_t ms_ticks = 0;
static char g_adc_last_state = 'U';
static char g_adc_candidate_state = 'U';
static uint32_t g_adc_candidate_ms = 0;
static const uint32_t g_adc_state_confirm_ms = 50; // задержка подтверждения смены состояния
static config_storage_t g_cfg;
static volatile bool g_spi_status_req = false; // запрос на SPI чтение статуса
static volatile bool g_spi_loopback_req = false; // запрос на SPI loopback (MOSI<->MISO)

// Универсальные функции для инициализации GPIO
static void uart4_irq_handler(void);
static void gpio_irq_handler(void);
static void uart_irq_init(void);
static void gpio_init(void);
static void gpio_irq_init(void);
static void tmr32_init(void);
static void tmr32_irq_handler(void);
static void cfg_store_and_report(void);

void UART4_init()
{
    uint32_t baud_icoef = HSECLK_VAL / (16 * UART4_BAUD);
    uint32_t baud_fcoef = ((HSECLK_VAL / (16.0f * UART4_BAUD) - baud_icoef) * 64 + 0.5f);
    // Настраиваем GPIO
    RCU->CGCFGAHB_bit.GPIOAEN = 1;
    RCU->RSTDISAHB_bit.GPIOAEN = 1;
    RCU->CGCFGAPB_bit.UART4EN = 1;
    RCU->RSTDISAPB_bit.UART4EN = 1;

    // UART4 на пинах PA8 (RX) и PA9 (TX)
    GPIOA->ALTFUNCNUM_bit.PIN8 = 1;
    GPIOA->ALTFUNCNUM_bit.PIN9 = 1;
    GPIOA->ALTFUNCSET = GPIO_ALTFUNCSET_PIN8_Msk | GPIO_ALTFUNCSET_PIN9_Msk;

    // Настраиваем UART4
    RCU->UARTCLKCFG[4].UARTCLKCFG_bit.CLKSEL = RCU_UARTCLKCFG_CLKSEL_HSE;
    RCU->UARTCLKCFG[4].UARTCLKCFG_bit.DIVEN = 0;
    RCU->UARTCLKCFG[4].UARTCLKCFG_bit.RSTDIS = 1;
    RCU->UARTCLKCFG[4].UARTCLKCFG_bit.CLKEN = 1;

    UART4->IBRD = baud_icoef;
    UART4->FBRD = baud_fcoef;
    UART4->LCRH = UART_LCRH_FEN_Msk | (3 << UART_LCRH_WLEN_Pos);
    UART4->IFLS = 0;
    UART4->CR = UART_CR_TXE_Msk | UART_CR_RXE_Msk | UART_CR_UARTEN_Msk;
}



//-- Peripheral init functions -------------------------------------------------
void periph_init()
{
  SystemInit();
  SystemCoreClockUpdate();
  retarget_init();
  adcsar_init();
  eeprom_init();
  gpio_init();
  tmr32_init();
  uart_irq_init();
  gpio_irq_init();
  InterruptEnable();
  osdp_init();
  cfg_store_and_report();
  adcsar_start();
}

static void cfg_store_and_report(void)
{
  // Пытаемся загрузить конфиг; если невалиден — пишем дефолтный и перезагружаем
  //if (!config_storage_load(&g_cfg)) {
  printf("CFG invalid, writing defaults\r\n");
  config_storage_default(&g_cfg);
  config_storage_save(&g_cfg);
  //}

  printf("CFG active: addr=%u baud=%u\r\n", g_cfg.osdp_addr, (unsigned)g_cfg.osdp_baud);
}

//--- USER FUNCTIONS ----------------------------------------------------------------------

// Инициализация прерываний UART4 и регистрация обработчика в PLIC
static void uart_irq_init(void)
{
  // Очистить возможные висящие флаги прерываний
  RETARGET_UART->ICR = UART_ICR_RXIC_Msk |
                       UART_ICR_RTIC_Msk |
                       UART_ICR_OEIC_Msk |
                       UART_ICR_FEIC_Msk |
                       UART_ICR_PEIC_Msk |
                       UART_ICR_BEIC_Msk;

  // Включить источники прерываний по приёму и таймауту
  RETARGET_UART->IMSC |= (UART_IMSC_RXIM_Msk |
                          UART_IMSC_RTIM_Msk |
                          UART_IMSC_OERIM_Msk |
                          UART_IMSC_FERIM_Msk |
                          UART_IMSC_PERIM_Msk |
                          UART_IMSC_BERIM_Msk);

  // Зарегистрировать обработчик в PLIC и включить линию прерываний UART4
  PLIC_SetPriority(PLIC_UART4_VECTNUM, 1);
  PLIC_SetIrqHandler(Plic_Mach_Target, PLIC_UART4_VECTNUM, uart4_irq_handler);
  PLIC_IntEnable(Plic_Mach_Target, PLIC_UART4_VECTNUM);
}

// Обработчик прерывания UART4 (через PLIC)
static void uart4_irq_handler(void)
{
  while (!RETARGET_UART->FR_bit.RXFE) {
    uint8_t ch = (uint8_t)RETARGET_UART->DR_bit.DATA;
    osdp_on_rx_byte(ch);
  }

  // Очистить флаги источников прерываний
  RETARGET_UART->ICR = UART_ICR_RXIC_Msk |
                       UART_ICR_RTIC_Msk |
                       UART_ICR_OEIC_Msk |
                       UART_ICR_FEIC_Msk |
                       UART_ICR_PEIC_Msk |
                       UART_ICR_BEIC_Msk;
}


static void gpio_init(void)
{
  // Включить тактирование GPIOA
  RCU->CGCFGAHB_bit.GPIOAEN = 1;
  RCU->RSTDISAHB_bit.GPIOAEN = 1;

  // LED на PA4-PA7 (начальное состояние: все выключены)
  gpio_init_output(GPIOA, GPIO_Pin_4, 0);
  gpio_init_output(GPIOA, GPIO_Pin_5, 0);
  gpio_init_output(GPIOA, GPIO_Pin_6, 0);
  gpio_init_output(GPIOA, GPIO_Pin_7, 0);
}

// Инициализация прерываний GPIO для кнопок
static void gpio_irq_init(void)
{
  // Кнопки PA0-PA3
  gpio_init_input_irq(GPIOA, GPIO_Pin_0, GPIO_PullMode_PU, &g_btn_last_state[0]);
  gpio_init_input_irq(GPIOA, GPIO_Pin_1, GPIO_PullMode_PU, &g_btn_last_state[1]);
  gpio_init_input_irq(GPIOA, GPIO_Pin_2, GPIO_PullMode_PU, &g_btn_last_state[2]);
  gpio_init_input_irq(GPIOA, GPIO_Pin_3, GPIO_PullMode_PU, &g_btn_last_state[3]);
  
  // Зарегистрировать обработчик в PLIC и включить линию прерываний GPIO
  PLIC_SetPriority(PLIC_GPIO_VECTNUM, 1);
  PLIC_SetIrqHandler(Plic_Mach_Target, PLIC_GPIO_VECTNUM, gpio_irq_handler);
  PLIC_IntEnable(Plic_Mach_Target, PLIC_GPIO_VECTNUM);
}

// Простой самотест EEPROM: запись и чтение известного шаблона


// Обработчик прерывания GPIO (кнопки)
static void gpio_irq_handler(void)
{
  uint32_t now = ms_ticks;
  
  const uint32_t btn_pins[4] = {GPIO_Pin_0, GPIO_Pin_1, GPIO_Pin_2, GPIO_Pin_3};
  const uint32_t led_pins[4] = {GPIO_Pin_4, GPIO_Pin_5, GPIO_Pin_6, GPIO_Pin_7};
  
  // Обработка всех 4 кнопок PA0-PA3
  for (uint8_t i = 0; i < 4; i++) {
    if (GPIO_ITStatus(GPIOA, btn_pins[i]) == SET) {
      uint8_t btn_current = GPIO_ReadBit(GPIOA, btn_pins[i]) ? 1 : 0;
      
      if (g_btn_last_state[i] == 1 && btn_current == 0) {
        // Кнопка нажата (переход из 1 в 0)
        if ((now - g_btn_last_ms[i]) >= g_debounce_ms) {
          // Переключаем состояние соответствующего LED (PA4-PA7)
          led_state[i] ^= 1u;
          if (led_state[i]) {
            GPIO_SetBits(GPIOA, led_pins[i]);
            printf("LED%d ON (PA%d)\r\n", i+1, i+4);
          } else {
            GPIO_ClearBits(GPIOA, led_pins[i]);
            printf("LED%d OFF (PA%d)\r\n", i+1, i+4);
          }
          g_btn_last_ms[i] = now;
          g_btn_last_state[i] = btn_current;
        }
      } else if (g_btn_last_state[i] == 0 && btn_current == 1) {
        // Кнопка отпущена (переход из 0 в 1)
        if ((now - g_btn_last_ms[i]) >= g_debounce_ms) {
          g_btn_last_state[i] = btn_current;
          g_btn_last_ms[i] = now;
        }
      }
      
      GPIO_ITStatusClear(GPIOA, btn_pins[i]);
    }
  }
}

static void tmr32_init(void) 
{
  // Тактирование TMR32
  RCU->CGCFGAPB_bit.TMR32EN = 1;
  RCU->RSTDISAPB_bit.TMR32EN = 1;

  // Настройка таймера: SYSCLK / 8, режим Up до CAPCOM0, период ~1 мс
  TMR32_SetClksel(TMR32_Clksel_SysClk);
  TMR32_SetDivider(TMR32_Div_8);
  TMR32_SetMode(TMR32_Mode_Capcom_Up);
  uint32_t cmp = (SystemCoreClock / 8u) / 1000u; // 1 мс
  if (cmp == 0) cmp = 1;
  TMR32_CAPCOM_SetComparator(TMR32_CAPCOM_0, cmp);
  TMR32_SetCounter(0);

  // Прерывание по переполнению таймера (обновление)
  TMR32_ITCmd(TMR32_IT_TimerUpdate, ENABLE);

  // Регистрация обработчика в PLIC
  PLIC_SetPriority(PLIC_TMR32_VECTNUM, 1);
  PLIC_SetIrqHandler(Plic_Mach_Target, PLIC_TMR32_VECTNUM, tmr32_irq_handler);
  PLIC_IntEnable(Plic_Mach_Target, PLIC_TMR32_VECTNUM);
}

static void tmr32_irq_handler(void)
{
  // Инкремент тиков, сброс флага
  ms_ticks++;
  // 1 мс тик для OSDP (временное управление LED)
  osdp_tick_1ms();
  TMR32_ITClear(TMR32_IT_TimerUpdate);
}




//-- Main ----------------------------------------------------------------------
int main(void)
{
  periph_init();
  
  while(1)
  {
    adcsar_sample_t sample;
    if (adcsar_poll(&sample)) {
      if (sample.state_char != g_adc_last_state) {
        if (g_adc_candidate_state != sample.state_char) {
          g_adc_candidate_state = sample.state_char;
          g_adc_candidate_ms = ms_ticks;
        } else if ((ms_ticks - g_adc_candidate_ms) >= g_adc_state_confirm_ms) {
          printf("ADC state: %c\r\n", sample.state_char);
          g_adc_last_state = sample.state_char;
        }
      } else {
        g_adc_candidate_state = sample.state_char;
        g_adc_candidate_ms = ms_ticks;
      }
    }
    __asm volatile("wfi");
  }

  return 0;
}
