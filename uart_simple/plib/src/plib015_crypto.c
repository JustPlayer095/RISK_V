/**
  ******************************************************************************
  * @file    plib015_crypto.c
  *
  * @brief   Файл содержит реализацию функций для работы с CRYPTO
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
#include "plib015_crypto.h"
#include <locale.h>

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
	CRYPTO_GCMPhaseConfig(InitStruct->GCMPhase);
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
//	DMAStruct->DescriptorPtr->NEXT_DESCR = 0x80;
//	DMAStruct->DescriptorPtr = 0x80;
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
	DMAStruct->DescriptorPtr->DST_ADDR = (uint32_t) DMAStruct->DestinationAddress;
	DMAStruct->DescriptorPtr->NEXT_DESCR = (uint32_t) DMAStruct->LastDescriptor ? 0 : CRYPTO_InitDMADescriptor((CRYPTO_DMAInit_TypeDef*) DMAStruct->NextDescriptor);

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
  * @param   key Массив слов, который формирует ключ шифрования
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
  * @brief   Заполнение регистров DATA_IN данными, приходящими в виде байтов
  * @param   data_in  Входящие данные
  * @param   block_size Размерность данных блочного шифра
  * @retval  void
  */
static void CRYPTO_SetDataInBytes(const unsigned char *data_in, uint32_t block_size)
{
	for(uint32_t i = 0; i < block_size; i++)
	{
		CRYPTO_SetTextInput(i, data_in[3] + (data_in[2] << 8) + (data_in[1] << 16) + (data_in[0] << 24));
		data_in += sizeof(uint32_t);
	}
}

/**
  * @brief   Заполнение регистров ключа данными, приходящими в виде байтов
  * @param   crypto Структура @CRYPTO_Init_Typedef, сконфигурированная для текущего
  * 				шифрования
  * @param   key Байтовый массив ключа, должен иметь достаточную длину для
  * 			выбранного в crypto структуре алгоритма
  * @retval  void
  */
void CRYPTO_SetKeyBytes(CRYPTO_Init_TypeDef* crypto, const unsigned char* key)
{
	uint32_t key_size = (crypto->Algorithm == CRYPTO_Algo_AES_128) ? 4 : 8;
	for(uint32_t i = 0; i < key_size; i++)
	{
		uint32_t key_word = key[3] + (key[2] << 8) + (key[1] << 16) + (key[0] << 24);
		CRYPTO_SetKeyInReg(i, key_word);
		key += sizeof(uint32_t);
	}
}

/**
  * @brief  Единичная операция шифрования/дешифрования над байтовыми данными
  * 		Примечание: размер data_in не должен превышать размера блока шифра,
  * 					который установлен в структуре crypto
  * @param	crypto Указатель на структуру типа @ref CRYPTO_InitTypedef, которая
  * 				конфигурирует текущую операцию надж данными
  * @param 	key Значение ключа для проведения операции, может быть NULL, если
  *				 ключ был уже установлен
  *	@param 	data_in Входящие байтовые данные
  *	@param 	iv Инициализационный вектор текущей операции, может быть NULL, если
  *			  вектор был уже установлен
  *	@param 	data_out Массив выходящих данных
  * @retval void
  */
