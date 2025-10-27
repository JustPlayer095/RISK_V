/*******************************************************************************
 *
 *  МОДУЛЬ        : OSDP.c
 *
 *  Автор         : Л.Стасенко
 *  Дата начала   : 02.12.2014
 *  Версия        : 1.1
 *  Комментарии    : Поддержка протокола OSDP (V 2.1.6 - 2014)
 *
 ******************************************************************************/
/*
  Драйвер работает ВМЕСТО touch-Parsec-wiegand, поэтому ресурсы
  указанных интерфейсов задействуются здесь альтернативно, в частности
  В режиме OSDP    
    1. Таймер 1 (32 бита) - для отсчета времен протокола
    2. UART0 в режиме RS-485
*/

#include "OSDP.h"
#include "BoardFlash.h"
#include "BoardGpio.h"
#include "OSDPQueue.h" 
#include "CcittCrc16.h"
#include "OsdpCommands.h"
#include "OsdpInputs.h"
#include "OsdpCrypto.h"
#include "Indication.h"
#ifdef USE_KEYPAD
#include "Keypad.h"
#endif
// Для работы с EEPROM
#ifndef READER_125
#include <phhalHw_Rc663_Cmd.h>
#include <ph_Status.h>

// AES keys manipulation
#ifndef USE_BANK_CARD
//#include "Reader13_MfpSb.h"
#include "Reader13.h"
#else
#include "BankUtil.h"
#endif
#endif
//
#ifdef QR_READER
#include "QrReader.h"
#endif
#ifndef READER_125
#include "AesKeys.h"
#endif

extern void CopyAesKeysFromEE(void);

/******************************************************************************* 
 *                   ПИН - код на отправку хосту
 *******************************************************************************/
#ifdef USE_KEYPAD

#endif

//////////////////////////////////////////////////////////////////////////////
//   Local ststaus of device (tamper, power) - not present, set to OK
//////////////////////////////////////////////////////////////////////////////
tLocalStatus OsdpLocalStatus;
static const tLocalStatus DefOsdpLocalStatus = {0, 0};

//////////////////////////////////////////////////////////////////////////////
// Если версия с выходным реле
#ifdef USE_RELAY
//////////////////////////////////////////////////////////////////////////////
tRelayStatus OsdpRelayStatus = {0, 0, 0};
#endif


uint8_t OsdpLastPackNumber = 5;

/******************************************************************************* 
 *                ФУНКЦИИ МОДУЛЯ ОБЩЕГО НАЗНАЧЕНИЯ
 *******************************************************************************/
#ifdef USE_RELAY
////////////////////////////////////////////////////////////////////////////////
// Function     :  OsdpRelayTimer
// Input        :  none
// Output       :  Нет
// Description  :  Decrement relay timer for OSDP
////////////////////////////////////////////////////////////////////////////////
void OsdpRelayTimer(void)
{
  if (OsdpRelayStatus.Timer) {
    if (--OsdpRelayStatus.Timer == 0) {
      // Change relay state to old state
      if (OsdpRelayStatus.OldState) {
        RELAY_ON;
        OsdpRelayStatus.State = 1;
      }
      else {
        RELAY_OFF;
        OsdpRelayStatus.State = 0;
      }
      __disable_irq();
      AddOutputStatusQueue(&OsdpRelayStatus.State, OUTPUT_STATUS_COUNT);
      __enable_irq();
    }
  }
}
#endif
 
