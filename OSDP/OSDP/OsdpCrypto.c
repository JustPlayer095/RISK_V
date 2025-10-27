/*******************************************************************************
 *
 *  МОДУЛЬ        : OsdpCrypto.c
 *
 *  Автор         : Л.Стасенко
 *  Дата начала   : 20.08.2024
 *  Версия        : 1.1
 *  Комментарии    : Поддержка шифрованного обмена для протокола OSDP 
 *                  (V 2.1.6 - 2014)
 ******************************************************************************/
/*

*/

#include "OsdpCrypto.h"
#include "OsdpCommands.h"
#include "aes.h"
#include "OsdpUart.h"

#define DEBUG_SCBK_AND_RND


/*******************************************************************************
 *               Global variables for external use
 *******************************************************************************/
tSecurExchState SecurExchState = stPlain;
//
volatile uint8_t SecurModeOn = 0;
// Флаг что можно включить защищенный режим
volatile uint8_t SecureReady = 0;

/*******************************************************************************
 *               Security channel variables as struct
 *******************************************************************************/
typedef struct  {
  uint8_t pd_client_uid[8];       // 8 байт от ID report клиента
  uint8_t scbk[16];               // Secure Channel Base Key
  uint8_t s_enc[16];              // Сессионный ключ шифрования
  uint8_t s_mac1[16];             // Сессионный ключ номер 1 аутентификации сообщений
  uint8_t s_mac2[16];             // Сессионный ключ номер 2 аутентификации сообщений
  uint8_t r_mac[16];              // Replay MAC (пакеты от PD к CP)
  uint8_t c_mac[16];              // Command MAC (пакеты от CP к PD)
  uint8_t cp_random[8];           // Случайное число от CP (RND.A)
  uint8_t pd_random[8];           // Случайное число от PD (RND.B)
  uint8_t cp_cryptogram[16];      // Криптограмма сервера (CP)
  uint8_t pd_cryptogram[16];      // Криптограмма клиента (PD)
} osdp_secure_channel_t;
osdp_secure_channel_t SC;
// Variable for temporary storage iv, MAC, e.t.c.
uint8_t temp_data[16];
//
uint8_t temp_c_mac[16];

/******************************************************************************* 
 *                  Local variables and types
 *******************************************************************************/
// SEC_BLK_TYPE constants
const uint8_t SCS_11 = 0x11; // CP->PD
const uint8_t SCS_12 = 0x12; // PD->CP
const uint8_t SCS_13 = 0x13; // CP->PD
const uint8_t SCS_14 = 0x14; // PD->CP
const uint8_t SCS_15 = 0x15; // CP->PD
const uint8_t SCS_16 = 0x16; // PD->CP
const uint8_t SCS_17 = 0x17; // CP->PD
const uint8_t SCS_18 = 0x18; // PD->CP

const int AES_BLOCK_LEN = 16;
const int AES_MAX_BLOCKS = 90;

// Случайное число PD
static const uint8_t RND_B[8] = {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7};
// Debug version of Secutity Channel Base Key
static uint8_t SCBK_D[16] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F};
/*
// Arrays for cryptogramm generation
static uint8_t SENC_Template[16]  = {0x01, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t SMAC1_Template[16] = {0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t SMAC2_Template[16] = {0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// Placeholder for session keys - see osdp_secure_channel_t above
*/

/******************************************************************************* 
 *                     LOCAL FUNCTIONS
 *******************************************************************************/
static void encrypt_block(const uint8_t* key, const uint8_t* block, uint8_t* result)
{
aes_context ctx;

  aes_setkey_enc(&ctx, key, 128);       // Длина ключа в битах!
  aes_crypt_ecb(&ctx, AES_ENCRYPT, block, (unsigned char*)result);
}

//==============================================================================
static void calculate_cryptogram(const uint8_t* S_ENC, const uint8_t* first_RND, 
              const uint8_t* second_RND, uint8_t* result)
{
uint8_t RND[16];
aes_context ctx;
  
  memcpy(RND, (const void*)first_RND, 8);
  memcpy(&RND[8], (const void*)second_RND, 8);
  aes_setkey_enc(&ctx, S_ENC, 128);
  aes_crypt_ecb(&ctx, AES_ENCRYPT, RND, (unsigned char*)result);
}

