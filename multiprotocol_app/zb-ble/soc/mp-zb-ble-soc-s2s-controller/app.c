/***************************************************************************//**
 * @file
 * @brief App code related to the Sleepy-to-Sleepy controller device.
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

// This callback file is created for your convenience. You may add application
// code to this file. If you regenerate this file over a previous version, the
// previous version will be overwritten and any code you have added will be
// lost.
#include PLATFORM_HEADER
#include "hal.h"
#include "sl_zigbee.h"
#include "app/framework/include/af.h"
#ifdef SL_CATALOG_ZIGBEE_DEBUG_PRINT_PRESENT
#include "sl_zigbee_debug_print.h"
#endif // SL_CATALOG_ZIGBEE_DEBUG_PRINT_PRESENT
#include "app/framework/util/af-main.h"

#include "app/framework/plugin/reporting/reporting.h"
#include "lower-mac.h"

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif
#include "sl_ble_event_handler.h"
#include "sl_zigbee_sleepy_to_sleepy_cli.h"

#ifdef SL_CATALOG_ZIGBEE_DISPLAY_PRESENT
#include "sl_dmp_ui.h"
#else
#include "sl_dmp_ui_stub.h"
#endif // SL_CATALOG_ZIGBEE_DISPLAY_PRESENT

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT)
  #define BUTTON0         0
  #define BUTTON1         1

  #include "sl_simple_button.h"
  #include "sl_simple_button_instances.h"

static uint8_t lastButton;
static bool longPress = false;
static bool leavingNwk = false;

sl_zigbee_af_event_t button_event;
  #define buttonEvent             (&button_event)
#endif //SL_CATALOG_SIMPLE_BUTTON_PRESENT

#if defined (SL_CATALOG_LED0_PRESENT)
  #include "sl_simple_led_instances.h"
  #define LED0     (&sl_led_led0)
  #define LED1     (&sl_led_led1)
  #define led_turn_on(led) sl_led_turn_on(led)
  #define led_turn_off(led) sl_led_turn_off(led)
#else // !SL_CATALOG_LED0_PRESENT
  #define led_turn_on(led)
  #define led_turn_off(led)
#endif

#define SOURCE_ADDRESS_LEN 8
#define BUTTON_LONG_PRESS_TIME_MSEC    3000

#define S2S_DEFAULT_GROUP_ID         0xA1B2
#define S2S_CLUSTER_IDS_FOR_BINDING  { ZCL_BASIC_CLUSTER_ID,         \
                                       ZCL_ON_OFF_CLUSTER_ID,        \
                                       ZCL_LEVEL_CONTROL_CLUSTER_ID, \
                                       ZCL_SHADE_CONFIG_CLUSTER_ID }

static uint8_t SwitchEUI[SOURCE_ADDRESS_LEN];
static bool identifying = false;
static volatile bool okToSleep = true;
static bool multicast_binding_created = false;

void sl_zigbee_af_identify_start_feedback_cb(uint8_t endpoint,
                                             uint16_t identifyTime)
{
  (void)endpoint;
  if (identifyTime > 0) {
    identifying = true;
    sl_zigbee_af_app_println("Start Identifying for %dS", identifyTime);
    sl_dmp_ui_zigbee_permit_join(true);
    sl_zigbee_af_set_default_poll_control_cb(SL_ZIGBEE_AF_SHORT_POLL);  // Use short poll while identifying.
  }
}

void sl_zigbee_af_identify_stop_feedback_cb(uint8_t endpoint)
{
  (void)endpoint;
  if (identifying) {
    identifying = false;
    sl_zigbee_af_app_println("Stop Identifying");
    sl_dmp_ui_zigbee_permit_join(false);
    sl_zigbee_af_set_default_poll_control_cb(SL_ZIGBEE_AF_LONG_POLL); // Revert to long poll when we stop identifying.
  }
}

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT)
void buttonEventHandler(sl_zigbee_af_event_t * event)
{
  (void)event;
  sl_status_t status;
  sl_zigbee_af_event_set_inactive(buttonEvent);

  if (!leavingNwk) { // Ignore button1 events while leaving.
    if (longPress) {
      if (lastButton == BUTTON0) {
        okToSleep = !okToSleep;
      } else {
        leavingNwk = true;
        sl_zigbee_af_app_println("Button- Leave Nwk");
        sl_zigbee_leave_network(SL_ZIGBEE_LEAVE_NWK_WITH_NO_OPTION);
        sl_zigbee_clear_binding_table();
      }
    } else {
      sl_zigbee_af_fill_external_buffer((ZCL_CLUSTER_SPECIFIC_COMMAND
                                         | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER),
                                        ZCL_ON_OFF_CLUSTER_ID,
                                        ((lastButton == BUTTON0) ? ZCL_ON_COMMAND_ID : ZCL_OFF_COMMAND_ID),
                                        "");

      sl_zigbee_af_get_command_aps_frame()->profileId           = sl_zigbee_af_profile_id_from_index(0);
      sl_zigbee_af_get_command_aps_frame()->sourceEndpoint      = sl_zigbee_af_endpoint_from_index(0);

      status = sl_zigbee_af_send_command_multicast_to_bindings();

      sl_zigbee_af_app_println("Sent to multicast group: 0x%02X", status);
    }
  }
}
#endif //#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT)

/** @brief Stack Status
 *
 * This function is called by the application framework from the stack status
 * handler.  This callbacks provides applications an opportunity to be notified
 * of changes to the stack status and take appropriate action.  The return code
 * from this callback is ignored by the framework.  The framework will always
 * process the stack status after the callback returns.
 *
 * @param status   Ver.: always
 */