void CRYPTO_SinglePerform(CRYPTO_Init_TypeDef* crypto, const unsigned char *key, const unsigned char *data_in, const unsigned char* iv, unsigned char *data_out)
{
	CRYPTO_Init(crypto);
	if (key != NULL) // if null then key is already set
		CRYPTO_SetKeyBytes(crypto, key);
	if (iv != NULL) // if null then iv is already set or there is no need in it (e.g. ECB mode)
		CRYPTO_SetIVBytes(iv);

	uint32_t block_size = (crypto->Algorithm == CRYPTO_Algo_MAGMA) ? 2 : 4;
	CRYPTO_SetDataInBytes(data_in, block_size);

	while (!CRYPTO_ReadyStatus()) {}

	CRYPTO_StartCmd();

	while (!(CRYPTO->STATUS & CRYPTO_STATUS_KEYS_READY_Msk)) {}
	while (!(CRYPTO->STATUS & CRYPTO_STATUS_READY_Msk)) {}

	if (crypto->Mode == CRYPTO_Mode_CBC && crypto->Direction == CRYPTO_Dir_Decrypt)
	{
		CRYPTO_DirectionConfig(CRYPTO_Dir_Encrypt);
		CRYPTO_ModeConfig(CRYPTO_Mode_ECB);
	}

	for (uint32_t i = 0; i < block_size; i++)
	{
		uint32_t out_word = CRYPTO_GetTextOutput(i);
		out_word = (out_word >> 24) + ((out_word >> 8) & 0xFF00) + ((out_word << 8) & 0xFF0000) + (out_word << 24);
		if (crypto->Mode == CRYPTO_Mode_CBC && crypto->Direction == CRYPTO_Dir_Decrypt)
		{
			out_word ^= (iv[3] + (iv[2] << 8) + (iv[1] << 16) + (iv[0] << 24));
			iv += sizeof(uint32_t);
		}
		*((uint32_t *) data_out) = out_word;
		data_out += sizeof(uint32_t);
	}
}

/**
  * @brief	Операция шифрования байтовых данных с помощью DMA
  * @param  crypto Указатель на структуру типа @ref CRYPTO_InitTypedef, которая
  * 				конфигурирует текущую операцию надж данными
  * @param  key Значение ключа для проведения операции, может быть NULL, если
  *				 ключ был уже установлен
  *	@param 	data_in Входящие байтовые данные
  *	@param 	data_in_size Размер входящих данных в байтах
  *	@param 	iv Инициализационный вектор текущей операции, может быть NULL, если
  *			  вектор был уже установлен
  *	@param 	data_out Массив выходящих данных
  * @retval void
  */
void CRYPTO_CryptWithDMA(CRYPTO_Init_TypeDef* crypto, const unsigned char *key, const unsigned char *data_in, uint32_t data_in_size, const unsigned char* iv, unsigned char *data_out)
{
	CRYPTO_DMA_DESCR_TypeDef DMA_CTRLDATA __attribute__((aligned (0x80)));
	DMA_CTRLDATA.CONTROL = 0;

	CRYPTO_DMAInit_TypeDef dma_init;

	if (key != NULL)
		CRYPTO_SetKeyBytes(crypto, key);
	if (iv != NULL)
		CRYPTO_SetIVBytes(iv);

	CRYPTO_DMAStructInit(&dma_init, crypto);
	dma_init.BlocksCount = data_in_size >> 3;
	if (crypto->Algorithm != CRYPTO_Algo_MAGMA)
		dma_init.BlocksCount >>= 1;
	--dma_init.BlocksCount; // n - 1 DMA

	dma_init.ByteSwap = ENABLE;
	dma_init.DescriptorPtr = &DMA_CTRLDATA;
	dma_init.SourceAddress = (void*) data_in;
	dma_init.DestinationAddress = (void*) data_out;

	while (!CRYPTO_ReadyStatus()) {}

	CRPYTO_ProcessData(&dma_init);

	while (CRYPTO_DMA_ActiveStatus()) {}
	while (!CRYPTO_ReadyStatus()) {}
}

/**
  * @brief	Установка инициализационного вектора в байтовой форме
  *	@param 	iv Инициализационный вектор текущей операции
  * @retval void
  */
void CRYPTO_SetIVBytes(const unsigned char *iv)
{
	for(int i = 0; i < 4; i++)
	{
		CRYPTO_SetInitVector(i, iv[3] + (iv[2] << 8) + (iv[1] << 16) + (iv[0] << 24));
		iv += sizeof(uint32_t);
	}
}

// GCM functions

/**
  * @brief	Инициализация структуры @ref CRYPTO_Init_Typedef для режима GCM
  *	@param 	descriptor_crypto Структура дескриптора, инициализация которой происходит
  *	@param	crypto Заполненная структура - источник данных для первого параметра
  *	@param	phase Фаза режима GCM
  * @retval void
  */
