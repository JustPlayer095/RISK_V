/*******************************************************************************
 *
 *  МОДУЛЬ        : Inputs.c
 *
 *  Автор         : Л.Стасенко
 *  Дата начала   : 12.12.2014
 *  Версия        : 1.1
 *  Комментарии    : Обработка логических входов с антидребезгом
 *
 ******************************************************************************/
/*
  В режиме OSDP у ридера имеется два свободных входа (оранжевый, LED-G и
  коричневый, LED-R), которые могут использоваться как входы для RTE & DC.
*/


#include "BoardGpio.h"
#include "OsdpInputs.h"
#include "OSDPQueue.h" 

/*******************************************************************************                                                             :
 *                       МАССИВ СТРУКТУР ЛОГИЧЕСКИХ ВХОДОВ
 *******************************************************************************/
volatile tLogicInput   Inputs[2];

/*******************************************************************************                                                             :
 *                ФУНКЦИИ МОДУЛЯ ОБЩЕГО НАЗНАЧЕНИЯ
 *******************************************************************************/

//////////////////////////////////////////////////////////////////////////////////////
//  Function    : InitInputs
//  Description : Инициализация входов, в данном случае пустая (нет АЦП)
//  Input       : None
//  Output      : None
//  Comments    : None
//////////////////////////////////////////////////////////////////////////////////////
void InitOsdpInputs(void)
{
  memset((void*)&Inputs, 0, sizeof(tLogicInput));
  Inputs[0].MaxCount = Inputs[1].MaxCount = DEFAULT_MAX_INP_COUNT;
}

//////////////////////////////////////////////////////////////////////////////////////
//  Function    : CheckInputs
//  Description : Вызывается по таймеру и проверяет дополнительные логические входы
//  Input       : None
//  Output      : None
//  Comments    : 
//////////////////////////////////////////////////////////////////////////////////////
void CheckOsdpInputs(void)
{
tInputStates ts;
uint8_t tmp[INPUT_STATUS_COUNT];
uint8_t changed = 0;

  // =======================================================
  // Проверяем вход зеленого светодиода - у нас DC
  if (LEDG_CONTROL_LOW)
    ts = istActive;
  else
    ts = istNormal;
  // Состояние уже было зафиксировано
  if (Inputs[0].OldState == ts) {
    Inputs[0].Count = 0;
  }
  else {
    // А новое состояние уже было?
    if (Inputs[0].NewState == ts) {
      if (Inputs[0].Count >= Inputs[0].MaxCount) {
        // Пора - вход активирован
        Inputs[0].OldState = Inputs[0].NewState;
        Inputs[0].Count = 0;
        changed = 1;
      }
      else
        Inputs[0].Count++;
    }
    else {
      Inputs[0].NewState = ts;
      Inputs[0].Count = 0;
    }
  }
  
  // =======================================================
  // Проверяем вход красного светодиода - у нас RTE
  if (LEDR_CONTROL_LOW)
    ts = istActive;
  else
    ts = istNormal;
  // Состояние уже было зафиксировано
  if (Inputs[1].OldState == ts) {
    Inputs[1].Count = 0;
  }
  else {
    // А новое состояние уже было?
    if (Inputs[1].NewState == ts) {
      if (Inputs[1].Count >= Inputs[1].MaxCount) {
        // Пора - вход активирован
        Inputs[1].OldState = Inputs[1].NewState;
        Inputs[1].Count = 0;
        changed = 1;
      }
      else
        Inputs[1].Count++;
    }
    else {
      Inputs[1].NewState = ts;
      Inputs[1].Count = 0;
    }
  }
  // If any changed - add to queue
  if (changed) {
    tmp[0] = Inputs[0].OldState;
    tmp[1] = Inputs[1].OldState;
    __disable_irq();
    AddInputStatusQueue(tmp, INPUT_STATUS_COUNT);
    __enable_irq();
  }
}


/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
