//-- Includes ------------------------------------------------------------------
#include "device/include/K1921VG015.h"
#include <stdint.h>
#include "device/include/system_k1921vg015.h"
#include "device/include/retarget.h"
#include "modules/calc/calc.h"

//-- Defines -------------------------------------------------------------------
#define UART4_BAUD  115200

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
  calc_init();
  InterruptEnable();
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
