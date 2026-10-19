/***************************************************************************//**
 * @file
 * @brief SYSTEMVIEW Custom API
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

#ifndef SYSVIEW_CUSTOM_API_H
#define SYSVIEW_CUSTOM_API_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *****************************   PROTOTYPES   **********************************
 ******************************************************************************/

/***************************************************************************//**
 * Gets the gecko_sdk_module event offset.
 *
 * @return gecko_sdk_module event offset value.
 ******************************************************************************/
int sl_systemview_GetGSDKEventOffset (void);

/***************************************************************************//**
 * Indicates whether SEGGER_SYSVIEW_Conf() has already run.
 *
 * @return true if SystemView is configured, false otherwise.
 ******************************************************************************/
bool sli_systemview_is_configured(void);

/***************************************************************************//**
 * Configures SystemView (once) and starts recording if not already started.
 *
 * @note Safe to call before sleeptimer and clocks are initialized, including
 *       from C++ pre-initialization. RTT must be initialized first.
 ******************************************************************************/
void sli_systemview_start(void);

/***************************************************************************//**
 * Updates the timestamp and CPU frequencies reported to the SystemView host.
 *
 * @note Call once the timestamp source is initialized when SystemView was
 *       started before it.
 ******************************************************************************/
void sli_systemview_update_timestamp_freq(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSVIEW_CUSTOM_API_H */
