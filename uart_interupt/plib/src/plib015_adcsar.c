/**
  ******************************************************************************
  * @file    plib015_ADCSAR.c
  *
  * @brief   Файл содержит реализацию функций для работы с ADCSAR
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
#include "plib015_adcsar.h"

/** @addtogroup Peripheral
  * @{
  */

/** @addtogroup ADCSAR
  * @{
  */

/** @defgroup ADCSAR_Private Приватные данные
  * @{
  */

/** @defgroup ADCSAR_Private_Defines Приватные константы
  * @{
  */

/**
  * @}
  */

/** @defgroup ADCSAR_Private_Functions Приватные функции
  * @{
  */

/**
  * @brief   Выбор каналов для запроса секвенсора
  * @param   SEQ_Num  Выбор секвенсора
  * @param   ReqNum  Выбор запроса
  * @param   Channel_Num  Выбор канала
  * @retval  void
  */
void ADCSAR_SEQ_ReqConfig(ADCSAR_SEQ_Num_TypeDef SEQ_Num, ADCSAR_SEQ_ReqNum_TypeDef ReqNum, ADCSAR_CH_Num_TypeDef Channel_Num)
{
    uint32_t req_pos;

    assert_param(IS_ADCSAR_SEQ_NUM(SEQ_Num));
    assert_param(IS_ADCSAR_SEQ_REQ_NUM(ReqNum));
    assert_param(IS_ADCSAR_CH_NUM(Channel_Num));

    req_pos = ((uint32_t)ReqNum % 4) * 8;

    if (ReqNum > ADCSAR_SEQ_ReqNum_7)
        MODIFY_REG(ADCSAR->SEQ[SEQ_Num].SRQSEL, 0x3F << req_pos, Channel_Num << req_pos); // srqsel2
    else if (ReqNum > ADCSAR_SEQ_ReqNum_3)
        MODIFY_REG(ADCSAR->SEQ[SEQ_Num].SRQSEL, 0x3F << req_pos, Channel_Num << req_pos); // srqsel1
    else
        MODIFY_REG(ADCSAR->SEQ[SEQ_Num].SRQSEL, 0x3F << req_pos, Channel_Num << req_pos); // srqsel0
}

/**
  * @brief   Устанавливает все регистры ADCSAR значениями по умолчанию
  * @retval  void
  */
void ADCSAR_DeInit()
{
    RCU_ADCSARRstCmd(DISABLE);
    RCU_ADCSARRstCmd(ENABLE);
}

/**
  * @brief   Инициализирует секвенсоры АЦП согласно параметрам структуры InitStruct
  * @param   SEQ_Num  Выбор секвенсора
  * @param   InitStruct  Указатель на структуру типа @ref ADCSAR_SEQ_Init_TypeDef,
  *                      которая содержит конфигурационную информацию
  * @retval  void
  */
void ADCSAR_SEQ_Init(ADCSAR_SEQ_Num_TypeDef SEQ_Num, ADCSAR_SEQ_Init_TypeDef* InitStruct)
{
    ADCSAR_SEQ_StartEventConfig(SEQ_Num, InitStruct->StartEvent);
    ADCSAR_SEQ_SwStartEnCmd(SEQ_Num, InitStruct->SWStartEn);
    for (uint32_t i = 0; i < ADCSAR_SEQ_Req_Total; i++) {
        ADCSAR_SEQ_ReqConfig(SEQ_Num, (ADCSAR_SEQ_ReqNum_TypeDef)i, InitStruct->Req[i]);
    }
    ADCSAR_SEQ_ReqMaxConfig(SEQ_Num, InitStruct->ReqMax);
    ADCSAR_SEQ_ReqAverageConfig(SEQ_Num, InitStruct->ReqAverage);
    ADCSAR_SEQ_ReqAverageCmd(SEQ_Num, InitStruct->ReqAverageEn);
    ADCSAR_SEQ_RestartConfig(SEQ_Num, InitStruct->RestartCount);
    ADCSAR_SEQ_RestartAverageCmd(SEQ_Num, InitStruct->RestartAverageEn);
    ADCSAR_SEQ_SetRestartTimer(SEQ_Num, InitStruct->RestartTimer);
    for (uint32_t i = 0; i < ADCSAR_DC_Total; i++) {
        ADCSAR_SEQ_DCEnableCmd(SEQ_Num, (ADCSAR_DC_Num_TypeDef)i, InitStruct->DCEn[i]);
    }
    ADCSAR_SEQ_DMAConfig(SEQ_Num, InitStruct->DMAFIFOLevel);
    ADCSAR_SEQ_DMACmd(SEQ_Num, InitStruct->DMAEn);
}

