/*******************************************************************************
 *
 *  ÃŒƒ”À‹        : OsdpCommans.h
 *
 *  ¿‚ÚÓ         : À.—Ú‡ÒÂÌÍÓ
 *  ƒ‡Ú‡ Ì‡˜‡Î‡   : 05.12.2014
 *  ¬ÂÒËˇ        : 1.1
 *   ÓÏÏÂÌÚ‡ËË    :  ŒÃ¿Õƒ€ œŒ —“¿Õƒ¿–“” OSDP
 *
 ******************************************************************************/

#ifndef OSDP_COMMANS_DEF
#define OSDP_COMMANS_DEF

//              ŒÃ¿Õƒ€ œŒ —“¿Õƒ¿–“” OSDP VER.2.1.6 (2014)
//      Name              Value      Meaning                          Data
#define osdp_POLL         0x60    // Poll                            None
#define osdp_ID           0x61    // ID Report Request               Id type
#define osdp_CAP          0x62    // PD Capabilities Request         Reply type
#define osdp_DIAG         0x63    // Diagnostic Function Command     Request code
#define osdp_LSTAT        0x64    // Local Status Report Request     None
#define osdp_ISTAT        0x65    // Input Status Report Request     None
#define osdp_OSTAT        0x66    // Output Status Report Request    None
#define osdp_RSTAT        0x67    // Reader Status Report Request    None
#define osdp_OUT          0x68    // Output Control Command          Output settings
#define osdp_LED          0x69    // Reader Led Control Command      LED settings
#define osdp_BUZ          0x6A    // Reader Buzzer Control Command   Buzzer settings
#define osdp_TEXT         0x6B    // Text Output Command             Text settings
#define osdp_RMODE        0x6C    // Ö removed Ö
#define osdp_TDSET        0x6D    // Time and Date Command           Time and Date
#define osdp_COMSET       0x6E    // PD Communication Configuration  Com settings
#define osdp_DATA         0x6F    // Data Transfer Command           Raw Data
#define osdp_XMIT         0x70    // Ö removed Ö
#define osdp_PROMPT       0x71    // Set Automatic Prompt Strings    Message string
#define osdp_SPE          0x72    // Ö removed Ö
#define osdp_BIOREAD      0x73    // Scan and Send Biometric Data    Requested Return Format
#define osdp_BIOMATCH     0x74    // Scan and Match Bio Template     Biometric Template
#define osdp_KEYSET       0x75    // Encryption Key Set Command      Encryption Key
#define osdp_CHLNG        0x76    // Challenge and Secure Init Rq.   Challenge Data
#define osdp_SCRYPT       0x77    // Server Cryptogram               Encryption Data
#define osdp_CONT         0x79    // Ö removed Ö
#define osdp_MFG          0x80    // Manufacturer Specific Command   Any
#define osdp_SCDONE       0xA0    // Ö removed Ö
#define osdp_XWR          0xA1    // See appendix                    Defined in Appendix E
//                  –¿—ÿ»–≈Õ»≈  ŒÃ¿Õƒ ƒÀﬂ osdp_MFG = 0x80
#define mfg_SETWGFMT      0x20    // Set wiegand format (non volatile memory)
#define mfg_GETWGFMT      0x21    // Get wiegand format (non volatile memory)
#define mfg_SETMIFSEC     0x22    // Set sector and key A for security Mifare mode
#define mfg_GETCARDP      0x23    // Read cards parameters
#define mfg_SETCARDP      0x24    // Write cards parameters
#define mfg_SETPINMOD     0x25    // Write keypad mode (card, PIN, card+PIN)
#define mfg_GETPINMOD     0x26    // Read  keypad mode 
#define mfg_CHGPINMOD     0x27    // Change keypad mode, not stored in non-volatile
#define mfg_CHGCOMSPEED   0x28    // Change comm speed, no stored in non-volatile
// Added 10.08.2021 for cancel wait PIN
#define mfg_CANCPINMOD    0x29    // Cancel keypad mode (cancel wait time)
// Added 17.02.2022
#define mfg_GETCMNCONFIG  0x2A    // Read common configuration without Mif sector and key
#define mfg_GETEXTCONFIG  0x2B    // Read extended configuration
#define mfg_SETCMNCONFIG  0x2C    // Set device configuration (if small, OMP-xxxx)

