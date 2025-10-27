/*******************************************************************************
 *
 *  МОДУЛЬ        : OsdpUart.c
 *
 *  Автор         : Л.Стасенко
 *  Дата начала   : 18.01.2023
 *  Версия        : 1.1
 *  Комментарии   : Драйвер переписан для работы с USART1 на Artery (AT32F415)
 *  
 ******************************************************************************/
/*
   Драйвер работает в полудуплексе, то есть пока ничего не принято, ничего
   не передаем. На случай, если при передаче от нас кто-то шлет нам опять,
   промежуточный буфер блокируется флагом Busy.
   // Используется таймер 2 для межсимвольного таймаута,
   PD должен фиксировать offline через 8 секунд отсутствия обмена на линии -
   это можно организовать на программном таймере.
   Частота тактирования USART1 равна 29,491 МГц
*/

#include "at32f415.h"
#include "at32f415_gpio.h"
#include "at32f415_clock.h"
#include "BoardGpio.h"
#include "OsdpUart.h"
#include "CcittCrc16.h"
#include "OsdpCommands.h"
#include "OSDPQueue.h" 
#include "OsdpInputs.h"
#include "OSDP.h"
#include "Indication.h"
#include "OsdpCrypto.h"
#include "Keypad.h"


/******************************************************************************
 *                     Pins and ports for OSDP UART
 ******************************************************************************/
#define   UART_TX_GPIO_PORT     GPIOA
#define   UART_TX_GPIO_PIN      GPIO_PINS_9
#define   UART_RX_GPIO_PORT     GPIOA
#define   UART_RX_GPIO_PIN      GPIO_PINS_10
#define   UART_DIR_GPIO_PORT    GPIOA
#define   UART_DIR_GPIO_PIN     GPIO_PINS_11

/******************************************************************************
 *                      Parameters for OSDP interface
 ******************************************************************************/
// Default interpacket timeout for 9600 bod
uint32_t InterPackOSDPTimeout = INTERPACK_TIMEOUT_9600;
uint32_t SendReceiveTimeout = SEND_REC_TIMEOUT_9600;
// Flag for timer interrupt processing
uint8_t SendRecTimeoutFlag = 0;


/******************************************************************************
 *                        Адресация устройства
 ******************************************************************************/
uint8_t DeviceAddress;
#define OSDP_BROADCAST  0x7F
extern uint8_t OsdpLastPackNumber;

/******************************************************************************
 *                    Структура сообщения OSDP
 ******************************************************************************/
volatile uint8_t     OsdpReceived;          // Есть принятые данные
volatile uint8_t     OsdpBusy;              // Мы заняты передачей

// Structure for OSDP packet on UART
tOsdpMessage OsdpMessage;
// Structure for send command from UART interrupt
tOsdpCommand OsdpCommand;


/******************************************************************************
 *                     ТЕРМИНАЛЬНЫЕ СИМВОЛЫ ПРОТОКОЛА
 ******************************************************************************/
#define   OSDP_SOM              0x53

/******************************************************************************
 *                     СОСТОЯНИЯ UART НА ПРИЕМЕ ПАКЕТА
 ******************************************************************************/
typedef enum {
//  ustIdle,
  ustWaitStart,
  ustWaitAddr,
  ustWaitDlen,
  ustReceive,
  ustSkeepPack,
  ustTransmit
} tReceiveStates;

/*static*/ tReceiveStates UartState;

// Общий для всех фиктивный char
volatile static uint8_t dummy;
// Счетчик байтов от начала пакета
volatile static uint16_t ByteCount = 0;

/******************************************************************************
 *                   Version 4.4 - temporary variables
 ******************************************************************************/
uint32_t NewCommSpeed = 0;
uint8_t  NewDevAddress = 0;

/*******************************************************************************                                                             :
 *                        ФУНКЦИИ МОДУЛЯ  
 *******************************************************************************/
void OsdpUartReset(void);
void OsdpUartSend(void);
void TimeoutTimerInit(void);
void TimeoutTimerRestart(void);
void OsdpUartSetBaudrate(uint32_t baud);
//// TEMPORARY DUMMY FUNCTION
//void OsdpUartSetParams(uint32_t baudrate, uint8_t addr)
//{
//  
//}

////////////////////////////////////////////////////////////////////////////////
// Function     :  AddManufacturerId
// Input        :  Нет
// Output       :  Нет
// Description  :  Добавляет 3 байта заголовка к ответам команд производителя
////////////////////////////////////////////////////////////////////////////////
static void AddManufacturerId(uint8_t repcore)
{
  OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_MFGREP;
  OsdpMessage.OsdpPacket.OsdpData.Data[0] = 'P';
  OsdpMessage.OsdpPacket.OsdpData.Data[1] = 'R';
  OsdpMessage.OsdpPacket.OsdpData.Data[2] = 'S';
  OsdpMessage.OsdpPacket.OsdpData.Data[3] = repcore;
  OsdpMessage.OsdpPacket.OsdpData.DLen += 4;
}

////////////////////////////////////////////////////////////////////////////////
// Function     :  CopyDataToUpperLayer
// Input        :  Command and data length
// Output       :  Нет
// Description  :  Copy command, data length and data to buffer, set flag Ready
////////////////////////////////////////////////////////////////////////////////
static void CopyDataToUpperLayer(uint8_t cmd, uint16_t len, uint8_t mfg)
{
  // Send to upper layer
  OsdpCommand.MfgCommand = mfg;
  OsdpCommand.Command = cmd;
  OsdpCommand.DLen = len;
  if (len) {
    if (mfg) {
      memcpy(&OsdpCommand.Data[0], &OsdpMessage.OsdpPacket.OsdpData.Data[4], len);
    }
    else {
      memcpy(&OsdpCommand.Data[0], &OsdpMessage.OsdpPacket.OsdpData.Data[0], len);
    }
  }
  OsdpCommand.Ready = 1;
}

