/***************************************************************************//**
 * @file sl_wisun_meter.h
 * @brief Wi-SUN Meter
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_WISUN_METER_H
#define SL_WISUN_METER_H

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include "sl_wisun_events.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------

/**************************************************************************//**
 * @brief Init Meter component.
 * @details Init Meter component resources
 *****************************************************************************/
void sl_wisun_meter_init(void);

/**************************************************************************//**
 * @brief Handle Wi-SUN stack indications for the Meter component.
 * @details Applications that implement sl_wisun_on_event() should call this
 *          helper so LFN wake-up waits in sl_wisun_meter_loop() keep working.
 * @param[in] evt Event received from the stack
 *****************************************************************************/
void sl_wisun_meter_on_event(sl_wisun_evt_t *evt);

/**************************************************************************//**
 * @brief Meter loop function
 * @details This function includes the entire Meter request handling and measurement.
 *          The function is called in the application thread.
 *****************************************************************************/
void sl_wisun_meter_loop(void);

#ifdef __cplusplus
}
#endif

#endif // SL_WISUN_METER_H
