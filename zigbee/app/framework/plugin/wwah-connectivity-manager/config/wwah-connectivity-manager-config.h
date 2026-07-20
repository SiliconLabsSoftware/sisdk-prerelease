/***************************************************************************//**
 * @brief Zigbee SL Works With All Hubs Connectivity Manager component configuration header.
 *\n*******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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

// <<< Use Configuration Wizard in Context Menu >>>

// <h>Zigbee SL Works With All Hubs Connectivity Manager configuration

// <q SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_USE_PREFERRED_CHANNEL_MASK> Use Preferred Channel Mask When Rejoining
// <i> Default: TRUE
// <i> Use preferred channel mask when attemping to rejoin the network in the event of connection failure. Connection failure includes loss of communication to either the hub or parent (the latter of which only applies to end devices)
#define SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_USE_PREFERRED_CHANNEL_MASK   1

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_PREFERRED_CHANNEL_MASK> Preferred Channel Mask For Rejoining (hex) <1-0xFFFFFFFF> <f.h>
// <i> Default: 0x02308800
// <i> The preferred channel mask to scan when attemping to rejoin the network in the event of connection failure.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_PREFERRED_CHANNEL_MASK   0x02308800

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_FAST_REJOIN_TIMEOUT_SEC> Fast Rejoin Timeout Seconds <1-65535>
// <i> Default: 35
// <i> The number of seconds that the device should be in fast rejoin mode when the devices loses parent connectivity.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_FAST_REJOIN_TIMEOUT_SEC   35

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_DURATION_BETWEEN_EACH_REJOIN_SEC> Duration Between Each Rejoin Seconds <1-65535>
// <i> Default: 7
// <i> The number of seconds that the device delays between each state when in fast rejoin mode.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_DURATION_BETWEEN_EACH_REJOIN_SEC   7

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_FAST_REJOIN_FIRST_BACKOFF_TIME_SEC> Fast Rejoin First Backoff Seconds <1-65535>
// <i> Default: 30
// <i> The number of seconds that the device delays after having completed its first fast rejoin cycle. For subsequent fast rejoin attempt completions, the delay is this value doubled each time (up to a maximum of Max Backoff Time Seconds).
#define SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_FAST_REJOIN_FIRST_BACKOFF_TIME_SEC   30

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_MAX_BACKOFF_TIME_SEC> Max Backoff Time Seconds <1-65535>
// <i> Default: 90
// <i> The maximum number of seconds that the device delays after having completed a fast rejoin mode.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_MAX_BACKOFF_TIME_SEC   90

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_MAX_BACKOFF_ITERATIONS> Max Backoff Iterations <1-65535>
// <i> Default: 15
// <i> The number of iterations of the Fast Rejoin Backoff the device must perform before it resets its backoff duration to the Fast Rejoin First Backoff Time In Seconds.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_MAX_BACKOFF_ITERATIONS   15

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_NON_FAST_REJOIN_BACKOFF_MIN> Non Fast Rejoin Backoff Minutes <1-300>
// <i> Default: 1
// <i> The amount of base time, in minutes, that the rejoin algorithm state machine will delay before continuing its attempts to restore hub or parent connectivity. This delay is applicable for all state transitions when not in fast rejoin mode (e.g. routers and end devices that lose hub connectivity only). This delay is also applicable to sleepy end devices when the rejoin state is one of the rejoin algorithm's delay states. Please refer to the Zigbee WWAH Requirements document for information regarding delays. The total delay will be equal to the base time plus the jitter value.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_NON_FAST_REJOIN_BACKOFF_MIN   1

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_NON_FAST_REJOIN_BACKOFF_JITTER_SEC> Non Fast Rejoin Backoff Jitter Seconds <0-300>
// <i> Default: 60
// <i> The amount of jitter time, in seconds, that the rejoin algorithm state machine will delay before continuing its attempts to restore hub or parent connectivity. This delay is applicable for all state transitions when not in fast rejoin mode (e.g. routers and end devices that lose hub connectivity only). This delay is also applicable to sleepy end devices when the rejoin state is one of the rejoin algorithm's delay states. Please refer to the Zigbee WWAH Requirements document for information regarding delays. The total delay will be equal to the base time plus the jitter value.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_NON_FAST_REJOIN_BACKOFF_JITTER_SEC   60

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_LONG_UPTIME_THRESHOLD> Long Uptime Threshold <0-2147483647>
// <i> Default: 1440
// <i> Long uptime threshold
#define SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_LONG_UPTIME_THRESHOLD   1440

// <q SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_ALLOW_TRIGGER_STATE_SEND_REJOIN> Allow to send rejoin attempts in the trigger waiting states
// <i> Default: FALSE
// <i> Send rejoin attempts when the state is in STATE_WAIT_FOR_TRIGGER or STATE_WAIT_FOR_SECOND_TRIGGER, which aims to accommodate the discrepancy between the "17-01066-029-Zigbee_WWAH_Requirements.docx" and "WWAH Test Cases - 191002.docx". We should remove this plugin option if the discrepancy no longer exists.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_CONNECTIVITY_MANAGER_ALLOW_TRIGGER_STATE_SEND_REJOIN   0

// </h>

// <<< end of configuration section >>>
