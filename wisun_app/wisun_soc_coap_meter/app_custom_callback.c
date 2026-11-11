/***************************************************************************//**
 * @file app_custom_callback.c
 * @brief Application custom callback implementations
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
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
#include <stdio.h>

#include "sl_status.h"
#include "sl_component_catalog.h"
#include "app_custom_callback.h"
#include "sl_wisun_app_core.h"
#if defined(SL_CATALOG_WISUN_COAP_PRESENT)
#include "sl_wisun_coap_meter.h"
#else
#include "sl_wisun_meter.h"
#endif
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

void sl_wisun_on_event(sl_wisun_evt_t *evt)
{
  sl_wisun_app_core_on_event(evt);
#if defined(SL_CATALOG_WISUN_COAP_PRESENT)
  sl_wisun_coap_meter_on_event(evt);
#else
  sl_wisun_meter_on_event(evt);
#endif

  switch (evt->header.id) {
    case SL_WISUN_MSG_CONNECTED_IND_ID:
      app_custom_connected_callback(evt);
      break;
    case SL_WISUN_MSG_SOCKET_DATA_IND_ID:
      app_custom_socket_data_callback(evt);
      break;
    case SL_WISUN_MSG_SOCKET_DATA_SENT_IND_ID:
      app_custom_socket_data_sent_callback(evt);
      break;
    default:
      break;
  }
}



/* Wi-SUN connected custom callback */
void app_custom_connected_callback(sl_wisun_evt_t *evt)
{
  (void) evt;
}

/* Socket data custom callback */
void app_custom_socket_data_callback(sl_wisun_evt_t *evt)
{
  (void) evt;
}

/* Socket data sent custom callback */
void app_custom_socket_data_sent_callback(sl_wisun_evt_t *evt)
{
  (void) evt;
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
