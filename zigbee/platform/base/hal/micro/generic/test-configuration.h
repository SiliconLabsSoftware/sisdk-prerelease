/***************************************************************************//**
 * @file
 * @brief
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
// Make big tokens for use in tests that don't care about token details.
// All it costs is RAM.

#define SL_ZIGBEE_BINDING_TABLE_SIZE      32

// If we don't set the child sizes then sl_zigbee_configuration_defaults.h
// helpfully ignores our definition of SL_ZIGBEE_CHILD_TABLE_SIZE.
#define SL_ZIGBEE_MAX_ROUTER_CHILDREN     32
#define SL_ZIGBEE_MAX_END_DEVICE_CHILDREN 32
#define SL_ZIGBEE_CHILD_TABLE_SIZE        64
#define SL_ZIGBEE_KEY_TABLE_SIZE     32