static void CRYPTO_InitCryptoStructGCM(CRYPTO_Init_TypeDef* descriptor_crypto, CRYPTO_Init_TypeDef* crypto, CRYPTO_GCM_PHASE_TypeDef phase)
{
	CRYPTO_StructInit(descriptor_crypto);

	descriptor_crypto->Algorithm = crypto->Algorithm;
	descriptor_crypto->Direction = crypto->Direction;
	descriptor_crypto->Mode = CRYPTO_Mode_GCM;
	descriptor_crypto->GCMPhase = phase;
}

/**
  * @brief	Инициализация дескриптора для передачи DMA
  * 		Примечание: Данная функция выставляет ByteSwap, это значит, что данные изначально
  * 		должны быть в байтовом формате для проведения корректного шифрования
  *	@param 	base Ссылка на структуру типа @CRYPTO_DMAInit_TypeDef, инициализация которой происходит
  *	@param 	crypto Ссылка на структуру типа @CRYPTO_Init_TypeDef, отвечающая за параметры блока CRYPTO
  *			при передаче DMA
  *	@param	next Ссылка на следующий дескриптор
  *	@param	dma_data Размещенный в памяти дескриптор DMA для CRYPTO
  *	@param	src Данные источника
  *	@param	src_size Размер данных источника
  *	@param	answer Массив размещения итоговых данных
  * @retval void
  */
static void CRYPTO_InitDescriptor(CRYPTO_DMAInit_TypeDef* base, CRYPTO_Init_TypeDef* crypto, CRYPTO_DMAInit_TypeDef* next,
						CRYPTO_DMA_DESCR_TypeDef* dma_data, const uint32_t* src, uint32_t src_size, uint32_t* answer)
{
	CRYPTO_DMAStructInit(base, crypto);
	base->DescriptorPtr = dma_data;
	base->SourceAddress = (void*) src;
	base->DestinationAddress = (void*) answer;

	base->ByteSwap = ENABLE;
	base->BlocksCount = (src_size != 0) ? (src_size >> 4) - 1: 0;
	base->LastDescriptor = (next == NULL) ? ENABLE : DISABLE;

	base->NextDescriptor = next;
}

/**
  * @brief	Функция шифрования с DMA байтовых данных
  * 		Примечание: Размерность data_in должны быть кратна размеру блока выбранного алгоритма шифрования
  *	@param 	crypto Ссылка на струткуру типа @ref CREYPTO_Init_TypeDef, содержащая параметры операции
  *	@param	key Байтовый массив, содержащий ключ шифрования
  *	@param	data_in	Байтовый массив, содержащий входные данные
  *	@param	data_in_size Размерность входных данных в байтах
  *	@param	iv Инициализационный ветор для проведения шифрования
  *	@param	additional Байтовый массив дополнительных аутетнифицированных данных
  *	@param	additional_size Размер дополнительных аутентифицированных данных
  *	@param	data_out Выходной массив с зашифрованными данными
  *	@param	tag Аутентификационный тэг, полученный после шифрования
  * @retval uint32_t Код завершения операции
  */