void sl_zigbee_af_stack_status_cb(sl_status_t status)
{
  // If a multi-network setup is used, this callback may be triggered
  // in a non-S2S network context where the node type is not an S2S initiator.
  sl_zigbee_node_type_t nodeType;
  sl_status_t ret = sl_zigbee_af_get_node_type(&nodeType);
  if ((ret == SL_STATUS_NOT_JOINED)
      || (ret == SL_STATUS_OK && nodeType != SL_ZIGBEE_S2S_INITIATOR_DEVICE)) {
    return;
  }

  sl_zigbee_network_status_t nwkState = sl_zigbee_af_network_state();

  sl_zigbee_af_core_println("Stack status=0x%02X, nwkState=%d", status, sl_zigbee_af_network_state());

  switch (nwkState) {
    case SL_ZIGBEE_JOINED_NETWORK:
    case SL_ZIGBEE_JOINED_NETWORK_S2S_INITIATOR:
    case SL_ZIGBEE_JOINED_NETWORK_S2S_TARGET:
    {
      if (multicast_binding_created == false) {
        uint16_t groupId = S2S_DEFAULT_GROUP_ID;
        sl_zigbee_binding_table_entry_t entry;
        sl_status_t status;
        uint16_t clusterId[] = S2S_CLUSTER_IDS_FOR_BINDING;
        uint8_t numClusters = sizeof(clusterId) / sizeof(uint16_t);

        entry.type = SL_ZIGBEE_MULTICAST_BINDING;
        memcpy(entry.identifier, &groupId, 2);
        entry.local = sl_zigbee_af_primary_endpoint();

        for (uint8_t index = 0; index < numClusters; index++) {
          entry.clusterId = clusterId[index];
          status = sl_zigbee_set_binding(index, &entry);
          sl_zigbee_af_core_println("Creating Multicast Binding, cluster = 0x%04x status = 0x%02X", clusterId[index], status);
        }
        multicast_binding_created = true;
      }
    }
    break;
    case SL_ZIGBEE_NO_NETWORK:
      leavingNwk = false; // leave has completed.
      sl_zigbee_clear_binding_table();
      multicast_binding_created = false;
      break;

    default:
      break;
  }
}

#if defined(SL_CATALOG_KERNEL_PRESENT) && (SL_LEGACY_HAL_ENABLE_WATCHDOG == 1)
static sl_zigbee_af_event_t watchdog_refresh_event;
static void watchdog_refresh_event_handler(sl_zigbee_af_event_t *event);
#define WATCHDOG_REFRESH_TIMEOUT_MS (1000u)

// In RTOS-based applications, while the Zigbee task refreshes the watchdog,
// when the node is not on a network the Zigbee task does not run.
// Therefore, there needs to be a way to ensure that the watchdog does not fire
// and reset the node unnecessarily. Below we provide an example of an application
// event that periodically resets the watchdog.
// Note, such a periodic event gets the node out of EM1/EM2 low power modes,
// which may result in unnecessary energy consumption.
// The application should ultimately own the refreshing of the watchdog and tailor
// it based on the specific use case.
static void watchdog_refresh_event_handler(sl_zigbee_af_event_t *event)
{
  (void)event;
  halResetWatchdog();
  sl_zigbee_af_event_set_delay_ms(&watchdog_refresh_event, WATCHDOG_REFRESH_TIMEOUT_MS);
}
#endif //#if defined(SL_CATALOG_KERNEL_PRESENT) && (SL_LEGACY_HAL_ENABLE_WATCHDOG == 1)

