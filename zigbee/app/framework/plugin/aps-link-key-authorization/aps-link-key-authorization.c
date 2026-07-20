/***************************************************************************//**
 * @file
 * @brief Implementation of the APS Link Key Authorization feature, which sets
 *        APS encryption requirement when interfacing defined clusters.
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

#include "app/framework/include/af.h"

#include "aps-link-key-authorization.h"
#include "app/framework/util/common.h"
#include "sl_zigbee_token.h"
#include "stack/config/sl_zigbee_token_defines.h"
#include "sl_token_manager_api.h"
#include "aps-link-key-authorization-tokens.h"

#ifdef SL_ZIGBEE_SCRIPTED_TEST
  #include "app/framework/util/af-main.h"
  #include "app/framework/plugin/aps-link-key-authorization/aps-link-key-authorization-test.h"
#else // SL_ZIGBEE_SCRIPTED_TEST
  #ifdef SL_CATALOG_ZIGBEE_SIMULATION_PRESENT
    #include "aps-link-key-authorization-zigbee-simulation-config.h"
  #else // SL_CATALOG_ZIGBEE_SIMULATION_PRESENT
    #include "aps-link-key-authorization-config.h"
  #endif // SL_CATALOG_ZIGBEE_SIMULATION_PRESENT
#endif // SL_ZIGBEE_SCRIPTED_TEST

static bool apsLinkKeyAuthorizationEnabled = false;
static uint8_t exemptClusterCount = 0;

// -----------------------------------------------------------------------------
// Helper functions

// -----------------------------------------------------------------------------
// Public functions

bool sl_zigbee_af_aps_link_key_authorization_is_enabled(void)
{
  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_APS_LINK_KEY_AUTH_ENABLED, (void *)&apsLinkKeyAuthorizationEnabled, sizeof(uint8_t));
  SL_ZIGBEE_TEST_ASSERT(status == SL_STATUS_OK);
  UNUSED_VAR(status);
  return apsLinkKeyAuthorizationEnabled;
}

bool sl_zigbee_af_aps_link_key_authorization_is_exempt_cluster_list_full(void)
{
  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT, (void *)&exemptClusterCount, sizeof(uint8_t));
  SL_ZIGBEE_TEST_ASSERT(status == SL_STATUS_OK);
  UNUSED_VAR(status);
  return (exemptClusterCount >= SL_ZIGBEE_AF_PLUGIN_APS_LINK_KEY_AUTHORIZATION_MAX_EXEMPT_CLUSTERS);
}

uint8_t sl_zigbee_af_aps_link_key_authorization_exempt_cluster_list_count(void)
{
  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT, (void *)&exemptClusterCount, sizeof(uint8_t));
  SL_ZIGBEE_TEST_ASSERT(status == SL_STATUS_OK);
  UNUSED_VAR(status);
  return exemptClusterCount;
}

void sl_zigbee_af_aps_link_key_authorization_get_exempt_cluster_list(uint8_t *exemptListCount, uint16_t *exemptClusterList)
{
  uint16_t idx;
  uint16_t tmpCluster;

  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT, (void *)&exemptClusterCount, sizeof(uint8_t));
  SL_ZIGBEE_TEST_ASSERT(status == SL_STATUS_OK);
  UNUSED_VAR(status);
  *exemptListCount = exemptClusterCount;

  for (idx = 0; idx < *exemptListCount; idx++) {
    sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST + idx, (void *)&tmpCluster, sizeof(uint16_t));
    SL_ZIGBEE_TEST_ASSERT(status == SL_STATUS_OK);
    UNUSED_VAR(status);
    exemptClusterList[idx] = tmpCluster;
  }
}

void sl_zigbee_af_aps_link_key_authorization_clear_exempt_cluster_list(void)
{
  exemptClusterCount = 0;
  sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT, (void *)&exemptClusterCount, sizeof(uint8_t));
  SL_ZIGBEE_TEST_ASSERT(status == SL_STATUS_OK);
  UNUSED_VAR(status);
}

void sl_zigbee_af_aps_link_key_authorization_enable(bool enable)
{
  apsLinkKeyAuthorizationEnabled = enable;
  sl_status_t status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_APS_LINK_KEY_AUTH_ENABLED, (void *)&apsLinkKeyAuthorizationEnabled, sizeof(uint8_t));
  SL_ZIGBEE_TEST_ASSERT(status == SL_STATUS_OK);
  UNUSED_VAR(status);
}

bool sl_zigbee_af_aps_link_key_authorization_is_cluster_exempted(uint16_t clusterId)
{
  uint16_t idx = 0;
  uint16_t tmpCluster;

  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT, (void *)&exemptClusterCount, sizeof(uint8_t));
  SL_ZIGBEE_TEST_ASSERT(status == SL_STATUS_OK);
  UNUSED_VAR(status);

  while (idx < exemptClusterCount) {
    sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST + idx, (void *)&tmpCluster, sizeof(uint16_t));
    SL_ZIGBEE_TEST_ASSERT(status == SL_STATUS_OK);
    UNUSED_VAR(status);
    if (clusterId == tmpCluster) {
      return true;
    }
    idx++;
  }

  return false;
}

sl_status_t sl_zigbee_af_aps_link_key_authorization_add_cluster_exempted(uint16_t clusterId)
{
  // Do not add twice
  if (sl_zigbee_af_aps_link_key_authorization_is_cluster_exempted(clusterId)) {
    return SL_STATUS_OK;
  }

  // Have more space?
  if (sl_zigbee_af_aps_link_key_authorization_is_exempt_cluster_list_full()) {
    return SL_STATUS_FULL;
  }

  // Add clusterId to the end of the list and increment list count
  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT, (void *)&exemptClusterCount, sizeof(uint8_t));
  SL_ZIGBEE_TEST_ASSERT(status == SL_STATUS_OK);
  UNUSED_VAR(status);
  status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST + exemptClusterCount, (void *)&clusterId, sizeof(clusterId));
  exemptClusterCount++;
  status = slx_zigbee_token_manager_set_data(COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT, (void*)&exemptClusterCount, sizeof(uint8_t));
  SL_ZIGBEE_TEST_ASSERT(status == SL_STATUS_OK);
  UNUSED_VAR(status);

  return SL_STATUS_OK;
}

sl_status_t sl_zigbee_af_aps_link_key_authorization_enable_with_exempt_cluster_list(
  uint8_t numberExemptClusters,
  uint8_t* exemptClusterList)
{
  sl_status_t retVal;

  // Clear full exempt list
  sl_zigbee_af_aps_link_key_authorization_clear_exempt_cluster_list();

  // Enable APS link key authorization for all clusters...
  sl_zigbee_af_aps_link_key_authorization_enable(true);

  // ... but for the ones on the exempt list
  while (numberExemptClusters) {
    retVal = sl_zigbee_af_aps_link_key_authorization_add_cluster_exempted(
      HIGH_LOW_TO_INT(exemptClusterList[2 * numberExemptClusters - 1], exemptClusterList[2 * numberExemptClusters - 2]));

    if (retVal != SL_STATUS_OK) {
      return retVal;
    }

    numberExemptClusters--;
  }

  return SL_STATUS_OK;
}

sl_status_t sl_zigbee_af_aps_link_key_authorization_token_init(void)
{
  sl_status_t status = SL_STATUS_OK;
  uint8_t tokAPSLinkKeyAuthEnabledDefault = TOKEN_APS_LINK_KEY_AUTH_ENABLED_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_APS_LINK_KEY_AUTH_ENABLED, &tokAPSLinkKeyAuthEnabledDefault, sizeof(uint8_t));
  uint8_t tokAPSLinkKeyAuthExemptyListCntDefault = TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT_DEFAULT;
  status = sl_zigbee_initialize_basic_token(COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_CNT, &tokAPSLinkKeyAuthExemptyListCntDefault, sizeof(uint8_t));
  uint16_t tokAPSLinkKeyAuthExemptyListDefault = TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST_DEFAULT;
  status = sl_zigbee_initialize_index_token(COMMON_TOKEN_APS_LINK_KEY_AUTH_EXEMPT_LIST, &tokAPSLinkKeyAuthExemptyListDefault, sizeof(uint16_t), SL_ZIGBEE_AF_PLUGIN_APS_LINK_KEY_AUTHORIZATION_MAX_EXEMPT_CLUSTERS);
  return status;
}

void sl_zigbee_af_aps_link_key_authorization_init(uint8_t init_level)
{
  (void)init_level;
  assert(SL_STATUS_OK == sl_zigbee_af_aps_link_key_authorization_token_init());
}

sl_status_t sl_zigbee_af_aps_link_key_authorization_disable_with_exempt_cluster_list(
  uint8_t numberExemptClusters,
  uint8_t* exemptClusterList)
{
  sl_status_t retVal;

  // Clear full exempt list
  sl_zigbee_af_aps_link_key_authorization_clear_exempt_cluster_list();

  // Disable APS link key authorization for all clusters...
  sl_zigbee_af_aps_link_key_authorization_enable(false);

  // ... but for the ones on the exempt list
  while (numberExemptClusters) {
    retVal = sl_zigbee_af_aps_link_key_authorization_add_cluster_exempted(
      HIGH_LOW_TO_INT(exemptClusterList[2 * numberExemptClusters - 1], exemptClusterList[2 * numberExemptClusters - 2]));

    if (retVal != SL_STATUS_OK) {
      return SL_STATUS_FAIL;
    }

    numberExemptClusters--;
  }

  return SL_STATUS_OK;
}

// Implement Cluster Security for APS Link Key Authorization
bool sl_zigbee_af_cluster_security_custom_cb(sl_zigbee_af_profile_id_t profileId,
                                             sl_zigbee_af_cluster_id_t clusterId,
                                             bool incoming,
                                             uint8_t commandId)
{
  UNUSED_VAR(profileId);
  UNUSED_VAR(commandId);
  UNUSED_VAR(incoming);

  return (sl_zigbee_af_aps_link_key_authorization_is_required_on_cluster(clusterId));
}
