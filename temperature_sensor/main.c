//-- Includes ------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>
#include "device/Include/K1921VG015.h"
#include "device/Include/retarget.h"
#include "device/Include/plic.h"
#include "plib/inc/plib015_tsens.h"
#include "plib/inc/plib015_adcsar.h"    
#include <plib015.h>
#include <system_k1921vg015.h>
//-- Defines -------------------------------------------------------------------
#define GPIOA_ALL_Msk 0xFFFF
#define GPIOB_ALL_Msk 0xFFFF

#define LEDS_MSK 0xFF00
#define LED0_MSK (1 << 8)
#define LED1_MSK (1 << 9)
#define LED2_MSK (1 << 10)
#define LED3_MSK (1 << 11)
#define LED4_MSK (1 << 12)
#define LED5_MSK (1 << 13)
#define LED6_MSK (1 << 14)
#define LED7_MSK (1 << 15)

// Константы для датчика температуры
#define TEMP_SENSOR_CHANNEL 10  // Канал ADC для датчика температуры
#define TEMP_MEASURE_INTERVAL 1000000  // Интервал измерения в микросекундах (1 сек)
#define TEMP_OFFSET 0x1F8  // Смещение для калибровки (примерное значение)
#define TEMP_SLOPE 0x1F8   // Наклон для калибровки (примерное значение)

void TMR32_IRQHandler();
void ADC_IRQHandler();

// Инициализация светодиодов
void BSP_led_init()
{
    // Разрешаем тактирование GPIOA
    RCU->CGCFGAHB_bit.GPIOAEN = 1;
    // Включаем GPIOA
    RCU->RSTDISAHB_bit.GPIOAEN = 1;
    GPIOA->OUTENSET = LEDS_MSK;
    GPIOA->DATAOUTSET = LEDS_MSK;
}

// Инициализация таймера для периодических измерений
void TMR32_init(uint32_t period)
{
    RCU->CGCFGAPB_bit.TMR32EN = 1;
    RCU->RSTDISAPB_bit.TMR32EN = 1;

    // Записываем значение периода в CAPCOM[0]
    TMR32->CAPCOM[0].VAL = period - 1;
    // Выбираем режим счета от 0 до значения CAPCOM[0]
    TMR32->CTRL_bit.MODE = 1;

    // Разрешаем прерывание по совпадению значения счетчика и CAPCOM[0]
    TMR32->IM = 2;

    // Настраиваем обработчик прерывания для TMR32
    PLIC_SetIrqHandler(Plic_Mach_Target, IsrVect_IRQ_TMR32, TMR32_IRQHandler);
    PLIC_SetPriority(IsrVect_IRQ_TMR32, 0x1);
    PLIC_IntEnable(Plic_Mach_Target, IsrVect_IRQ_TMR32);
}

// Инициализация ADC для датчика температуры
void ADC_init()
{
    // Разрешаем тактирование ADC
    RCU->CGCFGAPB_bit.ADCSAREN = 1;
    RCU->RSTDISAPB_bit.ADCSAREN = 1;
    
    // Включаем ADC
    ADCSAR->ACTL_bit.ADCEN = 1;
    
    // Настраиваем прерывания ADC
    ADCSAR->IM = 0x01;  // Разрешаем прерывание по завершению преобразования
    
    // Настраиваем обработчик прерывания для ADC
    PLIC_SetIrqHandler(Plic_Mach_Target, IsrVect_IRQ_ADC, ADC_IRQHandler);
    PLIC_SetPriority(IsrVect_IRQ_ADC, 0x2);
    PLIC_IntEnable(Plic_Mach_Target, IsrVect_IRQ_ADC);
}

// Инициализация датчика температуры
void TSENS_init()
{
    // Включаем датчик температуры
    TSENS_Cmd(1);
    
    // Настраиваем датчик температуры
    TSENS->CTRL_bit.ADCSEL = 1;  // Выбираем ADC для измерения
    TSENS->CTRL_bit.STYP = 0;    // Тип измерения
    TSENS->CTRL_bit.ISEL = 1;    // Источник тока
}

