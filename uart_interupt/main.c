//-- Includes ------------------------------------------------------------------
#include "device/Include/K1921VG015.h"
#include <stdint.h>
#include <stdio.h>
#include "device/Include/system_k1921vg015.h"
#include "device/Include/retarget.h"
#include "device/Include/plic.h"
#include "plib/inc/plib015_gpio.h"
#include "plib/inc/plib015_tmr32.h"
#include "osdp/osdp_min.h"

//-- Defines -------------------------------------------------------------------
#define UART4_BAUD  115200

// Состояния LED на PA12-PA15
static volatile uint8_t led_state[4] = {0, 0, 0, 0}; // состояние светодиодов PA12-PA15 (0=выкл,1=вкл)

// Время последнего события для антидребезга кнопок PA0-PA3
static volatile uint32_t g_btn_last_ms[4] = {0, 0, 0, 0};

// Предыдущее состояние кнопок PA0-PA3 (1=отпущена, 0=нажата)
static volatile uint8_t g_btn_last_state[4] = {1, 1, 1, 1};

static const uint32_t g_debounce_ms = 50; // время антидребезга в мс
// Таймер 1 мс на TMR32: 
static volatile uint32_t ms_ticks = 0;

// Универсальные функции для инициализации GPIO
static void gpio_reset_pin(GPIO_TypeDef* GPIOx, uint32_t Pin);
static void gpio_init_output(GPIO_TypeDef* GPIOx, uint32_t Pin, uint8_t initial_state);
static void gpio_init_input(GPIO_TypeDef* GPIOx, uint32_t Pin, GPIO_PullMode_TypeDef pull_mode);
static void gpio_init_input_irq(GPIO_TypeDef* GPIOx, uint32_t Pin, GPIO_PullMode_TypeDef pull_mode, volatile uint8_t* last_state);

static void uart4_irq_handler(void);
static void gpio_irq_handler(void);
static void uart_irq_init(void);
static void gpio_init(void);
static void gpio_irq_init(void);
static void tmr32_init(void);
static void tmr32_irq_handler(void);
 

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
  UART4_init();
  // Инициализация OSDP: 0xFF означает читать адрес из Flash памяти
  osdp_init(0xFF);
  gpio_init();
  tmr32_init();
  uart_irq_init();
  gpio_irq_init();
  InterruptEnable();
  printf("Hello World!\r\n");
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

static void gpio_reset_pin(GPIO_TypeDef* GPIOx, uint32_t Pin)
{
  // 1. Отключить выход (OUTENCLR)
  GPIO_OutCmd(GPIOx, Pin, DISABLE);
  // 2. Очистить данные выхода (DATAOUTCLR)
  GPIO_ClearBits(GPIOx, Pin);
  // 3. Отключить альтернативную функцию (ALTFUNCCLR)
  GPIO_AltFuncCmd(GPIOx, Pin, DISABLE); 
  // 4. Сбросить номер альтернативной функции (ALTFUNCNUM = 0)
  GPIO_AltFuncNumConfig(GPIOx, Pin, GPIO_AltFuncNum_None);
}

static void gpio_init_output(GPIO_TypeDef* GPIOx, uint32_t Pin, uint8_t initial_state)
{
  // Сначала сбрасываем состояние пина
  gpio_reset_pin(GPIOx, Pin);
  
  GPIO_Init_TypeDef gpio;
  GPIO_StructInit(&gpio);
  gpio.Pin = Pin;
  gpio.Out = ENABLE;                // включить выход
  gpio.AltFunc = DISABLE;           // обычный GPIO
  gpio.AltFuncNum = GPIO_AltFuncNum_None;
  gpio.OutMode = GPIO_OutMode_PP;   // push-pull
  gpio.PullMode = GPIO_PullMode_Disable;
  GPIO_Init(GPIOx, &gpio);
  
  // Устанавливаем начальное состояние
  if (initial_state) {
    GPIO_SetBits(GPIOx, Pin);
  } else {
    GPIO_ClearBits(GPIOx, Pin);
  }
}

static uint8_t gpio_qual_initialized = 0; // битовая маска: 0=GPIOA, 1=GPIOB, 2=GPIOC


static void gpio_init_input(GPIO_TypeDef* GPIOx, uint32_t Pin, GPIO_PullMode_TypeDef pull_mode)
{
  // Сначала сбрасываем состояние пина
  gpio_reset_pin(GPIOx, Pin);
  
  GPIO_Init_TypeDef gpio;
  GPIO_StructInit(&gpio);
  gpio.Pin = Pin;
  gpio.Out = DISABLE;               // вход
  gpio.AltFunc = DISABLE;
  gpio.AltFuncNum = GPIO_AltFuncNum_None;
  gpio.InMode = GPIO_InMode_Schmitt;
  gpio.PullMode = pull_mode;
  GPIO_Init(GPIOx, &gpio);

  // Аппаратная фильтрация дребезга: пересинхронизация + квалификатор входа
  GPIO_SyncCmd(GPIOx, Pin, ENABLE);
  uint8_t port_mask = 0;
  if (GPIOx == GPIOA) port_mask = 0x01;
  
  if (!(gpio_qual_initialized & port_mask)) {
    GPIO_QualSampleConfig(GPIOx, 1000); // период дискретизации фильтра (такты HCLK)
    gpio_qual_initialized |= port_mask;
  }
  
  GPIO_QualModeConfig(GPIOx, Pin, GPIO_QualMode_6Sample); // более жесткая фильтрация
  GPIO_QualCmd(GPIOx, Pin, ENABLE);
}

