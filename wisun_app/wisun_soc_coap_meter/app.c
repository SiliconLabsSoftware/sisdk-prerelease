/***************************************************************************//**
 * @file app.c
 * @brief Application code
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

#include "sl_assert.h"
#include "app.h"
#include "sl_wisun_app_core.h"
#include "sl_wisun_app_core_util.h"
#include "sl_component_catalog.h"
#include "sl_wisun_config.h"
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

#if defined(SL_WISUN_FG23_SUPPORT)
/* app_custom_callback.c is excluded on FG23; provide the event handler here. */
void sl_wisun_on_event(sl_wisun_evt_t *evt)
{
  sl_wisun_app_core_on_event(evt);
#if defined(SL_CATALOG_WISUN_COAP_PRESENT)
  sl_wisun_coap_meter_on_event(evt);
#else
  sl_wisun_meter_on_event(evt);
#endif
}
#endif

////////////////////////////////////////////////////////
//        CoAP Meter Application task function
////////////////////////////////////////////////////////
#if defined(SL_CATALOG_WISUN_COAP_PRESENT)
void app_task(void *args)
{
  (void) args;

  sl_wisun_device_type_t dev_type = SL_WISUN_ROUTER;

  // connect to the wisun network
  sl_wisun_app_core_util_connect_and_wait();

  dev_type = sl_wisun_app_core_get_device_type();

  while (1) {
    if (dev_type == SL_WISUN_LFN) {
      sl_wisun_coap_meter_wait_lfn_wakeup();
    } else {
      sl_wisun_app_core_util_dispatch_thread();
    }
  }
}

////////////////////////////////////////////////////////
//        Simple UDP Meter Application task function
////////////////////////////////////////////////////////
#else

void app_task(void *args)
{
  (void) args;

  // connect to the wisun network
  sl_wisun_app_core_util_connect_and_wait();

  // Simple Meter loop
  sl_wisun_meter_loop();
}
#endif

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
