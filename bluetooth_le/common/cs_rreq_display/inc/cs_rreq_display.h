/***************************************************************************//**
 * @file
 * @brief CS RREQ display API
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
#ifndef CS_RREQ_DISPLAY_H
#define CS_RREQ_DISPLAY_H

/***********************************************************************************************//**
 * @addtogroup cs_rreq_display
 * @{
 **************************************************************************************************/

// -----------------------------------------------------------------------------
// Includes

#include "sl_bt_api.h"
#include "sl_status.h"
#include "sl_rtl_service.h"

#ifdef __cplusplus
extern "C"
{
#endif

// -----------------------------------------------------------------------------
// Macros

#define CS_RREQ_DISPLAY_MODE_TEXT                  "Mode: "
#define CS_RREQ_DISPLAY_DISTANCE_TEXT              "Distance (m):"
#define CS_RREQ_DISPLAY_INSTANCE_NUM               "  1    2    3    4"
#define CS_RREQ_DISPLAY_STATUS_TEXT                "Status:"
#define CS_RREQ_DISPLAY_RSSI_DISTANCE_TEXT         "Distance [RSSI]:"
#define CS_RREQ_DISPLAY_RAW_DISTANCE_TEXT          "Raw distance [m]:"
#define CS_RREQ_DISPLAY_PERCENTAGE_TEXT            "Percentage (%):"
#define CS_RREQ_DISPLAY_LIKELINESS_TEXT            "Likeliness (%):"
#define CS_RREQ_DISPLAY_BER_TEXT                   "Bit error rate (%):"

#define CS_RREQ_DISPLAY_STATE_ESTIMATE_TEXT        "  EST"
#define CS_RREQ_DISPLAY_STATE_CONNECTED_TEXT       " CONN"
#define CS_RREQ_DISPLAY_STATE_SCANNING_TEXT        " SCAN"
#define CS_RREQ_DISPLAY_STATE_INITIALIZED_TEXT     " INIT"
#define CS_RREQ_DISPLAY_STATE_UNINITIALIZED_TEXT   "     "
#define CS_RREQ_DISPLAY_STATE_SPOOFED_TEXT         " SP!"

#define CS_RREQ_DISPLAY_MODE_RTT_TEXT              "RTT"
#define CS_RREQ_DISPLAY_MODE_PBR_TEXT              "PBR"
#define CS_RREQ_DISPLAY_AMODE_STATIONARY_OBJ_TEXT  " stationary "
#define CS_RREQ_DISPLAY_AMODE_MOVING_OBJ_TEXT      " moving     "
#define CS_RREQ_DISPLAY_AMODE_MOVING_OBJ_FAST_TEXT " moving fast"

// -----------------------------------------------------------------------------
// Enums, structs, typedefs

/// Display status
typedef enum {
  CS_RREQ_DISPLAY_STATUS_CONNECTED,     ///< Connected to a peer device.
  CS_RREQ_DISPLAY_STATUS_SCANNING,      ///< Scanning for peer devices.
  CS_RREQ_DISPLAY_STATUS_ESTIMATE,      ///< A distance estimate is available.
  CS_RREQ_DISPLAY_STATUS_INITIALIZED,   ///< Display instance is initialized.
  CS_RREQ_DISPLAY_STATUS_UNINITIALIZED, ///< Display instance is not initialized.
  CS_RREQ_DISPLAY_STATUS_SPOOFED        ///< Spoofing has been detected.
} cs_rreq_display_status_t;

/// Display content
typedef struct {
  sl_bt_cs_mode_t mode;       ///< CS measurement mode (e.g. RTT or PBR).
  uint8_t algo_mode;          ///< RTL algorithm / object-tracking mode.
  float distance;             ///< Estimated distance [m].
  float rssi_distance;        ///< RSSI-based distance estimate [m].
  float progress_percentage;  ///< Measurement progress [%].
  float likeliness;           ///< Likeliness of the estimate [%].
  float bit_error_rate;       ///< Bit error rate [%].
  float raw_distance;         ///< Raw (unfiltered) distance [m].
} cs_rreq_display_content_t;

/// Display instance
typedef struct {
  uint8_t connection_id;             ///< Connection handle of the instance.
  cs_rreq_display_status_t status;   ///< Current status of the instance.
  cs_rreq_display_content_t content; ///< Measurement data shown for the instance.
} cs_rreq_display_instance_t;

// -----------------------------------------------------------------------------
// Function declarations

/**************************************************************************//**
 * Create new display instance
 * @param[in] conn_handle Connection handle
 * @return SL_STATUS_OK on success
 *****************************************************************************/
sl_status_t cs_rreq_display_create_instance(uint8_t conn_handle);

/**************************************************************************//**
 * Delete display instance
 * The deleted instance will start scanning
 * @param[in] conn_handle Connection handle
 * @return SL_STATUS_OK on success
 *****************************************************************************/
sl_status_t cs_rreq_display_delete_instance(uint8_t conn_handle);

/**************************************************************************//**
 * Set the measurement mode and object tracking mode to display.
 * @param[in] mode CS Measurement mode
 * @param[in] algo_mode Algorithm mode
 *****************************************************************************/
void cs_rreq_display_set_measurement_mode(sl_bt_cs_mode_t mode,
                                          uint8_t algo_mode);

/**************************************************************************//**
 * Insert new floating point value to the specified row on the display
 * @param[in] new_value New value to display
 * @param[in] row Row number to display the value on
 * @param[in] conn_handle Connection handle
 *****************************************************************************/
void cs_rreq_display_insert_new_float_value(float new_value,
                                            uint8_t row,
                                            uint8_t conn_handle);

/**************************************************************************//**
 * Insert new distance value to the display
 * @param[in] new_value New distance value to display
 * @param[in] conn_handle Connection handle
 * @param[in] percentage Percentage of the distance
 * @param[in] algo_mode Algorithm mode
 *****************************************************************************/
void cs_rreq_display_insert_new_distance_value(float new_value,
                                               uint8_t conn_handle,
                                               float percentage,
                                               sl_rtl_cs_algo_mode algo_mode);

/**************************************************************************//**
 * Insert new status to the display
 * @param[in] new_status New status to display
 * @param[in] instance_num Instance number to display the status on
 * @return SL_STATUS_OK on success
 *****************************************************************************/
sl_status_t cs_rreq_display_insert_new_status(uint8_t new_status,
                                              uint8_t instance_num);

/**************************************************************************//**
 * Start scanning and show it on the display.
 *****************************************************************************/
void cs_rreq_display_start_scanning(void);

/**************************************************************************//**
 * Update the data for a specified display instance
 * @param[in] instance_num Instance number to update
 * @param[in] conn_handle Connection handle
 * @param[in] status Status of the display instance
 * @param[in] distance Distance value of the display instance
 * @param[in] rssi_distance RSSI distance value of the display instance
 * @param[in] likeliness Likeliness value of the display instance
 * @param[in] bit_error_rate Bit error rate value of the display instance
 * @param[in] raw_distance Raw distance value of the display instance
 * @param[in] progress_percentage Progress percentage of the display instance
 * @param[in] algo_mode Algorithm mode of the display instance
 * @param[in] cs_mode CS mode of the display instance
 *****************************************************************************/
void cs_rreq_display_update_data(uint8_t instance_num,
                                 uint8_t conn_handle,
                                 uint8_t status,
                                 float distance,
                                 float rssi_distance,
                                 float likeliness,
                                 float bit_error_rate,
                                 float raw_distance,
                                 float progress_percentage,
                                 uint8_t algo_mode,
                                 sl_bt_cs_mode_t cs_mode);

/**************************************************************************//**
 * Update instance values on the display.
 * @param[in] instance_num Instance number to update
 *****************************************************************************/
void cs_rreq_display_update_instance(uint8_t instance_num);

// -----------------------------------------------------------------------------
// Event / callback declarations

/**************************************************************************//**
 * Bluetooth stack event handler.
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void cs_rreq_display_on_event(sl_bt_msg_t *evt);

#ifdef __cplusplus
}
#endif

/** @} (end addtogroup cs_rreq_display) */
#endif // CS_RREQ_DISPLAY_H
