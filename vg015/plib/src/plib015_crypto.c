/**
  ******************************************************************************
  * @file    plib015_crypto.c
  *
  * @brief   Файл содержит реализацию функций для работы с CRYPTO
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
#include "plib015_crypto.h"

/** @addtogroup Peripheral
  * @{
  */

/** @addtogroup CRYPTO
  * @{
  */

/** @defgroup CRYPTO_Private Приватные данные
  * @{
  */

/** @defgroup CRYPTO_Private_Defines Приватные константы
  * @{
  */

/**
  * @}
  */

/** @defgroup CRYPTO_Private_Functions Приватные функции
  * @{
  */

/**
  * @brief   Устанавливает все регистры CRYPTO значениями по умолчанию
  * @retval  void
  */
void CRYPTO_DeInit()
{
	RCU_AHBRstCmd(RCU_AHBRst_CRYPTO, DISABLE);
	RCU_AHBRstCmd(RCU_AHBRst_CRYPTO, ENABLE);
}

/**
  * @brief   Инициализирует модуль CRYPTO согласно параметрам структуры InitStruct
  * @param   InitStruct  Указатель на структуру типа @ref CRYPTO_Init_TypeDef,
  *                      которая содержит конфигурационную информацию
  * @retval  void
  */
void CRYPTO_Init(CRYPTO_Init_TypeDef* InitStruct)
{
	CRYPTO_DirectionConfig(InitStruct->Direction);
	CRYPTO_AlgoConfig(InitStruct->Algorithm);
	CRYPTO_ModeConfig(InitStruct->Mode);
	CRYPTO_InitVectorAutoUpdateCmd(InitStruct->InitVectorAutoUpdate);
	CRYPTO_UpdateKeyCmd(InitStruct->UpdateKey);
}

/**
  * @brief   Заполнение каждого члена структуры InitStruct значениями по-умолчанию.
  * 		 Значение по-умолчанию соответсвует шифрованию TLS_AES_256_GCM с автоматическим
  * 		 обновлением nonce и ключа шифрования.
  * @param   InitStruct  Указатель на структуру типа @ref CRYPTO_Init_TypeDef,
  *                      которую необходимо проинициализировать
  * @retval  void
  */
void CRYPTO_StructInit(CRYPTO_Init_TypeDef* InitStruct)
{
	InitStruct->Direction = CRYPTO_Dir_Encrypt;
	InitStruct->Algorithm = CRYPTO_Algo_AES_256;
	InitStruct->Mode = CRYPTO_Mode_GCM;
	InitStruct->InitVectorAutoUpdate = ENABLE;
	InitStruct->UpdateKey = ENABLE;
	InitStruct->GCMPhase = CRYPTO_GCM_PHASE_INIT;
}

/**
  * @brief   Заполнение каждого члена структуры DMAStruct значениями по умолчанию
  * @param   DMAStruct  Указатель на структуру типа @ref CRYPTO_DMAInit_TypeDef,
  *                      которую необходимо проинициализировать
  * @param   CryptoStruct Указатель на стуктуру типа @ref CRYPTO_Init_TypeDef,
  * 					  которая содержит параметры шифрования
  * @retval  void
  */
void CRYPTO_DMAStructInit(CRYPTO_DMAInit_TypeDef* DMAStruct, CRYPTO_Init_TypeDef* CryptoStruct)
{
	DMAStruct->ByteSwap = DISABLE;
	DMAStruct->WordSwap = DISABLE;

	DMAStruct->CryptoSettings = CryptoStruct;
	DMAStruct->LastDescriptor = ENABLE;
	DMAStruct->ITEnable = DISABLE;
	DMAStruct->BlocksCount = 0;

	DMAStruct->SourceAddress = 0;
	DMAStruct->DestinationAddress = 0;
	DMAStruct->DescriptorPtr->NEXT_DESCR = 0;
	DMAStruct->DescriptorPtr = 0;
}

/**
  * @brief   Инициализация дескриптора значениями из структуры DMAStruct
  * @param   DMAStruct  Указатель на структуру типа @ref CRYPTO_DMAInit_TypeDef,
  *                      которую необходимо проинициализировать
  * @retval  void
  */
