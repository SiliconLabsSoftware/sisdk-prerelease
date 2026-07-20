/***************************************************************************//**
 * @file
 * @brief Watchdog Manager CLI commands
 ******************************************************************************/

#include "sl_component_catalog.h"

#if defined(SL_CATALOG_CLI_PRESENT)

#include "sli_watchdog_manager_hal.h"
#include "sli_watchdog_manager_hal_clock_config.h"
#include "sl_watchdog_manager_config.h"
#include "sl_cli.h"
#include "sl_status.h"
#include "sl_string.h"
#include <stdio.h>
#include <inttypes.h>

/// Pending timeout period for watchdog_cli_apply_hal_configuration().
static uint8_t watchdog_cli_pending_timeout_period;

/// Pending clock source for watchdog_cli_apply_hal_configuration().
static sli_watchdog_manager_hal_clock_source_t watchdog_cli_pending_clock_source;

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
 * @brief Convert HAL clock source enum to CLI clock name string.
 ******************************************************************************/
static const char *watchdog_cli_clock_source_to_string(
  sli_watchdog_manager_hal_clock_source_t clock_source)
{
  switch (clock_source) {
    case SLI_WATCHDOG_MANAGER_HAL_CLK_HCLKDIV1024:  return "hclk";
    case SLI_WATCHDOG_MANAGER_HAL_CLK_LFRCO:        return "lfrco";
    case SLI_WATCHDOG_MANAGER_HAL_CLK_LFXO:         return "lfxo";
    case SLI_WATCHDOG_MANAGER_HAL_CLK_ULFRCO:       return "ulfrco";
    default:                                        return "unknown";
  }
}

/***************************************************************************//**
 * @brief Parse a CLI clock source name into the HAL clock source enum.
 *
 * @param[in]  name          Clock name string (hclk, lfrco, ulfrco, or lfxo).
 * @param[out] clock_source  Parsed HAL clock source on success.
 *
 * @return true if @p name is a supported clock source for this project.
 * @return false if @p name is unknown or LFXO is not enabled in Clock Manager.
 ******************************************************************************/
static bool watchdog_cli_parse_clock_source_name(
  const char *name,
  sli_watchdog_manager_hal_clock_source_t *clock_source)
{
  if (sl_strcasecmp(name, "hclk") == 0) {
    *clock_source = SLI_WATCHDOG_MANAGER_HAL_CLK_HCLKDIV1024;
    return true;
  }
  if (sl_strcasecmp(name, "lfrco") == 0) {
    *clock_source = SLI_WATCHDOG_MANAGER_HAL_CLK_LFRCO;
    return true;
  }
  if (sl_strcasecmp(name, "ulfrco") == 0) {
    *clock_source = SLI_WATCHDOG_MANAGER_HAL_CLK_ULFRCO;
    return true;
  }
#if SLI_WATCHDOG_MANAGER_HAL_LFXO_CLOCK_AVAILABLE
  if (sl_strcasecmp(name, "lfxo") == 0) {
    *clock_source = SLI_WATCHDOG_MANAGER_HAL_CLK_LFXO;
    return true;
  }
#endif
  return false;
}

/***************************************************************************//**
 * @brief Disable WDOG, apply a HAL configuration change, feed, and re-enable.
 *
 * @param[in] apply_configuration  HAL setter to invoke while WDOG is disabled.
 * @param[in] command_name         CLI subcommand name for error messages.
 *
 * @return true if the full sequence completed successfully.
 ******************************************************************************/
static bool watchdog_cli_apply_hal_configuration(
  sl_status_t (*apply_configuration)(void),
  const char *command_name)
{
  sl_status_t status;
  bool wdog_disabled = false;

  status = sli_watchdog_manager_hal_disable();
  if (status != SL_STATUS_OK && status != SL_STATUS_NOT_SUPPORTED) {
    printf("wd %s error: 0x%04" PRIx32 "\r\n", command_name, status);
    return false;
  }
  wdog_disabled = true;

  status = apply_configuration();
  if (status != SL_STATUS_OK) {
    if (wdog_disabled) {
      (void)sli_watchdog_manager_hal_enable();
    }
    printf("wd %s error: 0x%04" PRIx32 "\r\n", command_name, status);
    return false;
  }

  status = sli_watchdog_manager_hal_feed();
  if (status != SL_STATUS_OK) {
    if (wdog_disabled) {
      (void)sli_watchdog_manager_hal_enable();
    }
    printf("wd %s error: 0x%04" PRIx32 "\r\n", command_name, status);
    return false;
  }

  status = sli_watchdog_manager_hal_enable();
  if (status != SL_STATUS_OK && status != SL_STATUS_NOT_SUPPORTED) {
    printf("wd %s error: 0x%04" PRIx32 "\r\n", command_name, status);
    return false;
  }

  return true;
}