////////////////////////////////////////////////////////////////////////////////
// Function     :  DecodePlainCommad
// Input        :  Command and data length
// Output       :  Data in OsdpMessage (if present)
// Description  :  Use global OsdpMessage structure with prepared ASK
////////////////////////////////////////////////////////////////////////////////
/*
  Here we process commands wich can be processed immideately. If it not
  possible, we fill command structure for main thread and command will
  be processed in OsdpCheckCommand() function. We set "processing" flag,
  and main thread will clear it after processing. If we receive new such
  command and flag not cleared, we answer with "Busy" replay.
*/
void DecodePlainCommad(uint8_t cmd, uint16_t dlen)
{
uint8_t n;
uint32_t tdw;
  
  switch (cmd) {
    //////////////////////////////////////////////////////////////
    //                                                        Poll
    case osdp_POLL:
      //Если есть карта на отправку
      if (GetCardFromQueue(&OsdpMessage.OsdpPacket.OsdpData.Data[0], &n)) {
        OsdpMessage.OsdpPacket.OsdpData.DLen += n;
        // Код ответа
        OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_RAW;
      }
      
      // -------------------------------------
      // Если есть ПИН с клавиатуры
      #ifdef USE_KEYPAD
      else if (OsdpPinData.Ready) {
        // Ридер 1
        OsdpMessage.OsdpPacket.OsdpData.Data[0] = 0;
        // Длина в нашей структуре
        OsdpMessage.OsdpPacket.OsdpData.Data[1] = OsdpPinData.Count;
        // Далее идут коды клавиш c OsdpData.Data[2]
        memcpy(&OsdpMessage.OsdpPacket.OsdpData.Data[2], &OsdpPinData.PinData[0], OsdpPinData.Count);
        // Длина такая
        OsdpMessage.OsdpPacket.OsdpData.DLen += (2 + OsdpPinData.Count);
        // Код ответа
        OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_KEYPPAD;
        // ПИН забрали
        ClearOsdpPinData();
      }
      #endif
      
      // -------------------------------------
      // Если менялся статус тампера - питания
      else if (GetLocalStatusFromQueue(&OsdpMessage.OsdpPacket.OsdpData.Data[0], &n)) {
        OsdpMessage.OsdpPacket.OsdpData.DLen += n;
        // Код ответа
        OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_LSTATR;
      }
      
      // -------------------------------------
      // Если менялся статус логических входов
      else if (GetInputStatusFromQueue(&OsdpMessage.OsdpPacket.OsdpData.Data[0], &n)) {
        // Должны послать статус всех входов (по стандарту)
        OsdpMessage.OsdpPacket.OsdpData.DLen += n;
        OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_ISTATR;
      }
      
      #ifdef USE_RELAY
      // -------------------------------------
      // Если менялся статус выходного реле
      else if (GetOutputStatusFromQueue(&OsdpMessage.OsdpPacket.OsdpData.Data[0], &n)) {
        // Должны послать статус всех выходов (по стандарту)
        OsdpMessage.OsdpPacket.OsdpData.DLen += n;
        OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_OSTATR;
      }
      #endif
      
      #ifdef QR_READER
      // -------------------------------------
      // Если есть данные с QR ридера uint8_t GetQrFromQueue(uint8_t *dest, uint8_t *len);
      else if (GetQrFromQueue(&OsdpMessage.OsdpPacket.OsdpData.Data[0], &n)) {
        // Посылаем как форматированный код карты
        OsdpMessage.OsdpPacket.OsdpData.DLen += n;
        OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_FMT;
      }
      #endif
    break;

    //////////////////////////////////////////////////////////////
    //                                           ID Report Request
    case osdp_ID:
      memcpy(&OsdpMessage.OsdpPacket.OsdpData.Data[0], (void*)&ReaderConfig.DeviceIdReport.VendorCode1,
              sizeof(tDeviceIdReport));
      OsdpMessage.OsdpPacket.OsdpData.DLen += sizeof(tDeviceIdReport);
      OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_PDID;
    break;

    //////////////////////////////////////////////////////////////
    //                                     PD Capabilities Request
    case osdp_CAP:
      memcpy(&OsdpMessage.OsdpPacket.OsdpData.Data[0], (void*)&ReaderConfig.Capability, sizeof(tCapability));
      OsdpMessage.OsdpPacket.OsdpData.DLen += sizeof(tCapability);
      OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_PDCAP;
    break;

    //////////////////////////////////////////////////////////////
    //                                 Diagnostic Function Command
    case osdp_DIAG:
      // Currently, this command is a placeholder
    break;

    //////////////////////////////////////////////////////////////
    //                 Local Status Report Request - tamper, power
    // We have't this status - send fictive state, all OK
    case osdp_LSTAT:
      OsdpMessage.OsdpPacket.OsdpData.Data[0] = OsdpLocalStatus.Power;
      OsdpMessage.OsdpPacket.OsdpData.Data[1] = OsdpLocalStatus.Tamper;
      OsdpMessage.OsdpPacket.OsdpData.DLen += 2;
      OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_LSTATR;
    break;
    //////////////////////////////////////////////////////////////
    //                                 Input Status Report Request
    case osdp_ISTAT:
      // Должны послать статус всех входов (по стандарту)
      OsdpMessage.OsdpPacket.OsdpData.Data[0] = Inputs[0].OldState;
      OsdpMessage.OsdpPacket.OsdpData.Data[1] = Inputs[1].OldState;
      OsdpMessage.OsdpPacket.OsdpData.DLen += 2;
      OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_ISTATR;
    break;
    
    //////////////////////////////////////////////////////////////
    //                                Output Status Report Request
    #ifdef USE_RELAY
    case osdp_OSTAT:
      memcpy(&OsdpMessage.OsdpPacket.OsdpData.Data[0], &OsdpRelayStatus.State, OUTPUT_STATUS_COUNT);
      OsdpMessage.OsdpPacket.OsdpData.DLen += OUTPUT_STATUS_COUNT;
      OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_OSTATR;
    break;
    #endif

    //////////////////////////////////////////////////////////////
    //                                Reader Status Report Request
    case osdp_RSTAT:
      // Это если есть ведомые ридеры, за которыми мы следим
    break;

    //////////////////////////////////////////////////////////////
    //                                      Output Control Command
    #ifdef USE_RELAY
    case osdp_OUT:
      if (OsdpCommand.Ready) {
        OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
      }
      else {
        // Команда 4 байта
        if (dlen != 4) {
          OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
          OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
          OsdpMessage.OsdpPacket.OsdpData.DLen++;
        }
        else if (OsdpMessage.OsdpPacket.OsdpData.Data[0] != 0) {
          // Такого выхода нет
          OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
          OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_UNABLE_PROCESS;
          OsdpMessage.OsdpPacket.OsdpData.DLen++;
        }
        else if ((OsdpMessage.OsdpPacket.OsdpData.Data[1] == 3) || (OsdpMessage.OsdpPacket.OsdpData.Data[1] == 4)) {
          // С отсрочкой до окончания таймерного поставить
          // не можем (коды 3, 4)
          OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
          OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_UNABLE_PROCESS;
          OsdpMessage.OsdpPacket.OsdpData.DLen++;
        }
        else {
          // Send to upper layer
          CopyDataToUpperLayer(cmd, dlen, 0);
          // Ответом будет ACK
        }
      }
    break;
    #endif

    //////////////////////////////////////////////////////////////
    //                                  Reader Led Control Command
    case osdp_LED:
      if (OsdpCommand.Ready) {
        OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
      }
      else {
        // Проверяем на корректность (по числу LED, по цветам, ...)
        if ((dlen % LED_CONTROL_SIZE) != 0) {
          // Длина не соответстует
          OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
          OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
          OsdpMessage.OsdpPacket.OsdpData.DLen++;
        }
        else {
          // Send to upper layer
          CopyDataToUpperLayer(cmd, dlen, 0);
          // Ответом будет ACK
        }
      }
    break;
      
    //////////////////////////////////////////////////////////////
    //                              Reader Buzzer Control Command
    case osdp_BUZ:
      if (OsdpCommand.Ready) {
        OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
      }
      else {
        // Проверяем что нам (ридер = 0)
        if (((dlen % BEEP_CONTROL_SIZE) != 0) || (OsdpMessage.OsdpPacket.OsdpData.Data[0] != 0)) {
          OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
          OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_UNABLE_PROCESS;
          OsdpMessage.OsdpPacket.OsdpData.DLen++;
        }
        else {
          // Send to upper layer
          CopyDataToUpperLayer(cmd, dlen, 0);
          // Ответом будет ACK
        }
      }
    break;
      
    //////////////////////////////////////////////////////////////
    //                              PD Communication Configuration
    case osdp_COMSET:
      // Check if prev comman processed on upper layer
      if (OsdpCommand.Ready) {
        OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
      }
      else {
        // Первый байт - новый адрес, затем 4 байта новой скорости (little endian)
        // Ответ - osdp_COM. По стандарту рекомендуется параметры запоминать в энергонезависимой памяти
        // Проверяем на допустимость значений. Это скорость
        memcpy(&tdw, &OsdpMessage.OsdpPacket.OsdpData.Data[1], sizeof(tdw));
        // ... а это адрес
        n = OsdpMessage.OsdpPacket.OsdpData.Data[0];
        if (((tdw == 9600) || (tdw == 19200) || (tdw == 38400) || (tdw == 57600) || (tdw == 115200)) && (n < 0x7E)) {
          // Copy parameters to upper layer
          CopyDataToUpperLayer(cmd, dlen, 0);
          // Store new params for reader config
          NewCommSpeed = tdw;
          NewDevAddress = n;
          // Prepare immideately answer
          OsdpMessage.OsdpPacket.OsdpData.Data[0] = n;
          memcpy(&OsdpMessage.OsdpPacket.OsdpData.Data[1], &tdw, sizeof(tdw));
          OsdpMessage.OsdpPacket.OsdpData.DLen += 5;
          OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_COM;
        }
        else {
          OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
          OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_UNABLE_PROCESS;
          OsdpMessage.OsdpPacket.OsdpData.DLen++;
        }
      }
    break;

    //////////////////////////////////////////////////////////////
    //                                  Encryption Key Set Command
    #ifndef READER_125
    case osdp_KEYSET:
      if (OsdpCommand.Ready) {
        OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
      }
      else {
//        if (OsdpMessage.OsdpPacket.OsdpData.Data[0] == 7) {
//          // Stop on key index == 7
//          cmd += 0;
//        }
        // Проверяем на валидную длину
        if (dlen != 18) {
          OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
          OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_UNABLE_PROCESS;
          OsdpMessage.OsdpPacket.OsdpData.DLen++;
        }
        else {
          // Check key number and key length (for Mifare Plus we use only keys 0, 1, 2, 3. New 4 and 5 for QR and NfcConfig)
          if ((OsdpMessage.OsdpPacket.OsdpData.Data[0] > 7) || (OsdpMessage.OsdpPacket.OsdpData.Data[1] != 16)) {
            OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
            OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_UNABLE_PROCESS;
            OsdpMessage.OsdpPacket.OsdpData.DLen++;
          }
          else {
            // Send to upper layer
            CopyDataToUpperLayer(cmd, dlen, 0);
            // Will be ACK answer
          }
        }
      }
    break;
    #endif
      
    //############################################################
    //               Manufacturer Specific Command
    //############################################################
    case osdp_MFG:
      // Структура команды:
      // - 3 байта vendor code - PRS, затем данные, состоящие из команды и данных
      // Ответ - osdp_ACK, osdp_NAK, osdp_MFRGxx
      if ((OsdpMessage.OsdpPacket.OsdpData.Data[0] != 'P') || 
          (OsdpMessage.OsdpPacket.OsdpData.Data[1] != 'R') || 
          (OsdpMessage.OsdpPacket.OsdpData.Data[2] != 'S'))
      {
        // Не наш формат пакета
        OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
        OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_UNABLE_PROCESS;
        OsdpMessage.OsdpPacket.OsdpData.DLen++;
      }
      else {
        // Process command here or send it to main thread
        cmd = OsdpMessage.OsdpPacket.OsdpData.Data[3];
        switch (cmd) {
          //========================================================
          //                             Copy of osdp_KEYSET command
          case mfg_KEY_SET:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              // Проверяем что нам (ридер = 0)
              if (dlen != 18) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_UNABLE_PROCESS;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // Check key number and key length (for Mifare Plus we use only keys 0, 1, 2, 3. New 4 and 5 for QR and NfcConfig)
                if ((OsdpMessage.OsdpPacket.OsdpData.Data[4] > 7) || (OsdpMessage.OsdpPacket.OsdpData.Data[5] != 16)) {
                  OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                  OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_UNABLE_PROCESS;
                  OsdpMessage.OsdpPacket.OsdpData.DLen++;
                }
                else {
                  // Send to upper layer
                  CopyDataToUpperLayer(cmd, dlen, 1);
                  // Will be ACK answer
                }
              }
            }
          break;
          
          //========================================================
          //                Set wiegand format (non volatile memory)
          case mfg_SETWGFMT:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              // Проверяем на длину, если неверная - osdp_err_CMD_LENGTH
              // Длина параметров должна быть dlen - 8 - 4 = dlen - 12
              if (dlen != sizeof(tWiegandFormat)) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // Send to upper layer
                CopyDataToUpperLayer(cmd, dlen, 1);
                // Ответом будет ACK
              }
            }
          break;
          
          //========================================================
          //                                   Wiegand format report
          case mfg_GETWGFMT:
            AddManufacturerId(mfg_WGFMTRD);
            memcpy(&OsdpMessage.OsdpPacket.OsdpData.Data[4], &ReaderConfig.CommonConfig.WiegandFormat, sizeof(tWiegandFormat));
            OsdpMessage.OsdpPacket.OsdpData.DLen += sizeof(tWiegandFormat);
          break;
          
          //========================================================
          //                              Set Mifare security params
          case mfg_SETMIFSEC:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              if (dlen != 7) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // Send to upper layer
                CopyDataToUpperLayer(cmd, dlen, 1);
                // Ответом будет ACK
              }
            }
          break;
          
          //========================================================
          //                           Get cards for read parameters
          case mfg_GETCARDP:
            // Take from configuration
            memcpy(&OsdpMessage.OsdpPacket.OsdpData.Data[4], &ReaderConfig.CommonConfig.MifareMode, sizeof(tMifareSecureModes));
            memcpy(&OsdpMessage.OsdpPacket.OsdpData.Data[5], &ReaderConfig.CommonConfig.CardForRead, sizeof(tCardForRead));
            // Сформировали ответ
            AddManufacturerId(mfg_CARDPRD);
            OsdpMessage.OsdpPacket.OsdpData.DLen += sizeof(tCardForRead) + 1;
          break;
          
          //========================================================
          //                           Set cards for read parameters
          case mfg_SETCARDP:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              if (dlen != (sizeof(tCardForRead) + 1)) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // Send to upper layer
                CopyDataToUpperLayer(cmd, dlen, 1);
                // Ответом будет ACK
              }
            }
          break;
            
          #ifdef USE_KEYPAD
          //========================================================
          //                 Write keypad mode (card, PIN, card+PIN)
          case mfg_SETPINMOD:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              if ((dlen != 1) || (OsdpMessage.OsdpPacket.OsdpData.Data[4] > 2)) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // Send to upper layer
                CopyDataToUpperLayer(cmd, dlen, 1);
                // Ответом будет ACK
              }
            }
          break;
            
          //========================================================
          //                 Read  keypad mode (card, PIN, card+PIN)
          case mfg_GETPINMOD:
            OsdpMessage.OsdpPacket.OsdpData.Data[4] = ReaderConfig.CommonConfig.CardOrKeyMode;
            // Сформировали ответ
            AddManufacturerId(mfg_PINMOD);
            OsdpMessage.OsdpPacket.OsdpData.DLen += 1;
          break;
          
          //========================================================
          //          Change keypad mode, not stored in non-volatile
          case mfg_CHGPINMOD:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              if (dlen != 2) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // Send to upper layer
                CopyDataToUpperLayer(cmd, dlen, 1);
                // Ответом будет ACK
              }
            }
          break;
            
          //========================================================
          //           Cancel PIN wait time
          // Added 10.08.2021 for stop wait PIN from software
          case mfg_CANCPINMOD:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              // Send to upper layer
              CopyDataToUpperLayer(cmd, dlen, 1);
              // Ответом будет ACK
            }
          break;
          #endif
            
          //========================================================
          //    Read common configuration without Mif sector and key
          // Added 17.02.2022
          case mfg_GETCMNCONFIG:
            // Take from configuration
            memcpy(&OsdpMessage.OsdpPacket.OsdpData.Data[4], &ReaderConfig.CommonConfig, sizeof(tCommonConfig) - 6);
            // Сформировали ответ
            AddManufacturerId(mfg_CARDPRD);
            OsdpMessage.OsdpPacket.OsdpData.DLen += sizeof(tCommonConfig) - 6;
          break;
            
          //========================================================
          //                             Read extended configuration
          // Added 17.02.2022
          case mfg_GETEXTCONFIG:
            // Take from configuration (without signature)
            memcpy(&OsdpMessage.OsdpPacket.OsdpData.Data[4], &ExtendedConfig, sizeof(tExtendedConfig) - 2);
            // Сформировали ответ
            AddManufacturerId(mfg_CARDPRD);
            OsdpMessage.OsdpPacket.OsdpData.DLen += sizeof(tExtendedConfig) - 2;
          break;

          //========================================================
          //           Change comm speed, not stored in non-volatile
          case mfg_CHGCOMSPEED:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              if (dlen != 4) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // 4 байта новой скорости (little endian)
                // Проверяем на допустимость - скорость
                memcpy(&tdw, &OsdpMessage.OsdpPacket.OsdpData.Data[4], sizeof(tdw));
                if ((tdw == 9600) || (tdw == 19200) || (tdw == 38400) || (tdw == 57600) || (tdw == 115200)) {
                  // Copy parameters to upper layer
                  CopyDataToUpperLayer(cmd, dlen, 1);
                  // Сформировали ответ
                  AddManufacturerId(mfg_CHGCOMSPEED);
                  memcpy(&OsdpMessage.OsdpPacket.OsdpData.Data[4], &tdw, sizeof(tdw));
                  OsdpMessage.OsdpPacket.OsdpData.DLen += 4;
                }
                else {
                  OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                  OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_UNABLE_PROCESS;
                  OsdpMessage.OsdpPacket.OsdpData.DLen++;
                }
              }
            }
          break;
          
          //========================================================
          //                                 Set indication LED mode
          case mfg_SETLEDINDIC:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              // Длина должна быть 4 байта (OnTime, OffTime, OnColor, OffColor)
              if (dlen != 4) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // Send to upper layer
                CopyDataToUpperLayer(cmd, dlen, 1);
                // Ответом будет ACK
              }
            }
          break;
            
          #ifdef GLASS_READER
          //========================================================
          //                                  Set backlight LED mode
          case mfg_SETLEDBACKLT:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              // Длина должна быть 4 байта (OnTime, OffTime, OnColor, OffColor)
              if (dlen != 4) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // Send to upper layer
                CopyDataToUpperLayer(cmd, dlen, 1);
                // Ответом будет ACK
              }
            }
          break;
          #endif
            
          //========================================================
          //             Set indication default colors (not volatile)
          case mfg_SETDEFCOLORS:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              // Длина 2 байта (4 цвета по 4 бита)
              if (dlen != sizeof(tIndicationColors)) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // Send to upper layer
                CopyDataToUpperLayer(cmd, dlen, 1);
                // Ответом будет ACK
              }
            }
          break;
            
          //========================================================
          //    Set new ID report (for version change in production)
          // Version 3.6
          case mfg_SETIDREPORT:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              // Длина 12 байт
              if (dlen != sizeof(tDeviceIdReport)) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // Send to upper layer
                CopyDataToUpperLayer(cmd, dlen, 1);
                // Ответом будет ACK
              }
            }
          break;

          //========================================================
          //                  Program QR scanner parameters and mode
          // 
          #ifdef QR_READER
          case mfg_QR_CONFIG_CMD:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
