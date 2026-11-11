/***************************************************************************//**
 * @file
 * @brief app_cli.c
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
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include "sl_rail.h"
#include "sl_component_catalog.h"
#include "app_log.h"
#include "sl_cli.h"
#include "app_process.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

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
/******************************************************************************
 * CLI - ieee802154_configure: Configure IEEE 802.15.4 addressing, frame and ACK
 *****************************************************************************/
void cli_ieee802154_configure(sl_cli_command_arg_t *arguments)
{
  uint16_t length = sl_rail_set_fixed_length(rail_handle, SL_RAIL_SET_FIXED_LENGTH_INVALID);
  if ((length != 0) && (length != 1)) {
    app_log_error("ERROR: IEEE 802.15.4 features cannot be used with fixed-length proprietary PHYs\n");
    return;
  }

  ieee802154_settings.enabled = true;
  ieee802154_settings.pan_id = sl_cli_get_argument_uint16(arguments, 0);
  ieee802154_settings.rx_address = sl_cli_get_argument_uint16(arguments, 1);
  ieee802154_settings.tx_address = sl_cli_get_argument_uint16(arguments, 2);
  ieee802154_settings.frame_length = sl_cli_get_argument_uint16(arguments, 3);
  ieee802154_settings.source_addressing = sl_cli_get_argument_uint8(arguments, 4);
  ieee802154_settings.pan_id_compression = sl_cli_get_argument_uint8(arguments, 5);
  ieee802154_settings.ack_timeout_us = sl_cli_get_argument_uint16(arguments, 6);
  ieee802154_settings.ack_enabled = (ieee802154_settings.ack_timeout_us != 0U);

  uint16_t min_frame_length = (uint16_t)(get_ieee802154_mhr_length() + 4U); // (max CRC length: 4)
  uint16_t max_frame_length = (uint16_t)(get_ieee802154_max_frame_length() 
                                          - get_ieee802154_mhr_length() 
                                          - 4U); // (max CRC length: 4)

  if ((ieee802154_settings.frame_length < min_frame_length)
      || (ieee802154_settings.frame_length > max_frame_length)) {
    app_log_error("IEEE 802.15.4 frame length must be %" PRIu16
                  "..%" PRIu16 " bytes for the selected addressing mode\n",
                  min_frame_length,
                  max_frame_length);
    ieee802154_settings.enabled = false;
    memset(&ieee802154_settings, 0, sizeof(ieee802154_settings_t));
    return;
  }
  app_log_info("IEEE 802.15.4 configured\n");
  app_log_info("PanId: 0x%04" PRIX16 "\n"
                "TxAddress: 0x%04" PRIX16 "\n"
                "RxAddress: 0x%04" PRIX16 "\n"
                "FrameLength: %" PRIu16 "\n"
                "SourceAddressing: %" PRIu8 "\n"
                "PanIdCompression: %" PRIu8 "\n"
                "AckEnabled: %s\n"
                "AckTimeout: %" PRIu16 "\n",
                ieee802154_settings.pan_id,
                ieee802154_settings.tx_address,
                ieee802154_settings.rx_address,
                ieee802154_settings.frame_length,
                ieee802154_settings.source_addressing,
                ieee802154_settings.pan_id_compression,
                ieee802154_settings.ack_enabled ? "true": "false",
                ieee802154_settings.ack_timeout_us);

}

/******************************************************************************
 * CLI - csma_configure: Configure CSMA/CA parameters
 *****************************************************************************/
void cli_csma_configure(sl_cli_command_arg_t *arguments)
{
  uint8_t csma_min_bo_exp = sl_cli_get_argument_uint8(arguments, 0);
  uint8_t csma_max_bo_exp = sl_cli_get_argument_uint8(arguments, 1);
  uint8_t csma_tries = sl_cli_get_argument_uint8(arguments, 2);
  int8_t cca_threshold_dbm = sl_cli_get_argument_int8(arguments, 3);
  uint16_t cca_backoff_us = sl_cli_get_argument_uint16(arguments, 4);
  uint16_t cca_duration_us = sl_cli_get_argument_uint16(arguments, 5);
  uint32_t csma_timeout_us = sl_cli_get_argument_uint32(arguments, 6);

  if (csma_min_bo_exp > SL_RAIL_MAX_CSMA_EXPONENT) {
    app_log_error("csma_min_bo_exp must be 0..%" PRIu8 "\n",
                  (uint8_t)SL_RAIL_MAX_CSMA_EXPONENT);
    return;
  }

  if (csma_max_bo_exp > SL_RAIL_MAX_CSMA_EXPONENT) {
    app_log_error("csma_max_bo_exp must be 0..%" PRIu8 "\n",
                  (uint8_t)SL_RAIL_MAX_CSMA_EXPONENT);
    return;
  }

  if (csma_max_bo_exp < csma_min_bo_exp) {
    app_log_error("csma_max_bo_exp must be >= csma_min_bo_exp\n");
    return;
  }

  // Fixed backoff only when min==0; otherwise RAIL returns INVALID_PARAMETER
  if ((csma_min_bo_exp == 0U) && (csma_max_bo_exp != 0U)) {
    app_log_error("csma_max_bo_exp must be 0 when csma_min_bo_exp is 0 "
                  "(sets a fixed backoff)\n");
    return;
  }

  if (csma_tries > SL_RAIL_MAX_LBT_TRIES) {
    app_log_error("csma_tries must be 0..%" PRIu8 "\n",
                  (uint8_t)SL_RAIL_MAX_LBT_TRIES);
    return;
  }

  if (cca_duration_us == 0U) {
    app_log_error("cca_duration_us must be non-zero\n");
    return;
  }

  csma_settings.enabled = true;
  csma_settings.config.csma_min_bo_exp = csma_min_bo_exp;
  csma_settings.config.csma_max_bo_exp = csma_max_bo_exp;
  csma_settings.config.csma_tries = csma_tries;
  csma_settings.config.cca_threshold_dbm = cca_threshold_dbm;
  csma_settings.config.cca_backoff_us = cca_backoff_us;
  csma_settings.config.cca_duration_us = cca_duration_us;
  csma_settings.config.csma_timeout_us = csma_timeout_us;

  app_log_info("MinBo:%" PRIu8 ","
                "MaxBo:%" PRIu8 ","
                "Tries:%" PRIu8 ","
                "Threshold:%" PRId8 ","
                "Backoff:%" PRIu16 ","
                "Duration:%" PRIu16 ","
                "Timeout:%" PRIu32 "\n",
                csma_settings.config.csma_min_bo_exp,
                csma_settings.config.csma_max_bo_exp,
                csma_settings.config.csma_tries,
                csma_settings.config.cca_threshold_dbm,
                csma_settings.config.cca_backoff_us,
                csma_settings.config.cca_duration_us,
                csma_settings.config.csma_timeout_us);
}

/******************************************************************************
 * CLI - start_measurement: Start the throughput measurement
 *****************************************************************************/
void cli_start_measurement(sl_cli_command_arg_t *arguments)
{
  uint16_t requested_channel = sl_cli_get_argument_uint16(arguments, 0);

  if (sl_rail_is_valid_channel(rail_handle, requested_channel) != SL_RAIL_STATUS_NO_ERROR) {
    app_log_error("Invalid channel: %" PRIu16 "\n", requested_channel);
    return;
  }

  channel_measurement = requested_channel;
  app_role = APP_ROLE_TRANSMITTER;
  transmitter_state = TRANSMITTER_STATE_SYNC;
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
