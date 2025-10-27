/**
  ******************************************************************************
  * @file    plib015_qspi.c
  *
  * @brief   Файл содержит реализацию функций для работы с QSPI
  *
  * @author  НИИЭТ, Александр Дыхно <dykhno@niiet.ru>
  * @author  НИИЭТ, Штоколов Филипп
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
  * <h2><center>&copy; 2022 АО "НИИЭТ"</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "plib015_qspi.h"

/** @addtogroup Peripheral
  * @{
  */

/** @addtogroup QSPI
  * @{
  */

/** @defgroup QSPI_Private Приватные данные
  * @{
  */

/** @defgroup QSPI_Private_Defines Приватные константы
  * @{
  */

/**
  * @}
  */

/** @defgroup QSPI_Private_Functions Приватные функции
  * @{
  */

/**
  * @brief   Устанавливает все регистры QSPI значениями по умолчанию
  * @retval  void
  */
void QSPI_DeInit()
{
	RCU_AHBRstCmd(RCU_AHBClk_QSPI, DISABLE);
    RCU_AHBRstCmd(RCU_AHBClk_QSPI, ENABLE);
}

/**
  * @brief   Инициализирует QSPI для транзакции по SPI согласно параметрам структуры InitStruct.
  * @param   InitStruct  Указатель на структуру типа @ref QSPI_SPI_Init_TypeDef,
  *                      которая содержит конфигурационную информацию.
  * @retval  Status  Статус результата инициализации
  */
void QSPI_SPI_Init(QSPI_SPI_Init_TypeDef* InitStruct)
{
	QSPI_ModeConfig(QSPI_Mode_SPI);
	QSPI_SPITransactionModeConfig(InitStruct->IOMode);
	QSPI_SPIDataRateConfig(InitStruct->dataRate);
	QSPI_SetWordLength(InitStruct->dataWidth);
	QSPI_TxCmd(InitStruct->TxEnable);
	QSPI_RxCmd(InitStruct->RxEnable);
	QSPI_SCKDivConfig(InitStruct->SCKDiv);
}

/**
  * @brief   Заполнение каждого члена структуры InitStruct значениями по умолчанию.
  * @param   InitStruct  Указатель на структуру типа @ref QSPI_SPI_Init_TypeDef,
  *                      которую необходимо проинициализировать.
  * @retval  void
  */
void QSPI_SPI_StructInit(QSPI_SPI_Init_TypeDef* InitStruct)
{
    InitStruct->IOMode = QSPI_SPI_IOMode_Single;
    InitStruct->dataRate = QSPI_SPI_DataRate_Single;
    InitStruct->dataWidth = 0x1;
    InitStruct->TxEnable = DISABLE;
    InitStruct->RxEnable = DISABLE;
    InitStruct->SCKDiv = 0x0;
}

/**
  * @brief   Инициализирует QSPI для передачи по формату QSPI согласно параметрам структуры InitStruct.
  * @param   InitStruct  Указатель на структуру типа @ref QSPI_Init_TypeDef,
  *                      которая содержит конфигурационную информацию.
  * @retval  Status  Статус результата инициализации
  */
void QSPI_Init(QSPI_Init_TypeDef* InitStruct)
{
    QSPI_ModeConfig(QSPI_Mode_QSPI);

	QSPI_InstructionConfig(InitStruct->instruction);
	QSPI_InstructionModeConfig(InitStruct->instructionMode);
	QSPI_AddressModeConfig(InitStruct->addressMode);
	QSPI_AddressSizeConfig(InitStruct->addressSize);
	QSPI_AdditionalModeConfig(InitStruct->additionalMode);
	QSPI_AdditionalSizeConfig(InitStruct->additionalSize);
	QSPI_WaitCyclesConfig(InitStruct->waitCycles);
	QSPI_DataModeConfig(InitStruct->dataMode);
	QSPI_DataDirectionConfig(InitStruct->dataDirection);
	QSPI_DataRateConfig(InitStruct->dataRate);
	QSPI_SCKDivConfig(InitStruct->SCKDiv);
}

/**
  * @brief   Заполнение каждого члена структуры InitStruct значениями по умолчанию.
  * @param   InitStruct  Указатель на структуру типа @ref QSPI_Init_TypeDef,
  *                      которую необходимо проинициализировать.
  * @retval  void
  */
void QSPI_StructInit(QSPI_Init_TypeDef* InitStruct)
{
	InitStruct->instruction = 0x0;
	InitStruct->instructionMode = QSPI_Lines_No;
	InitStruct->addressMode = QSPI_Lines_No;
	InitStruct->addressSize = QSPI_DataSize_8;
	InitStruct->additionalMode = QSPI_Lines_No;
	InitStruct->additionalSize = QSPI_DataSize_8;
	InitStruct->waitCycles = 0x0;
	InitStruct->dataMode = QSPI_Lines_No;
	InitStruct->dataDirection = QSPI_Direction_Write;
	InitStruct->dataRate = QSPI_DataRate_Single;
	InitStruct->SCKDiv = 0x0;
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

/******************* (C) COPYRIGHT 2019 NIIET *****END OF FILE****/
