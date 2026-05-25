/***************************************************************************//**
 * @file
 * @brief CS RREQ feature API
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

#ifndef CS_RREQ_H
#define CS_RREQ_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_common.h"
#include "cs_ras_client.h"
#include "cs_rreq_config.h"
#include "cs_rreq_api.h"

/***************************************************************************//**
 * @addtogroup cs_rreq
 * @{
 ******************************************************************************/

/// Maximum number of CS steps per procedure
#define CS_RREQ_MAX_STEP_COUNT 256

/// Ranging data buffer type
typedef struct {
  uint32_t data_size;                                  ///< Number of valid bytes data
  uint8_t  data[CS_RREQ_CONFIG_MAX_RANGING_DATA_SIZE]; ///< RAS ranging data body
} cs_rreq_ranging_buffer_t;

/// Ranging data buffer type
typedef struct {
  uint8_t num_steps;                             ///< Number of steps in the procedure
  uint8_t step_channels[CS_RREQ_MAX_STEP_COUNT]; ///< Channel used for each step
  cs_rreq_ranging_buffer_t initiator;            ///< Initiator RAS ranging data
  cs_rreq_ranging_buffer_t reflector;            ///< Reflector RAS ranging data
} cs_rreq_result_t;

/// Procedzre info type
typedef struct {
  uint32_t subevent_len;        ///< Duration for each subevent in microseconds
  uint16_t subevent_interval;   ///< Duration in microseconds between 
                                ///< consecutive CS subevents
  uint16_t event_interval;      ///< umber of ACL connection events between
                                ///< consecutive CS event anchor points
  uint16_t procedure_interval;  ///< Number of ACL connection events between 
                                ///< consecutive CS procedure anchor points
  uint16_t procedure_count;     ///< Number of procedures to continue until
                                ///< disabled.
  uint8_t  subevents_per_event; ///< Number of subevents anchored off the same
                                ///< ACL connection event
} cs_rreq_procedure_info_t;

/**************************************************************************//**
 * @brief Callback invoked when a create operation has completed.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] sc          Status of the create operation.
 *****************************************************************************/
typedef void (*cs_rreq_create_complete_t)(uint8_t conn_handle, sl_status_t sc);

/**************************************************************************//**
 * @brief Callback invoked when an enable or disable operation has completed.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] enable      Requested enable state (@ref cs_rreq_enable_t).
 * @param[in] sc          Status of the enable operation.
 *****************************************************************************/
typedef void (*cs_rreq_enable_complete_t)(uint8_t conn_handle, 
                                          cs_rreq_enable_t enable, 
                                          sl_status_t sc);

/**************************************************************************//**
 * @brief Callback invoked when an error occurs during RREQ operation.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] error       Error event identifier (@ref cs_rreq_error_t).
 * @param[in] sc          Underlying status code.
 *****************************************************************************/
typedef void (*cs_rreq_on_error_t)(uint8_t conn_handle,
                                   cs_rreq_error_t error,
                                   sl_status_t sc);

/**************************************************************************//**
 * @brief Callback invoked when a complete ranging result is ready.
 *
 * @note The result pointer is valid only for the duration of the callback.
 *       Call @ref cs_rreq_set_process_finished once the caller has consumed
 *       the result data to allow the RREQ module to reuse the buffer.
 *
 * @param[in] conn_handle     Connection handle.
 * @param[in] ranging_counter Ranging counter identifying the procedure.
 * @param[in] info            Procedure info.
 * @param[in] result          Pointer to the combined ranging result.
 *****************************************************************************/
typedef void (*cs_rreq_on_result_t)(uint8_t conn_handle,
                                    uint16_t ranging_counter,
                                    cs_rreq_procedure_info_t info,
                                    cs_rreq_result_t *result);

/// Collection type of event callbacks registered with 
/// @ref cs_rreq_set_event_callbacks
typedef struct {
  cs_rreq_create_complete_t on_create; ///< Called when a create operation 
                                       ///< has completed.
  cs_rreq_enable_complete_t on_enable; ///< Called when an enable/disable 
                                       ///< operation has completed.
  cs_rreq_on_error_t        on_error;  ///< Called when an error occurs.
} cs_rreq_event_callback_t;

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************//**
 * Set event callbacks for RREQ operations.
 *
 * @note All three callback pointers in @p cb must be non-NULL.
 *
 * @param[in] cb Callback structure containing create, enable, and error 
 *               handlers.
 * @return Status of the operation.
 * @retval SL_STATUS_OK           Callbacks set successfully.
 * @retval SL_STATUS_NULL_POINTER One or more callback pointers are NULL.
 *****************************************************************************/
sl_status_t cs_rreq_set_event_callbacks(cs_rreq_event_callback_t *cb);

/**************************************************************************//**
 * Set the ranging result callback.
 *
 * @note The callback is invoked once per completed CS procedure, after both
 *       initiator and reflector data have been collected.
 *
 * @param[in] on_result Callback to invoke with the completed ranging result.
 * @return Status of the operation.
 * @retval SL_STATUS_OK           Callback set successfully.
 * @retval SL_STATUS_NULL_POINTER @p on_result is NULL.
 *****************************************************************************/
sl_status_t cs_rreq_set_result_callback(cs_rreq_on_result_t on_result);

/**************************************************************************//**
 * Signal that the caller has finished processing a ranging result.
 *
 * @note Must be called after consuming data delivered through the result
 *       callback, so the RREQ module can reuse the internal ranging buffer.
 *
 * @param[in] conn_handle     Connection handle.
 * @param[in] ranging_counter Ranging counter of the processed result.
 * @return Status of the operation.
 * @retval SL_STATUS_OK                 Subevent buffer reset successfully.
 * @retval SL_STATUS_INVALID_HANDLE     @p conn_handle is invalid.
 * @retval SL_STATUS_INVALID_PARAMETER  @p ranging_counter is invalid.
 * @retval SL_STATUS_NOT_FOUND          No instance found for @p conn_handle.
 *****************************************************************************/
sl_status_t cs_rreq_set_process_finished(uint8_t conn_handle,
                                         uint16_t ranging_counter);

#ifdef __cplusplus
};
#endif

/** @} (end addtogroup cs_rreq) */
#endif // CS_RREQ_H
