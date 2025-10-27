/*******************************************************************************
 *
 *  МОДУЛЬ        : OSDPQueue.c
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


#include "OSDPQueue.h" 

/******************************************************************************* 
 *                    LOCAL MODULE VARIABLES
 *******************************************************************************/
static volatile tSendCardDataQueue SendCardDataQueue;
static volatile tLocalStatusQueue  LocalStatusQueue;
static volatile tInputStatusQueue  InputStatusQueue;
#ifdef USE_RELAY
static volatile tOutputStatusQueue OutputStatusQueue;
#endif
#ifdef QR_READER
static volatile tSendQrDataQueue SendQrDataQueue;
#endif

/******************************************************************************* 
 *                      CARD DATA QUEUE
 *******************************************************************************/
 
////////////////////////////////////////////////////////////////////////////////
// Function     : AddCardDataQueue                                               // NOTE: if use in UART interrupt, need disable INT
// Input        : reader  - reader number (0, 1, ...)
//                card    - card bit's array, left justif-d
//                type    -  0-no parity, 1-with parity
//                bits    - number of bits in card code (include parity if so)
// Output       : none
// Description  : Add new cards data to queue with convertation for OSDP out
////////////////////////////////////////////////////////////////////////////////
void AddCardDataQueue(uint8_t reader, uint64_t card, uint8_t type, uint8_t bits)
{
uint8_t count, n;

  // Transfer input data to queue
  SendCardDataQueue.CardData[SendCardDataQueue.WritePtr].ReaderNo = reader;
  SendCardDataQueue.CardData[SendCardDataQueue.WritePtr].CardFormat = type;
  SendCardDataQueue.CardData[SendCardDataQueue.WritePtr].BitCount = bits;
  SendCardDataQueue.CardData[SendCardDataQueue.WritePtr].CntMsb = 0;
  // Calculate and store card data bytes
  count = bits / 8;
  if (bits % 8 != 0)
    count++;
  // Fill card data bytes
  for (n = 0; n < count; n++) {
    SendCardDataQueue.CardData[SendCardDataQueue.WritePtr].CardBytes[n] = (uint8_t)(card >> (64 - (8 * (n + 1)))); 
  }
  // Store bytes count for output in GetCardFromQueue
  SendCardDataQueue.CardData[SendCardDataQueue.WritePtr].ByteCount = count;
  // Shift write pointer
  ++SendCardDataQueue.WritePtr;
  // Check if queue overflow
  if (SendCardDataQueue.WritePtr >= CARD_DATA_Q_LENGTH)
    SendCardDataQueue.WritePtr = 0;
  
  // Version 4.3 - shift ReadPtr if need
  if (SendCardDataQueue.WritePtr == SendCardDataQueue.ReadPtr) {
    ++SendCardDataQueue.ReadPtr;
    if (SendCardDataQueue.ReadPtr >= CARD_DATA_Q_LENGTH)
      SendCardDataQueue.ReadPtr = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Function     : GetCardFromQueue 
// Input        : pointer to buffer for data (dest)
// Output       : len - data lenght, added to buffer dest
//                0 if no data, 1 if data transfered
// Description  : Transfer card data (if present) to buffer dest (OSDP packet
//                assumed).
////////////////////////////////////////////////////////////////////////////////
uint8_t GetCardFromQueue(uint8_t *dest, uint8_t *len)
{
  if (SendCardDataQueue.ReadPtr == SendCardDataQueue.WritePtr)
    return 0;
    
  // We have data - transfer it to output
  *len = SendCardDataQueue.CardData[SendCardDataQueue.ReadPtr].ByteCount + 4;
  memcpy(dest, (void*)&SendCardDataQueue.CardData[SendCardDataQueue.ReadPtr], 
         SendCardDataQueue.CardData[SendCardDataQueue.ReadPtr].ByteCount + 4);
  // Shift read pointer
  ++SendCardDataQueue.ReadPtr;
  // Check if queue overflow
  if (SendCardDataQueue.ReadPtr >= CARD_DATA_Q_LENGTH)
    SendCardDataQueue.ReadPtr = 0;
  return 1;
}

/*******************************************************************************
 *                      LOCAL STATUS QUEUE
 *******************************************************************************/
 
////////////////////////////////////////////////////////////////////////////////
// Function     : AddLocalStatusQueue                                             // NOTE: if use in UART interrupt, need disable INT
// Input        : tamper and power status
// Output       : none
// Description  : Add changed local status to queue
////////////////////////////////////////////////////////////////////////////////
void AddLocalStatusQueue(uint8_t tamper, uint8_t power)
{
  // Put status to queue
  LocalStatusQueue.Status[LocalStatusQueue.WritePtr].Power = power;
  LocalStatusQueue.Status[LocalStatusQueue.WritePtr].Tamper = tamper;
  // Shift write pointer
  ++LocalStatusQueue.WritePtr;
  if (LocalStatusQueue.WritePtr >= LOCAL_SATAT_Q_LENGTH)
    LocalStatusQueue.WritePtr = 0;
  
  // Version 4.3 - shift ReadPtr if need
  if (LocalStatusQueue.WritePtr == LocalStatusQueue.ReadPtr) {
    ++LocalStatusQueue.ReadPtr;
    if (LocalStatusQueue.ReadPtr >= LOCAL_SATAT_Q_LENGTH)
      LocalStatusQueue.ReadPtr = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Function     : GetLocalStatusFromQueue  
// Input        : pointer to buffer for data (dest)
// Output       : len - data lenght, added to buffer dest
//                0 if no data, 1 if data transfered
// Description  : Transfer local status data (if present) to buffer dest 
//                (OSDP packet assumed).
////////////////////////////////////////////////////////////////////////////////
uint8_t GetLocalStatusFromQueue(uint8_t *dest, uint8_t *len)
{
  if (LocalStatusQueue.ReadPtr == LocalStatusQueue.WritePtr)
    return 0;
    
  // We have data - transfer it to output
  *len = sizeof(tLocalStatus);
  memcpy(dest, (void*)&LocalStatusQueue.Status[LocalStatusQueue.ReadPtr], sizeof(tLocalStatus));
  // Shift read pointer
  ++LocalStatusQueue.ReadPtr;
  // Check if queue overflow
  if (LocalStatusQueue.ReadPtr >= LOCAL_SATAT_Q_LENGTH)
    LocalStatusQueue.ReadPtr = 0;
  return 1;
}


/*******************************************************************************
 *                      INPUT STATUS QUEUE
 *******************************************************************************/

////////////////////////////////////////////////////////////////////////////////
// Function     : AddInputStatusQueue                                             // NOTE: if use in UART interrupt, need disable INT
// Input        : status for N logical inputs
// Output       : none
// Description  : Add changed input's status to queue
////////////////////////////////////////////////////////////////////////////////
void AddInputStatusQueue(uint8_t *status, uint8_t count)
{
  if (count > INPUT_STATUS_COUNT)
    count = INPUT_STATUS_COUNT;

  // Put status to queue
  memcpy ((void*)&InputStatusQueue.Status[InputStatusQueue.WritePtr], status, INPUT_STATUS_COUNT);
  // Shift write pointer
  ++InputStatusQueue.WritePtr;
  if (InputStatusQueue.WritePtr >= INPUT_SATAT_Q_LENGTH)
    InputStatusQueue.WritePtr = 0;
  
  // Version 4.3 - shift ReadPtr if need
  if (InputStatusQueue.WritePtr == InputStatusQueue.ReadPtr) {
    ++InputStatusQueue.ReadPtr;
    if (InputStatusQueue.ReadPtr >= INPUT_SATAT_Q_LENGTH)
      InputStatusQueue.ReadPtr = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Function     : GetInputStatusFromQueue  
// Input        : pointer to buffer for data (dest)
// Output       : len - data lenght, added to buffer dest
//                0 if no data, 1 if data transfered
// Description  : Transfer input status data (if present) to buffer dest 
//                (OSDP packet assumed).
////////////////////////////////////////////////////////////////////////////////
uint8_t GetInputStatusFromQueue(uint8_t *dest, uint8_t *len)
{
  if (InputStatusQueue.ReadPtr == InputStatusQueue.WritePtr)
    return 0;
    
  // We have data - transfer it to output
  *len = sizeof(tInputStatus);
  memcpy(dest, (void*)&InputStatusQueue.Status[InputStatusQueue.ReadPtr], sizeof(tInputStatus));
  // Shift read pointer
  ++InputStatusQueue.ReadPtr;
  // Check if queue overflow
  if (InputStatusQueue.ReadPtr >= INPUT_SATAT_Q_LENGTH)
    InputStatusQueue.ReadPtr = 0;
  return 1;
}


/*******************************************************************************
 *                      OUTPUT STATUS QUEUE
 *******************************************************************************/
#ifdef USE_RELAY
////////////////////////////////////////////////////////////////////////////////
// Function     : AddOutputStatusQueue                                             // NOTE: if use in UART interrupt, need disable INT
// Input        : status for N logical outputs
// Output       : none
// Description  : Add status of all outputs to queue
////////////////////////////////////////////////////////////////////////////////
void AddOutputStatusQueue(uint8_t *status, uint8_t count)
{
  if (count > OUTPUT_STATUS_COUNT)
    count = OUTPUT_STATUS_COUNT;

  // Put status to queue
  memcpy ((void*)&OutputStatusQueue.Status[OutputStatusQueue.WritePtr], status, OUTPUT_STATUS_COUNT);
  // Shift write pointer
  ++OutputStatusQueue.WritePtr;
  if (OutputStatusQueue.WritePtr >= OUTPUT_SATAT_Q_LENGTH)
    OutputStatusQueue.WritePtr = 0;
  
  // Version 4.3 - shift ReadPtr if need
  if (OutputStatusQueue.WritePtr == OutputStatusQueue.ReadPtr) {
    ++OutputStatusQueue.ReadPtr;
    if (OutputStatusQueue.ReadPtr >= OUTPUT_SATAT_Q_LENGTH)
      OutputStatusQueue.ReadPtr = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Function     : GetOutputStatusFromQueue  
// Input        : pointer to buffer for data (dest)
// Output       : len - data lenght, added to buffer dest
//                0 if no data, 1 if data transfered
// Description  : Transfer output status data (if present) to buffer dest 
//                (OSDP packet assumed).
////////////////////////////////////////////////////////////////////////////////
uint8_t GetOutputStatusFromQueue(uint8_t *dest, uint8_t *len)
{
  if (OutputStatusQueue.ReadPtr == OutputStatusQueue.WritePtr)
    return 0;
    
  // We have data - transfer it to output
  *len = sizeof(tOutputStatus);
  memcpy(dest, (void*)&OutputStatusQueue.Status[OutputStatusQueue.ReadPtr], sizeof(tOutputStatus));
  // Shift read pointer
  ++OutputStatusQueue.ReadPtr;
  // Check if queue overflow
  if (OutputStatusQueue.ReadPtr >= OUTPUT_SATAT_Q_LENGTH)
    OutputStatusQueue.ReadPtr = 0;
  return 1;
}
#endif

#ifdef QR_READER
////////////////////////////////////////////////////////////////////////////////
// Function     : AddQrDataQueue  
// Input        : Reader No., pinter to data, data length
// Output       : none
// Description  : Transfer QR data (if present) quiue
//                
////////////////////////////////////////////////////////////////////////////////
void AddQrDataQueue(uint8_t reader, uint8_t *data, uint8_t len)
{
  // Check for overflow
  if (len > QR_DATA_SIZE)
    return;
  
  // Transfer input data to queue
  SendQrDataQueue.QrData[SendQrDataQueue.WritePtr].ReaderNo = reader;       // This parameters need like OSDP standard
  SendQrDataQueue.QrData[SendQrDataQueue.WritePtr].Dir = 0;                 // This parameters need like OSDP standard
  SendQrDataQueue.QrData[SendQrDataQueue.WritePtr].DLen = len;
  memcpy((void*)SendQrDataQueue.QrData[SendQrDataQueue.WritePtr].Data, data, len);

  // Shift write pointer
  ++SendQrDataQueue.WritePtr;
  if (SendQrDataQueue.WritePtr >= QR_DATA_Q_LENGTH)
    SendQrDataQueue.WritePtr = 0;
  
  // Version 4.3 - shift ReadPtr if need
  if (SendQrDataQueue.WritePtr == SendQrDataQueue.ReadPtr) {
    ++SendQrDataQueue.ReadPtr;
    if (SendQrDataQueue.ReadPtr >= QR_DATA_Q_LENGTH)
      SendQrDataQueue.ReadPtr = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Function     : GetQrFromQueue  
// Input        : pointer to buffer for data (dest)
// Output       : len - data lenght, added to buffer dest
//                0 if no data, 1 if data transfered
// Description  : Transfer QR data (if present) to buffer dest 
//                (OSDP packet assumed).
////////////////////////////////////////////////////////////////////////////////
uint8_t GetQrFromQueue(uint8_t *dest, uint8_t *len)
{
  if (SendQrDataQueue.ReadPtr == SendQrDataQueue.WritePtr)
    return 0;
    
  // We have data - transfer it to output. Format: ReaderNo, Direction, char count, data
  *len = SendQrDataQueue.QrData[SendQrDataQueue.ReadPtr].DLen + 3;
  memcpy(dest, (void*)&SendQrDataQueue.QrData[SendQrDataQueue.ReadPtr], 
         SendQrDataQueue.QrData[SendQrDataQueue.ReadPtr].DLen + 3);
  // Shift read pointer
  ++SendQrDataQueue.ReadPtr;
  // Check if queue overflow
  if (SendQrDataQueue.ReadPtr >= QR_DATA_Q_LENGTH)
    SendQrDataQueue.ReadPtr = 0;
  return 1;
}
#endif


////////////////////////////////////////////////////////////////////////////////
// Function     : OsdpQueueInit
// Input        : none
// Output       : none
// Description  : Init all OSDP queue's - clear data, reset pointers
////////////////////////////////////////////////////////////////////////////////
void OsdpQueueInit(void)
{
  memset((void*)&SendCardDataQueue, 0, sizeof(tSendCardDataQueue));
  memset((void*)&LocalStatusQueue, 0, sizeof(tLocalStatusQueue));
  memset((void*)&InputStatusQueue, 0, sizeof(tInputStatusQueue));
#ifdef USE_RELAY
  memset((void*)&OutputStatusQueue, 0, sizeof(tOutputStatusQueue));
#endif
#ifdef QR_READER
  memset((void*)&SendQrDataQueue, 0, sizeof(tSendQrDataQueue));
#endif
}

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
