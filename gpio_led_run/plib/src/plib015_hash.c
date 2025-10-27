/**
  ******************************************************************************
  * @file    plib015_hash.c
  *
  * @brief   Файл содержит реализацию функций для работы с HASH
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
  * <h2><center>&copy; 2022 АО "НИИЭТ"</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "plib015_hash.h"

/** @addtogroup Peripheral
  * @{
  */

/** @addtogroup HASH
  * @{
  */

/** @defgroup HASH_Private Приватные данные
  * @{
  */

/** @defgroup HASH_Private_Defines Приватные константы
  * @{
  */

/**
  * @}
  */

/** @defgroup HASH_Private_Functions Приватные функции
  * @{
  */

/**
  * @brief   Устанваливает буфер значениями хеш-суммы
  * @param   buffer ссылка на начало буфера, размер которого должен соответсвовать размерности алгоритма шифрования см. Руководство пользователя Приложение А10 Регистр HR
  * @retval  length длина хеша
  */
uint32_t HASH_GetHashLen(HASH_ALGO_TypeDef algo)
{
	switch(HASH_GetAlgo()) {
		case HASH_ALGO_SHA1:
			return 5;
		case HASH_ALGO_MD5:
			return 4;
		case HASH_ALGO_SHA224:
			return 7;
		case HASH_ALGO_SHA256:
			return 8;
		default:
			return 0;
	}
}

uint32_t HASH_GetHashBuffer(uint32_t* buffer)
{
	uint32_t idx = 0, len = 0;
	idx = len = HASH_GetHashLen(HASH_GetAlgo());

	while(idx) {
		buffer[--idx] = HASH_GetHash(idx);
	}

	return len;
}

void HASH_SetHashBuffer(uint32_t* buffer, uint32_t len)
{
	for (uint32_t i = 0 ; i < len; i++)
	{
		HASH_SetData(buffer[i]);
	}
}

/**
  * @brief   Устанавливает все регистры HASH значениями по умолчанию
  * @retval  void
  */
void HASH_DeInit()
{
	HASH_InitCmd(DISABLE);

    RCU_AHBRstCmd(RCU_AHBRst_HASH, DISABLE);
    RCU_AHBRstCmd(RCU_AHBRst_HASH, ENABLE);
}

/**
  * @brief   Инициализирует модуль HASH согласно параметрам структуры InitStruct
  * @param   InitStruct  Указатель на структуру типа @ref HASH_Init_TypeDef,
  *                      которая содержит конфигурационную информацию
  * @retval  void
  */
void HASH_Init(HASH_Init_TypeDef* InitStruct)
{
	RCU_AHBClkCmd(RCU_AHBClk_HASH, ENABLE);
    RCU_AHBRstCmd(RCU_AHBRst_HASH, ENABLE);

	HASH_DMACmd(InitStruct->DMATransmition);
	HASH_KeyLengthCmd(InitStruct->LongKey);
	HASH_SetMultyDMATransmit(InitStruct->MultyDMATransmition);
	HASH_SetAlgo(InitStruct->Algo);
	HASH_DataTypeConfig(InitStruct->DataType);
	HASH_ModeCmd(InitStruct->Mode);
	HASH_SameKeyCmd(InitStruct->SameKey);

	HASH_InitCmd(ENABLE);
}

/**
  * @brief   Заполнение каждого члена структуры InitStruct значениями по умолчанию
  * @param   InitStruct  Указатель на структуру типа @ref HASH_Init_TypeDef,
  *                      которую необходимо проинициализировать
  * @retval  void
  */
void HASH_StructInit(HASH_Init_TypeDef* InitStruct)
{
	InitStruct->DMATransmition = 0x0;
	InitStruct->LongKey = HASH_LKEY_ShortKey;
	InitStruct->MultyDMATransmition = 0x0;
	InitStruct->Algo = HASH_ALGO_SHA1;
	InitStruct->DataType = HASH_DATATYPE_Word;
	InitStruct->Mode = HASH_MODE_Hash;
	InitStruct->SameKey = 0x0;
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

/******************* (C) COPYRIGHT 2022 NIIET *****END OF FILE****/
