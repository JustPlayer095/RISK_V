/*******************************************************************************
 *
 *  МОДУЛЬ        : CcittCrc16.h
 *
 *  Автор         : Л.Стасенко
 *  Дата начала   : 02.12.2014
 *  Версия        : 1.1
 *  Комментарии    : Расчет CRC-16 CCITT
 *
 ******************************************************************************/
/*
  Name  : CRC-16 CCITT
  Poly  : 0x1021    x^16 + x^12 + x^5 + 1
  Init  : 0xFFFF
  Revert: false
  XorOut: 0x0000
  Check : 0x29B1 ("123456789")
  MaxLen: 4095 байт (32767 бит) - обнаружение
          одинарных, двойных, тройных и всех 
          нечетных ошибок
*/

#ifndef CRC16_CCITT_DEF_H
#define CRC16_CCITT_DEF_H

#include "AppTypes.h"

/*******************************************************************************                                                             :
 *      ПРИ ДАННОМ DEFINE РАСЧЕТ ВЕДЕТСЯ ТАБЛИЧНЫМ МЕТОДОМ, ИНАЧЕ СЧЕТНЫМ
 *******************************************************************************/
#define CALC_CRC_TABLE
// Если определено, то включена проверка
//#define TEST_VECTORS

#ifdef TEST_VECTORS
extern void TestCrcVectors(void);
#endif

// В OSDP пример из аппендикса стандарта работает при таком начальном CRC
#define OSDP_INIT_CRC16   0x1D0F
// А это стандартное значение для CCITT вычисления CRC (у нас не используется)
//#define INITIAL_CRC16   0xFFFF

extern uint16_t UpdateCrc16(uint16_t crc, uint8_t byte);
extern uint16_t CalcCrc16(uint16_t initcrc, uint8_t *data, uint16_t len);
extern int OsdpCrcIsOk(uint8_t *data, uint16_t len);

#ifdef QR_READER
//uint16_t GetCrc16(uint16_t initialcrc, uint8_t *pdata, uint16_t len);
//uint16_t GetCrc16E4000(uint8_t *pdata, uint16_t len);
#endif

#endif    // CRC16_CCITT_DEF_H

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
