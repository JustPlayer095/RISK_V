/*******************************************************************************
 *
 *  МОДУЛЬ        : OSDPQueue.h
 *
 *  Автор         : Л.Стасенко
 *  Дата начала   : 27.07.2017
 *  Версия        : 1.0
 *  Комментарии    : Очереди данных на отправку для протокола OSDP (V 2.1.6 - 2014)
 *
 ******************************************************************************/
/*
  Очереди данных для отправки при запросах POLL на случай, если опрос 
  в какие-то моменты осуществляется медленнее, чем поступают данные (например, 
  одновременно сработало несколько логических входов в течение короткого времени).
  Очереди доступны извне модуля только через соответствующие функции.
*/


#ifndef OSDP_QUEUE_DEF_H
#define OSDP_QUEUE_DEF_H

// Defined types of output data
#include "OSDP.h" 
#ifdef QR_READER
#include "QrReader.h"
#endif

/*******************************************************************************
 *        How long are every queue. Some values depend from device
 *******************************************************************************/
// Queue for card codes
#define   CARD_DATA_Q_LENGTH      4
#define   MAX_CARD_BYTES          8
// Queue for device local status
#define   LOCAL_SATAT_Q_LENGTH    4
// Queue for Input status. Queue length depend from inputs count
#define   INPUT_STATUS_COUNT      2
#define   INPUT_SATAT_Q_LENGTH    (INPUT_STATUS_COUNT * 2)
// Queue for Output status. Length depend from outputs count
#define   OUTPUT_STATUS_COUNT     1
#define   OUTPUT_SATAT_Q_LENGTH   (OUTPUT_STATUS_COUNT * 4)
// Queue for kepad data

#define   QR_DATA_Q_LENGTH        4

//////////////////////////////////////////////////////////////////////////////
//                   ОЧЕРЕДЬ ПОДНЕСЕННЫХ КАРТ
//    Packet format for OSDP answer:
//0   Reader Number     0=First Reader 1=Second Reader
//1   Format Code       0 = not specified, raw bit array, 1 = P/data/P (Wiegand)
//2   Bit Count LSB     2-byte size (in bits) of the data at the end of the record
//3   Bit Count MSB
//4-N Data              8 bits of card data per data byte MSB to LSB (left justified)
//////////////////////////////////////////////////////////////////////////////
#pragma pack(1)
// Structure for store card ID data from reader N
typedef struct {
  uint8_t     ReaderNo;       // Reader number (0, 1, ...)
  uint8_t     CardFormat;     // 0-RAW, 1-RAW with parity
  uint8_t     BitCount;       // Bytes count in card data - low byte
  uint8_t     CntMsb;         // Dummy high byte of codebits
  uint8_t     CardBytes[MAX_CARD_BYTES];
  uint8_t     ByteCount;      // Used for transfer to output
} tCardData;
// Card data queue
typedef struct {
  uint8_t     ReadPtr;
  uint8_t     WritePtr;
  //
  tCardData   CardData[CARD_DATA_Q_LENGTH];
} tSendCardDataQueue;
typedef tSendCardDataQueue pSendCardDataQueue;
#pragma pack()

//////////////////////////////////////////////////////////////////////////////
//              ЛОКАЛЬНЫЙ СТАТУС РИДЕРА (тампер, питание)
//typedef struct {
//  uint8_t   Changed;    // 1 - изменен
//  uint8_t   Tamper;     // 0 - ОК, 1 - тампер
//  uint8_t   Power;      // 0 - ОК, 1 - power failure
//} tLocalStatus;
//typedef tLocalStatus *pLocalStatus;
//////////////////////////////////////////////////////////////////////////////
#pragma pack(1)
typedef struct {
  uint8_t   Tamper;     // 0 - ОК, 1 - тампер
  uint8_t   Power;      // 0 - ОК, 1 - power failure
} tLocalStatus;
typedef tLocalStatus *pLocalStatus;
extern tLocalStatus OsdpLocalStatus;
//
typedef struct {
  uint8_t       ReadPtr;
  uint8_t       WritePtr;
  //
  tLocalStatus  Status[LOCAL_SATAT_Q_LENGTH];
} tLocalStatusQueue;
typedef tLocalStatusQueue pLocalStatusQueue;
#pragma pack()

