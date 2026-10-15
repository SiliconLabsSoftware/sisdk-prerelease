/***************************************************************************//**
 * @file sl_rail_sdk_packet_assistant_cli.c
 * @brief RAIL SDK - RAIL Packet Assistant Component
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "sl_rail_sdk_packet_assistant.h"
#include "sl_cli.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
static void warning_deprecated_cli_command(const char *deprecated_command,
                                           const char *replacement_command);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
static void warning_deprecated_cli_command(const char *deprecated_command,
                                           const char *replacement_command)
{
#if defined(SL_CATALOG_APP_LOG_PRESENT) \
  && !defined(SL_SUPPRESS_DEPRECATION_WARNINGS_SDK_2026_12)
  app_log_warning("Command '%s' is deprecated; use '%s' instead.\n",
                  deprecated_command,
                  replacement_command);
#else
  (void) deprecated_command;
  (void) replacement_command;
#endif
}

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
/******************************************************************************
 * CLI - get_print_packet_info message: Get the print setting
 *****************************************************************************/
void cli_get_print_packet_info(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("Print setting is %s\n", sl_packet_assistant_get_print_packet_info() > 0 ? "ON" : "OFF");
#endif
}

/******************************************************************************
 * CLI - set_print_packet_info message: Set the print setting
 *****************************************************************************/
void cli_set_print_packet_info(sl_cli_command_arg_t *arguments)
{
  uint8_t new_print_settings = sl_cli_get_argument_uint8(arguments, 0);
  if (new_print_settings > 0) {
    sl_packet_assistant_set_print_packet_info(1U);
  } else {
    sl_packet_assistant_set_print_packet_info(0U);
  }
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("Print setting is %s\n", sl_packet_assistant_get_print_packet_info() > 0 ? "ON" : "OFF");
#endif
}

/******************************************************************************
 * CLI - get_sun_fsk_fcs message: Get the SUN FSK FCS type
 *****************************************************************************/
void cli_get_sun_fsk_fcs(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("SUN FSK FCS type is %s\n", sl_packet_assistant_get_sun_fsk_fcs() ? "2-byte" : "4-byte");
#endif
}

/******************************************************************************
 * CLI - set_sun_fsk_fcs message: Set the SUN FSK FCS type
 *****************************************************************************/
void cli_set_sun_fsk_fcs(sl_cli_command_arg_t *arguments)
{
  uint8_t new_fcs = sl_cli_get_argument_uint8(arguments, 0);
  if (sl_packet_assistant_set_sun_fsk_fcs(new_fcs)) {
#if defined(SL_CATALOG_APP_LOG_PRESENT)
    app_log_info("SUN FSK FCS type is %s\n", sl_packet_assistant_get_sun_fsk_fcs() ? "2-byte" : "4-byte");
#endif
  }
}

/******************************************************************************
 * CLI - get_sun_fsk_whitening message: Get the SUN FSK whitening setting
 *****************************************************************************/
void cli_get_sun_fsk_whitening(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("SUN FSK whitening setting is %s\n", sl_packet_assistant_get_sun_fsk_whitening() > 0 ? "ON" : "OFF");
#endif
}

/******************************************************************************
 * CLI - set_sun_fsk_whitening message: Set the SUN FSK whitening setting
 *****************************************************************************/
void cli_set_sun_fsk_whitening(sl_cli_command_arg_t *arguments)
{
  uint8_t new_whitening = sl_cli_get_argument_uint8(arguments, 0);
  if (sl_packet_assistant_set_sun_fsk_whitening(new_whitening)) {
#if defined(SL_CATALOG_APP_LOG_PRESENT)
    app_log_info("SUN FSK whitening setting is %s\n", sl_packet_assistant_get_sun_fsk_whitening() > 0 ? "ON" : "OFF");
#endif
  }
}

/******************************************************************************
 * CLI - get_sun_ofdm_scrambler message: Get the SUN OFDM scrambler setting
 *****************************************************************************/
void cli_get_sun_ofdm_scrambler(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("SUN OFDM scrambler setting is 0x%02" PRIX8 "\n", sl_packet_assistant_get_sun_ofdm_scrambler());
#endif
}

/******************************************************************************
 * CLI - set_sun_ofdm_scrambler message: Set the SUN OFDM scrambler setting
 *****************************************************************************/
void cli_set_sun_ofdm_scrambler(sl_cli_command_arg_t *arguments)
{
  uint8_t new_scrambler = sl_cli_get_argument_uint8(arguments, 0);
  if (sl_packet_assistant_set_sun_ofdm_scrambler(new_scrambler)) {
#if defined(SL_CATALOG_APP_LOG_PRESENT)
    app_log_info("SUN OFDM scrambler setting is 0x%02" PRIX8 "\n", sl_packet_assistant_get_sun_ofdm_scrambler());
#endif
  }
}

/******************************************************************************
 * CLI - get_sun_ofdm_rate message: Get the SUN OFDM rate setting
 *****************************************************************************/
void cli_get_sun_ofdm_rate(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("SUN OFDM rate setting is 0x%02" PRIX8 "\n", sl_packet_assistant_get_sun_ofdm_rate());
#endif
}

