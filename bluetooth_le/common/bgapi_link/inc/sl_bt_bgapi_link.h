/**************************************************************************//**
 * @file
 * @brief Public API for the BGAPI over cpc-ng component.
 ******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 ******************************************************************************
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
 *****************************************************************************/
#ifndef SL_BT_BGAPI_LINK_H
#define SL_BT_BGAPI_LINK_H

#include "sl_bt_link.h"
#include "sl_bt_link_runtime.h"

/***********************************************************************************************//**
 * @addtogroup sl_bt_bgapi_link
 * @{
 **************************************************************************************************/

/**************************************************************************//**
 * Return the BGAPI message link.
 *
 * @return Link object, or NULL before a successful
 *         @ref sli_bt_bgapi_link_init.
 *****************************************************************************/
sl_bt_link_t *sl_bt_bgapi_link_get_link(void);

/**************************************************************************//**
 * Return the shared cpc-ng message runtime.
 *
 * @return Runtime object, or NULL if the runtime is not initialized.
 *****************************************************************************/
sl_bt_link_runtime_t *sl_bt_bgapi_link_get_runtime(void);

/** @} (end addtogroup sl_bt_bgapi_link) */

#endif // SL_BT_BGAPI_LINK_H