// Функция для запуска измерения температуры
void start_temperature_measurement()
{
    // Запускаем преобразование ADC (упрощенная версия)
    // Здесь нужно настроить канал и запустить преобразование
    // Пока используем заглушку с имитацией измерения
    extern volatile uint16_t adc_result;
    extern volatile uint8_t measurement_ready;
    
    // Имитируем измерение с небольшим случайным изменением
    static uint16_t base_value = 0x1F8;
    extern volatile uint32_t measurement_count;
    base_value += (measurement_count % 10) - 5; // Небольшие колебания
    adc_result = base_value;
    measurement_ready = 1;
}

// Функция для конвертации ADC значения в температуру
float convert_adc_to_temperature(uint16_t adc_value)
{
    // Простая линейная конвертация (требует калибровки для точных значений)
    // Формула: Температура = (ADC_Value - OFFSET) / SLOPE
    float temperature = ((float)adc_value - (float)TEMP_OFFSET) / (float)TEMP_SLOPE;
    return temperature;
}

// Инициализация периферии
void periph_init()
{
    BSP_led_init();
    SystemInit();
    SystemCoreClockUpdate();
    BSP_led_init();
    
    // Инициализируем UART
    retarget_init();
    
    // Небольшая задержка для стабилизации UART
    for(volatile int i = 0; i < 100000; i++);
    
    printf("K1921VG015 SYSCLK = %d MHz\n", (int)(SystemCoreClock / 1E6));
    
    // Небольшая задержка между сообщениями
    for(volatile int i = 0; i < 50000; i++);
    
    printf("  UID[0] = 0x%X  UID[1] = 0x%X  UID[2] = 0x%X  UID[3] = 0x%X\n", 
           (unsigned int)PMUSYS->UID[0], (unsigned int)PMUSYS->UID[1], 
           (unsigned int)PMUSYS->UID[2], (unsigned int)PMUSYS->UID[3]);
    
    for(volatile int i = 0; i < 50000; i++);
    
    printf("  Инициализация датчика температуры\n");
    
    // Инициализируем датчик температуры и ADC
    TSENS_init();
    ADC_init();
    
    for(volatile int i = 0; i < 50000; i++);
    
    printf("  UART настроен на 115200 бод\n");
    
    for(volatile int i = 0; i < 50000; i++);
    
    printf("  Система готова к работе\n");
}

//--- USER FUNCTIONS ----------------------------------------------------------------------

volatile uint32_t led_shift;
volatile uint16_t adc_result = 0;
volatile uint8_t measurement_ready = 0;
volatile uint32_t measurement_count = 0;

//-- Main ----------------------------------------------------------------------
int main(void)
{
    periph_init();
    
    // Инициализируем таймер для периодических измерений (каждую секунду)
    TMR32_init(SystemCoreClock);
    InterruptEnable();
    
    led_shift = LED0_MSK;
    
    // Запускаем первое измерение
    printf("  Запуск измерений температуры...\n");
    
    // Задержка для стабилизации UART
    for(volatile int i = 0; i < 100000; i++);
    
    start_temperature_measurement();
    
    while (1)
    {
        // Если измерение готово, выводим результат
        if (measurement_ready)
        {
            measurement_ready = 0;
            
            // Конвертируем ADC значение в температуру
            float temperature = convert_adc_to_temperature(adc_result);
            
            // Выводим результат
            printf("Измерение #%d: ADC = 0x%03X, Температура = %.2f°C\n", 
                   (int)measurement_count, adc_result, temperature);
            
            // Задержка для стабилизации UART
            for(volatile int i = 0; i < 100000; i++);
            
            // Запускаем следующее измерение
            start_temperature_measurement();
        }
    }

    return 0;
}

//-- IRQ INTERRUPT HANDLERS ---------------------------------------------------------------

// Обработчик прерывания таймера
void TMR32_IRQHandler()
{
    // Переключаем светодиоды для индикации работы
    GPIOA->DATAOUTTGL = led_shift;
    led_shift = led_shift << 1;
    if (led_shift > LED7_MSK)
        led_shift = LED0_MSK;
    
    // Сбрасываем флаг прерывания таймера
    TMR32->IC = 3;
}

// Обработчик прерывания ADC
void ADC_IRQHandler()
{
    // Упрощенная версия - пока не используется
    // В реальной реализации здесь будет чтение результата ADC
    measurement_count++;
}

