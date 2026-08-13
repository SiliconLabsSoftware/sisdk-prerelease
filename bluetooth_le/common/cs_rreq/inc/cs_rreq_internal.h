/***************************************************************************//**
 * @file
 * @brief CS RREQ Internal header
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

#ifndef CS_RREQ_INTERNAL_H
#define CS_RREQ_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "cs_rreq.h"
#include "cs_rreq_types.h"
#include "cs_rreq_config.h"
#include "cs_ras_client.h"
#include "cs_ras_client_timeout.h"
#include "cs_ras_common.h"
#include "sl_bt_api.h"
#include "sl_enum.h"
#include "sl_status.h"
#include "app_rta.h"

#ifdef __cplusplus
extern "C" {
#endif

// RTA context guarding shared state.
extern app_rta_context_t cs_rreq_ctx;

// Registered event callbacks (create, enable, error).
// set via cs_rreq_set_event_callbacks.
extern cs_rreq_event_callback_t callback;

// Registered result callback; set via cs_rreq_set_result_callback
extern cs_rreq_on_result_t send_result;

/******************************************************************************
 * Find the first empty RREQ instance slot.
 *
 * @return Pointer to an empty rreq_t slot, or NULL if no slot is available.
 *****************************************************************************/
rreq_t *cs_rreq_find_empty(void);

/******************************************************************************
 * Find the RREQ instance associated with a connection handle.
 *
 * @param[in] conn_handle Connection handle to search for.
 * @return Pointer to the matching rreq_t, or NULL if not found.
 *****************************************************************************/
rreq_t *cs_rreq_find(uint8_t conn_handle);

/******************************************************************************
 * Route a Bluetooth stack event to the RREQ module.
 *
 * @param[in] evt Pointer to the Bluetooth stack event.
 * @return true  if the event was not consumed and must be forwarded to other
 *               handlers.
 * @return false if the event was fully handled by the RREQ module.
 *****************************************************************************/
bool cs_rreq_on_bt_event(sl_bt_msg_t *evt);

/******************************************************************************
 * Initialize the RREQ module.
 *
 * @note Resets all instance slots. Called automatically
 *       during application initialization.
 *****************************************************************************/
void cs_rreq_init(void);

/******************************************************************************
 * Create the RTA context used by the RREQ module.
 *****************************************************************************/
void cs_rreq_rta_init(void);

/******************************************************************************
 * Signal the RTA context that initialization is complete and operations may
 * proceed.
 *****************************************************************************/
void cs_rreq_rta_ready(void);

/******************************************************************************
 * RTA step function.
 *****************************************************************************/
void cs_rreq_step(void);

/******************************************************************************
 * Enqueue a RAS Client event for deferred processing in @ref cs_rreq_step.
 *
 * @note Safe to call from any context (e.g. the cs_ras_client callback
 *       chain that already holds cs_ras_client's guard). Does NOT acquire the
 *       cs_rreq guard.
 *
 * @param[in] evt Pointer to the populated event. Copied by value into the
 *                queue; @p evt does not need to outlive the call.
 *****************************************************************************/
void cs_rreq_post_ras_evt(const cs_rreq_ras_evt_t *evt);

/******************************************************************************
 * Invoke the registered error callback for an RREQ instance.
 *
 * @note If rreq is NULL the error is logged but no callback is invoked.
 *
 * @param[in] rreq RREQ instance reference, or NULL if unavailable.
 * @param[in] evt  Error event identifier.
 * @param[in] sc   Underlying status code.
 *****************************************************************************/
void rreq_error(rreq_t *rreq, cs_rreq_error_t evt, sl_status_t sc);

/******************************************************************************
 * Reset subevent data and synchronization for an RREQ instance.
 *
 * @param[in] rreq RREQ instance reference.
 * @param[in] init true if the reset is performed during initialization (skips
 *                 the debug log message).
 *****************************************************************************/
void reset_subevent_data(rreq_t *rreq, bool init);

/******************************************************************************
 * Extract CS results (step data, subevent data) into the ranging data buffer.
 *
 * @note Converts raw BT stack CS result event data into the RAS format.
 *       Appends step data to the appropriate role buffer (initiator or
 *       reflector) inside the rreq instance.
 *
 * @param[in] rreq              RREQ instance reference.
 * @param[in] cs_result_content Pointer to the extracted CS result event data.
 * @return CS_PROCEDURE_STATE_IN_PROGRESS if the procedure is still ongoing.
 * @return CS_PROCEDURE_STATE_COMPLETED   if all subevent data is collected.
 * @return CS_PROCEDURE_STATE_ABORTED     if the procedure was aborted.
 *****************************************************************************/
cs_procedure_state_t extract_cs_result_data(rreq_t *rreq,
                                            cs_result_data_t *cs_result_content);

/******************************************************************************
 * Check whether accumulated ranging data represents a complete CS procedure.
 *
 * @param[in] data             Pointer to the RAS ranging data body.
 * @param[in] size             Number of valid bytes in data.
 * @param[in] is_initiator     True for initiator role, false for reflector.
 * @param[in] antenna_path_num Number of antenna paths per step.
 * @return CS_PROCEDURE_STATE_COMPLETED if the procedure is complete.
 * @return CS_PROCEDURE_STATE_ABORTED   if the procedure was aborted or data
 *                                      is incomplete.
 *****************************************************************************/
cs_procedure_state_t ranging_data_is_complete(uint8_t *data,
                                              uint32_t size,
                                              bool is_initiator,
                                              uint8_t antenna_path_num);

/*******************************************************************************
 * Dispatch ranging data from the RREQ to every component that registered
 * a handler via the 'cs_rreq_on_ras_data' template contribution.
 *
 * @param[in] conn_handle     Connection handle.
 * @param[in] ranging_counter Procedure ranging counter.
 * @param[in] proc_info       Contains procedure info
 * @param[in] ranging_data    Pointer to the RREQ-owned ranging data buffer for
 *                            this procedure.
 ******************************************************************************/
void cs_rreq_dispatch_ras_data(uint8_t conn_handle,
                               uint16_t ranging_counter,
                               cs_rreq_procedure_info_t proc_info,
                               cs_rreq_result_t *ranging_data);
#ifdef __cplusplus
}
#endif

#endif // CS_RREQ_INTERNAL_H
