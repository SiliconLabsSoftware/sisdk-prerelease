/***************************************************************************//**
 * @file
 * @brief CS RREQ State Machine header
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

#ifndef CS_RREQ_STATE_H
#define CS_RREQ_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"
#include "cs_rreq_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Dispatch a state machine event to an RREQ instance.
 *
 * @note This is the single entry point for all RREQ state transitions.
 *       Callers must not assume any particular resulting state; side effects
 *       (callbacks, RAS client calls) may occur synchronously during dispatch.
 *       Passing NULL for data is valid for events that carry no payload.
 *
 * @param[in] rreq  RREQ instance to dispatch the event to.
 * @param[in] event State machine event identifier.
 * @param[in] data  Optional event payload; may be NULL for events with no data.
 * @return SL_STATUS_OK on a successful state transition, error code otherwise.
 *****************************************************************************/
sl_status_t sm_on_evt(rreq_t *rreq, sm_evt_t event, sm_evt_data_t *data);

/******************************************************************************
 * Set the state of an RREQ instance.
 *
 * @param[in] rreq  RREQ instance to set the state of.
 * @param[in] state New state to set.
 *****************************************************************************/
void set_state(rreq_t *rreq, rreq_state_t state);

#ifdef __cplusplus
};
#endif

#endif // CS_RREQ_STATE_H