// ¡˚ÒÚÓÂ ÛÔ‡‚ÎÂÌËÂ LED
#define mfg_SETLEDINDIC   0x30    // Indication LED set color or blink
#define mfg_SETLEDBACKLT  0x31    // Backlight LED set color or blink
#define mfg_SETDEFCOLORS  0x32    // Set default active/inactive colors for LED's
// Version 3.6
#define mfg_SETIDREPORT   0x33    // Set ID report for serial update in production
//
#define mfg_SETZONECONFIG 0x34    // Set guard zone configuration (for extender)
// For QR readers (progam illumination and target-lighter_
#define mfg_QR_CONFIG_CMD 0x35    // Program QR scanner via UART in reader
// Set HMAC-SHA mode (SHA-1 or SHA-256)
#define mfg_SET_SHA_MODE  0x36    // HMAC-SHA mode
//
#define mfg_CMDTEST       0x3F    // Command for any tests
#define mfg_RES_TO_FACT   0x40    // Reset to factory default settings
// Command for Parsec reader indication control
#define mfg_PRS_IND_CMD   0x41    // For translate Parsec indication control command
#define mfg_OMP_SET_CONFIG 0x42 // St OMP configuration from screen
#define mfg_OMP_GET_CONFIG 0x43 // St OMP configuration from screen
// Manufacturer command for AES key set
#define mfg_KEY_SET       0x44
// Program where to find ID in MFP or MFC cards
#define mfg_ID_LOCATE_CFG 0x45

//      Œ“¬≈“€ œ≈–»‘≈–»» œŒ —“¿Õƒ¿–“” OSDP VER.2.1.6 (2014)
//      Name            Value      Meaning                          Data
#define osdp_ACK          0x40    // Command accepted                None
#define osdp_NAK          0x41    // Command not processed           Reason for rejecting
#define osdp_PDID         0x45    // PD ID Report                    Report data
#define osdp_PDCAP        0x46    // PD Capabilities Report          Report data
#define osdp_LSTATR       0x48    // Local Status Report             Report data
#define osdp_ISTATR       0x49    // Input Status Report             Report data
#define osdp_OSTATR       0x4A    // Output Status Report            Report data
#define osdp_RSTATR       0x4B    // Reader Status Report            Report data
#define osdp_RAW          0x50    // Raw bit image of card data      Card data
#define osdp_FMT          0x51    // Formatted character stream      Card data
#define osdp_PRES         0x52    // Ö removed Ö
#define osdp_KEYPPAD      0x53    // Keypad Data                     Keypad data
#define osdp_COM          0x54    // PD Communications Report        Comm data
#define osdp_SCREP        0x55    // Ö removed Ö
#define osdp_SPER         0x56    // Ö removed Ö
#define osdp_BIOREADR     0x57    // Biometric Data                  Biometric data
#define osdp_BIOMATCHR    0x58    // Biometric Match Result          Result
#define osdp_CCRYPT       0x76    // Client's ID, RND, Cryptogram    Encryption Data
#define osdp_RMAC_I       0x78    // Initial R-MAC                   Encryption Data
#define osdp_MFGREP       0x90    // Manufacturer Specific Reply     Any
#define osdp_BUSY         0x79    // PD is Busy reply
#define osdp_XRD          0xB1    // See appendix                    Defined in Appendix E
//                  –¿—ÿ»–≈Õ»≈ Œ“¬≈“Œ¬ ƒÀﬂ osdp_MFGREP
//#define mfg_WGFMTWR       0x20    // Wiegand format - write       - ‚ÏÂÒÚÓ ÌÂ„Ó osdp_ACK
#define mfg_WGFMTRD       0x21    // Wiegand format report - read
//#define mfg_MIFSECWR      0x22    // Set sector and key           - ‚ÏÂÒÚÓ ÌÂ„Ó osdp_ACK
#define mfg_CARDPRD       0x23    // Cards parameters report
//#define mfg_CARDPWR       0x24    // Write cards parameters       - ‚ÏÂÒÚÓ ÌÂ„Ó osdp_ACK
#define mfg_PINMOD        0x26    // Read  keypad mode (card, PIN, card+PIN)


//       Œƒ€ Œÿ»¡Œ  ƒÀﬂ Œ“¬≈“¿ osdp_NAK (ÔÂ‚˚È ·‡ÈÚ ‰‡ÌÌ˚ı)
//        Name                  Value      Meaning
#define osdp_err_CECK_CHAR      0x01  // Message check character(s) error (bad cksum/crc)
#define osdp_err_CMD_LENGTH     0x02  // Command length error
#define osdp_err_CMD_UNKNOWN    0x03  // Unknown Command Code ñ Command not implemented by PD
#define osdp_err_SEQ_NUMBER     0x04  // Unexpected sequence number detected in the header
#define osdp_err_SECUR_UNKNOWN  0x05  // This PD does not support the security block that was received
#define osdp_err_SECUR_COMM     0x06  // Communication security conditions not met
#define osdp_err_BIO_TYPE       0x07  // BIO_TYPE not supported
#define osdp_err_BIO_FORMAT     0x08  // BIO_FORMAT not supported
#define osdp_err_UNABLE_PROCESS 0x09  // Unable to process command record
#endif

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