uint32_t CRYPTO_CryptGCMWithDMA(CRYPTO_Init_TypeDef* crypto, const unsigned char *key, const uint32_t *data_in, uint32_t data_in_size,
									const unsigned char* iv, const uint32_t *additional, uint32_t additional_size, uint32_t *data_out, uint32_t *tag)
{
	// cant use algorithm with 64 bit block size in GCM mode
	if (crypto->Algorithm == CRYPTO_Algo_MAGMA)
		return 1;

	CRYPTO_DMA_DESCR_TypeDef dma_init_data __attribute__((aligned (0x80)));
	CRYPTO_DMA_DESCR_TypeDef dma_payload_data __attribute__((aligned (0x80)));
	CRYPTO_DMA_DESCR_TypeDef dma_last_data __attribute__((aligned (0x80)));
	dma_init_data.CONTROL = 0;
	dma_payload_data.CONTROL = 0;
	dma_last_data.CONTROL = 0;

	CRYPTO_Init_TypeDef crypto_init, crypto_header, crypto_payload, crypto_last_block;

	crypto->InitVectorAutoUpdate = ENABLE;	// should be used for correct operation result
	CRYPTO_InitCryptoStructGCM(&crypto_init, crypto, CRYPTO_GCM_PHASE_INIT);
	CRYPTO_InitCryptoStructGCM(&crypto_payload, crypto, CRYPTO_GCM_PHASE_PAYLOAD);
	CRYPTO_InitCryptoStructGCM(&crypto_last_block, crypto, CRYPTO_GCM_PHASE_LAST_BLOCK);

	CRYPTO_DMAInit_TypeDef dma_init, dma_header, dma_payload, dma_last;

	if (key != NULL)
		CRYPTO_SetKeyBytes(crypto, key);
	if (iv != NULL)
		CRYPTO_SetIVBytes(iv);

	uint32_t add_size_bits = additional_size << 3;
	uint32_t data_size_bits = data_in_size << 3;
	const uint32_t tagger[] = {
		0,
		((add_size_bits & 0x000000FF) << 24) + ((add_size_bits & 0x0000FF00) << 8) + ((add_size_bits & 0x00FF0000) >> 8) + ((add_size_bits & 0xFF000000) >> 24),
		0,
		((data_size_bits & 0x000000FF) << 24) + ((data_size_bits & 0x0000FF00) << 8) + ((data_size_bits & 0x00FF0000) >> 8) + ((data_size_bits & 0xFF000000) >> 24),
	};

	CRYPTO_InitDescriptor(&dma_init, &crypto_init, (additional_size != 0 && additional != NULL) ? &dma_header : &dma_payload, &dma_init_data, NULL, 0, NULL);
	if (additional_size != 0 && additional != NULL)
	{
		CRYPTO_DMA_DESCR_TypeDef dma_header_data __attribute__((aligned (0x80)));
		dma_header_data.CONTROL = 0;
		CRYPTO_InitCryptoStructGCM(&crypto_header, crypto, CRYPTO_GCM_PHASE_HEADER);
		CRYPTO_InitDescriptor(&dma_header, &crypto_header, &dma_payload, &dma_header_data, additional, additional_size, NULL);
	}
	CRYPTO_InitDescriptor(&dma_payload, &crypto_payload, &dma_last, &dma_payload_data, data_in, data_in_size, data_out);
	CRYPTO_InitDescriptor(&dma_last, &crypto_last_block, NULL, &dma_last_data, tagger, 0, tag);

	while (!CRYPTO_ReadyStatus()) {}

	CRPYTO_ProcessData(&dma_init);

	while (CRYPTO_DMA_ActiveStatus()) {}
	while (!CRYPTO_ReadyStatus()) {}

	return 0;
}

/**
  * @brief	Проведение инициализационной фазы шифрования GCM
  * 		Примечание: Эта функция предполагает, что ключ уже был загружен в соответствующие регистры блока
  * @param	crypto Ссылка на струткуру типа @ref CREYPTO_Init_TypeDef, содержащая параметры операции
  *	@param 	iv Инициализационный вектор текущей операции
  * @retval uint32_t Код завершения операции
  */
uint32_t CRYPTO_GCMInitPhase(CRYPTO_Init_TypeDef* crypto, const unsigned char *iv)
{
	// cant use algorithm with 64 bit block size in GCM mode
	if (crypto->Algorithm == CRYPTO_Algo_MAGMA)
		return 1;

	CRYPTO_SetIVBytes(iv);

	crypto->GCMPhase = CRYPTO_GCM_PHASE_INIT;
	crypto->InitVectorAutoUpdate = ENABLE;

	CRYPTO_Init(crypto);

	while (!CRYPTO_ReadyStatus()) {}
	CRYPTO_StartCmd();
	while (!CRYPTO_ReadyStatus()) {}

	return 0;
}

/**
  * @brief	Проведение фазы заголовка шифрования GCM
  * 		Примечание: Эта функция предполагает, что ключ уже был загружен в соответствующие регистры блока
  * 		Является необязательной фазой, в случае если нет необходимости в дополнительных данных
  * @param	crypto Ссылка на струткуру типа @ref CREYPTO_Init_TypeDef, содержащая параметры операции
  *	@param 	additional Байтовый массив дополнительных аутентифицированных данных
  *	@param	additional_size Размер дополнительных аутентифицированных данных
  * @retval uint32_t Код завершения операции
  */
