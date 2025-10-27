
//-- Includes ------------------------------------------------------------------
#include "device/Include/K1921VG015.h"
#include <stdint.h>
#include <stdio.h>
#include "device/Include/system_k1921vg015.h"
#include "device/Include/retarget.h"
#include "device/Include/plic.h"

//-- Defines -------------------------------------------------------------------
#define UART1_BAUD  115200

#define UBUFF_SIZE 128

// Буфер и флаги для приёма через прерывания
static volatile char rx_line_buf[UBUFF_SIZE];
static volatile uint32_t rx_idx = 0;
static volatile uint8_t rx_line_ready = 0;

static void uart1_irq_handler(void);
static void uart_irq_init(void);

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
  uart_irq_init();
  InterruptEnable();
  printf("Hello World!");
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
  // Если строка ещё не обработана, накапливаем; иначе просто дренируем FIFO
  while (!RETARGET_UART->FR_bit.RXFE) {
    char ch = (char)RETARGET_UART->DR_bit.DATA;
    if (!rx_line_ready) {
      if (ch == '\r' || ch == '\n') {
        if (rx_idx > 0) {
          rx_line_ready = 1; // главная петля обработает содержимое
        }
      } else {
        if (rx_idx < (UBUFF_SIZE - 1)) {
          rx_line_buf[rx_idx++] = ch;
        }
      }
    }
  }

  // Очистить флаги источников прерываний
  RETARGET_UART->ICR = UART_ICR_RXIC_Msk |
                       UART_ICR_RTIC_Msk |
                       UART_ICR_OEIC_Msk |
                       UART_ICR_FEIC_Msk |
                       UART_ICR_PEIC_Msk |
                       UART_ICR_BEIC_Msk;
}


//-- Main ----------------------------------------------------------------------
int main(void)
{
  periph_init();
  while(1)
  {
    if (rx_line_ready) {
      for (uint32_t i = 0; i < rx_idx; i++) {
        retarget_put_char(rx_line_buf[i]);
      }
      retarget_put_char('\r');
      retarget_put_char('\n');
      rx_idx = 0;
      rx_line_ready = 0;
    }
  }

  return 0;
}