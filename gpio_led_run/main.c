/*==============================================================================
 * Реализация "бегущего огня" на старшей части GPIOA для K1921VG015
 * с использованием библиотеки plib015
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
 *                              2024 АО "НИИЭТ"
 *==============================================================================
 */

//-- Includes ------------------------------------------------------------------
#include <plib015.h>
#include <system_k1921vg015.h>

//-- Defines -------------------------------------------------------------------

void TMR32_IRQHandler();

void gpio_init()
{
  RCU_AHBClkCmd(RCU_AHBClk_GPIOA, ENABLE);
  RCU_AHBRstCmd(RCU_AHBRst_GPIOA, ENABLE);
  GPIO_OutCmd(GPIOA, GPIO_Pin_15_8, ENABLE);
}

void TMR32_init(uint32_t period)
{
  RCU_APBClkCmd(RCU_APBClk_TMR32, ENABLE);
  RCU_APBRstCmd(RCU_APBRst_TMR32, ENABLE);

  // Записываем значение периода в CAPCOM[0]
  TMR32_CAPCOM_SetComparator(TMR32_CAPCOM_0, period - 1);
  // Выбираем режим счета от 0 до значения CAPCOM[0]
  TMR32_SetMode(TMR32_Mode_Capcom_Up);

  // Разрешаем прерывание по совпадению значения счетчика и CAPCOM[0]
  TMR32_ITCmd(TMR32_IT_CAPCOM_0, ENABLE);

  // Настраиваем обработчик прерывания для TMR32
  PLIC_SetIrqHandler(Plic_Mach_Target, IsrVect_IRQ_TMR32, TMR32_IRQHandler);
  PLIC_SetPriority(IsrVect_IRQ_TMR32, 0x1);
  PLIC_IntEnable(Plic_Mach_Target, IsrVect_IRQ_TMR32);
}

//-- Peripheral init functions -------------------------------------------------
void periph_init()
{
  SystemInit();
  SystemCoreClockUpdate();

  gpio_init();
}

//--- USER FUNCTIONS ----------------------------------------------------------------------

volatile uint32_t led_shift;
//-- Main ----------------------------------------------------------------------
int main(void)
{
  periph_init();
  TMR32_init(SystemCoreClock >> 4);
  InterruptEnable();
  led_shift = GPIO_Pin_12;
  while (1)
  {
  }

  return 0;
}

//-- IRQ INTERRUPT HANDLERS ---------------------------------------------------------------
void TMR32_IRQHandler()
{
  GPIO_ToggleBits(GPIOA, led_shift);
  led_shift = led_shift << 1;
  if (led_shift > GPIO_Pin_15)
    led_shift = GPIO_Pin_12;
  // Сбрасываем флаг прерывания таймера
  TMR32_ITClear(TMR32_IT_TimerUpdate | TMR32_IT_CAPCOM_0);
}
