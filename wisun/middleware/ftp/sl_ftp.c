/***************************************************************************//**
 * @file sl_ftp.c
 * @brief File Transfer Protocol
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include "sl_ftp.h"
#include "sl_ftp_config.h"
#include "arpa/inet.h"
#include "sl_wisun_api.h"
#include "sl_memory_manager.h"
#include "sl_string.h"

#if SL_FTP_ENABLE_FTP_PROTOCOL
#include "sl_ftp_clnt.h"
#endif

#if SL_FTP_ENABLE_TFTP_PROTOCOL
#include "sl_tftp_clnt.h"
#endif
// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
/// Print line length
#define SL_FTP_DATA_PRINT_LINE_LEN    (20UL)

#define IPV6_STR_BUF_SIZE               (40U)
// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
void sl_ftp_init(void)
{
#if SL_FTP_ENABLE_FTP_PROTOCOL
  sl_ftp_clnt_service_init();
#endif
#if SL_FTP_ENABLE_TFTP_PROTOCOL
  sl_tftp_clnt_service_init();
#endif
}

void sl_ftp_dump_buff(const uint8_t * const data_ptr, const uint16_t data_size)
{
  size_t byte_cnt = 0UL;
  for (size_t i = 0; i < data_size; ++i) {
    if (i == (data_size - 1U)) {
      printf("0x%.2x\n", data_ptr[i]);
    } else if (byte_cnt < SL_FTP_DATA_PRINT_LINE_LEN - 1U) {
      printf("0x%.2x, ", data_ptr[i]);
    } else {
      printf("0x%.2x,\n", data_ptr[i]);
    }

    if (byte_cnt == SL_FTP_DATA_PRINT_LINE_LEN - 1U) {
      byte_cnt = 0U;
    } else {
      ++byte_cnt;
    }
  }
  printf("\n");
}

const char * sl_ftp_get_global_addr(void)
{
  static char str[IPV6_STR_BUF_SIZE] = { 0U };
  in6_addr_t addr = { 0U };

  if (sl_wisun_get_ip_address(SL_WISUN_IP_ADDRESS_TYPE_GLOBAL, &addr) != SL_STATUS_OK) {
    sl_strcpy_s(str, IPV6_STR_BUF_SIZE, SL_FTP_LOCAL_HOST_STR);
    return (const char *) str;
  }

  if (inet_ntop(AF_INET6, (const void *)&addr, str, IPV6_STR_BUF_SIZE) == NULL) {
    sl_strcpy_s(str, IPV6_STR_BUF_SIZE, SL_FTP_LOCAL_HOST_STR);
  }

  return (const char *) str;
}

#if SL_FTP_ENABLE_TFTP_PROTOCOL

void * sl_tftp_udp_get_addr(const char *host,
                            uint16_t port)
{
  sockaddr_in6_t *waddr = NULL;

  waddr = (sockaddr_in6_t *)sl_malloc(sizeof(sockaddr_in6_t));
  if (waddr == NULL) {
    return NULL;
  }

  if (inet_pton(AF_INET6, host, &waddr->sin6_addr) != 1) {
    sl_free(waddr);
    return NULL;
  }

  waddr->sin6_family = AF_INET6;
  waddr->sin6_port = htons(port);

  return (void *)waddr;
}

#endif // SL_FTP_ENABLE_TFTP_PROTOCOL
