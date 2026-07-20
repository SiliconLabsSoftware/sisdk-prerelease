/***************************************************************************//**
 * @brief Zigbee Compact Logger component configuration header.
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

// <h>Zigbee Compact Logger configuration

// <q SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_MILLISECOND_PRECISION> Millisecond Precision
// <i> Default: FALSE
// <i> By default this plugin only stores log messages with second granularity.  If this option is selected, all messages are stored with millisecond precision, adding an additional 2-bytes per entry.
#define SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_MILLISECOND_PRECISION   0

// <o SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_STORAGE_SIZE> Storage Size <1-4096>
// <i> Default: 256
// <i> Sets the size of the ring buffer for storing events.
#define SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_STORAGE_SIZE   256

// </h>

// <<< end of configuration section >>>
