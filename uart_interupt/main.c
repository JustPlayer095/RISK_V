//-- Includes ------------------------------------------------------------------
#include "device/Include/K1921VG015.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "device/Include/system_k1921vg015.h"
#include "device/Include/retarget.h"
#include "device/Include/plic.h"
#include "plib/inc/plib015_gpio.h"
#include "plib/inc/plib015_tmr32.h"
#include "osdp/osdp_min.h"

//-- Defines -------------------------------------------------------------------
#define UART1_BAUD  115200

static uint8_t prev_btn_state = 1; // с PU отпущенная кнопка = 1
static volatile uint8_t led_state = 0; // состояние светодиода (0=выкл,1=вкл)
static uint8_t pressed_lock = 0; // блок повторного срабатывания, пока кнопка не отпущена стабильно
static void uart1_irq_handler(void);
static void uart_irq_init(void);
static void gpio_init(void);
static void tmr32_init(void);
static void tmr32_irq_handler(void);
 

void UART1_init()
{
    uint32_t baud_icoef = HSECLK_VAL / (16 * UART1_BAUD);
    uint32_t baud_fcoef = ((HSECLK_VAL / (16.0f * UART1_BAUD) - baud_icoef) * 64 + 0.5f);
    // Настраиваем GPIO
    RCU->CGCFGAHB_bit.GPIOAEN = 1;
    RCU->RSTDISAHB_bit.GPIOAEN = 1;
    RCU->CGCFGAPB_bit.UART1EN = 1;
    RCU->RSTDISAPB_bit.UART1EN = 1;

    GPIOA->ALTFUNCNUM_bit.PIN2 = 1;
    GPIOA->ALTFUNCNUM_bit.PIN3 = 1;
    GPIOA->ALTFUNCSET = GPIO_ALTFUNCSET_PIN2_Msk | GPIO_ALTFUNCSET_PIN3_Msk;

    // Настраиваем UART1
    RCU->UARTCLKCFG[1].UARTCLKCFG_bit.CLKSEL = RCU_UARTCLKCFG_CLKSEL_HSE;
    RCU->UARTCLKCFG[1].UARTCLKCFG_bit.DIVEN = 0;
    RCU->UARTCLKCFG[1].UARTCLKCFG_bit.RSTDIS = 1;
    RCU->UARTCLKCFG[1].UARTCLKCFG_bit.CLKEN = 1;

    UART1->IBRD = baud_icoef;
    UART1->FBRD = baud_fcoef;
    UART1->LCRH = UART_LCRH_FEN_Msk | (3 << UART_LCRH_WLEN_Pos);
    UART1->IFLS = 0;
    UART1->CR = UART_CR_TXE_Msk | UART_CR_RXE_Msk | UART_CR_UARTEN_Msk;
}



//-- Peripheral init functions -------------------------------------------------
void periph_init()
{
  SystemInit();
  SystemCoreClockUpdate();
  retarget_init();
  UART1_init();
  // Инициализация OSDP с адресом PD = 0x01
  osdp_init(0x01);
  gpio_init();
  tmr32_init();
  uart_irq_init();
  InterruptEnable();
  printf("Hello World!\r\n");
}

//--- USER FUNCTIONS ----------------------------------------------------------------------

// Инициализация прерываний UART1 и регистрация обработчика в PLIC
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

  // Зарегистрировать обработчик в PLIC и включить линию прерываний UART1
  PLIC_SetPriority(PLIC_UART1_VECTNUM, 1);
  PLIC_SetIrqHandler(Plic_Mach_Target, PLIC_UART1_VECTNUM, uart1_irq_handler);
  PLIC_IntEnable(Plic_Mach_Target, PLIC_UART1_VECTNUM);
}

// Обработчик прерывания UART1 (через PLIC)
static void uart1_irq_handler(void)
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
  // Тактирование GPIOA уже включено в UART1_init()

  GPIO_Init_TypeDef gpio;

  // LED на PA0
  GPIO_StructInit(&gpio);
  gpio.Pin = GPIO_Pin_0;
  gpio.Out = ENABLE;                // включить выход
  gpio.AltFunc = DISABLE;           // обычный GPIO
  gpio.AltFuncNum = GPIO_AltFuncNum_None;
  gpio.OutMode = GPIO_OutMode_PP;   // push-pull
  gpio.InMode = GPIO_InMode_Schmitt;
  gpio.PullMode = GPIO_PullMode_Disable;
  GPIO_Init(GPIOA, &gpio);
  GPIO_ClearBits(GPIOA, GPIO_Pin_0); // LED погашен

  // Кнопка на PA1 (вход, внутренняя подтяжка к питанию)
  GPIO_StructInit(&gpio);
  gpio.Pin = GPIO_Pin_1;
  gpio.Out = DISABLE;               // вход
  gpio.AltFunc = DISABLE;
  gpio.AltFuncNum = GPIO_AltFuncNum_None;
  gpio.InMode = GPIO_InMode_Schmitt;
  gpio.PullMode = GPIO_PullMode_PU; // подтяжка к VDD
  GPIO_Init(GPIOA, &gpio);

  // Аппаратная фильтрация дребезга: пересинхронизация + квалификатор входа
  GPIO_SyncCmd(GPIOA, GPIO_Pin_1, ENABLE);
  GPIO_QualSampleConfig(GPIOA, 1000); // период дискретизации фильтра (такты HCLK)
  GPIO_QualModeConfig(GPIOA, GPIO_Pin_1, GPIO_QualMode_6Sample); // более жесткая фильтрация
  GPIO_QualCmd(GPIOA, GPIO_Pin_1, ENABLE);
}

// Таймер 1 мс на TMR32: 
static volatile uint32_t ms_ticks = 0;

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

    // Фиксация: переключаем состояние LED по каждому нажатию (сигнал 1->0)
    uint8_t btn_now = GPIO_ReadBit(GPIOA, GPIO_Pin_1) ? 1 : 0;
    // Неблокирующий антидребезг на 1 мс тикере
    static uint32_t last_event_ms = 0;
    const uint32_t debounce_ms = 100;

    if (!pressed_lock) {
      if (prev_btn_state == 1 && btn_now == 0) {
        uint32_t now = ms_ticks;
        if ((now - last_event_ms) >= debounce_ms) {
          led_state ^= 1u; // переключение состояния светодиода
          if (led_state) {
            GPIO_SetBits(GPIOA, GPIO_Pin_0);
            printf("LED ON\r\n");
          } else {
            GPIO_ClearBits(GPIOA, GPIO_Pin_0);
            printf("LED OFF\r\n");
          }
          pressed_lock = 1; // гарантирует всего одно выполнение
          last_event_ms = now;
        }
      }
    } else {
      if (prev_btn_state == 0 && btn_now == 1) {
        uint32_t now = ms_ticks;
        if ((now - last_event_ms) >= debounce_ms) {
          pressed_lock = 0;
          last_event_ms = now;
        }
      }
    }
    prev_btn_state = btn_now;

  }

  return 0;
}
