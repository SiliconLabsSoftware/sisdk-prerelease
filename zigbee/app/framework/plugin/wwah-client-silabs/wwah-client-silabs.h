/***************************************************************************//**
 * @file
 * @brief Definitions for the WWAH Client Silabs plugin.
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

#ifndef _WWAH_CLIENT_SILABS_H_
#define _WWAH_CLIENT_SILABS_H_

/**
 * @defgroup wwah-client-silabs Silicon Labs WWAH Client
 * @ingroup component cluster
 * @brief API and Callbacks for the WWAH Cluster Client Silicon Labs Component
 *
 * Silicon Labs proprietary Works With All Hubs (WWAH) client cluster.
 *
 */

/**
 * @addtogroup wwah-client-silabs
 * @{
 */

/**
 * @name Callbacks
 * @{
 */

/**
 * @defgroup wwah_client_silabs_cb WWAH Client Silicon Labs
 * @ingroup af_callback
 * @brief Callbacks for WWAH Client Silicon Labs Component
 *
 */

/**
 * @addtogroup wwah_client_silabs_cb
 * @{
 */

/** @brief WWAH cluster client init callback.
 *
 * @param endpoint end point
 *
 */
void sl_zigbee_af_sl_wwah_cluster_client_init_cb(uint8_t endpoint);

/** @brief WWAH cluster debug report query response callback.
 *
 * @param debugReportId debug report ID
 * @param debugReportData debug report data
 *
 */
bool sl_zigbee_af_sl_wwah_cluster_debug_report_query_response_cb(uint8_t debugReportId,
                                                                 uint8_t* debugReportData);

/** @} */ // end of wwah_client_silabs_cb
/** @} */ // end of name Callbacks
/** @} */ // end of wwah-client-silabs

#endif  // #ifndef _WWAH_CLIENT_SILABS_H_