// генерация ключа, используя RNDA
void calculate_enc(const uint8_t* key, const uint8_t* RNDA, uint8_t* result)
{
uint8_t input[16];
    
  input[0] = 0x01;
  input[1] = 0x82;
  input[2] = RNDA[0];
  input[3] = RNDA[1];
  input[4] = RNDA[2];
  input[5] = RNDA[3];
  input[6] = RNDA[4];
  input[7] = RNDA[5];
  memset(&input[8], 0x00, 8);

  encrypt_block(key, input, result);
}

// генерация SMAC1, используя RNDA
void calculate_smac1(const uint8_t* key, const uint8_t* RNDA, uint8_t* result)
{
uint8_t input[16];

  input[0] = 0x01;
  input[1] = 0x01;
  input[2] = RNDA[0];
  input[3] = RNDA[1];
  input[4] = RNDA[2];
  input[5] = RNDA[3];
  input[6] = RNDA[4];
  input[7] = RNDA[5];
  memset(&input[8], 0x00, 8);

  encrypt_block(key, input, result);
}

// генерация SMAC2, используя RNDA
void calculate_smac2(const uint8_t* key, const uint8_t* RNDA, uint8_t* result)
{
uint8_t input[ 16];

  input[0] = 0x01;
  input[1] = 0x02;
  input[2] = RNDA[0];
  input[3] = RNDA[1];
  input[4] = RNDA[2];
  input[5] = RNDA[3];
  input[6] = RNDA[4];
  input[7] = RNDA[5];
  memset(&input[8], 0x00, 8);

  encrypt_block(key, input, result);
}

// сторона клиента PD
void client_calculate_aes_rmaci(const uint8_t* cryptogram, const uint8_t* S_MAC1, const uint8_t* S_MAC2, uint8_t* result)
{
aes_context ctx;

  aes_setkey_enc(&ctx, S_MAC1, 128);
  aes_crypt_ecb(&ctx, AES_ENCRYPT, cryptogram, (unsigned char*)result);
  aes_setkey_enc(&ctx, S_MAC2, 128 );
  aes_crypt_ecb(&ctx, AES_ENCRYPT, (unsigned char*)result, (unsigned char*)result);
}

// SLEO - calculate c_mac
void client_calculate_aes_cmac(const uint8_t* cryptogram, const uint8_t* S_MAC1, const uint8_t* S_MAC2, uint8_t* result)
{
aes_context ctx;

  aes_setkey_enc(&ctx, S_MAC2, 128 );
  aes_crypt_ecb(&ctx, AES_ENCRYPT, cryptogram, (unsigned char*)result);
  aes_setkey_enc(&ctx, S_MAC1, 128);
  aes_crypt_ecb(&ctx, AES_ENCRYPT, (unsigned char*)result, (unsigned char*)result);
}

// Вычисление дополнения (получение инверсного МАС)
void calculate_one_complement(const uint8_t* source, uint8_t* destination)
{
  uint32_t* s = (uint32_t*)source;
  uint32_t* d = (uint32_t*)destination;
  *d = ~(*s);

  ++s;
  ++d;
  *d = ~(*s);

  ++s;
  ++d;
  *d = ~(*s);

  ++s;
  ++d;
  *d = ~(*s);
}

////////////////////////////////////////////////////////////////////////////////
// Основной рабочий механизм шифрования данных
void osdp_encrypt_aes(uint8_t *enc_key, uint8_t* ICV, uint8_t* data, 
                      uint16_t data_len, uint8_t* result)
{
aes_context ctx_skey_enc;
  
  // Инициализируем контекст
  aes_setkey_enc(&ctx_skey_enc, enc_key, 128);
  aes_crypt_cbc(&ctx_skey_enc, AES_ENCRYPT, data_len, ICV, data, result);
}

// основной рабочий механизм дешифрования
void osdp_decrypt_aes(uint8_t *dec_key, uint8_t* ICV, uint8_t* data, 
                      uint16_t data_len, uint8_t* result)
{
aes_context ctx_skey_dec;
  
  // Инициализируем контекст
  aes_setkey_dec(&ctx_skey_dec, dec_key, 128);
  aes_crypt_cbc(&ctx_skey_dec, AES_DECRYPT, data_len, ICV, data, result);
}