/***************************************************************************//**
 * @brief Apply the pending timeout period via the HAL.
 ******************************************************************************/
static sl_status_t watchdog_cli_apply_pending_timeout_period(void)
{
  return sli_watchdog_manager_hal_set_timeout_period(
    watchdog_cli_pending_timeout_period);
}

/***************************************************************************//**
 * @brief Apply the pending clock source via the HAL.
 ******************************************************************************/
static sl_status_t watchdog_cli_apply_pending_clock_source(void)
{
  return sli_watchdog_manager_hal_set_clock_source(
    watchdog_cli_pending_clock_source);
}

/***************************************************************************//**
 * @brief CLI handler for `wd timeout-get`.
 *
 * @param[in] arguments CLI argument list (unused).
 ******************************************************************************/
void sl_watchdog_manager_cli_timeout_get(sl_cli_command_arg_t *arguments)
{
  uint8_t period = 0;
  sl_status_t status;

  (void)arguments;

  status = sli_watchdog_manager_hal_get_timeout_period(&period);
  if (status != SL_STATUS_OK) {
    printf("wd timeout-get error: 0x%04" PRIx32 "\r\n", status);
    return;
  }

  printf("Hardware watchdog timeout period: %u (%s)\r\n",
         period, watchdog_cli_timeout_period_to_string(period));
}

/***************************************************************************//**
 * @brief CLI handler for `wd timeout-set`.
 *
 * @param[in] arguments CLI argument list; argument 0 is PERSEL index (0-15).
 ******************************************************************************/
void sl_watchdog_manager_cli_timeout_set(sl_cli_command_arg_t *arguments)
{
  uint8_t period = sl_cli_get_argument_uint8(arguments, 0);

#if SL_WATCHDOG_MANAGER_LOCK != 0
  (void)period;
  printf("wd timeout-set rejected: configuration is locked\r\n");
  return;
#endif

  if (period > 15) {
    printf("wd timeout-set invalid: use 0-15\r\n");
    return;
  }

  watchdog_cli_pending_timeout_period = period;

  if (!watchdog_cli_apply_hal_configuration(watchdog_cli_apply_pending_timeout_period,
                                            "timeout-set")) {
    return;
  }

  printf("Hardware watchdog timeout period set to %u (%s)\r\n",
         period, watchdog_cli_timeout_period_to_string(period));
}

/***************************************************************************//**
 * @brief CLI handler for `wd clock-get`.
 *
 * @param[in] arguments CLI argument list (unused).
 ******************************************************************************/
void sl_watchdog_manager_cli_clock_get(sl_cli_command_arg_t *arguments)
{
  sli_watchdog_manager_hal_clock_source_t clock_source;
  sl_status_t status;

  (void)arguments;

  status = sli_watchdog_manager_hal_get_clock_source(&clock_source);
  if (status != SL_STATUS_OK) {
    printf("wd clock-get error: 0x%04" PRIx32 "\r\n", status);
    return;
  }

  printf("Hardware watchdog clock source: %s\r\n",
         watchdog_cli_clock_source_to_string(clock_source));
}

/***************************************************************************//**
 * @brief CLI handler for `wd clock-set`.
 *
 * @param[in] arguments CLI argument list; argument 0 is clock name
 *                      (hclk, lfrco, ulfrco, or lfxo when enabled).
 ******************************************************************************/
void sl_watchdog_manager_cli_clock_set(sl_cli_command_arg_t *arguments)
{
  const char *clock_name = sl_cli_get_argument_string(arguments, 0);
  sli_watchdog_manager_hal_clock_source_t clock_source;

#if SL_WATCHDOG_MANAGER_LOCK != 0
  (void)clock_name;
  printf("wd clock-set rejected: configuration is locked\r\n");
  return;
#endif

  if (!watchdog_cli_parse_clock_source_name(clock_name, &clock_source)) {
#if SLI_WATCHDOG_MANAGER_HAL_LFXO_CLOCK_AVAILABLE
    printf("wd clock-set invalid: use hclk, lfrco, ulfrco, or lfxo\r\n");
#else
    printf("wd clock-set invalid: use hclk, lfrco, or ulfrco\r\n");
#endif
    return;
  }

  watchdog_cli_pending_clock_source = clock_source;

  if (!watchdog_cli_apply_hal_configuration(watchdog_cli_apply_pending_clock_source,
                                            "clock-set")) {
    return;
  }

  printf("Hardware watchdog clock source set to %s\r\n",
         watchdog_cli_clock_source_to_string(clock_source));
}

#endif // SL_CATALOG_CLI_PRESENT
