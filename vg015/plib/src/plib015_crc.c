/**
  ******************************************************************************
  * @file    plib015_crc.c
  *
  * @brief   Файл содержит реализацию функций для работы с CRC
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
#include "plib015_crc.h"

/** @addtogroup Peripheral
  * @{
  */

/** @addtogroup CRC
  * @{
  */

/** @defgroup CRC_Private Приватные данные
  * @{
  */

/** @defgroup CRC_Private_Defines Приватные константы
  * @{
  */

/**
  * @}
  */

/** @defgroup CRC_Private_Functions Приватные функции
  * @{
  */

/**
  * @brief   Устанавливает все регистры CRC значениями по умолчанию
  * @retval  void
  */
void CRC_DeInit(CRC_TypeDef* CRCx)
{
	assert_param(IS_CRC_PERIPH(CRCx));

	CRC_ResetCmd(CRCx, ENABLE);
	CRC_ResetCmd(CRCx, DISABLE);
	uint32_t CRC_num = (CRCx == CRC0) ? RCU_AHBRst_CRC0 : RCU_AHBRst_CRC1;
    RCU_AHBRstCmd(CRC_num, DISABLE);
    RCU_AHBRstCmd(CRC_num, ENABLE);
}

/**
  * @brief   Инициализирует секвенсоры АЦП сигма-дельта согласно параметрам структуры InitStruct
  * @param	 CRCx Выбор модуля CRC, где x = 0 | 1
  * @param   InitStruct  Указатель на структуру типа @ref CRC_Init_TypeDef,
  *                      которая содержит конфигурационную информацию
  * @retval  void
  */
void CRC_Init(CRC_TypeDef* CRCx, CRC_Init_TypeDef* InitStruct)
{
	CRC_SetInit(CRCx, InitStruct->Init);
	CRC_SetRevIn(CRCx, InitStruct->RevIn);
	CRC_RevOutCmd(CRCx, InitStruct->RevOut);
	CRC_ModeCmd(CRCx, InitStruct->Mode);
	CRC_XOROutCmd(CRCx, InitStruct->XorOut);
	CRC_SetPolysize(CRCx, InitStruct->Polysize);
	CRC_SetPol(CRCx, InitStruct->Pol);
}

/**
  * @brief   Заполнение каждого члена структуры InitStruct значениями по умолчанию
  * @param   InitStruct  Указатель на структуру типа @ref CRC_Init_TypeDef,
  *                      которую необходимо проинициализировать
  * @retval  void
  */
void CRC_StructInit(CRC_Init_TypeDef* InitStruct)
{
	InitStruct->Init = 0x0;
	InitStruct->RevIn = 0x0;
	InitStruct->RevOut = 0x0;
	InitStruct->Mode = 0x0;
	InitStruct->XorOut = 0x0;
	InitStruct->Polysize = 0x0;
	InitStruct->Pol = 0x4C11DB7;	// CRC-32 (Ethernet)
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
