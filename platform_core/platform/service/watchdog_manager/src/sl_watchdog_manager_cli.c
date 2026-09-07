/***************************************************************************//**
 * @file
 * @brief Watchdog Manager CLI commands
 ******************************************************************************/

#include "sl_component_catalog.h"

#if defined(SL_CATALOG_CLI_PRESENT)

#include "sl_watchdog_manager.h"
#include "sli_watchdog_manager_hal_clock_config.h"
#include "sl_cli.h"
#include "sl_status.h"
#include "sl_string.h"
#include <stdio.h>
#include <inttypes.h>

/***************************************************************************//**
 * @brief Convert PERSEL index to a short cycle-count label for CLI output.
 ******************************************************************************/
static const char *watchdog_cli_timeout_period_to_string(uint8_t period)
{
  static const char * const names[] = {
    "9K", "17K", "33K", "65K", "129K", "257K", "513K",
    "1M", "2M", "4M", "8M", "16M", "32M", "64M", "128M", "256M"
  };

  if (period > 15) {
    return "unknown";
  }

  return names[period];
}

/***************************************************************************//**
 * @brief Convert clock source enum to CLI clock name string.
 ******************************************************************************/
static const char *watchdog_cli_clock_source_to_string(
  sl_watchdog_manager_clock_source_t clock_source)
{
  switch (clock_source) {
    case SL_WATCHDOG_MANAGER_CLOCK_SOURCE_HCLKDIV1024:  return "hclk";
    case SL_WATCHDOG_MANAGER_CLOCK_SOURCE_LFRCO:        return "lfrco";
    case SL_WATCHDOG_MANAGER_CLOCK_SOURCE_LFXO:         return "lfxo";
    case SL_WATCHDOG_MANAGER_CLOCK_SOURCE_ULFRCO:       return "ulfrco";
    default:                                            return "unknown";
  }
}

/***************************************************************************//**
 * @brief Parse a CLI clock source name into the clock source enum.
 ******************************************************************************/
static bool watchdog_cli_parse_clock_source_name(
  const char *name,
  sl_watchdog_manager_clock_source_t *clock_source)
{
  if (sl_strcasecmp(name, "hclk") == 0) {
    *clock_source = SL_WATCHDOG_MANAGER_CLOCK_SOURCE_HCLKDIV1024;
    return true;
  }
  if (sl_strcasecmp(name, "lfrco") == 0) {
    *clock_source = SL_WATCHDOG_MANAGER_CLOCK_SOURCE_LFRCO;
    return true;
  }
  if (sl_strcasecmp(name, "ulfrco") == 0) {
    *clock_source = SL_WATCHDOG_MANAGER_CLOCK_SOURCE_ULFRCO;
    return true;
  }
#if SLI_WATCHDOG_MANAGER_HAL_LFXO_CLOCK_AVAILABLE
  if (sl_strcasecmp(name, "lfxo") == 0) {
    *clock_source = SL_WATCHDOG_MANAGER_CLOCK_SOURCE_LFXO;
    return true;
  }
#endif
  return false;
}

/***************************************************************************//**
 * @brief CLI handler for `wd timeout-get`.
 ******************************************************************************/
void sl_watchdog_manager_cli_timeout_get(sl_cli_command_arg_t *arguments)
{
  uint8_t period = 0;
  sl_status_t status;

  (void)arguments;

  status = sl_watchdog_manager_get_timeout_period(&period);
  if (status != SL_STATUS_OK) {
    printf("wd timeout-get error: 0x%04" PRIx32 "\r\n", status);
    return;
  }

  printf("Hardware watchdog timeout period: %u (%s)\r\n",
         period, watchdog_cli_timeout_period_to_string(period));
}

/***************************************************************************//**
 * @brief CLI handler for `wd timeout-set`.
 ******************************************************************************/
void sl_watchdog_manager_cli_timeout_set(sl_cli_command_arg_t *arguments)
{
  uint8_t period = sl_cli_get_argument_uint8(arguments, 0);
  sl_status_t status;

  if (period > 15) {
    printf("wd timeout-set invalid: use 0-15\r\n");
    return;
  }

  status = sl_watchdog_manager_set_timeout_period(period);
  if (status != SL_STATUS_OK) {
    printf("wd timeout-set error: 0x%04" PRIx32 "\r\n", status);
    return;
  }

  printf("Hardware watchdog timeout period set to %u (%s)\r\n",
         period, watchdog_cli_timeout_period_to_string(period));
}

/***************************************************************************//**
 * @brief CLI handler for `wd clock-get`.
 ******************************************************************************/
void sl_watchdog_manager_cli_clock_get(sl_cli_command_arg_t *arguments)
{
  sl_watchdog_manager_clock_source_t clock_source;
  sl_status_t status;

  (void)arguments;

  status = sl_watchdog_manager_get_clock_source(&clock_source);
  if (status != SL_STATUS_OK) {
    printf("wd clock-get error: 0x%04" PRIx32 "\r\n", status);
    return;
  }

  printf("Hardware watchdog clock source: %s\r\n",
         watchdog_cli_clock_source_to_string(clock_source));
}

/***************************************************************************//**
 * @brief CLI handler for `wd clock-set`.
 ******************************************************************************/
void sl_watchdog_manager_cli_clock_set(sl_cli_command_arg_t *arguments)
{
  const char *clock_name = sl_cli_get_argument_string(arguments, 0);
  sl_watchdog_manager_clock_source_t clock_source;
  sl_status_t status;

  if (!watchdog_cli_parse_clock_source_name(clock_name, &clock_source)) {
#if SLI_WATCHDOG_MANAGER_HAL_LFXO_CLOCK_AVAILABLE
    printf("wd clock-set invalid: use hclk, lfrco, ulfrco, or lfxo\r\n");
#else
    printf("wd clock-set invalid: use hclk, lfrco, or ulfrco\r\n");
#endif
    return;
  }

  status = sl_watchdog_manager_set_clock_source(clock_source);
  if (status != SL_STATUS_OK) {
    printf("wd clock-set error: 0x%04" PRIx32 "\r\n", status);
    return;
  }

  printf("Hardware watchdog clock source set to %s\r\n",
         watchdog_cli_clock_source_to_string(clock_source));
}

#endif // SL_CATALOG_CLI_PRESENT