//              // Длина 2 байт
//              if (dlen != 2) {
//                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
//                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
//                OsdpMessage.OsdpPacket.OsdpData.DLen++;
//              }
//              else 
              {
                // Send to upper layer
                CopyDataToUpperLayer(cmd, dlen, 1);
                // Ответом будет ACK
              }
            }
          break;
          #endif

          //========================================================
          //                    Set HMAC-SHA mode (SHA-1 or SHA-256)
          case mfg_SET_SHA_MODE:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              // Длина 1 байт
              if (dlen != 1) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // Send to upper layer
                CopyDataToUpperLayer(cmd, dlen, 1);
                // Ответом будет ACK
              }
            }
          break;

          //========================================================
          //                   Set ID configuration for Mifare cards
          case mfg_ID_LOCATE_CFG:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              // Длина 7 байт
              if (dlen != 7) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else if ((OsdpMessage.OsdpPacket.OsdpData.Data[5] > 31) || 
                       (OsdpMessage.OsdpPacket.OsdpData.Data[6] > 2) || 
                       (OsdpMessage.OsdpPacket.OsdpData.Data[7] > 11) ||
                       (OsdpMessage.OsdpPacket.OsdpData.Data[8] > 8)) 
              { 
                // NEED CHECK PARAMETERS VALIDITY
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_UNABLE_PROCESS;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // Send to upper layer
                CopyDataToUpperLayer(cmd, dlen, 1);
                // Ответом будет ACK
              }
            }
          break;
          
          //========================================================
          //                                Reset to factory default
          case mfg_RES_TO_FACT:
            // Check if prev comman processed on upper layer
            if (OsdpCommand.Ready) {
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_BUSY;
            }
            else {
              if (dlen != 0) {
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_LENGTH;
                OsdpMessage.OsdpPacket.OsdpData.DLen++;
              }
              else {
                // Send to upper layer
                CopyDataToUpperLayer(cmd, dlen, 1);
              }
            }
          break;

          default:
            // Такую пользовательскую команду не знаем
            OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
            OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_UNKNOWN;
            OsdpMessage.OsdpPacket.OsdpData.DLen++;
          break;
        }
      }
    break;

    default:
      OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
      OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_CMD_UNKNOWN;
      OsdpMessage.OsdpPacket.OsdpData.DLen++;
    break;
  }
} 
 
