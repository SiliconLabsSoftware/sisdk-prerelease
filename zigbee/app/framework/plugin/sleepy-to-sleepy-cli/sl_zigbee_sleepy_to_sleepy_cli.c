/***************************************************************************//**
 * @file
 * @brief Handles commands related to the Sleepy-to-Sleepy feature.
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

#include PLATFORM_HEADER
#include "hal.h"
#include "sl_zigbee.h"
#include "app/framework/include/af.h"
#ifdef SL_CATALOG_ZIGBEE_DEBUG_PRINT_PRESENT
#include "sl_zigbee_debug_print.h"
#endif // SL_CATALOG_ZIGBEE_DEBUG_PRINT_PRESENT
#include "app/framework/util/af-main.h"

#define S2S_DEFAULT_EXT_PAN_ID       { 0x00, 0x01, 0x02, 0x03, \
                                       0x04, 0x05, 0x06, 0x07 }
#define S2S_CLUSTER_IDS_FOR_BINDING  { ZCL_BASIC_CLUSTER_ID,         \
                                       ZCL_ON_OFF_CLUSTER_ID,        \
                                       ZCL_LEVEL_CONTROL_CLUSTER_ID, \
                                       ZCL_SHADE_CONFIG_CLUSTER_ID }

static uint8_t extendedPanId[EXTENDED_PAN_ID_SIZE] = S2S_DEFAULT_EXT_PAN_ID;;
static uint8_t nwkKey[SL_ZIGBEE_ENCRYPTION_KEY_SIZE];

void sli_zigbee_sleepy_to_sleepy_init_nwk_key(void)
{
  memset(nwkKey, 0xFF, sizeof(nwkKey));
}

static bool validateArray(uint8_t *array, uint8_t keyValue, uint8_t keySize)
{
  for (uint8_t i = 0; i < keySize; i++) {
    if (*array != keyValue) {
      return true;
    }
    array++;
  }
  return false;
}

static bool keyIsValid(uint8_t *key)
{
  return validateArray(key, 0xFF, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
}

void sl_zigbee_sleepy_to_sleepy_commission_cli_handler(SL_CLI_COMMAND_ARG)
{
  sl_zigbee_network_parameters_t networkParams;
  sl_zigbee_initial_security_state_t securityState = { 0u };
  sl_zigbee_key_data_t centralizedKey = { { 0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41,
                                            0x6C, 0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39 } };
  sl_status_t status;
  bool isInitiator;

  memset(&networkParams, 0, sizeof(sl_zigbee_network_parameters_t));

  networkParams.radioChannel = sl_cli_get_argument_uint8(arguments, 1);
  networkParams.radioTxPower = sl_cli_get_argument_int8(arguments, 2);
  networkParams.panId = sl_cli_get_argument_uint16(arguments, 3);
  sl_zigbee_set_node_id(sl_cli_get_argument_uint16(arguments, 4));
  isInitiator = (bool)sl_cli_get_argument_uint32(arguments, 0);

  memcpy(networkParams.extendedPanId, extendedPanId, EXTENDED_PAN_ID_SIZE);

  securityState.bitmask = (SL_ZIGBEE_AF_SECURITY_PROFILE_Z3_NODE_SECURITY_BITMASK
                           | SL_ZIGBEE_DISTRIBUTED_TRUST_CENTER_MODE);

  if (keyIsValid(nwkKey)) {
    memcpy(sl_zigbee_key_contents(&securityState.networkKey), nwkKey, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  } else {
    memcpy(sl_zigbee_key_contents(&securityState.networkKey), centralizedKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);
  }
  securityState.bitmask |= (SL_ZIGBEE_HAVE_NETWORK_KEY
                            | SL_ZIGBEE_DISTRIBUTED_TRUST_CENTER_MODE
                            | SL_ZIGBEE_HAVE_TRUST_CENTER_EUI64);
  securityState.networkKeySequenceNumber = 0;
  memcpy(sl_zigbee_key_contents(&securityState.preconfiguredKey), centralizedKey.contents, SL_ZIGBEE_ENCRYPTION_KEY_SIZE);

  status = sl_zigbee_set_initial_security_state(&securityState);

  if (status != SL_STATUS_OK) {
    sl_zigbee_af_cli_println("sl_zigbee_set_initial_security_state error: 0x%02X", status);
  }

  status = sl_zigbee_sleepy_to_sleepy_network_start(&networkParams, isInitiator);

  if (status != SL_STATUS_OK) {
    sl_zigbee_af_cli_println("sl_zigbee_sleepy_to_sleepy_network_start error: 0x%02X", status);
  }
}

void sl_zigbee_sleepy_to_sleepy_set_nwk_key_cli_handler(SL_CLI_COMMAND_ARG)
{
  sl_zigbee_copy_hex_arg(arguments, 0, nwkKey, SL_ZIGBEE_ENCRYPTION_KEY_SIZE, false);

  sl_zigbee_af_cli_println("S2S network key set");
}

void sl_zigbee_sleepy_to_sleepy_set_ext_pan_id_cli_handler(SL_CLI_COMMAND_ARG)
{
  sl_zigbee_copy_hex_arg(arguments, 0, extendedPanId, EXTENDED_PAN_ID_SIZE, false);

  sl_zigbee_af_cli_println("S2S extended PAN ID set");
}
