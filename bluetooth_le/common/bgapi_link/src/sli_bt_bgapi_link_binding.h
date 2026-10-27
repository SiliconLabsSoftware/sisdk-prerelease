/**************************************************************************//**
 * @file
 * @brief Role-specific link binding callbacks for BGAPI over cpc-ng.
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
#ifndef SLI_BT_BGAPI_LINK_BINDING_H
#define SLI_BT_BGAPI_LINK_BINDING_H

#include "sl_bt_link.h"
#include "sl_component_catalog.h"

#if defined(SL_CATALOG_CPC_NG_SECONDARY_PRESENT)
/******************************************************************************
 * Binding: a complete BGAPI message is available on the link.
 *
 * Host and NCP each provide an implementation. @p user is the value from
 * link configuration (unused by the component singleton).
 *
 * @param[in] link Link that received the message.
 * @param[in] user User context from the link configuration.
 *****************************************************************************/
void sli_bt_bgapi_link_binding_on_message(sl_bt_link_t *link,
                                          void *user);

/******************************************************************************
 * Binding: BGAPI link initialization or startup failed.
 *
 * @param[in] status Error status from the failing operation.
 *****************************************************************************/
void sli_bt_bgapi_link_binding_on_init_error(sl_status_t status);
#endif

/******************************************************************************
 * Binding: transmit of one BGAPI message has completed.
 *
 * @param[in] link Link that sent the message.
 * @param[in] status SL_STATUS_OK on success, otherwise an error code.
 * @param[in] user User context from the link configuration.
 *****************************************************************************/
void sli_bt_bgapi_link_binding_on_transmit(sl_bt_link_t *link,
                                           sl_status_t status,
                                           void *user);

#if defined(SL_CATALOG_CPC_NG_PRIMARY_PRESENT)
/******************************************************************************
 * Binding: the link reported an error or is recovering the endpoint.
 *
 * @param[in] link Link object, or NULL if create failed before a link existed.
 * @param[in] status Error status from the link or from initialization.
 * @param[in] user User context from the link configuration.
 *****************************************************************************/
void sli_bt_bgapi_link_binding_on_error(sl_bt_link_t *link,
                                        sl_status_t status,
                                        void *user);

/******************************************************************************
 * Binding: the endpoint reached the connected state.
 *
 * @param[in] link Link that became connected.
 * @param[in] user User context from the link configuration.
 *****************************************************************************/
void sli_bt_bgapi_link_binding_on_connected(sl_bt_link_t *link,
                                            void *user);
#endif

#endif // SLI_BT_BGAPI_LINK_BINDING_H
