#ifndef CALC_H
#define CALC_H

#include <stdint.h>

// Логические идентификаторы кнопок калькулятора
typedef enum {
  KEY_0, KEY_1, KEY_2, KEY_3,
  KEY_4, KEY_5, KEY_6, KEY_7,
  KEY_8, KEY_9,
  KEY_PLUS, KEY_MINUS, KEY_MUL, KEY_DIV,
  KEY_DEL, KEY_EQ,
  KEY_COUNT
} key_id_t;

// Обработка нажатия логической кнопки калькулятора
void on_key_pressed(key_id_t key);

#endif // CALC_H