/******************************************************************************
 * CLI - set_sun_ofdm_rate message: Set the SUN OFDM rate setting
 *****************************************************************************/
void cli_set_sun_ofdm_rate(sl_cli_command_arg_t *arguments)
{
  uint8_t new_rate = sl_cli_get_argument_uint8(arguments, 0);
  if (sl_packet_assistant_set_sun_ofdm_rate(new_rate)) {
#if defined(SL_CATALOG_APP_LOG_PRESENT)
    app_log_info("SUN OFDM rate setting is 0x%02" PRIX8 "\n", sl_packet_assistant_get_sun_ofdm_rate());
#endif
  }
}

/******************************************************************************
 * CLI - get_sun_oqpsk_spreading_mode message: Get the SUN OQPSK spreading mode setting
 *****************************************************************************/
void cli_get_sun_oqpsk_spreading_mode(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("SUN OQPSK spreading mode setting is %s\n", sl_packet_assistant_get_sun_oqpsk_spreading_mode() > 0 ? "ON" : "OFF");
#endif
}

/******************************************************************************
 * CLI - set_sun_oqpsk_spreading_mode message: Set the SUN OQPSK spreading mode setting
 *****************************************************************************/
void cli_set_sun_oqpsk_spreading_mode(sl_cli_command_arg_t *arguments)
{
  uint8_t new_spreading_mode = sl_cli_get_argument_uint8(arguments, 0);
  if (sl_packet_assistant_set_sun_oqpsk_spreading_mode(new_spreading_mode)) {
#if defined(SL_CATALOG_APP_LOG_PRESENT)
    app_log_info("SUN OQPSK spreading mode setting is %s\n", sl_packet_assistant_get_sun_oqpsk_spreading_mode() > 0 ? "ON" : "OFF");
#endif
  }
}

/******************************************************************************
 * CLI - get_sun_oqpsk_rate_mode message: Get the SUN OQPSK rate mode setting
 *****************************************************************************/
void cli_get_sun_oqpsk_rate_mode(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("SUN OQPSK rate mode setting is %" PRIu8 "\n", sl_packet_assistant_get_sun_oqpsk_rate_mode());
#endif
}

/******************************************************************************
 * CLI - set_sun_oqpsk_rate_mode message: Set the SUN OQPSK rate mode setting
 *****************************************************************************/
void cli_set_sun_oqpsk_rate_mode(sl_cli_command_arg_t *arguments)
{
  uint8_t new_rate_mode = sl_cli_get_argument_uint8(arguments, 0);
  if (sl_packet_assistant_set_sun_oqpsk_rate_mode(new_rate_mode)) {
#if defined(SL_CATALOG_APP_LOG_PRESENT)
    app_log_info("SUN OQPSK rate mode setting is %" PRIu8 "\n", sl_packet_assistant_get_sun_oqpsk_rate_mode());
#endif
  }
}

/******************************************************************************
 * CLI - get_sidewalk_fcs_type message: Get the Sidewalk FSK FCS type
 *****************************************************************************/
void cli_get_sidewalk_fcs_type(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("Sidewalk FSK FCS type is %s\n", sl_packet_assistant_get_sidewalk_fcs_type() ? "2-byte" : "4-byte");
#endif
}

/******************************************************************************
 * CLI - set_sidewalk_fcs_type message: Set the Sidewalk FSK FCS type
 *****************************************************************************/
void cli_set_sidewalk_fcs_type(sl_cli_command_arg_t *arguments)
{
  uint8_t new_fcs = sl_cli_get_argument_uint8(arguments, 0);
  if (sl_packet_assistant_set_sidewalk_fcs_type(new_fcs)) {
#if defined(SL_CATALOG_APP_LOG_PRESENT)
    app_log_info("Sidewalk FSK FCS type is %s\n", sl_packet_assistant_get_sidewalk_fcs_type() ? "2-byte" : "4-byte");
#endif
  }
}

/******************************************************************************
 * CLI - get_sidewalk_whitening message: Get the Sidewalk FSK whitening setting
 *****************************************************************************/
void cli_get_sidewalk_whitening(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("Sidewalk FSK whitening setting is %s\n", sl_packet_assistant_get_sidewalk_whitening() > 0 ? "ON" : "OFF");
#endif
}

/******************************************************************************
 * CLI - set_sidewalk_whitening message: Set the Sidewalk FSK whitening setting
 *****************************************************************************/
void cli_set_sidewalk_whitening(sl_cli_command_arg_t *arguments)
{
  uint8_t new_whitening = sl_cli_get_argument_uint8(arguments, 0);
  if (sl_packet_assistant_set_sidewalk_whitening(new_whitening)) {
#if defined(SL_CATALOG_APP_LOG_PRESENT)
    app_log_info("Sidewalk FSK whitening setting is %s\n", sl_packet_assistant_get_sidewalk_whitening() > 0 ? "ON" : "OFF");
#endif
  }
}

/******************************************************************************
 * CLI - get_wisun_fsk_fcs message: Get the Wi-SUN FSK FCS type
 *****************************************************************************/