/*******************************************************************************
 *******************************************************************************
 *
 *                         ПОДДЕРЖКА UART
 *
 *******************************************************************************
 *******************************************************************************/

#define   OSDP_PREAMBLE   0xFF

volatile static uint16_t OsdpCRC;
//volatile static uint16_t FullLength;
static volatile uint16_t FullPackLen;
// For store current address from received packet
static volatile uint8_t CurrentAddr;

////////////////////////////////////////////////////////////////////////////////
// Function     :  UART_IRQHandler
// Input        :  Нет
// Output       :  Нет
// Description  :  Прием в режиме перываний данных с RS-485
////////////////////////////////////////////////////////////////////////////////
//
void USART1_IRQHandler(void)
{
uint8_t c;
uint8_t cmd;
uint16_t dlen;
  
  /////////////////////////////////////////////////////////////
  // If receive any character
  if (usart_flag_get(USART1, USART_RDBF_FLAG) != RESET) {
    // Interrupt flags will be cleared at the end of function
    // read one byte from the receive data register
    c = usart_data_receive(USART1);
    // Сколько байт принято после пересброса
    ++ByteCount;
    // Перевзвели таймер интервала между посылками
    TimeoutTimerRestart();
    
    switch (UartState) {
      case ustWaitStart:
        if ((c == OSDP_SOM) && (ByteCount < 3)) {
          memset(&OsdpMessage, 0, sizeof(tOsdpMessage));
          OsdpMessage.OsdpPacket.RawData[OsdpMessage.Ptr++] = c;
          UartState = ustWaitAddr;
          CurrentAddr = 0;
        }
      break;
      case ustWaitAddr:
        // Store address and go for receive data length
        CurrentAddr = (c & 0x7F);
        OsdpMessage.OsdpPacket.RawData[OsdpMessage.Ptr++] = c;
        UartState = ustWaitDlen;
      break;
      case ustWaitDlen:
        OsdpMessage.OsdpPacket.RawData[OsdpMessage.Ptr++] = c;
        // If we receive full length
        if (OsdpMessage.Ptr > 3) {
          // Полная длина пакета с CRC но без SOM
          FullPackLen = OsdpMessage.OsdpPacket.OsdpData.DLen;
          // Check if our address and valid packet length
          if ((CurrentAddr == DeviceAddress) || (CurrentAddr == OSDP_BROADCAST) ||
              // Addr 0 added for debug porpoce - 2024-08
              (CurrentAddr == 0)) {
            if (FullPackLen > (COMM_LEN - 3)) {
              // We must skip long packet
              UartState = ustSkeepPack;
            }
            else {
              UartState = ustReceive;
            }
          }
          else {
            UartState = ustSkeepPack;
          }
        }
      break;
      case ustReceive:
        OsdpMessage.OsdpPacket.RawData[OsdpMessage.Ptr++] = c;
        if (OsdpMessage.Ptr >= FullPackLen) {
          // Проверили CRC (начиная с  SOM, длина - включая CRC
          if (OsdpCrcIsOk(&OsdpMessage.OsdpPacket.RawData[0], 
              OsdpMessage.OsdpPacket.OsdpData.DLen) != 0)
          {
            // If packet is new - try to decode it. From V4.9 (2021-04-29) enable Pack.No=0
            if ((OsdpLastPackNumber != (OsdpMessage.OsdpPacket.OsdpData.Ctrl & 0x03)) ||
               ((OsdpMessage.OsdpPacket.OsdpData.Ctrl & 0x03) == 0)) {
              // Plain or secured message
              if ((OsdpMessage.OsdpPacket.OsdpData.Ctrl & 0x08) == 0) {
                // If secure mode was ON - set it off
                if (SecurModeOn) {
                  SecurModeOn = 0;
                  SecurExchState = stPlain;
                }
                // Plain exchange (dlen without data preamble)
                // Команду и длину запомнили
                dlen = OsdpMessage.OsdpPacket.OsdpData.DLen;
                // Store command code
                cmd = OsdpMessage.OsdpPacket.OsdpData.CmdReplay;
                // Это пока что для пустого пакета c CRC на отправку
                OsdpMessage.OsdpPacket.OsdpData.DLen = 8;
                // Пока что ASK
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_ACK;
                // Для ответа к адресу добавили 0x80
                OsdpMessage.OsdpPacket.OsdpData.Addr |= 0x80;
                // Below in DecodePlainCommad we store new params for osdp_COMSET (ver. 4.4)
                // NewCommSpeed, NewDevAddress. It will reset after store in ReaderConfig
                if (cmd != osdp_MFG)
                  DecodePlainCommad(cmd, dlen - 8);
                else
                  DecodePlainCommad(cmd, dlen - 12);
              }
              else {
                #ifdef    USE_SECURITY
                // Secured message
                DecodeSecuredCommand(OsdpMessage.OsdpPacket.RawData);
                #else
                // For debug if security not supported
                // Это пакет with CRC и причиной NAK
                OsdpMessage.OsdpPacket.OsdpData.DLen = 8 + 1;
                // NAK
                OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
                // Для ответа к адресу добавили 0x80
                OsdpMessage.OsdpPacket.OsdpData.Addr |= 0x80;
                // Reset SEC bit in CTRL
                OsdpMessage.OsdpPacket.OsdpData.Ctrl &= ~0x08;
                OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_SECUR_UNKNOWN;
                #endif
              }  
            }
            // Если не ошибка, то запоминаем номер пакета
            if (OsdpMessage.OsdpPacket.OsdpData.CmdReplay != osdp_NAK) {
              OsdpLastPackNumber = (OsdpMessage.OsdpPacket.OsdpData.Ctrl & 0x03);
            }
            // Start transmission (answer)
            if ((OsdpMessage.OsdpPacket.OsdpData.Addr & 0x7F) != OSDP_BROADCAST) {
              // We will busy now for transmission
              //OsdpUartSend();
              if (SecurModeOn != 0) {
                // Convert packet for SC
                EncodeAnswerPacket((uint8_t*)&OsdpMessage.OsdpPacket.RawData);
              }
              __disable_irq();
              // This will start transmission in timer interrupt
              SendRecTimeoutFlag = 1;
              // Сбросили timer
              tmr_counter_enable(TMR2, FALSE);
              // Set counter to start
              tmr_counter_value_set(TMR2, 0);
              tmr_div_value_set(TMR2, SendReceiveTimeout);
              tmr_counter_enable(TMR2, TRUE);
              __enable_irq();
            }
            else {
              // Immideately prepare for receive
              OsdpUartReset();
            }
          }
          // at32_led_on(LED4);
        }
      break;
      case ustSkeepPack:
      default:
        // Пропускаем чужой пакет на линии
        OsdpMessage.Ptr++;
        if (OsdpMessage.Ptr >= FullPackLen) {
          // Prepare for receive next packet
          OsdpUartReset();
        }
      break;
    }
  }

  /////////////////////////////////////////////////////////////
  // Transmitt data buffer empty flag
  if ((usart_flag_get(USART1, USART_TDBE_FLAG) != RESET) && (UartState == ustTransmit)) {
    if (OsdpMessage.Ptr < OsdpMessage.SLen) {
      usart_data_transmit(USART1, OsdpMessage.OsdpPacket.RawData[OsdpMessage.Ptr++]);
    } 
    else {
      // Все передали - запрещаем прерывания на передачу:
      usart_interrupt_enable(USART1, USART_TDBE_INT, FALSE);
      // Enable interrupt for switch direction
      usart_interrupt_enable(USART1, USART_TDC_INT, TRUE);
      // If need - set new params for UART
      if (NewCommSpeed != 0) {
        // NewCommSpeed will cleared on upper layer after store
        // ReaderConfig in EEPROM
        ReaderConfig.CommonConfig.BaudRate = NewCommSpeed;
        ReaderConfig.CommonConfig.ReaderAddress = NewDevAddress;
        DeviceAddress = NewDevAddress;
        // Set new UART params on upper layer ...
      }
      // Передача закончена
      OsdpBusy = 0;
      // По флагу готовности к защищенному обмену взводим флаг режима.
      // Делается после отправки ответа на Scrypt, чтобы его самого
      // не обрабатывать с криптографией
      if (SecureReady) {
        // Сам  флаг сбрасываем, и ставим признак защищенного обмена
        SecureReady = 0;
        SecurModeOn = 1;
      }
    }
  }
  
  /////////////////////////////////////////////////////////////
  // For direction change - last symbol sent
  if ((usart_flag_get(USART1, USART_TDC_FLAG) != RESET) && (UartState == ustTransmit)) {
    // Reset UART for receive new packet
    OsdpUartReset();
    // ... and switch direction to receive
    gpio_bits_reset(UART_DIR_GPIO_PORT, UART_DIR_GPIO_PIN);
  }
  
  // Clear ALL interrupt flags
  usart_flag_clear(USART1, USART_RDBF_FLAG);
  usart_flag_clear(USART1, USART_TDBE_FLAG);
  usart_flag_clear(USART1, USART_TDC_FLAG);
}

