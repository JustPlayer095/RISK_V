#include <stdio.h>
#include "calc.h"

#define EXPR_MAX_LEN  64
#define MAX_TOKENS    64

typedef struct {
  char type;   // 'n' = number, 'o' = operator
  int  value;  // для числа
  char op;     // для оператора
} Token;

static char expr[EXPR_MAX_LEN];
static int  expr_len = 0;

static int precedence(char op)
{
  if (op == '+' || op == '-') return 1;
  if (op == '*' || op == '/') return 2;
  return 0;
}

static int parse_to_rpn(const char *expr_str, Token *out, int *out_len)
{
  char op_stack[MAX_TOKENS];
  int  op_top = 0;
  int  i = 0;

  *out_len = 0;

  while (expr_str[i] != '\0') {
    char c = expr_str[i];

    if (c == ' ' || c == '\t') {
      i++;
      continue;
    }

    // Число (подряд идущие цифры)
    if (c >= '0' && c <= '9') {
      int value = 0;
      while (expr_str[i] >= '0' && expr_str[i] <= '9') {
        value = value * 10 + (expr_str[i] - '0');
        i++;
      }
      if (*out_len >= MAX_TOKENS) return -1;
      out[*out_len].type  = 'n';
      out[*out_len].value = value;
      (*out_len)++;
      continue;
    }

    // Оператор
    if (c == '+' || c == '-' || c == '*' || c == '/') {
      while (op_top > 0 &&
             precedence(op_stack[op_top - 1]) >= precedence(c)) {
        if (*out_len >= MAX_TOKENS) return -1;
        out[*out_len].type = 'o';
        out[*out_len].op   = op_stack[--op_top];
        (*out_len)++;
      }
      if (op_top >= MAX_TOKENS) return -2;
      op_stack[op_top++] = c;
      i++;
      continue;
    }

    // Неизвестный символ
    return -3;
  }

  // Выталкиваем оставшиеся операторы
  while (op_top > 0) {
    if (*out_len >= MAX_TOKENS) return -1;
    out[*out_len].type = 'o';
    out[*out_len].op   = op_stack[--op_top];
    (*out_len)++;
  }

  return 0;
}

static int eval_rpn(const Token *rpn, int len, int *result)
{
  int stack[MAX_TOKENS];
  int sp = 0;

  for (int i = 0; i < len; i++) {
    if (rpn[i].type == 'n') {
      if (sp >= MAX_TOKENS) return -1;
      stack[sp++] = rpn[i].value;
    } else if (rpn[i].type == 'o') {
      if (sp < 2) return -2;
      int b = stack[--sp];
      int a = stack[--sp];
      int r;

      switch (rpn[i].op) {
      case '+': r = a + b; break;
      case '-': r = a - b; break;
      case '*': r = a * b; break;
      case '/':
        if (b == 0) return -3;
        r = a / b;
        break;
      default:
        return -4;
      }

      stack[sp++] = r;
    } else {
      return -5;
    }
  }

  if (sp != 1) return -6;
  *result = stack[0];
  return 0;
}

void on_key_pressed(key_id_t key)
{
  switch (key) {
  case KEY_0: case KEY_1: case KEY_2: case KEY_3:
  case KEY_4: case KEY_5: case KEY_6: case KEY_7:
  case KEY_8: case KEY_9: {
    char c = '0' + (key - KEY_0);
    if (expr_len < EXPR_MAX_LEN - 1) {
      expr[expr_len++] = c;
      expr[expr_len] = '\0';
    }
    printf("EXPR: %s\r\n", expr);
    break;
  }

  case KEY_PLUS: case KEY_MINUS: case KEY_MUL: case KEY_DIV: {
    // Нельзя ставить оператор первым символом
    if (expr_len == 0) {
      break;
    }

    // Нельзя ставить два оператора подряд
    char last = expr[expr_len - 1];
    if (last == '+' || last == '-' || last == '*' || last == '/') {
      break;
    }

    char c = 0;
    if (key == KEY_PLUS)  c = '+';
    if (key == KEY_MINUS) c = '-';
    if (key == KEY_MUL)   c = '*';
    if (key == KEY_DIV)   c = '/';

    if (expr_len < EXPR_MAX_LEN - 1) {
      expr[expr_len++] = c;
      expr[expr_len] = '\0';
    }
    printf("EXPR: %s\r\n", expr);
    break;
  }

  case KEY_DEL:
    if (expr_len > 0) {
      expr[--expr_len] = '\0';
    }
    printf("EXPR: %s\r\n", expr);
    break;

  case KEY_EQ: {
    if (expr_len == 0){
      break;
    }

    char last = expr[expr_len - 1];
    if (last == '+' || last == '-' || last == '*' || last == '/') {
        break;
    }

    Token rpn[MAX_TOKENS];
    int rpn_len;
    int rc = parse_to_rpn(expr, rpn, &rpn_len);
    if (rc != 0) {
      printf("Parse error: %d\r\n", rc);
      return;
    }

    int result;
    rc = eval_rpn(rpn, rpn_len, &result);
    if (rc != 0) {
      printf("Eval error: %d\r\n", rc);
      return;
    }

    printf("RESULT: %d\r\n", result);

    // Очистить выражение после вычисления
    expr_len = 0;
    expr[0] = '\0';
    break;
  }

  default:
    break;
  }
}

