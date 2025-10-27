/*******************************************************************************
 *
 *  МОДУЛЬ        : OsdpUart.h
 *
 *  Автор         : Л.Стасенко
 *  Дата начала   : 02.12.2014
 *  Версия        : 1.1
 *  Комментарии    : Работа с UART-0 в режиме OSDP
 *
 ******************************************************************************/
/*
   Принимает и передает пакеты без разбора содержимого.
   Трактовка содержимого - за верхним уровнем.
   Драйвер работает в полудуплексе, то есть пока ничего не принято, ничего
   не передаем. На случай, если при передаче от нас кто-то шлет нам опять,
   промежуточный буфер блокируется флагом Busy.
*/
/*******************************************************************************                                                             :
 *               ОБЩИЙ ФОРМАТ ПАКЕТОВ OSDP
 *******************************************************************************/
/*
 Byte   Name          Mean                               Value
------------------------------------------------------------------
  0     SOM           Start of Message                  0x53
  1     ADDR          Physical Address of the PD        0x00 – 0x7E
                                                        0x7F = broadcast
  2     LEN_LSB       Packet Length LSB                 Any
  3     LEN_MSB       Packet Length MSB                 Any
  4     CTRL          Message Control Information       See List
        -----------------------------------------------------------
        SEC_BLK_LEN   (optional) Length of Security Control Block - Any
        SEC_BLK_TYPE  (optional) Security Block Type - See List
        SEC_BLK_DATA  (optional) Security Block Data - Based on type
        -----------------------------------------------------------
        CMND/REPLY    Command or Replay Code             See List
        DATA          (optional) Data Block             Based on CMD/REPLY
        -----------------------------------------------------------
        MAC [0]       (optional) Present for secured messages,dependent on SEC_BLK_TYPE
        MAC [1]       
        MAC [2] “
        MAC [3] “
        -----------------------------------------------------------
        CKSUM/CRC_LSB Checksum, or, CRC-16 LSB
        CRC_MSB       (optional) CRC-16 MSB
        
    Опциональные параметры (Security Control Block, MAC[x]) мы пока не
    поддерживаем.
    PD при ответе ставит в поле адреса свой адрес с установленным старшим
    битом.
    
    Байт CTRL содержит следующую информацию:
  Бит    Маска  Имя       Толкование
  ----------------------------------------------------------------------  
  0-1   0x03    SQN         The sequence number of the message is used 
                            for message delivery confirmation and for
                            error recovery.
                            0>1>2>3>1>2>3>1>2>3... (Ноль только при
                            вхождении в связь, первый пакет).
  2     0x04    CKSUM/CRC   Set – 16-bit CRC is contained in the last 2 
                            bytes of the message, Clear – 8-bit CHECKSUM
                            is contained in the last byte of the message
  3     0x08    SCB         Set – Security Control Block is present in the
                            message, Clear – No Security Control block
*/


#ifndef OSDP_UART_1_DEF_H
#define OSDP_UART_1_DEF_H

// Основные типы данных
#include "Board.h"

