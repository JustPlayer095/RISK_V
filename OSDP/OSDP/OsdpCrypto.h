/*******************************************************************************
 *
 *  МОДУЛЬ        : OsdpCrypto.h
 *
 *  Автор         : Л.Стасенко
 *  Дата начала   : 10.07.2018
 *  Версия        : 1.1
 *  Комментарии    : Поддержка шифрованного обмена для протокола OSDP 
 *                  (V 2.1.6 - 2014)
 ******************************************************************************/
/*

*/


#ifndef OSDP_CRYPTO_DEF_H
#define OSDP_CRYPTO_DEF_H

#include <string.h>
#include "AppTypes.h"
#include "OsdpUart.h"


/*******************************************************************************                                                             :
 *      
 *******************************************************************************/
typedef enum {
  stPlain,          // Standard plain exchange, first step to state Challenge
  stChallenge,      // Continue, second step
  stCrypto,         // Work in security mode, session opened
  //stSecur           // 
} tSecurExchState;
extern tSecurExchState SecurExchState;

extern volatile uint8_t SecurModeOn;
extern volatile uint8_t SecureReady;

/*******************************************************************************                                                             :
 *                ФУНКЦИИ МОДУЛЯ ОБЩЕГО НАЗНАЧЕНИЯ
 *******************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif

// Reset secure communication
void reset_sc(uint8_t* msg, uint8_t reason);
// SC initialization
void SecureChannelInit(uint8_t *scbk);
// Decode commands with SC flag set
void DecodeSecuredCommand(uint8_t *msg);
void EncodeAnswerPacket(uint8_t *msg);

#if defined(__cplusplus)
}
#endif

#endif

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
