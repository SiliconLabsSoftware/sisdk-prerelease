/***************************************************************************//**
 * @file
 * @brief Definitions for the DIDO Storage Linux plugin.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#ifndef _DIDO_STORAGE_LINUX_H_
#define _DIDO_STORAGE_LINUX_H_

/**
 * @defgroup dido-storage-linux DIDO Storage Linux
 * @ingroup component host
 * @brief API and Callbacks for the DIDO Storage Linux Component
 *
 * File storage module for DIDO reports, which uses a POSIX file system as the
 * underlying storage device and can therefore store any number of files.
 *
 */

/**
 * @addtogroup dido-storage-linux
 * @{
 */

typedef enum {
  SL_ZIGBEE_AF_DIDO_STORAGE_SUCCESS               = 0,
  SL_ZIGBEE_AF_DIDO_STORAGE_ERROR                 = 1,
} sl_zigbee_af_dido_storage_status_t;

/**
 * @name API
 * @{
 */

sl_zigbee_af_dido_storage_status_t sl_zigbee_af_dido_storage_write_report(uint8_t* data,
                                                                          uint32_t length);

/** @} */ // end of name API

/**
 * @name Callbacks
 * @{
 */

/**
 * @defgroup dido_storage_linux_cb Dido Storage Linux
 * @ingroup af_callback
 * @brief Callbacks for Dido Storage Linux Component
 *
 */

/**
 * @addtogroup dido_storage_linux_cb
 * @{
 */

/** @brief Dido Storage init callback.
 *
 */
void sl_zigbee_af_dido_storage_init_cb(void);
/** @} */ // dido_storage_linux_cb
/** @} */ // end of name Callbacks
/** @} */ // end of dido-storage-linux

#endif // #ifndef _DIDO_STORAGE_LINUX_H_
