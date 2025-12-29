#ifndef GPIO_HELPERS_H
#define GPIO_HELPERS_H

#include <stdint.h>
#include "../device/Include/K1921VG015.h"
#include "../plib/inc/plib015_gpio.h"

void gpio_reset_pin(GPIO_TypeDef* GPIOx, uint32_t Pin);
void gpio_init_output(GPIO_TypeDef* GPIOx, uint32_t Pin, uint8_t initial_state);
void gpio_init_input(GPIO_TypeDef* GPIOx, uint32_t Pin, GPIO_PullMode_TypeDef pull_mode);
void gpio_init_input_irq(GPIO_TypeDef* GPIOx, uint32_t Pin, GPIO_PullMode_TypeDef pull_mode, volatile uint8_t* last_state);

#endif // GPIO_HELPERS_H

