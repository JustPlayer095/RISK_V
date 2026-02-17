/*==============================================================================
 * Перенаправление printf() для К1921ВГ015
 *------------------------------------------------------------------------------
 * НИИЭТ, Александр Дыхно <dykhno@niiet.ru>
 *==============================================================================
 * ДАННОЕ ПРОГРАММНОЕ ОБЕСПЕЧЕНИЕ ПРЕДОСТАВЛЯЕТСЯ «КАК ЕСТЬ», БЕЗ КАКИХ-ЛИБО
 * ГАРАНТИЙ, ЯВНО ВЫРАЖЕННЫХ ИЛИ ПОДРАЗУМЕВАЕМЫХ, ВКЛЮЧАЯ ГАРАНТИИ ТОВАРНОЙ
 * ПРИГОДНОСТИ, СООТВЕТСТВИЯ ПО ЕГО КОНКРЕТНОМУ НАЗНАЧЕНИЮ И ОТСУТСТВИЯ
 * НАРУШЕНИЙ, НО НЕ ОГРАНИЧИВАЯСЬ ИМИ. ДАННОЕ ПРОГРАММНОЕ ОБЕСПЕЧЕНИЕ
 * ПРЕДНАЗНАЧЕНО ДЛЯ ОЗНАКОМИТЕЛЬНЫХ ЦЕЛЕЙ И НАПРАВЛЕНО ТОЛЬКО НА
 * ПРЕДОСТАВЛЕНИЕ ДОПОЛНИТЕЛЬНОЙ ИНФОРМАЦИИ О ПРОДУКТЕ, С ЦЕЛЬЮ СОХРАНИТЬ ВРЕМЯ
 * ПОТРЕБИТЕЛЮ. НИ В КАКОМ СЛУЧАЕ АВТОРЫ ИЛИ ПРАВООБЛАДАТЕЛИ НЕ НЕСУТ
 * ОТВЕТСТВЕННОСТИ ПО КАКИМ-ЛИБО ИСКАМ, ЗА ПРЯМОЙ ИЛИ КОСВЕННЫЙ УЩЕРБ, ИЛИ
 * ПО ИНЫМ ТРЕБОВАНИЯМ, ВОЗНИКШИМ ИЗ-ЗА ИСПОЛЬЗОВАНИЯ ПРОГРАММНОГО ОБЕСПЕЧЕНИЯ
 * ИЛИ ИНЫХ ДЕЙСТВИЙ С ПРОГРАММНЫМ ОБЕСПЕЧЕНИЕМ.
 *
 *                              2023 АО "НИИЭТ"
 *==============================================================================
 */

#include "../Include/retarget.h"
// #include "../Include/system_k1921vg015.h"
#define SystemCoreClock_uart	SystemCoreClock
//-- Functions -----------------------------------------------------------------
void retarget_init()
{
#if defined RETARGET
    uint32_t baud_icoef = HSECLK_VAL / (16 * RETARGET_UART_BAUD);
    uint32_t baud_fcoef = ((HSECLK_VAL / (16.0f * RETARGET_UART_BAUD) - baud_icoef) * 64 + 0.5f);

    // Настраиваем GPIO
    RCU->CGCFGAHB_bit.GPIOAEN = 1;
    RCU->RSTDISAHB_bit.GPIOAEN = 1;
    #if (RETARGET_UART_NUM == 0)
    RCU->CGCFGAPB_bit.UART0EN = 1;
    RCU->RSTDISAPB_bit.UART0EN = 1;
    #elif (RETARGET_UART_NUM == 1)
    RCU->CGCFGAPB_bit.UART1EN = 1;
    RCU->RSTDISAPB_bit.UART1EN = 1;
    #elif (RETARGET_UART_NUM == 2)
    RCU->CGCFGAPB_bit.UART2EN = 1;
    RCU->RSTDISAPB_bit.UART2EN = 1;
    #elif (RETARGET_UART_NUM == 3)
    RCU->CGCFGAPB_bit.UART3EN = 1;
    RCU->RSTDISAPB_bit.UART3EN = 1;
    #elif (RETARGET_UART_NUM == 4)
    RCU->CGCFGAPB_bit.UART4EN = 1;
    RCU->RSTDISAPB_bit.UART4EN = 1;
    #endif

    // Настройка альтернативной функции для пинов UART
    // Используем макросы RETARGET_UART_PIN_TX_POS и RETARGET_UART_PIN_RX_POS
    if (RETARGET_UART_PIN_TX_POS == 2) {
      RETARGET_UART_PORT->ALTFUNCNUM_bit.PIN2 = 1;
    } else if (RETARGET_UART_PIN_TX_POS == 3) {
      RETARGET_UART_PORT->ALTFUNCNUM_bit.PIN3 = 1;
    } else if (RETARGET_UART_PIN_TX_POS == 8) {
      RETARGET_UART_PORT->ALTFUNCNUM_bit.PIN8 = 1;
    } else if (RETARGET_UART_PIN_TX_POS == 9) {
      RETARGET_UART_PORT->ALTFUNCNUM_bit.PIN9 = 1;
    }
    
    if (RETARGET_UART_PIN_RX_POS == 2) {
      RETARGET_UART_PORT->ALTFUNCNUM_bit.PIN2 = 1;
    } else if (RETARGET_UART_PIN_RX_POS == 3) {
      RETARGET_UART_PORT->ALTFUNCNUM_bit.PIN3 = 1;
    } else if (RETARGET_UART_PIN_RX_POS == 8) {
      RETARGET_UART_PORT->ALTFUNCNUM_bit.PIN8 = 1;
    } else if (RETARGET_UART_PIN_RX_POS == 9) {
      RETARGET_UART_PORT->ALTFUNCNUM_bit.PIN9 = 1;
    }
    
    RETARGET_UART_PORT->ALTFUNCSET = (1 << RETARGET_UART_PIN_TX_POS) | (1 << RETARGET_UART_PIN_RX_POS);

    // Настраиваем UART0 с более детальной конфигурацией
    RCU->UARTCLKCFG[RETARGET_UART_NUM].UARTCLKCFG_bit.CLKSEL = RCU_UARTCLKCFG_CLKSEL_HSE;
    RCU->UARTCLKCFG[RETARGET_UART_NUM].UARTCLKCFG_bit.DIVEN = 0;
    RCU->UARTCLKCFG[RETARGET_UART_NUM].UARTCLKCFG_bit.RSTDIS = 1;
    RCU->UARTCLKCFG[RETARGET_UART_NUM].UARTCLKCFG_bit.CLKEN = 1;

    RETARGET_UART->IBRD = baud_icoef;
    RETARGET_UART->FBRD = baud_fcoef;
    RETARGET_UART->LCRH = UART_LCRH_FEN_Msk | (3 << UART_LCRH_WLEN_Pos);
    RETARGET_UART->IFLS = 0;  // Настройка уровня прерываний
    RETARGET_UART->CR = UART_CR_TXE_Msk | UART_CR_RXE_Msk | UART_CR_UARTEN_Msk;
#endif //RETARGET
}

int retarget_get_char()
{
#if defined RETARGET
    while (RETARGET_UART->FR_bit.RXFE) {
    };
    return (int)RETARGET_UART->DR_bit.DATA;
#endif //RETARGET
    return -1;
}

int retarget_put_char(int ch)
{
#if defined RETARGET
    while (RETARGET_UART->FR_bit.BUSY) {
    };
    RETARGET_UART->DR = ch;
#endif //RETARGET
    return 0;
}