uint32_t CRYPTO_GCMHeaderPhase(CRYPTO_Init_TypeDef* crypto, const unsigned char *additional, uint32_t additional_size)
{
	if (crypto->Algorithm == CRYPTO_Algo_MAGMA)
		return 1;

	CRYPTO_GCMPhaseConfig(CRYPTO_GCM_PHASE_HEADER);

	for(uint32_t i = 0; i < additional_size; i += 16)
	{
		CRYPTO_SetDataInBytes(additional + i, 4);

		while (!CRYPTO_ReadyStatus()) {}
		CRYPTO_StartCmd();
		while (!CRYPTO_ReadyStatus()) {}
	}

	return 0;
}

/**
  * @brief	Проведение фазы шифрования GCM
  * 		Примечание: Размерность input должны быть кратна размеру блока выбранного алгоритма шифрования
  * @param	crypto Ссылка на струткуру типа @ref CREYPTO_Init_TypeDef, содержащая параметры операции
  *	@param 	input Входные данные в байтовом формате
  *	@param	input_length размер входных данных
  *	@param	output Выходные зашифрованные данные
  * @retval uint32_t Код завершения операции
  */
uint32_t CRYPTO_GCMPayloadPhase(CRYPTO_Init_TypeDef* crypto, const unsigned char *input, uint32_t input_length, unsigned char *output)
{
	if (crypto->Algorithm == CRYPTO_Algo_MAGMA)
		return 1;

	CRYPTO_GCMPhaseConfig(CRYPTO_GCM_PHASE_PAYLOAD);

	for(uint32_t i = 0; i < input_length; i += 16)
	{
		CRYPTO_SetDataInBytes(input + i, 4);

		while (!CRYPTO_ReadyStatus()) {}
		CRYPTO_StartCmd();
		while (!CRYPTO_ReadyStatus()) {}

		for(uint32_t j = 0; j < 4; j++)
		{
			uint32_t output_word = CRYPTO_GetTextOutput(j);

			output[i + (j << 2)    ] = (output_word & 0x000000FF);
			output[i + (j << 2) + 1] = (output_word & 0x0000FF00) >> 8;
			output[i + (j << 2) + 2] = (output_word & 0x00FF0000) >> 16;
			output[i + (j << 2) + 3] = (output_word & 0xFF000000) >> 24;
		}
	}

	return 0;
}

/**
  * @brief	Проведение заключительной фазы шифрования GCM
  * 		Примечание: Размерность input должны быть кратна размеру блока выбранного алгоритма шифрования
  * @param	crypto Ссылка на струткуру типа @ref CREYPTO_Init_TypeDef, содержащая параметры операции
  *	@param 	aditional_size Размер дополнительных данных, в байтах, загруженных в фазе HEADER
  *	@param	payload_size Размер данных, в байтах, загруженных в основной фазе шифрования
  *	@param	tag	Аутентификационный тэг сообщения, имеющий размерность 16 байт
  * @retval uint32_t Код завершения операции
  */
uint32_t CRYPTO_GCMLastBlockPhase(CRYPTO_Init_TypeDef* crypto, uint32_t additional_size, uint32_t payload_size, unsigned char *tag)
{
	if (crypto->Algorithm == CRYPTO_Algo_MAGMA)
		return 1;

	CRYPTO_GCMPhaseConfig(CRYPTO_GCM_PHASE_LAST_BLOCK);

	CRYPTO_SetTextInput(0, 0);
	CRYPTO_SetTextInput(2, 0);
	CRYPTO_SetTextInput(1, additional_size << 3);
	CRYPTO_SetTextInput(3, payload_size << 3);

	while (!CRYPTO_ReadyStatus()) {}
	CRYPTO_StartCmd();
	while (!CRYPTO_ReadyStatus()) {}

	for(uint32_t i = 0; i < 16; i += 4)
	{
		uint32_t output_word = CRYPTO_GetGCMTag(i >> 2);

		tag[i    ] = (output_word & 0x000000FF);
		tag[i + 1] = (output_word & 0x0000FF00) >> 8;
		tag[i + 2] = (output_word & 0x00FF0000) >> 16;
		tag[i + 3] = (output_word & 0xFF000000) >> 24;
	}

	return 0;
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
