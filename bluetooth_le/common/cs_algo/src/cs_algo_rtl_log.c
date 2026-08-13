/***************************************************************************//**
 * @file
 * @brief CS Algo - RTL library logging
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "cs_algo_rtl_log.h"
#include "cs_algo_config.h"
#include "sl_component_catalog.h"

#if CS_ALGO_CONFIG_RTL_LOG && defined(SL_CATALOG_BGAPI_TRACE_PRESENT)
#include <stdio.h>
#include "sl_rtl_clib_log_api.h"
#include "sl_bt_version.h"
#include "sli_bgapi_trace.h"
#include "cs_algo_log_internal.h"

static void cs_algo_rtl_log_callback(uint8_t *log_data, size_t log_data_len);

void cs_algo_rtl_log_init(void)
{
  int ret;
  enum sl_rtl_error_code ec;
  const char hash[] = SL_BT_VERSION_HASH;
  sl_rtl_log_params config_params = {
    .log_callback_function = cs_algo_rtl_log_callback,
    .sdk_version = { 0 },
    .command_line_options = { 0 }
  };

  ret = snprintf(config_params.sdk_version,
                 SL_RTL_LOG_SDK_VERSION_CHAR_ARRAY_MAX_SIZE,
                 "%u.%u.%u-%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                 SL_BT_VERSION_MAJOR,
                 SL_BT_VERSION_MINOR,
                 SL_BT_VERSION_PATCH,
                 hash[0], hash[1], hash[2], hash[3], hash[4],
                 hash[5], hash[6], hash[7], hash[8], hash[9],
                 hash[10], hash[11], hash[12], hash[13], hash[14],
                 hash[15], hash[16], hash[17], hash[18], hash[19]);
  if (ret <= 0 || ret >= SL_RTL_LOG_SDK_VERSION_CHAR_ARRAY_MAX_SIZE) {
    algo_log_error("RTL log - failed to construct version string!" LOG_NL);
    return;
  }

  ec = sl_rtl_log_init();
  if (ec != SL_RTL_ERROR_SUCCESS) {
    algo_log_error("RTL log - init failed! [E: 0x%x]" LOG_NL, ec);
    return;
  }

  ec = sl_rtl_log_configure(&config_params);
  if (ec != SL_RTL_ERROR_SUCCESS) {
    algo_log_error("RTL log - configure failed! [E: 0x%x]" LOG_NL, ec);
  }
}

void cs_algo_rtl_log_deinit(void)
{
  enum sl_rtl_error_code ec = sl_rtl_log_deinit();
  if (ec != SL_RTL_ERROR_SUCCESS) {
    algo_log_error("RTL log - deinit failed! [E: 0x%x]" LOG_NL, ec);
  }
}

/******************************************************************************
 * RTL library log output sink. Forwards each log chunk to the BGAPI trace
 * channel. The RTL library serializes callback invocations via the
 * rtl_log_mutex component, making this RTOS-safe.
 *****************************************************************************/
static void cs_algo_rtl_log_callback(uint8_t *log_data, size_t log_data_len)
{
  while (log_data_len > 0) {
    size_t log_written = sli_bgapi_trace_log_custom_message(log_data, log_data_len);
    log_data_len -= log_written;
    log_data += log_written;
  }
}

#else // CS_ALGO_CONFIG_RTL_LOG && SL_CATALOG_BGAPI_TRACE_PRESENT

void cs_algo_rtl_log_init(void)
{
}

void cs_algo_rtl_log_deinit(void)
{
}

#endif // CS_ALGO_CONFIG_RTL_LOG && SL_CATALOG_BGAPI_TRACE_PRESENT
