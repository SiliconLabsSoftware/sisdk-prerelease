/***************************************************************************/ /**
 * @file
 * @brief APIs for the APS Link Key Authorization feature.
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

#ifndef SILABS_APS_LINK_KEY_AUTHORIZATION_H
#define SILABS_APS_LINK_KEY_AUTHORIZATION_H

/**
 * @defgroup aps-link-key-authorization APS Link Key Authorization
 * @ingroup component
 * @brief API and Callbacks for the APS Link Key Authorization Component
 *
 * This component enables a better granularity of cluster-specific APS link key security.
 *
 */

/**
 * @addtogroup aps-link-key-authorization
 * @{
 */

/**
 * @name API
 * @{
 */

#define sl_zigbee_af_aps_link_key_authorization_is_required_on_cluster(clusterId)        \
  /* If APS level security is enabled, the exempt list serves as a security disabler, */ \
  /* whereas, if security is disabled, the exempt list serves as a security enabler */   \
  (sl_zigbee_af_aps_link_key_authorization_is_enabled() != sl_zigbee_af_aps_link_key_authorization_is_cluster_exempted(clusterId))

/** @brief Get APS Link key authorization state.
 *
 * @return bool APS link key authorization state
 */
bool sl_zigbee_af_aps_link_key_authorization_is_enabled(void);

/** @brief APS link key authorization cluster exempted state.
 *
 * @param clusterID Ver.:Always
 *
 * @return bool exempted state
 */
bool sl_zigbee_af_aps_link_key_authorization_is_cluster_exempted(uint16_t clusterId);

/** @brief Exempted cluster list full state.
 *
 * @return bool cluster list full state
 *
 */
bool sl_zigbee_af_aps_link_key_authorization_is_exempt_cluster_list_full(void);

/** Get exempt cluster list count.
 *
 * @return uint8_t exempt cluster link count
 *
 */
uint8_t sl_zigbee_af_aps_link_key_authorization_exempt_cluster_list_count(void);

/** @brief Get exempt cluster list.
 *
 * @param exemptListCount Ver.:Always
 * @param exemptClusterList Ver.:Always
 *
 */
void sl_zigbee_af_aps_link_key_authorization_get_exempt_cluster_list(uint8_t *exemptListCount, uint16_t *exemptClusterList);

/** @brief Clear exempt cluster list.
 *
 */
void sl_zigbee_af_aps_link_key_authorization_clear_exempt_cluster_list(void);

/** @brief Enable/disable APS link ket authorization.
 *
 * @param enable enable value Ver.:Always
 *
 */
void sl_zigbee_af_aps_link_key_authorization_enable(bool enable);

/** @brief Add a cluster to the exempted cluster list.
 *
 * @param clusterId Ver.:Always
 *
 * @return sl_zigbee_staus_t status code
 *
 */
sl_status_t sl_zigbee_af_aps_link_key_authorization_add_cluster_exempted(uint16_t clusterId);

/** @brief Enable APS link key authorization with exempted cluster list.
 *
 * @param numberExemptClusters exempt cluster count Ver.:Always
 * @param exemptClusterList Ver.:Always
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_aps_link_key_authorization_enable_with_exempt_cluster_list(uint8_t numberExemptClusters, uint8_t *exemptClusterList);

/** @brief Disable link ket authorization with exempt cluster list.
 *
 * @param numberExemptClusters exempt cluster count Ver.:Always
 * @param exemptClusterList Ver.:Always
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_aps_link_key_authorization_disable_with_exempt_cluster_list(uint8_t numberExemptClusters, uint8_t *exemptClusterList);

/** @} */ // end of name APIs
/** @} */ // end of group aps-link-key-authorization

#endif // SILABS_APS_LINK_KEY_AUTHORIZATION_H
