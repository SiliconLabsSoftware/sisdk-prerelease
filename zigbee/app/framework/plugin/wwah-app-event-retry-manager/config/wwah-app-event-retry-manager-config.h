/***************************************************************************//**
 * @brief Zigbee SL Works With All Hubs App Event Retry Manager component configuration header.
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

// <h>Zigbee SL Works With All Hubs App Event Retry Manager configuration

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_QUEUE_SIZE> State Change Queue Size <1-128>
// <i> Default: 10
// <i> This option controls the number of entries in the transaction queue.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_QUEUE_SIZE   10

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_FIRST_BACKOFF_TIME_SEC> First Backoff Time <1-255>
// <i> Default: 3
// <i> This option controls the first backoff time in seconds.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_FIRST_BACKOFF_TIME_SEC   3

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_BACKOFF_SEQUENCE_COMMON_RATIO> Backoff Sequence Common Ratio <1-255>
// <i> Default: 2
// <i> This option controls the common ratio (ie. multiplier) of the backoff sequence.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_BACKOFF_SEQUENCE_COMMON_RATIO   2

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_MAX_BACKOFF_TIME_SEC> Maximum Backoff Time <1-86400>
// <i> Default: 12
// <i> This option controls the maximum of the backoff time in seconds.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_MAX_BACKOFF_TIME_SEC   12

// <q SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_UNLIMITED_RETRIES> Unlimite Retries
// <i> Default: TRUE
// <i> This option controls unlimited retries.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_UNLIMITED_RETRIES   1

// <o SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_MAX_RETRY_ATTEMPTS> Maximum Number of Retry Attempts <1-254>
// <i> Default: 100
// <i> This option controls the maximum number of retry attempts. Note, this is discarded if the Unlimited Retries option is checked.
#define SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_MAX_RETRY_ATTEMPTS   100

// </h>

// <<< end of configuration section >>>
