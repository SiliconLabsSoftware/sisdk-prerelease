/***************************************************************************//**
 * @file app.c
 * @brief Callbacks implementation and application specific code.
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
#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif

#ifdef SL_CATALOG_ZIGBEE_NETWORK_TEST_PRESENT
#include "network_test_config.h"
#endif // SL_CATALOG_ZIGBEE_NETWORK_TEST_PRESENT

/* Join-on-boot is for product Empty only. UC simulation sets
 * LARGE_NETWORK_TESTING=1 and links zigbee_network_test, which already
 * defines sl_zigbee_af_main_init_cb (same pattern as zigbee_z3_light). */
#if (LARGE_NETWORK_TESTING == 0)
#include "network-steering.h"
#include "zigbee_soc_empty_freertos_config.h"

#define ZIGBEE_STARTUP_DELAY_MS  40
#define STEERING_RETRY_DELAY_MS  5000

static sl_zigbee_af_event_t commissioning_event;

static bool network_is_joined(sl_zigbee_network_status_t state)
{
  return (state == SL_ZIGBEE_JOINED_NETWORK
          || state == SL_ZIGBEE_JOINED_NETWORK_NO_PARENT);
}

static void schedule_steering_retry(void)
{
  if (!network_is_joined(sl_zigbee_af_network_state())) {
    sl_zigbee_af_event_set_delay_ms(&commissioning_event, STEERING_RETRY_DELAY_MS);
  }
}

//---------------
// Event handlers

static void commissioning_event_handler(sl_zigbee_af_event_t *event)
{
  (void)event;
#if SL_ZIGBEE_EMPTY_JOIN_ON_BOOT
  if (network_is_joined(sl_zigbee_af_network_state())) {
    (void)sl_zigbee_leave_network(SL_ZIGBEE_LEAVE_NWK_WITH_NO_OPTION);
    /* Steering starts from sl_zigbee_af_stack_status_cb on NETWORK_DOWN. */
    return;
  }
#endif

  if (!network_is_joined(sl_zigbee_af_network_state())) {
    sl_zigbee_af_network_steering_autostart();
  }
}

//----------------------
// Implemented Callbacks

/** @brief Init
 * Application init function
 */
void sl_zigbee_af_main_init_cb(void)
{
  sl_zigbee_af_event_init(&commissioning_event, commissioning_event_handler);
  sl_zigbee_af_event_set_delay_ms(&commissioning_event, ZIGBEE_STARTUP_DELAY_MS);
}

/** @brief Stack Status
 * Leave is asynchronous; start steering once the network is down.
 */
void sl_zigbee_af_stack_status_cb(sl_status_t status)
{
  if (status == SL_STATUS_NETWORK_DOWN) {
    sl_zigbee_af_network_steering_autostart();
  }
}

/** @brief Complete network steering.
 *
 * This callback is fired when the Network Steering plugin is complete.
 *
 * @param status On success this will be set to SL_STATUS_OK to indicate a
 * network was joined successfully. On failure this will be the status code of
 * the last join or scan attempt. Ver.: always
 *
 * @param totalBeacons The total number of 802.15.4 beacons that were heard,
 * including beacons from different devices with the same PAN ID. Ver.: always
 * @param joinAttempts The number of join attempts that were made to get onto
 * an open Zigbee network. Ver.: always
 *
 * @param finalState The finishing state of the network steering process. From
 * this, one is able to tell on which channel mask and with which key the
 * process was complete. Ver.: always
 */
void sl_zigbee_af_network_steering_complete_cb(sl_status_t status,
                                               uint8_t totalBeacons,
                                               uint8_t joinAttempts,
                                               uint8_t finalState)
{
  (void)totalBeacons;
  (void)joinAttempts;
  (void)finalState;
  if (status != SL_STATUS_OK) {
    schedule_steering_retry();
  }
}
#endif // (LARGE_NETWORK_TESTING == 0)

/** @brief
 *
 * Application framework equivalent of ::sl_zigbee_radio_needs_calibrating_handler
 */
void sl_zigbee_af_radio_needs_calibrating_cb(void)
{
  sl_mac_calibrate_current_channel();
}