/** @brief Main Init
 *
 * This function is called from the application's main function. It gives the
 * application a chance to do any initialization required at system startup. Any
 * code that you would normally put into the top of the application's main()
 * routine should be put into this function. This is called before the clusters,
 * plugins, and the network are initialized so some functionality is not yet
 * available.
   Note: No callback in the Application Framework is
 * associated with resource cleanup. If you are implementing your application on
 * a Unix host where resource cleanup is a consideration, we expect that you
 * will use the standard Posix system calls, including the use of atexit() and
 * handlers for signals such as SIGTERM, SIGINT, SIGCHLD, SIGPIPE and so on. If
 * you use the signal() function to register your signal handler, please mind
 * the returned value which may be an Application Framework function. If the
 * return value is non-null, please make sure that you call the returned
 * function from your handler to avoid negating the resource cleanup of the
 * Application Framework itself.
 *
 */
void sl_zigbee_af_main_init_cb(void)
{
  #if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT)
  sl_zigbee_af_isr_event_init(buttonEvent, buttonEventHandler);
  #endif //#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT)

  sli_zigbee_sleepy_to_sleepy_init_nwk_key();

  #if defined(SL_CATALOG_KERNEL_PRESENT) && (SL_LEGACY_HAL_ENABLE_WATCHDOG == 1)
  sl_zigbee_af_event_init(&watchdog_refresh_event, watchdog_refresh_event_handler);
  sl_zigbee_af_event_set_active(&watchdog_refresh_event);
  #endif //SL_CATALOG_KERNEL_PRESENT && (SL_LEGACY_HAL_ENABLE_WATCHDOG == 1)
}
/** @brief Post Attribute Change
 *
 * This function is called by the application framework after it changes an
 * attribute value. The value passed into this callback is the value to which
 * the attribute was set by the framework.
 *
 * @param endpoint   Ver.: always
 * @param clusterId   Ver.: always
 * @param attributeId   Ver.: always
 * @param mask   Ver.: always
 * @param type   Ver.: always
 * @param size   Ver.: always
 * @param value   Ver.: always
 */
void sl_zigbee_af_post_attribute_change_cb(uint8_t endpoint,
                                           sl_zigbee_af_cluster_id_t clusterId,
                                           sl_zigbee_af_attribute_id_t attributeId,
                                           uint8_t mask,
                                           uint16_t manufacturerCode,
                                           uint8_t type,
                                           uint8_t size,
                                           uint8_t* value)
{
  (void)manufacturerCode;
  (void)type;
  (void)size;
  (void)value;
  if (clusterId == ZCL_ON_OFF_CLUSTER_ID
      && attributeId == ZCL_ON_OFF_ATTRIBUTE_ID
      && mask == CLUSTER_MASK_SERVER) {
    uint8_t data;
    sl_status_t status = sl_zigbee_af_read_attribute(endpoint,
                                                     ZCL_ON_OFF_CLUSTER_ID,
                                                     ZCL_ON_OFF_ATTRIBUTE_ID,
                                                     CLUSTER_MASK_SERVER,
                                                     (uint8_t*) &data,
                                                     sizeof(data),
                                                     NULL);

    if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
      if (data == 0x00) {
        led_turn_off(BOARDLED0);
        led_turn_off(BOARDLED1);
      } else {
        led_turn_on(BOARDLED0);
        led_turn_on(BOARDLED1);
      }
    }
  }
}

/** @brief Hal Button Isr
 *
 * This callback is called by the framework upon button state change.
 * This callback is called within ISR context.
 *
 * @param handle : Pointer to button handle
 */
#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT)
void sl_button_on_change(const sl_button_t *handle)
{
  static uint16_t buttonPressTime;
  uint16_t currentTime = 0;
  uint8_t button;
  uint8_t state = sl_button_get_state(handle);

  if (handle == SL_SIMPLE_BUTTON_INSTANCE(BUTTON0)) {
    button = BUTTON0;
  } else if (handle == SL_SIMPLE_BUTTON_INSTANCE(BUTTON1)) {
    button = BUTTON1;
  } else {
    return;
  }

  if (state == BUTTON_PRESSED) {
    buttonPressTime = halCommonGetInt16uMillisecondTick();
  } else if (state == BUTTON_RELEASED) {
    currentTime = halCommonGetInt16uMillisecondTick();
    longPress = ((currentTime - buttonPressTime) > BUTTON_LONG_PRESS_TIME_MSEC);
    lastButton = button;
    sl_zigbee_wakeup_common_task();
    sl_zigbee_af_event_set_active(buttonEvent);
  }
}
#endif //#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT)