/**
  * @brief   Заполнение каждого члена структуры InitStruct значениями по умолчанию
  * @param   InitStruct  Указатель на структуру типа @ref ADCSAR_SEQ_Init_TypeDef,
  *                      которую необходимо проинициализировать
  * @retval  void
  */
void ADCSAR_SEQ_StructInit(ADCSAR_SEQ_Init_TypeDef* InitStruct)
{
    InitStruct->StartEvent = ADCSAR_SEQ_StartEvent_SwReq;
    InitStruct->SWStartEn = DISABLE;
    for (uint32_t i = 0; i < ADCSAR_SEQ_Req_Total; i++) {
        InitStruct->Req[i] = ADCSAR_CH_Num_0;
    }
    InitStruct->ReqMax = ADCSAR_SEQ_ReqNum_0;
    InitStruct->ReqAverage = ADCSAR_SEQ_Average_2;
    InitStruct->ReqAverageEn = DISABLE;
    InitStruct->RestartCount = 0;
    InitStruct->RestartAverageEn = DISABLE;
    InitStruct->RestartTimer = 0;
    for (uint32_t i = 0; i < ADCSAR_DC_Total; i++) {
        InitStruct->DCEn[i] = DISABLE;
    }
    InitStruct->DMAFIFOLevel = ADCSAR_SEQ_DMAFIFOLevel_1;
    InitStruct->DMAEn = DISABLE;
}

/**
  * @brief   Инициализирует цифровые компараторы АЦП согласно параметрам структуры InitStruct
  * @param   DC_Num  Выбор компаратора
  * @param   InitStruct  Указатель на структуру типа @ref ADCSAR_DC_Init_TypeDef,
  *                      которая содержит конфигурационную информацию
  * @retval  void
  */
void ADCSAR_DC_Init(ADCSAR_DC_Num_TypeDef DC_Num, ADCSAR_DC_Init_TypeDef* InitStruct)
{
    ADCSAR_DC_OutputCmd(DC_Num, InitStruct->DCOutput);
    ADCSAR_DC_SetThresholdLow(DC_Num, InitStruct->ThresholdLow);
    ADCSAR_DC_SetThresholdHigh(DC_Num, InitStruct->ThresholdHigh);
    ADCSAR_DC_SourceConfig(DC_Num, InitStruct->Source);
    ADCSAR_DC_ChannelConfig(DC_Num, InitStruct->Channel);
    ADCSAR_DC_Config(DC_Num, InitStruct->Mode, InitStruct->Condition);
}

/**
  * @brief   Заполнение каждого члена структуры InitStruct значениями по умолчанию
  * @param   InitStruct  Указатель на структуру типа @ref ADCSAR_DC_Init_TypeDef,
  *                      которую необходимо проинициализировать
  * @retval  void
  */
void ADCSAR_DC_StructInit(ADCSAR_DC_Init_TypeDef* InitStruct)
{
    InitStruct->DCOutput = DISABLE;
    InitStruct->ThresholdLow = 0;
    InitStruct->ThresholdHigh = 0;
    InitStruct->Source = ADCSAR_DC_Source_EOC;
    InitStruct->Channel = ADCSAR_CH_Num_0;
    InitStruct->Mode = ADCSAR_DC_Mode_Multiple;
    InitStruct->Condition = ADCSAR_DC_Condition_Low;
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
