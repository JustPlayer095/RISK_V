/**
  ******************************************************************************
  * @file    plib015_trng.c
  *
  * @brief   Файл содержит реализацию функций для работы с TRNG
  *
  * @author  НИИЭТ, Александр Дыхно <dykhno@niiet.ru>
  * @author  НИИЭТ, Филипп Штоколов 
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
#include "plib015_trng.h"

/** @addtogroup Peripheral
  * @{
  */

/** @addtogroup TRNG
  * @{
  */

/** @defgroup TRNG_Private Приватные данные
  * @{
  */

/** @defgroup TRNG_Private_Defines Приватные константы
  * @{
  */

/**
  * @}
  */

/** @defgroup TRNG_Private_Functions Приватные функции
  * @{
  */

/**
  * @brief   Устанавливает все регистры TRNG значениями по умолчанию
  * @retval  void
  */
void TRNG_DeInit()
{
	TRNG_SwResetCmd(ENABLE);

    RCU_APBRstCmd(RCU_APBRst_TRNG, DISABLE);
    RCU_APBRstCmd(RCU_APBRst_TRNG, ENABLE);

    TRNG_SwResetCmd(DISABLE);
}

/**
  * @brief   Инициализирует модуль TRNG согласно параметрам структуры InitStruct
  * @param   InitStruct  Указатель на структуру типа @ref TRNG_Init_TypeDef,
  *                      которая содержит конфигурационную информацию
  * @retval  void
  */
void TRNG_Init(TRNG_Init_TypeDef* InitStruct)
{
	TRNG_PseudorandomGeneratorCmd(InitStruct->PseudoRandomEnable);
//	TRNG_TestSourceCmd(InitStruct->TestSourceEnable);
	TRNG_BypassHandlerCmd(InitStruct->BypassHandler);
//	TRNG_ITTestFailCmd(InitStruct->ITOnAnyTestFail);
//	TRNG_ITFIFOfullCmd(InitStruct->ITOnFIFOfull);
	TRNG_ForceGeneratorCmd(InitStruct->ForceGenerator);
//	TRNG_IgnoreHardwareTestsCmd(InitStruct->IgnoreHardwareTests);
	TRNG_SetBlockAmountForHandler(InitStruct->AmountBlocksForHandler);
	TRNG_FIFOfillOnStartCmd(InitStruct->FIFOfillOnStart);
//	TRNG_RepeatTestDisableCmd(InitStruct->RepeatTestDisable);
//	TRNG_ProportionTestDisableCmd(InitStruct->ProportionTestDisable);
//	TRNG_AutoCorrelationTestDisableConfig(InitStruct->AutocorrelationTestDisable);
//	TRNG_CorrelationTestDisableConfig(InitStruct->CorrelationTestDisable);
	TRNG_BlendMethodConfig(InitStruct->BlendMethod);
	TRNG_SwResetCmd(DISABLE);
}

/**
  * @brief   Заполнение каждого члена структуры InitStruct значениями по умолчанию
  * @param   InitStruct  Указатель на структуру типа @ref TRNG_Init_TypeDef,
  *                      которую необходимо проинициализировать
  * @retval  void
  */
void TRNG_StructInit(TRNG_Init_TypeDef* InitStruct)
{
	InitStruct->PseudoRandomEnable = DISABLE;
//	InitStruct->TestSourceEnable = DISABLE;
	InitStruct->BypassHandler = DISABLE;
//	InitStruct->ITOnAnyTestFail = DISABLE;
//	InitStruct->ITOnFIFOfull = DISABLE;
	InitStruct->ForceGenerator = DISABLE;
//	InitStruct->IgnoreHardwareTests = DISABLE;
	InitStruct->AmountBlocksForHandler = 0x0;
	InitStruct->FIFOfillOnStart = DISABLE;
//	InitStruct->RepeatTestDisable = DISABLE;
//	InitStruct->ProportionTestDisable = DISABLE;
//	InitStruct->AutocorrelationTestDisable = TRNG_AUTOCORRELATIONDISABLE_Nothing;
//	InitStruct->CorrelationTestDisable = TRNG_CORRELATIONDISABLE_Nothing;
	InitStruct->BlendMethod = TRNG_BLENDMETHOD_Concatenation;
	InitStruct->WarmPeriod = 0x00000200;
	InitStruct->CoolPeriod = 0x00000000;
	InitStruct->SamplePeriod = 0x0000001E;
}

/**
  * @brief   Генерация ключа размерности keyLength с количеством блоков для функции обработки 4,
  * 		 эта функция блокирует поток
  * @param   key Ссылка на память, куда будет сохранен ключ размерности keyLength
  * @param   keyLength Размерность ключа в 32-битных словах
  * @retval  void
  */
void TRNG_GenerateKey(uint32_t *key, uint32_t keyLength)
{
	TRNG_DeInit();
	RCU_APBClkCmd(RCU_APBClk_TRNG, ENABLE);
	RCU_APBRstCmd(RCU_APBRst_TRNG, ENABLE);
	TRNG_SwResetCmd(ENABLE);
	TRNG_Init_TypeDef trngInit;
	TRNG_StructInit(&trngInit);

	// Start filling FIFO with noise source, with conditioning, all start-up tests and wait until interrupt triggers
	trngInit.WarmPeriod       = 0x00000200; //
	trngInit.SamplePeriod     = 0x00000020;
	trngInit.CoolPeriod       = 0x00000000;
	trngInit.AmountBlocksForHandler = 0x4;
	trngInit.BypassHandler    = DISABLE;

	TRNG_Init(&trngInit);

	TRNG_StartCmd(ENABLE);

	while (TRNG_GetFIFOlength() != keyLength) {}

	TRNG_StartCmd(DISABLE);

	while (keyLength--)
	{
		*key++ = TRNG_GetFIFOValue();
	}

	TRNG_SwResetCmd(ENABLE);
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
