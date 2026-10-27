/**************************************************************************//**
 * @file
 * @brief Internal init handlers for the BGAPI over cpc-ng component.
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
#ifndef SLI_BT_BGAPI_LINK_H
#define SLI_BT_BGAPI_LINK_H

/******************************************************************************
 * Create the BGAPI message link on the shared cpc-ng runtime.
 *
 * Called from system initialization. On failure the error binding is invoked.
 *****************************************************************************/
void sli_bt_bgapi_link_init(void);

/******************************************************************************
 * Start the BGAPI CPC endpoint after the runtime is ready.
 *
 * Primary role connects; secondary role listens. On failure the error binding
 * is invoked. Does nothing if @ref sli_bt_bgapi_link_init failed.
 *****************************************************************************/
void sli_bt_bgapi_link_ready(void);

/******************************************************************************
 * Initialize the host-side BGAPI client for non-blocking CPC I/O.
 *
 * Host role only. Wires output, input, and peek callbacks into
 * @ref sl_bt_api_initialize_nonblock.
 *****************************************************************************/
void sli_bt_bgapi_link_host_init(void);

#endif // SLI_BT_BGAPI_LINK_H
