/***************************************************************************//**
 * @file
 * @brief CS RREQ API
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

#ifndef CS_RREQ_API_H
#define CS_RREQ_API_H

#include "sl_status.h"
#include "sl_common.h"
#include "cs_ras_common.h"

/***************************************************************************//**
 * @addtogroup cs_rreq
 * @{
 ******************************************************************************/

/// Ranging Requester (RREQ) configuration type
SL_PACK_START(1)
typedef struct {
  uint8_t real_time_mode;                 ///< Real-time (1) or on-demand (0) RAS mode
  cs_ras_client_config_t ras_config;      ///< RAS client CCCD configuration override
  uint32_t service;                       ///< Ranging Service GATT handle
  cs_ras_gattdb_handles_t gattdb_handles; ///< Ranging characteristic GATT database handles
  uint16_t mtu;                           ///< ATT MTU negotiated for the connection
  uint8_t antenna_config;                 ///< CS tone antenna configuration index (0-7)
  uint8_t is_initiator;                   ///< Role: 1 for Initiator, 0 for Reflector
} SL_ATTRIBUTE_PACKED cs_rreq_create_config_t;
SL_PACK_END()


/// RREQ error type
SL_ENUM(cs_rreq_error_t) {
  CS_RREQ_ERROR_STATE_MACHINE_FAILED,                    ///< Internal state machine transition failed
  CS_RREQ_ERROR_RAS_CLIENT_INIT_FEATURE_NOT_SUPPORTED,   ///< RAS server does not support a required feature
  CS_RREQ_ERROR_RAS_CLIENT_INIT_FAILED,                  ///< RAS client initialization failed
  CS_RREQ_ERROR_RAS_CLIENT_CONFIG_FAILED,                ///< RAS client CCCD configuration failed
  CS_RREQ_ERROR_RAS_CLIENT_MODE_CHANGE_FAILED,           ///< RAS client mode selection failed
  CS_RREQ_ERROR_RAS_CLIENT_REALTIME_RECEIVE_FAILED,      ///< Real-time data reception failed
  CS_RREQ_ERROR_RAS_CLIENT_DATA_RECEPTION_FINISH_FAILED, ///< Data reception finish handling failed
  CS_RREQ_ERROR_RAS_CLIENT_RANGING_DATA_READY_FAILED,    ///< Ranging-data-ready notification handling failed
  CS_RREQ_ERROR_RAS_CLIENT_GET_RANGING_DATA_FAILED,      ///< On-demand ranging data retrieval failed
  CS_RREQ_ERROR_RAS_CLIENT_RANGING_DATA_OVERWRITTEN_FAILED, ///< Ranging data overwritten handling failed
  CS_RREQ_ERROR_RAS_CLIENT_ABORT_FINISHED_FAILED,        ///< Abort control point operation failed
  CS_RREQ_ERROR_RAS_CLIENT_ACK_FAILED,                   ///< Acknowledgement control point operation failed
  CS_RREQ_ERROR_RAS_CLIENT_REQUEST_LOST_SEGMENTS_FAILED, ///< Lost segment retrieval failed
  CS_RREQ_ERROR_RAS_CLIENT_ON_ACK_FINISHED_FAILED,       ///< Ack-finished callback handling failed
  CS_RREQ_ERROR_RAS_CLIENT_TIMEOUT                       ///< Data reception timeout occurred
};

/// RREQ enable type
SL_ENUM(cs_rreq_enable_t) {
  CS_RREQ_DISABLE = 0, ///< Disable ranging
  CS_RREQ_ENABLE  = 1  ///< Enable ranging
};

#ifdef __cplusplus
extern "C" {
#endif


/**************************************************************************//**
 * Create a Ranging Requester instance for a connection.
 *
 * @note Creates the underlying RAS client and starts the initialization
 *       sequence. The @ref cs_rreq_create_complete_t callback is invoked
 *       when initialization has completed successfully or has failed.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] config      Pointer to the RREQ creation configuration.
 * @return Status of the operation.
 * @retval SL_STATUS_OK                  Instance created successfully.
 * @retval SL_STATUS_NULL_POINTER        @p config is NULL.
 * @retval SL_STATUS_INVALID_HANDLE      @p conn_handle or service handle
 *                                       is invalid.
 * @retval SL_STATUS_NOT_SUPPORTED       On-demand mode requested but not
 *                                       compiled in.
 * @retval SL_STATUS_ALREADY_INITIALIZED An RREQ instance already exists.
 * @retval SL_STATUS_NO_MORE_RESOURCE    Maximum number of instances reached.
 *****************************************************************************/
sl_status_t cs_rreq_create(uint8_t conn_handle,
                           cs_rreq_create_config_t *config);

/**************************************************************************//**
 * Enable or disable ranging for a connection.
 *
 * @note On the first enable call, the RAS server features are read before
 *       the ranging procedure is started. Subsequent enable calls skip the
 *       feature-read step. The @ref cs_rreq_enable_complete_t callback is
 *       invoked when the operation has completed.
 *
 * @param[in] conn_handle Connection handle.
 * @param[in] enable      @ref CS_RREQ_ENABLE to start ranging,
 *                        @ref CS_RREQ_DISABLE to stop.
 * @return Status of the operation.
 * @retval SL_STATUS_OK              Enable/disable operation started.
 * @retval SL_STATUS_INVALID_HANDLE  @p conn_handle is invalid.
 * @retval SL_STATUS_NOT_FOUND       No instance found for @p conn_handle.
 * @retval SL_STATUS_INVALID_STATE   Instance is not in the expected state.
 *****************************************************************************/
sl_status_t cs_rreq_enable(uint8_t conn_handle, cs_rreq_enable_t enable);

/**************************************************************************//**
 * Remove the Ranging Requester instance for a connection.
 *
 * @note Releases all resources associated with the instance, including the
 *       RAS client. Any pending callbacks will not be invoked after removal.
 *
 * @param[in] conn_handle Connection handle.
 * @return Status of the operation.
 * @retval SL_STATUS_OK        Instance removed successfully.
 * @retval SL_STATUS_NOT_FOUND No instance found for @p conn_handle.
 *****************************************************************************/
sl_status_t cs_rreq_remove(uint8_t conn_handle);

#ifdef __cplusplus
};
#endif

/** @} (end addtogroup cs_rreq) */

#endif // CS_RREQ_API_H
