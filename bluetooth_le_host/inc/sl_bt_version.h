/***************************************************************************//**
 * @brief Bluetooth stack version definition
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_BT_VERSION_H
#define SL_BT_VERSION_H

/***************************************************************************//**
 * @addtogroup sl_bt_version Bluetooth SDK version
 * @brief Bluetooth SDK version information
 * @{
 */

/**
 * @brief The major number of Bluetooth SDK version
 *
 * An increment indicates incompatible Bluetooth API changes.
 */
#define SL_BT_VERSION_MAJOR 11

/**
 * @brief The minor number of Bluetooth SDK version
 *
 * An increment indicates new backwards compatible functionalities.
 */
#define SL_BT_VERSION_MINOR 0

/**
 * @brief The patch number of Bluetooth SDK version
 *
 * An increment indicates backwards compatible bug fixes.
 */
#define SL_BT_VERSION_PATCH 0

/**
 * @brief The hash value of the build the Bluetooth SDK was created from
 */
#define SL_BT_VERSION_HASH {0xee,0xd9,0x08,0x3f,0x29,0x45,0x0b,0xd2,0x82,0x00,0xec,0xdd,0x88,0x45,0xeb,0x01,0x54,0x9e,0x9f,0x7c}

/** @} */ // end addtogroup sl_bt_version

#endif