////////////////////////////////////////////////////////////////////////////////
//     Формирование МАС для полного сообщения OSDP (предваритель padded !!!)
////////////////////////////////////////////////////////////////////////////////
void osdp_calculate_mac(uint8_t *key_mac1, uint8_t *key_mac2, uint16_t length,
        uint8_t* icv, const uint8_t* input, uint8_t* output)
{
//предыдущий обработанный блок
uint8_t iv[16];
uint8_t curr[16];
int i;
aes_context ctx_mac1, ctx_mac2;

    // прихраниваем IV
    memcpy(iv, icv, 16);
    // Инициализируем контексты
    aes_setkey_enc(&ctx_mac1, key_mac1, 128);
    aes_setkey_enc(&ctx_mac2, key_mac2, 128);

    // шифровка, все блоки, кроме последнего шифруются первым ключём.
    // последний шифруется вторым ключом
    while (length > 16) {
        //printf("aes block to sign:\n");
        //hex_print(input, 16);
        // подмешивается IV
        for (i = 0; i < 16; i++)
            curr[i] = (unsigned char)(input[i] ^ iv[i]);
        // Криптуем
        aes_crypt_ecb(&ctx_mac1, AES_ENCRYPT, curr, curr);
        // обновляем IV
        memcpy(iv, curr, 16);
        input += 16;
        length -= 16;
    }

    // последний блок шифруем вторым ключом,  подмешивается IV
    for (i = 0; i < 16; i++)
        curr[i] = (unsigned char)(input[i] ^ iv[i]);
    // Криптуем последний блок
    aes_crypt_ecb(&ctx_mac2, AES_ENCRYPT, curr, curr);

    //  curr теперь содержит MAC сообщения
    memcpy(output, curr, 16);
}

//*************************************************************************
// Reset crypto seance, prepare replay with reason code
//*************************************************************************
void reset_sc(uint8_t* msg, uint8_t reason)
{
tOsdpSecurPacket* pkt;
  
  pkt = (tOsdpSecurPacket*)msg;
  // Reset autentification
  pkt->OsdpData.CmdReplay = osdp_NAK;
  pkt->OsdpData.SecBlkType = 0xFF;        // Error code
  pkt->OsdpData.Data[0] = reason;
  pkt->OsdpData.DLen = 11;
  SecurModeOn = 0;
  SecureReady = 0;
  SecurExchState = stPlain;
}

/*************************************************************************** 
 *                ФУНКЦИИ МОДУЛЯ ОБЩЕГО НАЗНАЧЕНИЯ
 ***************************************************************************/

void SecureChannelInit(uint8_t *scbk)
{
  SecurExchState = stPlain;
  memset(&SC, 0, sizeof(SC));
  // Fill pd_random
  memcpy(SC.pd_random, RND_B, sizeof(SC.pd_random));
  // Fill device ID
  memcpy(SC.pd_client_uid, &ReaderConfig.DeviceIdReport, 8);
  // Fill SCBK_D
  memcpy(SC.scbk, scbk, sizeof(SC.scbk));
}  


