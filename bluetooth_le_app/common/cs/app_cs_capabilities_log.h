/***************************************************************************//**
 * @file
 * @brief CS supported capabilities comparison logging
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

#ifndef APP_CS_CAPABILITIES_LOG_H
#define APP_CS_CAPABILITIES_LOG_H

#include "app_config.h"
#include "app_cs_capabilities.h"
#include "sl_bt_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CS_CAPABILITIES_LOG) && (CS_CAPABILITIES_LOG == 1)

/****************************************************************************//**
 * Log local and remote CS supported capabilities as a comparison table.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] local Local capabilities.
 * @param[in] remote Remote capabilities.
 *******************************************************************************/
void app_cs_log_supported_capabilities(
  uint8_t conn_handle,
  const app_cs_capabilities_t *local,
  const sl_bt_evt_cs_read_remote_supported_capabilities_complete_t *remote);

#else // defined(CS_CAPABILITIES_LOG) && (CS_CAPABILITIES_LOG == 1)

/****************************************************************************//**
 * Log local and remote CS supported capabilities as a comparison table.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] local Local capabilities.
 * @param[in] remote Remote capabilities.
 *******************************************************************************/
#define app_cs_log_supported_capabilities(conn_handle, local, remote)

#endif // defined(CS_CAPABILITIES_LOG) && (CS_CAPABILITIES_LOG == 1)

#ifdef __cplusplus
}
#endif

#endif // APP_CS_CAPABILITIES_LOG_H