#if defined(__cplusplus)
extern "C" {
#endif

/******************************************************************************
 *            ТАЙМАУТ НА ПРИЕМЕ ПАКЕТА, ЗАДЕРЖКА ПЕРЕД ПЕРЕДАЧЕЙ
 ******************************************************************************/
#define INTERPACK_TIMEOUT_9600    15000      // 15 msec
extern uint32_t InterPackOSDPTimeout;
#define SEND_REC_TIMEOUT_9600     3000       // 3 msec = 3 characters
extern uint32_t SendReceiveTimeout;
// Flag for timer interrupt
extern uint8_t SendRecTimeoutFlag;

/*******************************************************************************                                                             :
 *               ФОРМАТ ПАКЕТА ДЛЯ OSDP СООБЩЕНИЙ НА ЛИНИИ
 *******************************************************************************/
#define     DEFAULT_OSDP_BAUDRATE   9600
//#define     INTER_CHAR_TIMEOUT      20      // 20 мсек между пакетами
#define     OSDP_CHARACTER_TIMEOUT  10      // 5 мсек - конец пакета
#define     OSDP_BROADCAST          0x7F
extern uint8_t DeviceAddress;

extern volatile uint8_t     OsdpReceived;          // Есть принятые данные
extern volatile uint8_t     OsdpBusy;              // Мы заняты передачей

#pragma pack(1)
// Сначала протокольная часть структуры
typedef union {
  struct {
    uint8_t     Som;                      // Нужен для CRC
    uint8_t     Addr;                     // Адрес клиента
    uint16_t    DLen;                     // Длина данных
    uint8_t     Ctrl;                     // Байт управления
    uint8_t     CmdReplay;                // Код команды - ответа
    // ------- 6 байт
    uint8_t     Data[COMM_LEN];           // Опциональные данные, include MAC
  } OsdpData;
  // RAW данные от хоста
  uint8_t       RawData[COMM_LEN + 8 + 2];
} tOsdpPacket;
typedef tOsdpPacket *pOsdpPacket;

// Полная структура с полями для отработки отправки  
typedef struct {
  uint16_t    Ptr;                          // Указатель на текущий байт  
  uint16_t    SLen;                         // Полная длина данных на отправку
  // Далее принимаемые по протоколу данные
  tOsdpPacket OsdpPacket;
} tOsdpMessage;
#pragma pack()
typedef tOsdpMessage *pOsdpMessage;

extern tOsdpMessage OsdpMessage;

// Message structure for security packet
#pragma pack(1)
typedef union {
  struct {
    uint8_t     Som;                      // Нужен для CRC
    uint8_t     Addr;                     // Адрес клиента
    uint16_t    DLen;                     // Длина данных
    uint8_t     Ctrl;                     // Байт управления
    uint8_t     SecBlkLen;                // Length of Security Control Block - Any
    uint8_t     SecBlkType;               // Security Block Type
    uint8_t     SecBlkData;               // Security Block Data - Based on type
    uint8_t     CmdReplay;                // Код команды - ответа
    uint8_t     Data[COMM_LEN];           // Опциональные данные
  } OsdpData;
  // RAW данные от хоста
  uint8_t       RawData[COMM_LEN + 8 + 2 + 3];
} tOsdpSecurPacket;
#pragma pack()
typedef tOsdpSecurPacket *pOsdpSecurPacket;


/*******************************************************************************                                                             :
 *               ФОРМАТ ПАКЕТА OSDP ДЛЯ ПЕРЕДАЧИ ИЗ ПРЕРЫВАНИЯ
 *******************************************************************************/
#pragma pack(1)
typedef struct {
  uint8_t     Ready;              // Command ready for processing
  uint8_t     MfgCommand;         // Flag for manufacturer commans
  uint8_t     Command;            // Standard or Mfg command code
  uint8_t     DLen;               // Length of data
  uint8_t     Data[COMM_LEN]; //COM_BUFF_LEN]; // Data for command
} tOsdpCommand;
#pragma pack()
typedef tOsdpCommand *pOsdpCommand;

extern tOsdpCommand OsdpCommand;

/******************************************************************************
 *                   Version 4.4 - temporary variables
 ******************************************************************************/
extern uint32_t NewCommSpeed;

/*******************************************************************************                                                             :
 *                ФУНКЦИИ МОДУЛЯ ОБЩЕГО НАЗНАЧЕНИЯ
 *******************************************************************************/
void InitOsdpUart(uint32_t baudrate);
void InitDirectionPin(void);
void OsdpUartSend(void);
void OsdpUartReset(void);
void OsdpUartSetBaudrate(uint32_t baud);
//
void DecodePlainCommad(uint8_t cmd, uint16_t dlen);


#if defined(__cplusplus)
}
#endif

#endif    // OSDP_UART_1_DEF_H

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