////////////////////////////////////////////////////////////////////////////////
// Function     :  OsdpUartSend
// Input        :  Нет
// Output       :  Нет
// Description  :  Отправка пакета через UART
//  STANDAR answer to POLL command take near 9 usec (near 1 usec/byte)
////////////////////////////////////////////////////////////////////////////////
void OsdpUartSend(void)
{
uint16_t crc;

  // Если мы еще не все передали, то ждем:
//  while (OsdpBusy);
  // Теперь мы заняли
  OsdpBusy = 1;
  // Вычисляем CRC пакета
  crc = OSDP_INIT_CRC16;
  crc = CalcCrc16(crc, &OsdpMessage.OsdpPacket.RawData[0], OsdpMessage.OsdpPacket.OsdpData.DLen - 2);
  OsdpMessage.OsdpPacket.RawData[OsdpMessage.OsdpPacket.OsdpData.DLen - 2] = (uint8_t)(crc & 0x00FF);
  OsdpMessage.OsdpPacket.RawData[OsdpMessage.OsdpPacket.OsdpData.DLen - 1] = (uint8_t)((crc >> 8) & 0x00FF);
  // Отправляем
  OsdpMessage.Ptr = 0;
  OsdpMessage.SLen = OsdpMessage.OsdpPacket.OsdpData.DLen;
  // State of FSM
  UartState = ustTransmit;
  // Set DIR to high state
  gpio_bits_set(UART_DIR_GPIO_PORT, UART_DIR_GPIO_PIN);
  // Disable receive, enable transmit for UART
  usart_receiver_enable(USART1, FALSE);
  usart_transmitter_enable(USART1, TRUE);
  // Reinit interrupts - only transmit enabled
  usart_interrupt_enable(USART1, USART_RDBF_INT, FALSE);
  usart_interrupt_enable(USART1, USART_TDC_INT, FALSE);
  // Сначала пустой FF
  usart_data_transmit(USART1, OSDP_PREAMBLE);
  // Теперь все отправляем, разрешив прерывание transmit buffer empty
  usart_interrupt_enable(USART1, USART_TDBE_INT, TRUE);
  // 
  //at32_led_off(LED4);
}