////////////////////////////////////////////////////////////////////////////////
//            Remove MAC and padding from secured message
//        msg length is with CRC (2 bytes) and MAC (4 bytes)
//  Used for any secured message - with data and not (pure command)
////////////////////////////////////////////////////////////////////////////////
void RemoveMacPadding(pOsdpSecurPacket msg)
{
  // Set data pointer before MAC and CRC
  msg->OsdpData.DLen -= 6;
  // NEED CHECK FOR 0x00 ON THE END OF PACKET ??????
  
  // Now remove padding (start from tail == 0*n and finish on 0x80 mark
  while (msg->RawData[--msg->OsdpData.DLen] == 0) {
  }
  // ... and remove 0x80
  if (msg->RawData[msg->OsdpData.DLen] == 0x80) {
    // msg->OsdpData.DLen--;
    msg->OsdpData.DLen += 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
//         Remove data padding from secured message data field
//        msg length on input is with CRC (2 bytes) and MAC (4 bytes)
//            Used for secured message with data field
//        Source message are without MAC and MAC padding
////////////////////////////////////////////////////////////////////////////////
void RemoveDataPadding(pOsdpSecurPacket msg)
{
  // Set data pointer
  msg->OsdpData.DLen -= 0;                    // ??????????
  // NEED CHECK FOR 0x00 ON THE END OF PACKET ??????
  
  // Now remove padding (start from tail == 0*n and finish on 0x80 mark
  while (msg->RawData[--msg->OsdpData.DLen] == 0) {
  }
  // ... and remove 0x80
  if (msg->RawData[msg->OsdpData.DLen] == 0x80) {
    //msg->OsdpData.DLen--;
    msg->OsdpData.DLen += 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
//   For pure command (without data) convert it to plain format 
//            (without 3 bytes of secured header)
//       Message is cleared from MAC and padding before call
////////////////////////////////////////////////////////////////////////////////
void ConvertCommandToPlain(pOsdpSecurPacket msg)
{
  // Перемещаем байты после криптозаголовка на новые позиции, 
  memmove(&msg->RawData[5],
          &msg->OsdpData.CmdReplay, 1);   // Pure command without data !!!
  // Длину сообщения корректируем
  msg->OsdpData.DLen -= 3;
  // CTRL byte - reset crypto flag for standard processing
  msg->OsdpData.Ctrl &= ~0x08;
}

////////////////////////////////////////////////////////////////////////////////
//      For command (with data) convert it to plain format 
//            (without 3 bytes of secured header)
//       Message is cleared from MAC and padding before call
////////////////////////////////////////////////////////////////////////////////
void ConvertCmdAndDataToPlain(pOsdpSecurPacket msg)
{
uint16_t move_len;
uint8_t iv[16];
uint8_t i;
  
  // Дешифруем поле данных
  memcpy(iv, SC.c_mac, sizeof(iv));                 /////////// c_mac?
  for (i = 0; i < 16; i++) {
    iv[i] = ~iv[i];
  }
  // Длина поля данных для дешифрации
  move_len = msg->OsdpData.DLen - 9;
  osdp_decrypt_aes(SC.s_enc, iv, msg->OsdpData.Data, move_len, msg->OsdpData.Data);
  // Удаляем из них padding
  RemoveDataPadding(msg);
  // Длина перемещаемой части пакета
  move_len = msg->OsdpData.DLen - 8;
  // Перемещаем байты после криптозаголовка на новые позиции, 
  memmove(&msg->RawData[5],
          &msg->OsdpData.CmdReplay, move_len);
  // Длину сообщения корректируем  на удаляемые SCS
  msg->OsdpData.DLen -= 3;
  // CTRL byte - reset crypto flag for standard processing
  msg->OsdpData.Ctrl &= ~0x08;
  // Финальная длина пакета (без CRC)
  msg->OsdpData.DLen += 0;
}

////////////////////////////////////////////////////////////////////////////////
// Function     :  PadBlock
// Input        :  Начало блока, исходная длина в блоке
// Output       :  padded - сколько добавлено байт
// Description  :  Дополняет блок данных до кратности 16 (для шифрования)
////////////////////////////////////////////////////////////////////////////////
void PadBlock(uint8_t *block_ptr, uint16_t src_len, uint16_t *padded)
{
  // Сначала ставим маркер 0х80
  block_ptr[src_len++] = 0x80;
  *padded += 1;
  while ((src_len % 16) != 0) {
    block_ptr[src_len++] = 0x00;
    *padded += 1;
  }
}
  
////////////////////////////////////////////////////////////////////////////////
// Function     :  EncodeAnswerPacket
// Input        :  RAW OSDP packet
// Output       :  Нет
// Description  :  Convert plain packet to secured one
////////////////////////////////////////////////////////////////////////////////
void EncodeAnswerPacket(uint8_t *msg)
{
tOsdpPacket* pkt_p;        // Plain version of packet
tOsdpSecurPacket* pkt_s;   // Secured version of packet
uint16_t lendata, pad_len;           // Есть ли данные?
uint8_t iv[16];
uint8_t i;
  
  // Две версии трактовки пакета
  pkt_p = (tOsdpPacket*)msg;
  pkt_s = (tOsdpSecurPacket*)msg;
  // На входе получили длину (с данными и CRC)
  // Сначала раздвигаем пакет и вставляем 3 байта SCS
  memmove(&pkt_s->OsdpData.CmdReplay, &pkt_p->OsdpData.CmdReplay, 
          pkt_p->OsdpData.DLen - 4);
  // Получаем длину блока данных (до вставки SCS)
  lendata = pkt_p->OsdpData.DLen - 8;
  // Вставляем байты SecBlk
  pkt_s->OsdpData.SecBlkData = 1;
  // В CTRL ставим флаг секьюрити
  pkt_s->OsdpData.Ctrl |= 0x08;
  pkt_s->OsdpData.SecBlkLen = 3;
  // Корректируем общую длину на величину вставки
  pkt_s->OsdpData.DLen += 3;
  if (lendata > 0) {
    pkt_s->OsdpData.SecBlkType = SCS_18;
    // Если надо - криптуем данные с падингом
    if ((lendata % 16) != 0) {
      // Дополняем до кратности 16 байтам
      pad_len = 0;
      PadBlock(pkt_s->OsdpData.Data, lendata, &pad_len);
      pkt_s->OsdpData.DLen += pad_len; 
      // Теперь криптуем данные. Длина данных
      lendata += pad_len;
      // Криптуем с ключом SC.s_enc и IV = inverted c_mac. 
      // Пробуем источник и результат использовать один
      memcpy(iv, SC.c_mac, sizeof(iv));
      for (i = 0; i < 15; i++) {
        iv[i] = ~iv[i];
      }
      osdp_encrypt_aes(SC.s_enc, iv, pkt_s->OsdpData.Data,
                      lendata, pkt_s->OsdpData.Data);
    }
  }
  else {
    // Данных нет - ставим SCS_16
    pkt_s->OsdpData.SecBlkType = SCS_16;
  }
  // Теперь добавляем MAC с падингом
  pad_len = 0;
  PadBlock(pkt_s->RawData, pkt_s->OsdpData.DLen - 2, &pad_len);
  pkt_s->OsdpData.DLen += pad_len;
  // Сразу добавляем длину МАС для его корректного вычисления
  pkt_s->OsdpData.DLen += 4;
  // Вычисляем МАС на базе c_mac от последней команды без инверсии
  memcpy(iv, SC.c_mac, sizeof(iv));
  osdp_calculate_mac(SC.s_mac1, SC.s_mac2,          // Two keys for MAC calculation
                    pkt_s->OsdpData.DLen - 6,       // Data length without CRC and MAC
                    iv,                             // IV for calculation
                    pkt_s->RawData,                 // Pointer to SOM
                    temp_data);                     // Result of calculation
  // Сохраняем полученный МАС как отправленный
  memcpy(SC.r_mac, temp_data, sizeof(SC.r_mac));
  // Добавляем МАС перед CRC
  memcpy(&pkt_s->RawData[pkt_s->OsdpData.DLen - 6], temp_data, 4);
  // ... CRC будет добавлена при отправке в UartSend(), длина уже учтена
}

////////////////////////////////////////////////////////////////////////////////
// Function     :  DecodeSecuredCommad
// Input        :  RAW OSDP packet
// Output       :  Нет
// Description  :  
////////////////////////////////////////////////////////////////////////////////
void DecodeSecuredCommand(uint8_t *msg)
{
//tOsdpMessage* pkt;
tOsdpSecurPacket* pkt_s;
uint16_t ptr;
uint8_t iv[16];
//
uint16_t dlen;
uint8_t cmd;  

  pkt_s = (tOsdpSecurPacket*)msg;
  switch (SecurExchState) {
    //------------------------------------
    // First autorization step
    case stPlain:
      // Can start to establish security channel
      if ((pkt_s->OsdpData.SecBlkType == SCS_11) && 
          (pkt_s->OsdpData.CmdReplay == osdp_CHLNG)) {
        // Clear for debug
        //memset(&pkt_s->OsdpPacket.OsdpData.Data, 0, sizeof(pkt_s->OsdpPacket.OsdpData.Data));
        // SC for new communication, depend from mode (debug/release
        SecureChannelInit((uint8_t*)SCBK_D);
        // Can process command - store server RND
        memcpy(SC.cp_random, &pkt_s->OsdpData.Data, sizeof(SC.cp_random));
        //--------------------------------------
        // Calculate s_enc[16], s_mac1[16], s_mac2[16]
        calculate_enc(SC.scbk, SC.cp_random, SC.s_enc);
        calculate_smac1(SC.scbk, SC.cp_random, SC.s_mac1);
        calculate_smac2(SC.scbk, SC.cp_random, SC.s_mac2);
        // Now calculate PD cryptogramm, based on SC.s_enc, SC.cp_random, SC.pd_random
        calculate_cryptogram(SC.s_enc, SC.cp_random, SC.pd_random, SC.pd_cryptogram);
        //--------------------------------------
        // Set replay code, block type, SEC_BLK_DATA stay as in request
        pkt_s->OsdpData.CmdReplay = osdp_CCRYPT;
        pkt_s->OsdpData.SecBlkType = SCS_12;
        // Set pointer to DATA start
        pkt_s->OsdpData.DLen = 11;
        // Now prepare osdp_CCRYPT - Client's ID, RND, Cryptogram
        ptr = 0;
        memcpy(&pkt_s->OsdpData.Data[ptr], SC.pd_client_uid, sizeof(SC.pd_client_uid));
        pkt_s->OsdpData.DLen += sizeof(SC.pd_client_uid);
        ptr += sizeof(SC.pd_client_uid);
        // Now copy RND to data
        memcpy(&pkt_s->OsdpData.Data[ptr], SC.pd_random, sizeof(SC.pd_random));
        pkt_s->OsdpData.DLen += sizeof(SC.pd_random);
        ptr += sizeof(SC.pd_random);
        // Add criptogramm - pd_cryptogram[16];
        memcpy(&pkt_s->OsdpData.Data[ptr], SC.pd_cryptogram, sizeof(SC.pd_cryptogram));
        pkt_s->OsdpData.DLen += sizeof(SC.pd_cryptogram);
        // Ready for next step
        SecurExchState = stChallenge;
      }
      else {
        // This PD does not support the security block that was received
        reset_sc(msg, osdp_err_SECUR_UNKNOWN);
      }
    break;
    
    //------------------------------------
    // Second autorization step
    case stChallenge:
      // Process server criptogramm
      if ((pkt_s->OsdpData.SecBlkType == SCS_13) && 
          (pkt_s->OsdpData.CmdReplay == osdp_SCRYPT)) {
        // Can process command - store server criptogramm
        memcpy(SC.cp_cryptogram, pkt_s->OsdpData.Data, sizeof(SC.cp_cryptogram));
        // Calculate and verify server criptogramm, use temp_data[]
        calculate_cryptogram(SC.s_enc, SC.pd_random, SC.cp_random, temp_data);
        if (memcmp(temp_data, SC.cp_cryptogram, sizeof(temp_data)) == 0) {
          // OK - Calculate osdp_RMAC_I
          client_calculate_aes_rmaci(SC.cp_cryptogram, SC.s_mac1, SC.s_mac2, SC.r_mac);
          // SLEO - c_mac calculated on pd_criptogramm base
          //client_calculate_aes_cmac(SC.pd_cryptogram, SC.s_mac1, SC.s_mac2, SC.c_mac);
          // Use c_mac like r_mac ???
          memcpy(SC.c_mac, SC.r_mac, sizeof(SC.c_mac));
          //--------------------------------------
          // Set replay code, block type, SEC_BLK_DATA stay as in request
          pkt_s->OsdpData.CmdReplay = osdp_RMAC_I;
          pkt_s->OsdpData.SecBlkType = SCS_14;
          // Set pointer to DATA start
          pkt_s->OsdpData.DLen = 11;
          // Now prepare osdp_RMAC_I - copy RMAC-I to data
          ptr = 0;
          memcpy(&pkt_s->OsdpData.Data[ptr], SC.r_mac, sizeof(SC.r_mac));
          pkt_s->OsdpData.DLen += sizeof(SC.r_mac);
          ptr += sizeof(SC.r_mac);
          // Set state stSCrypto
          SecurExchState = stCrypto;
          // We start secure mode ???
          SecureReady = 1;
        }
        else {
          reset_sc(msg, osdp_err_SECUR_COMM);
        }
      }
      else {
        reset_sc(msg, osdp_err_SECUR_COMM);
      }
    break;
    
    //------------------------------------
    // Work in security (encrypted) mode
    case stCrypto:
      // Recalculate MAC, received from CP (full packet, start from SOM)
      memcpy(iv, SC.c_mac, sizeof(iv));
      osdp_calculate_mac(SC.s_mac1, SC.s_mac2,          // Two keys for MAC calculation
                        pkt_s->OsdpData.DLen - 6,       // Data length without CRC and MAC
                        iv,                             // IV for calculation
                        pkt_s->RawData,                 // Pointer to SOM
                        temp_data);                     // Result of calculation
      // Compare received and calculated
      if (memcmp(temp_data, &pkt_s->RawData[pkt_s->OsdpData.DLen - 6], 4) == 0) {
        // If OK - store received MAC (use as ICV in next replay)
        memcpy(temp_c_mac, temp_data, sizeof(SC.c_mac));
        // Process command in security mode, SCS_15 and SCS_17 support,
        // use SCS_16 (no data) and SCS_18 with data field for answer
        switch (pkt_s->OsdpData.SecBlkType) {
          // Only command, used MAC, no encrypted data
          case SCS_15:
            // In any case (for SCS_15 and SCS_17) remove MAC and padding for MAC
            RemoveMacPadding(pkt_s);
            // Convert to no-encripted form and dispatch to standart hanler
            ConvertCommandToPlain(pkt_s);
            // Now we can store c_mac for next exchange
            memcpy(SC.c_mac, temp_c_mac, sizeof(SC.c_mac));
            // Здесь длина данных БЕЗ CRC
            dlen = OsdpMessage.OsdpPacket.OsdpData.DLen;
            // Store command code
            cmd = OsdpMessage.OsdpPacket.OsdpData.CmdReplay;
            // Это пока что для пустого пакета c CRC
            OsdpMessage.OsdpPacket.OsdpData.DLen = 8;
            // Пока что ASK
            OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_ACK;
            // Для ответа к адресу добавили 0x80
            OsdpMessage.OsdpPacket.OsdpData.Addr |= 0x80;
            // Below in DecodePlainCommad we store new params for osdp_COMSET (ver. 4.4)
            // NewCommSpeed, NewDevAddress. It will reset after store in ReaderConfig
            if (cmd != osdp_MFG)
              DecodePlainCommad(cmd, dlen - 6);
            else {
              // MFG command has data ! - Use SCS_17 for such command
              reset_sc(msg, osdp_err_SECUR_COMM);
            }
          break;
          // Command, used MAC and encrypted data
          case SCS_17:
            // In any case (for SCS_15 and SCS_17) remove MAC and padding for MAC
            RemoveMacPadding(pkt_s);
            // Decrypt packet with data, convert to no-encripted form and
            // dispatch to standart hanler. MAC removed before
            ConvertCmdAndDataToPlain(pkt_s);
            // After conversion prepare ASK answer
            // Now we can store c_mac for next exchange
            memcpy(SC.c_mac, temp_c_mac, sizeof(SC.c_mac));
            // Здесь длина данных БЕЗ CRC
            dlen = OsdpMessage.OsdpPacket.OsdpData.DLen;
            // Store command code
            cmd = OsdpMessage.OsdpPacket.OsdpData.CmdReplay;
            // Это пока что для пустого пакета c CRC
            OsdpMessage.OsdpPacket.OsdpData.DLen = 8;
            // Пока что ASK
            OsdpMessage.OsdpPacket.OsdpData.CmdReplay = osdp_ACK;
            // Для ответа к адресу добавили 0x80
            OsdpMessage.OsdpPacket.OsdpData.Addr |= 0x80;
            if (cmd == osdp_MFG) {
              // Manufacturer command, optional with data
              DecodePlainCommad(cmd, dlen - 10);
            }
            else {
              // Standard command with data field
              DecodePlainCommad(cmd, dlen - 6);
            }
          break;
          // ERROR - reset SC
          default:
            // ERROR - prepare error answer and reset SC
            reset_sc(msg, osdp_err_SECUR_COMM);
          break;
        }
      }
      else {
        // ERROR - prepare error answer and reset SC
        reset_sc(msg, osdp_err_SECUR_COMM);
      }
    break;
    
    default:
      reset_sc(msg, osdp_err_SECUR_COMM);
    break;
  }
}

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
