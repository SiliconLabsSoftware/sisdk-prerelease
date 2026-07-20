/***************************************************************************//**
 * @file
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
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

/** @brief Write Attribute Enabled Callback
 *
 * This callback is called when the enabled backoff parameter is changed.
 *
 * @param enabled   Ver.: always
 */
WEAK(void sl_zigbee_af_wwah_app_event_retry_manager_write_attribute_enabled(uint8_t enabled))
{
  UNUSED_VAR(enabled);
}

/** @brief Write Attribute Queue Size Callback
 *
 * This callback is called when the app event retry queue size is being reported.
 *
 * @param queueSize   Ver.: always
 */
WEAK(void sl_zigbee_af_wwah_app_event_retry_manager_write_attribute_queue_size(uint8_t queueSize))
{
  UNUSED_VAR(queueSize);
}

/** @brief WWAH App Event Retry Manager Config Backoff Parameters Callback
 *
 * This callback is called when the app event retry manager (re)configures the backoff parameters of the critical message queue provider plugin.
 *
 * @param firstBackoffTimeSeconds Ver.: always
 * @param backoffSeqCommonRatio   Ver.: always
 * @param maxBackoffTimeSeconds   Ver.: always
 * @param maxRedeliveryAttempts   Ver.: always
 */
//TODO: parameters
WEAK(sl_status_t sl_zigbee_af_wwah_app_event_retry_manager_config_backoff_params_cb(uint8_t firstBackoffTimeSeconds,
                                                                                    uint8_t backoffSeqCommonRatio,
                                                                                    uint32_t maxBackoffTimeSeconds,
                                                                                    uint8_t maxRedeliveryAttempts))
{
  UNUSED_VAR(firstBackoffTimeSeconds);
  UNUSED_VAR(backoffSeqCommonRatio);
  UNUSED_VAR(maxBackoffTimeSeconds);
  UNUSED_VAR(maxRedeliveryAttempts);

  return SL_STATUS_NOT_FOUND;
}

/** @brief WWAH App Event Retry Manager Set Backoff Parameters to Default
 *
 * This callback is called when the app event retry manager is about to set the backoff parameters of the critical message queue provider plugin back to their default.
 *
 */
WEAK(void sl_zigbee_af_wwah_app_event_retry_manager_set_backoff_params_to_default(void))
{
}
