#include "gpio_helpers.h"
#include <stddef.h>

void gpio_reset_pin(GPIO_TypeDef* GPIOx, uint32_t Pin)
{
    // 1. Отключить выход (OUTENCLR)
    GPIO_OutCmd(GPIOx, Pin, DISABLE);
    // 2. Очистить данные выхода (DATAOUTCLR)
    GPIO_ClearBits(GPIOx, Pin);
    // 3. Отключить альтернативную функцию (ALTFUNCCLR)
    GPIO_AltFuncCmd(GPIOx, Pin, DISABLE); 
    // 4. Сбросить номер альтернативной функции (ALTFUNCNUM = 0)
    GPIO_AltFuncNumConfig(GPIOx, Pin, GPIO_AltFuncNum_None);
    // 5. Сбросить режимы в значения по умолчанию
    GPIO_InModeConfig(GPIOx, Pin, GPIO_InMode_Disable);
    GPIO_PullModeConfig(GPIOx, Pin, GPIO_PullMode_Disable);
    GPIO_OutModeConfig(GPIOx, Pin, GPIO_OutMode_PP);
    // 6. Отключить прерывания, если были включены
    GPIO_ITCmd(GPIOx, Pin, DISABLE);
    GPIO_ITStatusClear(GPIOx, Pin);
    // 7. Отключить квалификатор входа
    GPIO_QualCmd(GPIOx, Pin, DISABLE);
    GPIO_SyncCmd(GPIOx, Pin, DISABLE);
}

void gpio_init_output(GPIO_TypeDef* GPIOx, uint32_t Pin, uint8_t initial_state)
{
    // Сначала сбрасываем состояние пина
    gpio_reset_pin(GPIOx, Pin);
    
    GPIO_Init_TypeDef gpio;
    GPIO_StructInit(&gpio);
    gpio.Pin = Pin;
    gpio.Out = ENABLE;                // включить выход
    gpio.AltFunc = DISABLE;           // обычный GPIO
    gpio.AltFuncNum = GPIO_AltFuncNum_None;
    gpio.OutMode = GPIO_OutMode_PP;   // push-pull
    gpio.PullMode = GPIO_PullMode_Disable;
    GPIO_Init(GPIOx, &gpio);
    
    // Устанавливаем начальное состояние
    if (initial_state) {
      GPIO_SetBits(GPIOx, Pin);
    } else {
      GPIO_ClearBits(GPIOx, Pin);
    }
}

static uint8_t gpio_qual_initialized = 0;

void gpio_init_input(GPIO_TypeDef* GPIOx, uint32_t Pin, GPIO_PullMode_TypeDef pull_mode)
{
    // Сначала сбрасываем состояние пина
    gpio_reset_pin(GPIOx, Pin);
    
    GPIO_Init_TypeDef gpio;
    GPIO_StructInit(&gpio);
    gpio.Pin = Pin;
    gpio.Out = DISABLE;               
    gpio.AltFunc = DISABLE;
    gpio.AltFuncNum = GPIO_AltFuncNum_None;
    gpio.InMode = GPIO_InMode_Schmitt;
    gpio.PullMode = pull_mode;
    GPIO_Init(GPIOx, &gpio);

    // Аппаратная фильтрация дребезга: пересинхронизация + квалификатор входа
    GPIO_SyncCmd(GPIOx, Pin, ENABLE);
    uint8_t port_mask = 0;
    
    if (!(gpio_qual_initialized & port_mask)) {
      GPIO_QualSampleConfig(GPIOx, 1000); // период дискретизации фильтра (такты HCLK)
      gpio_qual_initialized |= port_mask;
    }
    
    GPIO_QualModeConfig(GPIOx, Pin, GPIO_QualMode_6Sample); // более жесткая фильтрация
    GPIO_QualCmd(GPIOx, Pin, ENABLE);
}

void gpio_init_input_irq(GPIO_TypeDef* GPIOx, uint32_t Pin, GPIO_PullMode_TypeDef pull_mode, volatile uint8_t* last_state)
{
    // Сначала инициализируем как обычный вход
    gpio_init_input(GPIOx, Pin, pull_mode);
    
    // Настраиваем прерывания
    GPIO_ITTypeConfig(GPIOx, Pin, GPIO_IntType_Edge);
    GPIO_ITPolConfig(GPIOx, Pin, GPIO_IntPol_Negative);
    GPIO_ITEdgeConfig(GPIOx, Pin, GPIO_IntEdge_Any); // прерывание по обоим фронтам
    GPIO_ITStatusClear(GPIOx, Pin);
    
    // Инициализировать предыдущее состояние кнопки (отпущена = 1)
    if (last_state != NULL) {
      *last_state = GPIO_ReadBit(GPIOx, Pin) ? 1 : 0;
    }
    
    // Включить прерывание
    GPIO_ITCmd(GPIOx, Pin, ENABLE);
}