uint32_t CRYPTO_InitDMADescriptor(CRYPTO_DMAInit_TypeDef* DMAStruct)
{
	/* CRYPTO DMA operation control word */
	DMAStruct->DescriptorPtr->CONTROL_bit.UPDATE_KEY = DMAStruct->CryptoSettings->UpdateKey;
	DMAStruct->DescriptorPtr->CONTROL_bit.LAST_DESCRIPTOR = DMAStruct->LastDescriptor;
	DMAStruct->DescriptorPtr->CONTROL_bit.DIRECTION = DMAStruct->CryptoSettings->Direction;
	DMAStruct->DescriptorPtr->CONTROL_bit.ALGORITHM = DMAStruct->CryptoSettings->Algorithm;
	DMAStruct->DescriptorPtr->CONTROL_bit.MODE = DMAStruct->CryptoSettings->Mode;
	DMAStruct->DescriptorPtr->CONTROL_bit.GCM_PHASE = DMAStruct->CryptoSettings->GCMPhase;
	DMAStruct->DescriptorPtr->CONTROL_bit.INTERRUPT_ENABLE = DMAStruct->ITEnable;
	DMAStruct->DescriptorPtr->CONTROL_bit.BLOCKS_COUNT = DMAStruct->BlocksCount;

	DMAStruct->DescriptorPtr->SRC_ADDR = (uint32_t) DMAStruct->SourceAddress;
	DMAStruct->DescriptorPtr->DST_ADDR = (uint32_t)DMAStruct->DestinationAddress;
	DMAStruct->DescriptorPtr->NEXT_DESCR = (uint32_t) DMAStruct->LastDescriptor ? 0 : CRYPTO_InitDMADescriptor((CRYPTO_DMAInit_TypeDef*) DMAStruct->NextDescriptor);

//	if (DMAStruct->CryptoSettings->Mode == CRYPTO_Mode_GCM) {
//		// todo
//		// wrong
//		DMAStruct->DescriptorPtr->CONTROL_bit.GCM_PHASE = CRYPTO_GCM_PHASE_INIT;
//		DMAStruct->DescriptorPtr->CONTROL_bit.LAST_DESCRIPTOR = DISABLE;
//		CRYPTO_DMA_DESCR_TypeDef headerDescriptor = *DMAStruct->DescriptorPtr;
//		CRYPTO_DMA_DESCR_TypeDef payloadDescriptor = *DMAStruct->DescriptorPtr;
//		CRYPTO_DMA_DESCR_TypeDef lastBlockDescriptor = *DMAStruct->DescriptorPtr;
//		headerDescriptor.CONTROL_bit.GCM_PHASE = CRYPTO_GCM_PHASE_HEADER;
//		payloadDescriptor.CONTROL_bit.GCM_PHASE = CRYPTO_GCM_PHASE_PAYLOAD;
//		lastBlockDescriptor.CONTROL_bit.GCM_PHASE = CRYPTO_GCM_PHASE_LAST_BLOCK;
//		lastBlockDescriptor.CONTROL_bit.LAST_DESCRIPTOR = ENABLE;
//		DMAStruct->DescriptorPtr->NEXT_DESCR = (uint32_t) &headerDescriptor;
//		headerDescriptor.NEXT_DESCR = (uint32_t) &payloadDescriptor;
//		payloadDescriptor.NEXT_DESCR = (uint32_t) &lastBlockDescriptor;
//	}

	return (uint32_t) DMAStruct->DescriptorPtr;
}

/**
  * @brief   Инициализация дескриптора значениями из структуры DMAStruct
  * @param   DMAStruct  Указатель на структуру типа @ref CRYPTO_DMAInit_TypeDef,
  *                      которая будет исполнена как дескриптор DMA
  * @retval  void
  */
void CRPYTO_ProcessData(CRYPTO_DMAInit_TypeDef* DMAStruct)
{
	CRYPTO_DMA_ByteSwapCmd(DMAStruct->ByteSwap);
	CRYPTO_DMA_WordSwapCmd(DMAStruct->WordSwap);
	CRYPTO_DMA_SetBaseDescriptor(CRYPTO_InitDMADescriptor(DMAStruct));
	CRYPTO_DMA_StartCmd();
}

/**
  * @brief   Устанавливает значение ключа, максимум 256 бит, для AES-128 128 бит
  * @param   key массив слов, который формирует ключ шифрования
  * @retval  void
  */
void CRYPTO_SetKey(uint32_t* key, uint32_t len)
{
	uint32_t idx = 0;
	while(idx < len)
	{
		CRYPTO_SetKeyInReg(idx++, *key++);
	}
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