////////////////////////////////////////////////////////////////////////////////
// Function     :  OsdpCheckCommand
// Input        :  Нет
// Output       :  Нет
// Description  :  Если есть принятые данные, то разбираем их
////////////////////////////////////////////////////////////////////////////////
void OsdpCheckCommand(void)
{
uint16_t dlen;
uint8_t cmd;
uint8_t n, i;
uint32_t tdw;
uint8_t *ptr;
#ifndef READER_125
uint8_t ret;
uint16_t tw;
#endif
  
  if (OsdpCommand.Ready) {
    cmd = OsdpCommand.Command;
    dlen = OsdpCommand.DLen;
    if (OsdpCommand.MfgCommand == 0) {
      // Расшифровываем и исполняем команду
      switch (cmd) {
        /* В ДАННОМ РИДЕРЕ НЕ ПОДДЕРЖИВАЕТСЯ
        */
        //////////////////////////////////////////////////////////////
        //                                      Output Control Command
        #ifdef USE_RELAY
        case osdp_OUT:
          // Наш первый выход - смотрим что делать
          switch (OsdpCommand.Data[1]) {
            // 0 - не делать ничего, 1 - выключить, 2 - включить
            case 0:
            break;
            // Включить-выключить, прервав таймерное состояние
            case 1:
            case 2:
              // Отработку таймера прерываем
              OsdpRelayStatus.Timer = 0;
              // Ставим заданное состояние
              if (OsdpCommand.Data[1] == 1) {
                RELAY_OFF;
                OsdpRelayStatus.State = OsdpRelayStatus.OldState = 0;
              }
              else {
                RELAY_ON;
                OsdpRelayStatus.State = OsdpRelayStatus.OldState = 1;
              }
              // Статус поменяли и в очередь
              __disable_irq();
              AddOutputStatusQueue(&OsdpRelayStatus.State, OUTPUT_STATUS_COUNT);
              __enable_irq();
            break;
            // Отработка по таймеру с последующим восстановлением
            case 5:
            case 6:
              OsdpRelayStatus.OldState = OsdpRelayStatus.State;
              // Поставили время
              OsdpRelayStatus.Timer = OsdpCommand.Data[2] + 
                 ((uint16_t)OsdpCommand.Data[3] << 8);
              // ... и заданное состояние
              if (OsdpCommand.Data[1] == 6) {
                RELAY_OFF;
                OsdpRelayStatus.State = 0;
              }
              else {
                RELAY_ON;
                OsdpRelayStatus.State = 1;
              }
              // Статус поменяли и в очередь
              __disable_irq();
              AddOutputStatusQueue(&OsdpRelayStatus.State, OUTPUT_STATUS_COUNT);
              __enable_irq();
            break;
              // С отсрочкой до окончания таймерного поставить
              // не можем (коды 3, 4)
            default:
            break;
          }
          /*
          // Если отработалось корректно, то возвращаем статус выхода - ставим в очередь
          if (OsdpMessage.OsdpPacket.OsdpData.CmdReplay != osdp_NAK) {
            OsdpMessage.OsdpPacket.OsdpData.Data[0] = OsdpRelayStatus.State;
            OsdpMessage.OsdpPacket.OsdpData.DLen += 1;
            OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_OSTATR;
          }
          */
        break;
        #endif
              
        //////////////////////////////////////////////////////////////
        //                                  Reader Led Control Command
        case osdp_LED:
          // Определяем число светодиодов
          n = (dlen / LED_CONTROL_SIZE);
          // Разбираем все светодиоды
          for (i = 0; i < n; i++) {
            // [0] - номер ридера, [1] - номер LED
            ptr = &OsdpCommand.Data[0] + (i * LED_CONTROL_SIZE);
            if (SetLedControl(ptr) == 0) {
              // Ошибка
              OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_NAK;
              OsdpMessage.OsdpPacket.OsdpData.Data[0] = osdp_err_UNABLE_PROCESS;
              OsdpMessage.OsdpPacket.OsdpData.DLen++;
            }
          }
        break;
          
        //////////////////////////////////////////////////////////////
        //                              Reader Buzzer Control Command
        case osdp_BUZ:
          // Копируем структуру к себе
          memcpy(&OsdpBeeperControl.ReaderNumber , &OsdpCommand.Data[0], BEEP_CONTROL_SIZE);
          OsdpBeeperControl.BeepCounValue = 1;
        break;
        
        //////////////////////////////////////////////////////////////
        //                              PD Communication Configuration
        case osdp_COMSET:
          // Wait answer just send
          while (OsdpBusy) {
          };
          // Первый байт - новый адрес, затем 4 байта новой скорости (little endian)
          memcpy(&tdw, &OsdpCommand.Data[1], sizeof(tdw));
          // ... а это адрес
          n = OsdpCommand.Data[0];
          // Новый адрес и скорость запоминаем
          ReaderConfig.CommonConfig.ReaderAddress = n;
          DeviceAddress = n;
          ReaderConfig.CommonConfig.BaudRate = tdw;
          // Now we can set new parameters
          //OsdpUartSetParams(tdw, n);
          SaveReaderConfig(&ReaderConfig);
          // Reset temp variable - we finish update params
          NewCommSpeed = 0;
          // Инициализировали драйвер RS-485
          OsdpUartSetBaudrate(ReaderConfig.CommonConfig.BaudRate);
          OsdpUartReset();  
        break;
        
        /* В ДАННОМ РИДЕРЕ НЕ ПОДДЕРЖИВАЕТСЯ
        // Text Output Command
        case osdp_TEXT:
        // Time and Date Command
        case osdp_TDSET:
        // Data Transfer Command - obsolette
        case osdp_DATA:
        break;
        // See Appendix E
        case osdp_XWR:
        break;
        // Set Automatic Prompt Strings
        case osdp_PROMPT:
        // Scan and Send Biometric Data
        case osdp_BIOREAD:
        // Scan and Match Bio Template
        case osdp_BIOMATCH:
        // Challenge and Secure Init Rq.
        case osdp_CHLNG:
        // Server Cryptogram
        case osdp_CONT:
        break;
        */

        //////////////////////////////////////////////////////////////
        //                                  Encryption Key Set Command
        #ifndef READER_125
        case osdp_KEYSET:
          // All check done before - just process command. Take key number
          n =  OsdpCommand.Data[0];
          // Calculate key address
          tw = CRIPTO_KEYS_START + CRIPTO_KEYS_SIZE * n;
          ret = WriteCriptoKeyToEe(tw, &OsdpCommand.Data[2]);
          if (ret == PH_ERR_SUCCESS) {
            // Copy from EEPROM to working var's
            CopyAesKeysFromEE();
          }
        break;
        #endif
        
        default:
        break;
      }
    }
    else {
      //////////////////////////////////////////////////////////////
      //                               Manufacturer Specific Command
      //////////////////////////////////////////////////////////////
      switch (cmd) {
        //========================================================
        //                             Copy of osdp_KEYSET command
        case mfg_KEY_SET:
          // All check done before - just process command. Take key number
          n =  OsdpCommand.Data[0];
          // Calculate key address
          tw = CRIPTO_KEYS_START + CRIPTO_KEYS_SIZE * n;
          ret = WriteCriptoKeyToEe(tw, &OsdpCommand.Data[2]);
          if (ret == PH_ERR_SUCCESS) {
            // Copy from EEPROM to working var's
            CopyAesKeysFromEE();
          }
        break;
        
        //========================================================
        //                Set wiegand format (non volatile memory)
        case mfg_SETWGFMT:
          // Пишем в память
          memcpy(&ReaderConfig.CommonConfig.WiegandFormat, &OsdpCommand.Data[0], sizeof(tWiegandFormat));
          SaveReaderConfig(&ReaderConfig);
        break;
        
        //                              Set Mifare security params
        case mfg_SETMIFSEC:
          // Пишем в память
          ReaderConfig.CommonConfig.MifareSecurSector = OsdpCommand.Data[0];
          memcpy(&ReaderConfig.CommonConfig.MifareSecurKey[0], &OsdpCommand.Data[1], 6);
          SaveReaderConfig(&ReaderConfig);
        break;
        
        //========================================================
        //                           Set cards for read parameters
        case mfg_SETCARDP:
          // Take from host and write to config
          memcpy(&ReaderConfig.CommonConfig.MifareMode, &OsdpCommand.Data[0], sizeof(tMifareSecureModes));
          memcpy(&ReaderConfig.CommonConfig.CardForRead, &OsdpCommand.Data[1], sizeof(tCardForRead));
          SaveReaderConfig(&ReaderConfig);
        break;
          
        #ifdef USE_KEYPAD
        //========================================================
        //                 Write keypad mode (card, PIN, card+PIN)
        case mfg_SETPINMOD:
          // Take from host and write to config
          ReaderConfig.CommonConfig.CardOrKeyMode = OsdpCommand.Data[0];
          SaveReaderConfig(&ReaderConfig);
        break;

        //========================================================
        //                 Change PIN mode for OSDP
        case mfg_CHGPINMOD:
          // Take from host and set variables
          OsdpPinOnly = OsdpCommand.Data[0];
          OsdpSingleKeyMode = OsdpCommand.Data[1];
          if (OsdpPinOnly)
            KeypadOn(1);
        break; 

        //========================================================
        //           Cancel PIN wait time
        // Added 10.08.2021 for stop wait PIN from software
        case mfg_CANCPINMOD:
//        __disable_irq();
          if (KeyWaitTimer != 0)
            KeyWaitTimer = 0;
//        __enable_irq();
        break;
        #endif
                
        //========================================================
        //           Change comm speed, not stored in non-volatile
        case mfg_CHGCOMSPEED:
          memcpy(&tdw, &OsdpCommand.Data[0], sizeof(tdw));
          // Новую скорость запоминаем
          ReaderConfig.CommonConfig.BaudRate = tdw;
          // Wait answer just send
          while (OsdpBusy) {
          };
          // Now we can sey new parameters
          OsdpUartSetBaudrate(tdw);
        break;

/* */
        //========================================================
        //                                 Set indication LED mode
        case mfg_SETLEDINDIC:
          // Мигание или непрерывный
          if ((OsdpCommand.Data[0] == 0) || (OsdpCommand.Data[1] == 0)) {
            // Непрерывный
            SetOsdpIndicationNormal(OsdpCommand.Data[2]);
          }
          else {
            // Мигание
            SetOsdpIndicationBlink(OsdpCommand.Data[2], // OnColor
                               OsdpCommand.Data[0],     // OnTime
                               OsdpCommand.Data[1]);    // OffTime
          }
        break;
        
        #ifdef GLASS_READER
        #ifdef USE_KEYPAD
        //========================================================
        //                                  Set backlight LED mode
        case mfg_SETLEDBACKLT:
          // Мигание или непрерывный
          if ((OsdpCommand.Data[0] == 0) || (OsdpCommand.Data[1] == 0)) {
            // Непрерывный
            // SLEO !!! SetBacklightNormal(OsdpCommand.Data[2]);
          }
          else {
            // Мигание
            // SLEO !!!SetBacklightBlink(OsdpCommand.Data[2],     // OnColor
            //                  OsdpCommand.Data[0],     // OnTime
            //                  OsdpCommand.Data[1]);    // OffTime
          }
        break;
        #endif
        #endif
/* */

        //========================================================
        //             Set indication default colors (not volatile)
        case mfg_SETDEFCOLORS:
          memcpy(&ReaderConfig.CommonConfig.IndicationColors, &OsdpCommand.Data[0], sizeof(tIndicationColors));
          SaveReaderConfig(&ReaderConfig);
          // Ответом будет ACK
        break;
            
        //========================================================
        //    Set new ID report (for version change in production)
        // Version 3.6
        case mfg_SETIDREPORT:
          memcpy(&ReaderConfig.DeviceIdReport, &OsdpCommand.Data[0], sizeof(tDeviceIdReport));
          SaveReaderConfig(&ReaderConfig);
          // Version 3.6 - save production configuration to InfoPage-2
          SaveProductionConfig(&ReaderConfig);
          // Ответом будет ACK
        break;
        
        //========================================================
        //                  Program QR scanner parameters and mode
        // 
        #ifdef QR_READER
        case mfg_QR_CONFIG_CMD:
          // Depend of config parameters (Now support only light config)
          switch (OsdpCommand.Data[0]) {
            case 1:
              // Copy QR light config to variable
              memcpy((void*)&QrLightConfig, &OsdpCommand.Data[1], sizeof(tQrLightConfig));
              // Check parameters validity
              if (QrCodeData.QrVarData.ProgData.Config.Light > SET_LIGHT_AUTO) {
                QrCodeData.QrVarData.ProgData.Config.Light = SET_PARA_AUTO;
              }
              if (QrCodeData.QrVarData.ProgData.Config.Sight >= SET_LIGHT_AUTO) {
                QrCodeData.QrVarData.ProgData.Config.Sight = SET_PARA_AUTO;
              }
              // Check for valid light level
              if (QrLightConfig.LightVal < QR_MIN_LIGHT)
                QrLightConfig.LightVal = QR_MIN_LIGHT;
              if (QrLightConfig.LightVal > QR_MAX_LIGHT)
                QrLightConfig.LightVal = QR_MAX_LIGHT;
              // ... and write config to EEPROM. Parameters will be changed after call
              // QrCheckLightConfig() function from MainReaderTask
              QrWriteLightConfig();
            break;
            default:
            break;
          }
        break;
        #endif

        //========================================================
        //                    Set HMAC-SHA mode (SHA-1 or SHA-256)
        case mfg_SET_SHA_MODE:
          // Take byte with mode
          n =  OsdpCommand.Data[0];
          // Write it to device configuration in flash
          SetPanHashMode(n);
        break;
        
        //========================================================
        //                   Set ID configuration for Mifare cards
        case mfg_ID_LOCATE_CFG:
          if (OsdpCommand.Data[0] == 0) {
            // Mifare Classic
            memcpy(&MfcIdConfig, &OsdpCommand.Data[1], sizeof(MifareIdConfig_t));
            SetMfcIdConfig(&MfcIdConfig);
          }
          else {
            // Mifare plus
            memcpy(&MfpIdConfig, &OsdpCommand.Data[1], sizeof(MifareIdConfig_t));
            SetMfpIdConfig(&MfpIdConfig);
          }
        break;
        //========================================================
        //                                Reset to factory default
        case mfg_RES_TO_FACT:
          SetDefaultReaderConfig();
        break;
      }
    }   
    // Освободили структуру в любом случае
    OsdpCommand.Ready = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Function     : OsdpInit
// Input        : Нет
// Output       : Нет
// Description  : Поумолчательные значения переменных OSDP и очистка 
//                параметров
////////////////////////////////////////////////////////////////////////////////
void OsdpInit(void)
{
  OsdpLastPackNumber = 5;
  // Clear structure for upper level
  memset(&OsdpCommand, 0, sizeof(tOsdpCommand));
#ifdef USE_KEYPAD
  ClearOsdpPinData();
#endif
  OsdpLocalStatus = DefOsdpLocalStatus;
  OsdpQueueInit();
  // Init secure channel
  // SecureChannelInit(); - init at start of new session
}

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
