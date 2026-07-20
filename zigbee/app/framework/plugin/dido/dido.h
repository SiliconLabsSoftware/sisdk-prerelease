/***************************************************************************//**
 * @file
 * @brief APIs and defines for the DIDO plugin.
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

#ifndef _DIDO_H_
#define _DIDO_H_

/**
 * @defgroup dido DIDO Server
 * @ingroup component cluster
 * @brief API and Callbacks for the DIDO Cluster Server Component
 *
 * This component manages the diagnostic information data object (DIDO) interface.
 * It wraps diagnostic data into a single buffer to be sent out for processing.
 *
 */

/**
 * @addtogroup dido
 * @{
 */

/**
 * @name API
 * @{
 */

/** @brief Send a debug report to the specified nodeId.
 *
 * @param debugReportId Ver.: always
 * @param nodeId Ver.: always
 * @param endpoint Ver.: always
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_dido_cluster_send_debug_report(uint8_t debugReportId,
                                                        sl_802154_short_addr_t nodeId,
                                                        uint8_t endpoint);

/** @} */ // end of name API
/** @} */ // end of dido

#endif  // #ifndef _DIDO_H_