////////////////////////////////////////////////////////////////////////////////
// Function     :  OsdpUartReset
// Input        :  Нет
// Output       :  Нет
// Description  :  Сброс FSM OsdpUart
////////////////////////////////////////////////////////////////////////////////
void OsdpUartReset(void)
{
  usart_transmitter_enable(USART1, FALSE);
  usart_receiver_enable(USART1, TRUE);
  // Reinit interrupts - only receive enabled
  usart_interrupt_enable(USART1, USART_TDC_INT, FALSE);
  usart_interrupt_enable(USART1, USART_TDBE_INT, FALSE);
  usart_interrupt_enable(USART1, USART_RDBF_INT, TRUE);
  // Это для OSDP
  UartState = ustWaitStart;
  ByteCount = 0;
  //SendRecTimeoutFlag = 0;
  // Clear OSDP buffer
  memset(&OsdpMessage, 0, sizeof(tOsdpMessage));
  // Set DIR low
  gpio_bits_reset(UART_DIR_GPIO_PORT, UART_DIR_GPIO_PIN);
  // Таймер межсимвольного обмена остановлен. Запустится после приема
  // первого символа нового пакета
  tmr_counter_enable(TMR2, FALSE);
  // Set counter to start
  tmr_counter_value_set(TMR2, 0);
  // Timer value calculate depend from UART speed
  switch (ReaderConfig.CommonConfig.BaudRate) {
    case 9600: 
      SendReceiveTimeout = SEND_REC_TIMEOUT_9600;
      tmr_div_value_set(TMR2, SendReceiveTimeout);
    break;
    case 19200: 
      SendReceiveTimeout = SEND_REC_TIMEOUT_9600 >> 1;
      tmr_div_value_set(TMR2, SendReceiveTimeout);
    break;
    case 38400: 
    case 57600:
      SendReceiveTimeout = SEND_REC_TIMEOUT_9600 >> 2;
      tmr_div_value_set(TMR2, SendReceiveTimeout);
    break;
    case 115200: 
      SendReceiveTimeout = SEND_REC_TIMEOUT_9600 >> 3;
      tmr_div_value_set(TMR2, SendReceiveTimeout);
    break;
    default:
      SendReceiveTimeout = SEND_REC_TIMEOUT_9600;
      tmr_div_value_set(TMR2, SendReceiveTimeout);
    break;
  }
}

