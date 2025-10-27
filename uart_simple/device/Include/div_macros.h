/**
  ******************************************************************************
  * @file    plib015_HASH.h
  *
  * @brief   Файл содержит макросы с ассемблерными вставками для устранения
  *          ошибки, возникающей при делении, макросы имеют разные реализации
  *          для разных базовых типов, общее правило:
  *          DIVIDE_{RETURN_TYPE}_{DIVEDEND}_{DIVIDER}(ret, dividend, divider)
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
  * <h2><center>&copy; 2025 АО "НИИЭТ"</center></h2>
  ******************************************************************************
  */


#define DIVIDE_INT_INT_INT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.w fa4,%1\n"\
        "fcvt.s.w fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.w.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_FLOAT_INT_INT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.w fa4,%1\n"\
        "fcvt.s.w fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fmv.x.w %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_UINT_INT_INT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.w fa4,%1\n"\
        "fcvt.s.w fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.wu.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_INT_FLOAT_INT(output, val1, val2)\
    asm volatile (\
        "fmv.w.x fa4,%1\n"\
        "fcvt.s.w fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.w.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_FLOAT_FLOAT_INT(output, val1, val2)\
    asm volatile (\
        "fmv.w.x fa4,%1\n"\
        "fcvt.s.w fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fmv.x.w %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_UINT_FLOAT_INT(output, val1, val2)\
    asm volatile (\
        "fmv.w.x fa4,%1\n"\
        "fcvt.s.w fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.wu.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_INT_UINT_INT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.wu fa4,%1\n"\
        "fcvt.s.w fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.w.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_FLOAT_UINT_INT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.wu fa4,%1\n"\
        "fcvt.s.w fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fmv.x.w %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_UINT_UINT_INT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.wu fa4,%1\n"\
        "fcvt.s.w fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.wu.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_INT_INT_FLOAT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.w fa4,%1\n"\
        "fmv.w.x fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.w.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_FLOAT_INT_FLOAT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.w fa4,%1\n"\
        "fmv.w.x fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fmv.x.w %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_UINT_INT_FLOAT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.w fa4,%1\n"\
        "fmv.w.x fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.wu.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_INT_FLOAT_FLOAT(output, val1, val2)\
    asm volatile (\
        "fmv.w.x fa4,%1\n"\
        "fmv.w.x fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.w.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_FLOAT_FLOAT_FLOAT(output, val1, val2)\
    asm volatile (\
        "fmv.w.x fa4,%1\n"\
        "fmv.w.x fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fmv.x.w %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_UINT_FLOAT_FLOAT(output, val1, val2)\
    asm volatile (\
        "fmv.w.x fa4,%1\n"\
        "fmv.w.x fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.wu.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_INT_UINT_FLOAT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.wu fa4,%1\n"\
        "fmv.w.x fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.w.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_FLOAT_UINT_FLOAT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.wu fa4,%1\n"\
        "fmv.w.x fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fmv.x.w %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_UINT_UINT_FLOAT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.wu fa4,%1\n"\
        "fmv.w.x fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.wu.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_INT_INT_UINT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.w fa4,%1\n"\
        "fcvt.s.wu fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.w.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_FLOAT_INT_UINT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.w fa4,%1\n"\
        "fcvt.s.wu fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fmv.x.w %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_UINT_INT_UINT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.w fa4,%1\n"\
        "fcvt.s.wu fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.wu.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_INT_FLOAT_UINT(output, val1, val2)\
    asm volatile (\
        "fmv.w.x fa4,%1\n"\
        "fcvt.s.wu fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.w.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_FLOAT_FLOAT_UINT(output, val1, val2)\
    asm volatile (\
        "fmv.w.x fa4,%1\n"\
        "fcvt.s.wu fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fmv.x.w %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_UINT_FLOAT_UINT(output, val1, val2)\
    asm volatile (\
        "fmv.w.x fa4,%1\n"\
        "fcvt.s.wu fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.wu.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_INT_UINT_UINT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.wu fa4,%1\n"\
        "fcvt.s.wu fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.w.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_FLOAT_UINT_UINT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.wu fa4,%1\n"\
        "fcvt.s.wu fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fmv.x.w %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )

#define DIVIDE_UINT_UINT_UINT(output, val1, val2)\
    asm volatile (\
        "fcvt.s.wu fa4,%1\n"\
        "fcvt.s.wu fa5,%2\n"\
        "fdiv.s fa4,fa4,fa5\n"\
        "fcvt.wu.s %0,fa4\n"\
        : "=r"((output))\
        : "r"((val1)), "r"((val2))\
        : "fa4", "fa5", "memory"\
    )
