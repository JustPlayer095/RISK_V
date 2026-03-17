//-- Includes ------------------------------------------------------------------
#include "device/include/K1921VG015.h"
#include <stdint.h>
#include <stdbool.h>
#include "device/include/system_k1921vg015.h"
#include "device/include/retarget.h"
#include "device/include/plic.h"
#include "plib/inc/plib015_gpio.h"
#include "plib/inc/plib015_tmr32.h"
#include "modules/gpio/gpio_helpers.h"
#include "modules/calc/calc.h"

//-- Defines -------------------------------------------------------------------
#define UART4_BAUD  115200

// Время последнего события для антидребезга кнопок
static volatile uint32_t g_btn_last_ms[KEY_COUNT] = {0};

// Предыдущее состояние кнопок (1=отпущена, 0=нажата)
static volatile uint8_t g_btn_last_state[KEY_COUNT] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

static const uint32_t g_debounce_ms = 50; // время антидребезга в мс
// Таймер 1 мс на TMR32: 
volatile uint32_t ms_ticks = 0;

// Таблица соответствия физических кнопок и логических действий калькулятора
static GPIO_TypeDef* const g_btn_ports[KEY_COUNT] = {
  GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB,
  GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB
};
static const uint32_t g_btn_pins[KEY_COUNT] = {
  GPIO_Pin_0,  GPIO_Pin_1,  GPIO_Pin_2,  GPIO_Pin_3,
  GPIO_Pin_4,  GPIO_Pin_5,  GPIO_Pin_6,  GPIO_Pin_7,
  GPIO_Pin_8,  GPIO_Pin_9,  GPIO_Pin_10, GPIO_Pin_11,
  GPIO_Pin_12, GPIO_Pin_13, GPIO_Pin_14, GPIO_Pin_15
};
static const key_id_t g_btn_keys[KEY_COUNT] = {
  KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7,
  KEY_8, KEY_9, KEY_PLUS, KEY_MINUS, KEY_MUL, KEY_DIV, KEY_DEL, KEY_EQ
};

// Универсальные функции для инициализации GPIO
static void gpio_irq_handler(void);
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
  gpio_init();
  tmr32_init();
  gpio_irq_init();
  InterruptEnable();
}

//--- USER FUNCTIONS ----------------------------------------------------------------------

static void gpio_init(void)
{
  // Кнопки все на GPIOB
  RCU->CGCFGAHB_bit.GPIOBEN = 1;
  RCU->RSTDISAHB_bit.GPIOBEN = 1;
}

// Инициализация прерываний GPIO для кнопок
static void gpio_irq_init(void)
{
  // Кнопки калькулятора: 10 цифр + 4 операции + Del + "="
  for (uint8_t i = 0; i < KEY_COUNT; i++) {
    gpio_init_input_irq(g_btn_ports[i], g_btn_pins[i], GPIO_PullMode_PU, &g_btn_last_state[i]);
  }
  
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

  for (uint8_t i = 0; i < KEY_COUNT; i++) {
    if (GPIO_ITStatus(g_btn_ports[i], g_btn_pins[i]) == SET) {
      uint8_t btn_current = GPIO_ReadBit(g_btn_ports[i], g_btn_pins[i]) ? 1 : 0;

      if (g_btn_last_state[i] == 1 && btn_current == 0) {
        // Кнопка нажата (переход из 1 в 0)
        if ((now - g_btn_last_ms[i]) >= g_debounce_ms) {
          // Сразу передаём событие в модуль калькулятора
          on_key_pressed(g_btn_keys[i]);

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
      
      GPIO_ITStatusClear(g_btn_ports[i], g_btn_pins[i]);
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
  TMR32_ITCmd(TMR32_IT_CAPCOM_0, ENABLE);

  // Регистрация обработчика в PLIC
  PLIC_SetPriority(PLIC_TMR32_VECTNUM, 1);
  PLIC_SetIrqHandler(Plic_Mach_Target, PLIC_TMR32_VECTNUM, tmr32_irq_handler);
  PLIC_IntEnable(Plic_Mach_Target, PLIC_TMR32_VECTNUM);
}

static void tmr32_irq_handler(void)
{
  // Инкремент тиков, сброс флага
  ms_ticks++;
  TMR32_ITClear(TMR32_IT_CAPCOM_0);
}



//-- Main ----------------------------------------------------------------------
int main(void)
{
  periph_init();
  while(1)
  {
    __asm volatile("wfi");
  }
  return 0;
}
