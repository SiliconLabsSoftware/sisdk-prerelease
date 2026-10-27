/***************************************************************************//**
 * @brief Zigbee Green Power v1.1.3 component configuration header.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

// <h>Zigbee Green Power v1.1.3 configuration

// <o SL_ZIGBEE_GP_TC_REQUEST_TIMEOUT_MS> GP TC Request timeout (ms) <1000-60000>
// <i> Default: 5000
#ifndef SL_ZIGBEE_GP_TC_REQUEST_TIMEOUT_MS
#define SL_ZIGBEE_GP_TC_REQUEST_TIMEOUT_MS  5000U
#endif

// <o SL_ZIGBEE_GP_TC_REQUEST_RETRY_MAX> GP TC Request retry count <0-5>
// <i> Default: 1
#ifndef SL_ZIGBEE_GP_TC_REQUEST_RETRY_MAX
#define SL_ZIGBEE_GP_TC_REQUEST_RETRY_MAX   1U
#endif

// <o SL_ZIGBEE_GP_OOB_KEY_TABLE_SIZE> OOB key table size <1-64>
// <i> Default: 16
// <i> Number of pre-configured per-GPD OOB keys stored for TC/sink verification.
#ifndef SL_ZIGBEE_GP_OOB_KEY_TABLE_SIZE
#define SL_ZIGBEE_GP_OOB_KEY_TABLE_SIZE  16U
#endif

// <q SL_ZIGBEE_AF_PLUGIN_GREEN_POWER_V113_USE_OOB_KEY_TOKENS> On SoC, persist OOB keys in NVM
// <i> Default: 1
#ifndef SL_ZIGBEE_AF_PLUGIN_GREEN_POWER_V113_USE_OOB_KEY_TOKENS
#define SL_ZIGBEE_AF_PLUGIN_GREEN_POWER_V113_USE_OOB_KEY_TOKENS  1
#endif

// </h>

// <<< end of configuration section >>>
