
//-- Includes ------------------------------------------------------------------
#include "device/Include/K1921VG015.h"
#include <stdint.h>
#include "device/Include/system_k1921vg015.h"

//-- Defines -------------------------------------------------------------------
#define UART1_BAUD  9600

#define UBUFF_SIZE 128

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
    // 8E1: 8 data bits, Even parity, 1 stop bit, FIFO enabled
    UART1->LCRH = UART_LCRH_FEN_Msk | (3 << UART_LCRH_WLEN_Pos) | UART_LCRH_PEN_Msk | UART_LCRH_EPS_Msk;
    UART1->IFLS = 0;
    UART1->CR = UART_CR_TXE_Msk | UART_CR_RXE_Msk | UART_CR_UARTEN_Msk;
}



//-- Peripheral init functions -------------------------------------------------
void periph_init()
{
  SystemInit();
  SystemCoreClockUpdate();
  UART1_init();
}

//--- USER FUNCTIONS ----------------------------------------------------------------------


//-- Main ----------------------------------------------------------------------
int main(void)
{
  periph_init();
  while(1)
  {
    // TODO: OSDP обработчик будет добавлен здесь
  }

  return 0;
}