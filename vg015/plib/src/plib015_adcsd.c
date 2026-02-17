/**
  ******************************************************************************
  * @file    plib015_adcsd.c
  *
  * @brief   Файл содержит реализацию функций для работы с ADCSD
  *
  * @author  НИИЭТ, Александр Дыхно <dykhno@niiet.ru>
  *
  ******************************************************************************
  * @attention
  *
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
  * <h2><center>&copy; 2024 АО "НИИЭТ"</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "plib015_adcsd.h"

/** @addtogroup Peripheral
  * @{
  */

/** @addtogroup ADCSD
  * @{
  */

/** @defgroup ADCSD_Private Приватные данные
  * @{
  */

/** @defgroup ADCSD_Private_Defines Приватные константы
  * @{
  */

/**
  * @}
  */

/** @defgroup ADCSD_Private_Functions Приватные функции
  * @{
  */

/**
  * @brief   Устанавливает все регистры ADCSD значениями по умолчанию
  * @retval  void
  */
void ADCSD_DeInit(void)
{
    RCU_ADCSDRstCmd(DISABLE);
    RCU_ADCSDRstCmd(ENABLE);
}

/**
  * @brief   Инициализирует АЦП сигма-дельта согласно параметрам структуры InitStruct
  * @param   InitStruct  Указатель на структуру типа @ref ADCSD_Init_TypeDef,
  *                      которая содержит конфигурационную информацию
  * @retval  void
  */
void ADCSD_Init(ADCSD_Init_TypeDef* InitStruct)
{
	ADCSD_WaitCycleCmd(InitStruct->WaitCycle);
	ADCSD_MainDivCmd(InitStruct->MainDiv);
	ADCSD_SampleDivCmd(InitStruct->SampleDiv);
}

/**
  * @brief   Заполнение каждого члена структуры InitStruct значениями по умолчанию
  * @param   InitStruct  Указатель на структуру типа @ref ADCSD_Init_TypeDef,
  *                      которую необходимо проинициализировать
  * @retval  void
  */
void ADCSD_StructInit(ADCSD_Init_TypeDef* InitStruct)
{
	InitStruct->MainDiv = 0x0;
	InitStruct->SampleDiv = 0x0;
	InitStruct->WaitCycle = 0x0;
}

/**
  * @brief   Инициализирует канал АЦП сигма-дельта согласно параметрам структуры InitStruct
  * @param   InitStruct  Указатель на структуру типа @ref ADCSD_CH_Init_TypeDef,
  *                      которая содержит конфигурационную информацию
  * @retval  void
  */
void ADCSD_CH_Init(ADCSD_CH_Num_TypeDef ch_num, ADCSD_CH_Init_TypeDef* InitStruct)
{
	ADCSD_SetMode(ch_num, InitStruct->Mode);
	ADCSD_SetAmplification(ch_num, InitStruct->Amplifier);
}

/**
  * @brief   Заполнение каждого члена структуры InitStruct значениями по умолчанию
  * @param   InitStruct  Указатель на структуру типа @ref ADCSD_CH_Init_TypeDef,
  *                      которую необходимо проинициализировать
  * @retval  void
  */
void ADCSD_CH_StructInit(ADCSD_CH_Init_TypeDef* InitStruct)
{
	InitStruct->Amplifier = 0x0;
	InitStruct->Mode = 0x1;
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/******************* (C) COPYRIGHT 2024 NIIET *****END OF FILE****/