//////////////////////////////////////////////////////////////////////////////
//           СТАТУС ЛОГИЧЕСКИХ (и не только) ВХОДОВ
//
//
//////////////////////////////////////////////////////////////////////////////
#pragma pack(1)
typedef struct {
  uint8_t Stat[INPUT_STATUS_COUNT];
} tInputStatus;
typedef struct {
  uint8_t       ReadPtr;
  uint8_t       WritePtr;
  //
  tInputStatus  Status[INPUT_SATAT_Q_LENGTH];
} tInputStatusQueue;
typedef tInputStatusQueue pInputStatusQueue;
#pragma pack()

//////////////////////////////////////////////////////////////////////////////
//                         СТАТУС ВЫХОДОВ (РЕЛЕ)
//
//
//////////////////////////////////////////////////////////////////////////////
// Если версия с выходным реле
#ifdef USE_RELAY
#pragma pack(1)
typedef struct {
  uint8_t Stat[OUTPUT_STATUS_COUNT];
} tOutputStatus;
typedef struct {
  uint8_t       ReadPtr;
  uint8_t       WritePtr;
  //
  tOutputStatus  Status[OUTPUT_SATAT_Q_LENGTH];
} tOutputStatusQueue;
typedef tOutputStatusQueue pOutputStatusQueue;
#pragma pack()
#endif

//////////////////////////////////////////////////////////////////////////////
//                        ОЧЕРЕДЬ ДАННЫХ QR РИДЕРОВ
//
//
//////////////////////////////////////////////////////////////////////////////
#ifdef QR_READER
#pragma pack(1)
// Structure for store QR data from reader N
typedef struct {
  uint8_t     ReaderNo;       // Reader number (0, 1, ...)
  uint8_t     Dir;            // Direction: 0 - forward, 1 - back
  uint8_t     DLen;           // Char count in card code
  uint8_t     Data[QR_DATA_SIZE];
} tQrData;
// QR data queue
typedef struct {
  uint8_t     ReadPtr;
  uint8_t     WritePtr;
  //
  tQrData     QrData[QR_DATA_Q_LENGTH];
}  tSendQrDataQueue;
typedef tSendQrDataQueue pSendQrDataQueue;
#pragma pack()
#endif


/*******************************************************************************
 *                ФУНКЦИИ МОДУЛЯ ОБЩЕГО НАЗНАЧЕНИЯ
 *******************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif

// Card data routines
void AddCardDataQueue(uint8_t reader, uint64_t card, uint8_t type, uint8_t bits);
uint8_t GetCardFromQueue(uint8_t *dest, uint8_t *len);
// Local status routines
void AddLocalStatusQueue(uint8_t tamper, uint8_t power);
uint8_t GetLocalStatusFromQueue(uint8_t *dest, uint8_t *len);
//void GetCurrentLocalStatus(uint8_t *dest, uint8_t *len);
// Input status routines
void AddInputStatusQueue(uint8_t *status, uint8_t count);
uint8_t GetInputStatusFromQueue(uint8_t *dest, uint8_t *len);
// Output status routines
#ifdef USE_RELAY
void AddOutputStatusQueue(uint8_t *status, uint8_t count);
uint8_t GetOutputStatusFromQueue(uint8_t *dest, uint8_t *len);
#endif
#ifdef QR_READER
void AddQrDataQueue(uint8_t reader, uint8_t *data, uint8_t len);
uint8_t GetQrFromQueue(uint8_t *dest, uint8_t *len);
#endif

// Queue initialisation
void OsdpQueueInit(void);

#if defined(__cplusplus)
}
#endif

#endif // OSDP_QUEUE_DEF_H

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