/************************************************************************
*************************************************************************
                 Timer TMR2 support for timeout check
*************************************************************************
************************************************************************/

////////////////////////////////////////////////////////////////////////////////
// Function     :  TimeoutTimerInit
// Input        :  Нет
// Output       :  Нет
// Description  :  Таймер 2 используем для контроля таймаута между символами в
//                 режиме OSDP. Для остальных интерфейсов можно использовать,
//                 например, таймер 3 (он вроде аналогичный).
////////////////////////////////////////////////////////////////////////////////
static uint32_t count = 5000;    // 5 msec
void TimeoutTimerInit(void)
{
crm_clocks_freq_type crm_clocks_freq_struct = {0};
uint32_t clk, presc;

  // enable TMR2 clock (возможно надо включать вместе с TMR5
  // при их каскадировании в драйвере выходного интерфейса)
  crm_periph_clock_enable(CRM_TMR2_PERIPH_CLOCK, TRUE);

  // tmr configuration
  // get system clock 
  crm_clocks_freq_get(&crm_clocks_freq_struct);
  // Timer 2 on apb1 timer clock (APB/4*2 in our configuration)
  clk = crm_clocks_freq_struct.apb1_freq * 2;
  // Prescaller for 1 usec
  presc = (clk / 1000000) - 1;
  // Set presc, compare val (1000 - 1 msec) - временно, в
  // UartReset будет ставиться реальное значение для 
  // заданной скорости
  tmr_base_init(TMR2, presc, count);
  tmr_cnt_dir_set(TMR2, TMR_COUNT_UP);
  // Don't count before char arrive
  tmr_counter_enable(TMR2, FALSE);
  // overflow interrupt disable
  tmr_interrupt_enable(TMR2, TMR_OVF_INT, FALSE);

  NVIC_ClearPendingIRQ(TMR2_GLOBAL_IRQn);
  // tmr1 overflow interrupt nvic init
  nvic_irq_enable(TMR2_GLOBAL_IRQn, 1, 0);
}

