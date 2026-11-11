/***************************************************************************//**
 * @file
 * @brief app_init.c
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
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
#include <inttypes.h>
#include <stdint.h>
#include "sl_rail.h"
#include "sl_rail_util_init.h"
#include "sl_component_catalog.h"
#include "rail_config.h"
#include "sl_clock_manager_oscillator_config.h"
#include "sl_rail_sdk_packet_assistant.h"
#include "app_log.h"
#include "app_process.h"
#include "app_assert.h"
#include "app_config.h"
#include "sl_memory_manager.h"

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "app_task_init.h"
#endif

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
/// Number of RAIL channels to scan at init (0 .. CHANNEL_SCAN_COUNT-1)
#define CHANNEL_SCAN_COUNT 1000U

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
/******************************************************************************
 * Checks that HFXO and radio config crystal frequencies match when both are present.
 *****************************************************************************/
static void xtal_frequency_validation_check(void);

/******************************************************************************
 * Logs a contiguous valid channel range (or a single channel).
 *****************************************************************************/
static void log_valid_channel_range(uint16_t range_start, uint16_t range_end);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
/******************************************************************************
 * The function is used for some basic initialization related to the app.
 *****************************************************************************/
void rail_app_init(void)
{
  app_assert(THROUGHPUT_MEASUREMENT_PERIOD_SEC <= 30,
             "Measurement period (%u s) must be at most 30 s\n",
             (unsigned)THROUGHPUT_MEASUREMENT_PERIOD_SEC);

  xtal_frequency_validation_check();

  app_log_info("\n");
  app_log_info("Throughput Measurement App\n");
  
  rail_handle = sl_rail_util_get_handle(SL_RAIL_UTIL_HANDLE_INST0);
  
  if ((channelConfigs[0] == NULL)
      || (channelConfigs[0]->configs == NULL)
      || (channelConfigs[0]->configs[0].stackInfo == NULL)) {
    app_assert(0, "Incompatible PHY\n");
  }

  bool valid_channel_found = false;
  uint16_t range_start = 0U;
  bool in_range = false;

  for (uint16_t ch = 0; ch < CHANNEL_SCAN_COUNT; ++ch) {
    bool valid = (sl_rail_is_valid_channel(rail_handle, ch) == SL_RAIL_STATUS_NO_ERROR);
    if (valid) {
      if (!valid_channel_found) {
        channel_commands = ch;
        valid_channel_found = true;
      }
      if (!in_range) {
        range_start = ch;
        in_range = true;
      }
    } else if (in_range) {
      log_valid_channel_range(range_start, (uint16_t)(ch - 1U));
      in_range = false;
    }
  }
  if (in_range) {
    log_valid_channel_range(range_start, (uint16_t)(CHANNEL_SCAN_COUNT - 1U));
  }
  
  app_assert(valid_channel_found, "No valid channels found for PHY\n");

  sl_packet_assistant_select_phy(0);

  uint16_t cmd_payload_length = get_cmd_payload_length();
  app_assert(cmd_payload_length >= CMD_PACKET_LENGTH,
            "Frame length (%" PRIu16 " bytes) has to be at least %" PRIu16 " bytes\n",
            cmd_payload_length,
            (uint16_t)CMD_PACKET_LENGTH);

  rx_cmd_frame_buffer = (uint8_t *)sl_calloc(cmd_payload_length + sl_packet_assistant_get_phr_length(), sizeof(uint8_t));
  tx_cmd_buffer = (uint8_t *)sl_calloc(cmd_payload_length, sizeof(uint8_t));

  sl_rail_status_t status = sl_rail_start_rx(rail_handle, channel_commands, NULL);
  if (status != SL_RAIL_STATUS_NO_ERROR) {
    app_log_warning("sl_rail_start_rx() result: 0x%08" PRIX32 "\n", status);
  }
}

void app_init(void)
{
#if !defined(SL_CATALOG_KERNEL_PRESENT)
  rail_app_init();
#else
  app_task_init();
#endif
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
/******************************************************************************
 * Checks that HFXO and radio config crystal frequencies match when both are present.
 *****************************************************************************/
static void xtal_frequency_validation_check(void)
{
#if defined(SL_CLOCK_MANAGER_HFXO_FREQ) && defined(RADIO_CONFIG_XTAL_FREQUENCY)
  _Static_assert(SL_CLOCK_MANAGER_HFXO_FREQ == RADIO_CONFIG_XTAL_FREQUENCY,
                 "SL_CLOCK_MANAGER_HFXO_FREQ must match RADIO_CONFIG_XTAL_FREQUENCY.");
#endif
}

/******************************************************************************
 * Logs a contiguous valid channel range (or a single channel).
 *****************************************************************************/
static void log_valid_channel_range(uint16_t range_start, uint16_t range_end)
{
  if (range_start == range_end) {
    app_log_info("Valid channel: %" PRIu16 "\n", range_start);
  } else {
    app_log_info("Valid channel range: %" PRIu16 "-%" PRIu16 "\n",
                 range_start, range_end);
  }
}