/** @brief Pre Command Received
 *
 * This callback is the second in the Application Framework's message processing
 * chain. At this point in the processing of incoming over-the-air messages, the
 * application has determined that the incoming message is a ZCL command. It
 * parses enough of the message to populate an sl_zigbee_af_cluster_command_t struct. The
 * Application Framework defines this struct value in a local scope to the
 * command processing but also makes it available through a global pointer
 * called sl_zigbee_af_current_command, in app/framework/util/util.c. When command
 * processing is complete, this pointer is cleared.
 *
 * @param cmd   Ver.: always
 */
bool sl_zigbee_af_pre_command_received_cb(sl_zigbee_af_cluster_command_t* cmd)
{
  if ((cmd->commandId == ZCL_ON_COMMAND_ID)
      || (cmd->commandId == ZCL_OFF_COMMAND_ID)
      || (cmd->commandId == ZCL_TOGGLE_COMMAND_ID)) {
    memset(SwitchEUI, 0, SOURCE_ADDRESS_LEN);
    sl_zigbee_lookup_eui64_by_node_id(cmd->source, SwitchEUI);
    sl_zigbee_af_core_println(
      "SWITCH ZCL toggle/on/off EUI [%02X %02X %02X %02X %02X %02X %02X %02X]",
      SwitchEUI[7],
      SwitchEUI[6],
      SwitchEUI[5],
      SwitchEUI[4],
      SwitchEUI[3],
      SwitchEUI[2],
      SwitchEUI[1],
      SwitchEUI[0]);
  }
  return false;
}

/** @brief Trust Center Join
 *
 * This callback is called from within the application framework's
 * implementation of sl_zigbee_internal_trust_center_join_handler or sl_zigbee_ezsp_trust_center_join_handler.
 * This callback provides the same arguments passed to the
 * TrustCenterJoinHandler. For more information about the TrustCenterJoinHandler
 * please see documentation included in stack/include/trust-center.h.
 *
 * @param newNodeId   Ver.: always
 * @param newNodeEui64   Ver.: always
 * @param parentOfNewNode   Ver.: always
 * @param status   Ver.: always
 * @param decision   Ver.: always
 */
void sl_zigbee_af_trust_center_join_cb(sl_802154_short_addr_t newNodeId,
                                       sl_802154_long_addr_t newNodeEui64,
                                       sl_802154_short_addr_t parentOfNewNode,
                                       sl_zigbee_device_update_t status,
                                       sl_zigbee_join_decision_t decision)
{
  (void)newNodeId;
  (void)parentOfNewNode;
  (void)decision;
  // If a multi-network setup is used, this callback may be triggered
  // in a non-S2S network context where the node type is not an S2S initiator.
  sl_zigbee_node_type_t nodeType;
  if (sl_zigbee_af_get_node_type(&nodeType) == SL_STATUS_OK) {
    if (nodeType != SL_ZIGBEE_S2S_INITIATOR_DEVICE) {
      return;
    }
  }

  if (status == SL_ZIGBEE_DEVICE_LEFT) {
    for (uint8_t i = 0; i < SL_ZIGBEE_BINDING_TABLE_SIZE; i++) {
      sl_zigbee_binding_table_entry_t entry;
      sl_zigbee_get_binding(i, &entry);
      if ((entry.type == SL_ZIGBEE_UNICAST_BINDING)
          && (entry.clusterId == ZCL_ON_OFF_CLUSTER_ID)
          && ((memcmp(entry.identifier, newNodeEui64, EUI64_SIZE)
               == 0))) {
        sl_zigbee_delete_binding(i);
        sl_zigbee_af_app_println("deleted binding entry: %d", i);
        break;
      }
    }
  }
}

void sl_zigbee_sleepy_to_sleepy_set_initiator_wakeup_period(SL_CLI_COMMAND_ARG)
{
  uint16_t initiatorWakeUpPeriodMs;

  if (sl_cli_get_argument_count(arguments) > 0) {
    initiatorWakeUpPeriodMs = sl_cli_get_argument_uint16(arguments, 0);
    sli_mac_set_mac_csl_max_period(0, initiatorWakeUpPeriodMs);
  } else {
    initiatorWakeUpPeriodMs = sli_mac_get_mac_csl_max_period(0);
  }

  sl_zigbee_af_cli_println("Initiator Wake Up Period: %d ms", initiatorWakeUpPeriodMs);
}

/** @brief Application framework equivalent of
 * ::sl_zigbee_radio_needs_calibrating_handler
 */
void sl_zigbee_af_radio_needs_calibrating_cb(void)
{
  sl_mac_calibrate_current_channel();
}
