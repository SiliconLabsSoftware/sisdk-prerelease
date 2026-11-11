/***************************************************************************//**
 * @file sli_mac_mux_aux_debug.c
 * @brief Strong mux-aux debug hook bodies (application compile time).
 *
 * Library objects call sli_mac_mux_aux_dbg, sli_mac_mux_aux_dbg_println, and
 * sli_rail_mux_aux_dbg_step. Weak no-ops in sli_mac_mux_aux_debug_weak.c keep
 * the prebuilt library linking. This file is compiled with rail_mux_aux and
 * overrides those weaks. SL_MAC_MUX_AUX_DEBUG selects print vs no-op.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include PLATFORM_HEADER
#include <stdarg.h>
#include <stdio.h>
#include "sl_mac_mux_aux_config.h"
#include "app/framework/plugin/debug-print/sl_zigbee_debug_print.h"
#include "mac-mux-aux.h"
#include "sli_rail_mux_aux.h"

void sli_mac_mux_aux_dbg_println(const char *format, ...)
{
#if SL_MAC_MUX_AUX_DEBUG
  char buffer[192];
  va_list args;
  va_start(args, format);
  (void)vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  sl_zigbee_core_debug_println("%s", buffer);
#else
  (void)format;
#endif
}

void sli_mac_mux_aux_dbg(uint8_t tag, uint32_t val)
{
#if SL_MAC_MUX_AUX_DEBUG
  sl_zigbee_core_debug_println("[mux-aux] dbg t=0x%02X v=0x%08lX", tag, (unsigned long)val);
#else
  (void)tag;
  (void)val;
#endif
}

void sli_rail_mux_aux_dbg_step(uint8_t step, uint32_t val)
{
#if SL_MAC_MUX_AUX_DEBUG
  sl_zigbee_core_debug_println("[mux-aux] step=0x%02X v=0x%08lX",
                               (unsigned)step,
                               (unsigned long)val);
#else
  (void)step;
  (void)val;
#endif
}