static void gpio_init_input_irq(GPIO_TypeDef* GPIOx, uint32_t Pin, GPIO_PullMode_TypeDef pull_mode, volatile uint8_t* last_state)
{
  // Сначала инициализируем как обычный вход
  gpio_init_input(GPIOx, Pin, pull_mode);
  
  // Настраиваем прерывания
  GPIO_ITTypeConfig(GPIOx, Pin, GPIO_IntType_Edge);
  GPIO_ITPolConfig(GPIOx, Pin, GPIO_IntPol_Negative);
  GPIO_ITEdgeConfig(GPIOx, Pin, GPIO_IntEdge_Any); // прерывание по обоим фронтам
  GPIO_ITStatusClear(GPIOx, Pin);
  
  // Инициализируем предыдущее состояние кнопки (отпущена = 1)
  if (last_state != NULL) {
    *last_state = GPIO_ReadBit(GPIOx, Pin) ? 1 : 0;
  }
  
  // Включить прерывание
  GPIO_ITCmd(GPIOx, Pin, ENABLE);
}

static void gpio_init(void)
{
  // Включить тактирование GPIOA
  RCU->CGCFGAHB_bit.GPIOAEN = 1;
  RCU->RSTDISAHB_bit.GPIOAEN = 1;

  // LED на PA12-PA15 (начальное состояние: все выключены)
  gpio_init_output(GPIOA, GPIO_Pin_12, 0);
  gpio_init_output(GPIOA, GPIO_Pin_13, 0);
  gpio_init_output(GPIOA, GPIO_Pin_14, 0);
  gpio_init_output(GPIOA, GPIO_Pin_15, 0);

  // Кнопки будут инициализированы в gpio_irq_init() через gpio_init_input_irq()
}

// Инициализация прерываний GPIO для кнопок
static void gpio_irq_init(void)
{
  // Кнопки PA0-PA3 с прерываниями (PA8 и PA9 используются для UART4)
  gpio_init_input_irq(GPIOA, GPIO_Pin_0, GPIO_PullMode_PU, &g_btn_last_state[0]);
  gpio_init_input_irq(GPIOA, GPIO_Pin_1, GPIO_PullMode_PU, &g_btn_last_state[1]);
  gpio_init_input_irq(GPIOA, GPIO_Pin_2, GPIO_PullMode_PU, &g_btn_last_state[2]);
  gpio_init_input_irq(GPIOA, GPIO_Pin_3, GPIO_PullMode_PU, &g_btn_last_state[3]);
  
  // Зарегистрировать обработчик в PLIC и включить линию прерываний GPIO
  PLIC_SetPriority(PLIC_GPIO_VECTNUM, 1);
  PLIC_SetIrqHandler(Plic_Mach_Target, PLIC_GPIO_VECTNUM, gpio_irq_handler);
  PLIC_IntEnable(Plic_Mach_Target, PLIC_GPIO_VECTNUM);
}

// Обработчик прерывания GPIO (кнопки)
static void gpio_irq_handler(void)
{
  uint32_t now = ms_ticks;
  
  const uint32_t btn_pins[4] = {GPIO_Pin_0, GPIO_Pin_1, GPIO_Pin_2, GPIO_Pin_3};
  const uint32_t led_pins[4] = {GPIO_Pin_12, GPIO_Pin_13, GPIO_Pin_14, GPIO_Pin_15};
  
  // Обработка всех 4 кнопок PA0-PA3
  for (uint8_t i = 0; i < 4; i++) {
    if (GPIO_ITStatus(GPIOA, btn_pins[i]) == SET) {
      uint8_t btn_current = GPIO_ReadBit(GPIOA, btn_pins[i]) ? 1 : 0;
      
      if (g_btn_last_state[i] == 1 && btn_current == 0) {
        // Кнопка нажата (переход из 1 в 0)
        if ((now - g_btn_last_ms[i]) >= g_debounce_ms) {
          // Переключаем состояние соответствующего LED (PA12-PA15)
          led_state[i] ^= 1u;
          if (led_state[i]) {
            GPIO_SetBits(GPIOA, led_pins[i]);
            printf("LED%d ON (PA%d)\r\n", i+1, 12+i);
          } else {
            GPIO_ClearBits(GPIOA, led_pins[i]);
            printf("LED%d OFF (PA%d)\r\n", i+1, 12+i);
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
  }

  return 0;
}
