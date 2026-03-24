#ifndef BL_CONFIG_H
#define BL_CONFIG_H

#include <stdint.h>

#define BL_BASE_ADDR         ((uint32_t)0x80000000u)
#define BL_SIZE_BYTES        ((uint32_t)0x00002000u)

#define APP_BASE_ADDR        ((uint32_t)0x80002000u)
#define APP_MAX_SIZE_BYTES   ((uint32_t)0x000FE000u)
#define APP_END_ADDR         (APP_BASE_ADDR + APP_MAX_SIZE_BYTES)

#define REPLY_WAITING              ((uint8_t)1u)
#define REPLY_ACK                  ((uint8_t)2u)
#define ERR_SIZE                   ((uint8_t)3u)
#define ERR_RECEIVE                ((uint8_t)4u)
#define ERR_CRC32                  ((uint8_t)5u)
#define ERR_WAIT_WRITE_PAGE        ((uint8_t)6u)
#define ERR_WAIT_ERASE_PAGE        ((uint8_t)7u)

#define BL_UPDATE_WAIT_TIMEOUT_MS           ((uint32_t)500u)

#define APP_HEADER_ADDR                     (APP_BASE_ADDR)
#define APP_PAYLOAD_ADDR                    (APP_HEADER_ADDR + 8u)
#define APP_PAYLOAD_MAX_SIZE_BYTES          (APP_MAX_SIZE_BYTES - 8u)
#define APP_ENTRY_ADDR                      (APP_PAYLOAD_ADDR)

#endif /* BL_CONFIG_H */