void cli_get_wisun_fsk_fcs(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
  warning_deprecated_cli_command("get_wisun_fsk_fcs", "get_sun_fsk_fcs");
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("Wi-SUN FSK FCS type is %s\n", sl_packet_assistant_get_sun_fsk_fcs() ? "2-byte" : "4-byte");
#endif
}

/******************************************************************************
 * CLI - set_wisun_fsk_fcs message: Set the Wi-SUN FSK FCS type
 *****************************************************************************/
void cli_set_wisun_fsk_fcs(sl_cli_command_arg_t *arguments)
{
  uint8_t new_fcs = sl_cli_get_argument_uint8(arguments, 0);
  warning_deprecated_cli_command("set_wisun_fsk_fcs", "set_sun_fsk_fcs");
  if (sl_packet_assistant_set_sun_fsk_fcs(new_fcs)) {
#if defined(SL_CATALOG_APP_LOG_PRESENT)
    app_log_info("Wi-SUN FSK FCS type is %s\n", sl_packet_assistant_get_sun_fsk_fcs() ? "2-byte" : "4-byte");
#endif
  }
}

/******************************************************************************
 * CLI - get_wisun_fsk_whitening message: Get the Wi-SUN FSK whitening setting
 *****************************************************************************/
void cli_get_wisun_fsk_whitening(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
  warning_deprecated_cli_command("get_wisun_fsk_whitening",
                                 "get_sun_fsk_whitening");
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("Wi-SUN FSK whitening setting is %s\n", sl_packet_assistant_get_sun_fsk_whitening() > 0 ? "ON" : "OFF");
#endif
}

/******************************************************************************
 * CLI - set_wisun_fsk_whitening message: Set the Wi-SUN FSK whitening setting
 *****************************************************************************/
void cli_set_wisun_fsk_whitening(sl_cli_command_arg_t *arguments)
{
  uint8_t new_whitening = sl_cli_get_argument_uint8(arguments, 0);
  warning_deprecated_cli_command("set_wisun_fsk_whitening",
                                 "set_sun_fsk_whitening");
  if (sl_packet_assistant_set_sun_fsk_whitening(new_whitening)) {
#if defined(SL_CATALOG_APP_LOG_PRESENT)
    app_log_info("Wi-SUN FSK whitening setting is %s\n", sl_packet_assistant_get_sun_fsk_whitening() > 0 ? "ON" : "OFF");
#endif
  }
}

/******************************************************************************
 * CLI - get_wisun_ofdm_scrambler message: Get the Wi-SUN OFDM scrambler setting
 *****************************************************************************/
void cli_get_wisun_ofdm_scrambler(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
  warning_deprecated_cli_command("get_wisun_ofdm_scrambler",
                                 "get_sun_ofdm_scrambler");
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("Wi-SUN OFDM scrambler setting is 0x%02" PRIX8 "\n", sl_packet_assistant_get_sun_ofdm_scrambler());
#endif
}

/******************************************************************************
 * CLI - set_wisun_ofdm_scrambler message: Set the Wi-SUN OFDM scrambler setting
 *****************************************************************************/
void cli_set_wisun_ofdm_scrambler(sl_cli_command_arg_t *arguments)
{
  uint8_t new_scrambler = sl_cli_get_argument_uint8(arguments, 0);
  warning_deprecated_cli_command("set_wisun_ofdm_scrambler",
                                 "set_sun_ofdm_scrambler");
  if (sl_packet_assistant_set_sun_ofdm_scrambler(new_scrambler)) {
#if defined(SL_CATALOG_APP_LOG_PRESENT)
    app_log_info("Wi-SUN OFDM scrambler setting is 0x%02" PRIX8 "\n", sl_packet_assistant_get_sun_ofdm_scrambler());
#endif
  }
}

/******************************************************************************
 * CLI - get_wisun_ofdm_rate message: Get the Wi-SUN OFDM rate setting
 *****************************************************************************/
void cli_get_wisun_ofdm_rate(sl_cli_command_arg_t *arguments)
{
  (void) arguments;
  warning_deprecated_cli_command("get_wisun_ofdm_rate", "get_sun_ofdm_rate");
#if defined(SL_CATALOG_APP_LOG_PRESENT)
  app_log_info("Wi-SUN OFDM rate setting is 0x%02" PRIX8 "\n", sl_packet_assistant_get_sun_ofdm_rate());
#endif
}

/******************************************************************************
 * CLI - set_wisun_ofdm_rate message: Set the Wi-SUN OFDM rate setting
 *****************************************************************************/
void cli_set_wisun_ofdm_rate(sl_cli_command_arg_t *arguments)
{
  uint8_t new_rate = sl_cli_get_argument_uint8(arguments, 0);
  warning_deprecated_cli_command("set_wisun_ofdm_rate", "set_sun_ofdm_rate");
  if (sl_packet_assistant_set_sun_ofdm_rate(new_rate)) {
#if defined(SL_CATALOG_APP_LOG_PRESENT)
    app_log_info("Wi-SUN OFDM rate setting is 0x%02" PRIX8 "\n", sl_packet_assistant_get_sun_ofdm_rate());
#endif
  }
}