////////////////////////////////////////////////////////////////////////////////
// Function     :  TimeoutTimerRestart
// Input        :  Нет
// Output       :  Нет
// Description  :  Stop and restart timer
////////////////////////////////////////////////////////////////////////////////
void TimeoutTimerRestart(void)
{
  // Stop count
  tmr_counter_enable(TMR2, FALSE);
  // Set counter to start
  tmr_counter_value_set(TMR2, 0);
  tmr_counter_enable(TMR2, TRUE);
  // overflow interrupt enable
  tmr_interrupt_enable(TMR2, TMR_OVF_INT, TRUE);
}

////////////////////////////////////////////////////////////////////////////////
// Function     :  Прерывание от таймера таймаута
// Input        :  Нет
// Output       :  Нет
// Description  :  
////////////////////////////////////////////////////////////////////////////////
void TMR2_GLOBAL_IRQHandler(void)
{
  if (tmr_flag_get(TMR2, TMR_OVF_FLAG) != RESET) {
    tmr_flag_clear(TMR2, TMR_OVF_FLAG);
    if (SendRecTimeoutFlag) {
      // Pause before send finished
      SendRecTimeoutFlag = 0;
      // Сбросили и остановили timer
      tmr_counter_enable(TMR2, FALSE);
      tmr_counter_value_set(TMR2, 0);
      // Prepare UART send process
      OsdpUartSend();
    }
    else {
      // It is timeout on receive
      OsdpUartReset();
    }
  }
}

/////////////////////////////////////////////////////////////////
//         Change USART1 parameters
/////////////////////////////////////////////////////////////////
void OsdpUartSetBaudrate(uint32_t baud)
{
  // Disable UART interrupt
  nvic_irq_disable(USART1_IRQn);
  // Stop UART
  usart_enable(USART1, FALSE);
  usart_transmitter_enable(USART1, FALSE);
  usart_receiver_enable(USART1, FALSE);
  // Change baudrate
  usart_init(USART1, baud, USART_DATA_8BITS, USART_STOP_1_BIT);
  // Enable UART again
  usart_transmitter_enable(USART1, FALSE);
  usart_receiver_enable(USART1, TRUE);
  usart_enable(USART1, TRUE);
  //
  
  NVIC_ClearPendingIRQ(USART1_IRQn);
  // Enable UART interrupt
  nvic_irq_enable(USART1_IRQn, 0, 0);
}

/////////////////////////////////////////////////////////////////
//   Initialize USART1, PA9 - TX, PA10 - RX, PA11 - direction
/////////////////////////////////////////////////////////////////
void InitOsdpUart(uint32_t baudrate)
{
gpio_init_type gpio_init_struct;

  // enable the usart1 clock
  crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);

  //InitDirectionPin();       // THIS CALLED FROM BOARD INIT() !!!!
  
  // set default parameters and setup TX pin
  gpio_default_para_init(&gpio_init_struct);
  // configure the usart tx/rx pin
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type  = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  // In our board RX and TX on same port
  gpio_init_struct.gpio_pins = UART_TX_GPIO_PIN | UART_RX_GPIO_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP; //GPIO_PULL_NONE;
  gpio_init(UART_TX_GPIO_PORT, &gpio_init_struct);

  // configure usart params
  OsdpUartSetBaudrate(baudrate);
  OsdpUartReset();  
  
  // Init Timeout timer in stop state - start then char arrive
  TimeoutTimerInit();
}

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
