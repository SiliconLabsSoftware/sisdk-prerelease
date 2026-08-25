/***************************************************************************//**
 * @brief BGAPI Service version definition
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_BGAPI_SERVICE_VERSION_H
#define SL_BGAPI_SERVICE_VERSION_H

/***************************************************************************//**
 * @addtogroup sl_bgapi_service_version BGAPI Service version
 * @brief BGAPI Service version information
 * @{
 */

/**
 * @brief The major number of BGAPI Service version
 *
 * An increment indicates incompatible API changes.
 */
#define SL_BGAPI_SERVICE_VERSION_MAJOR 1

/**
 * @brief The minor number of BGAPI Service version
 *
 * An increment indicates new backwards-compatible functionalities.
 */
#define SL_BGAPI_SERVICE_VERSION_MINOR 0

/**
 * @brief The patch number of BGAPI Service version
 *
 * An increment indicates backwards-compatible bug fixes.
 */
#define SL_BGAPI_SERVICE_VERSION_PATCH 0

/**
 * @brief The hash value of the build the BGAPI Service was created from
 */
#define SL_BGAPI_SERVICE_VERSION_HASH {0xfa,0xe0,0x32,0x2c,0x3e,0xb7,0x05,0x15,0x12,0x80,0x30,0x08,0xae,0x49,0x82,0x20,0xc3,0x7b,0x9a,0xd9}

/** @} */ // end addtogroup sl_bgapi_service_version

#endif // SL_BGAPI_SERVICE_VERSION_H
