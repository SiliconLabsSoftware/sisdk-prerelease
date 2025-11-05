/***************************************************************************//**
 * @file
 * @brief APIs and defines for the Zigbee Dynamic Node Type plugin.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef ZIGBEE_DYNAMIC_NODE_TYPE_STACK_H
#define ZIGBEE_DYNAMIC_NODE_TYPE_STACK_H

#include "sl_status.h"
#include "sl_zigbee_types.h"

/**
 * @addtogroup dynamic_node_type
 *
 * This file describes APIs for Zigbee Dynamic Node Type functionality,
 * allowing a device to switch between the Router and Sleepy End Device
 * node types during runtime.
 *
 * @{
 */

/**
 * @brief Switches the Zigbee node type to Sleepy End Device if currently a Router.
 *
 * This function checks if the current node type is SL_ZIGBEE_ROUTER. If so, it leaves the network quietly,
 * erases the child table, sets the end device poll timeout and configuration, changes the node type to
 * SL_ZIGBEE_SLEEPY_END_DEVICE, and attempts to rejoin the network as a sleepy end device.
 *
 * @param end_device_configuration End device configuration flags to set.
 * @return SL_STATUS_OK if the operation was successful, or an error status otherwise.
 */
sl_status_t sl_zigbee_switch_role_sleepy_end_device(uint8_t end_device_configuration);

/**
 * @brief Switches the Zigbee node type to Router if currently a Sleepy End Device.
 *
 * This function checks if the current node type is SL_ZIGBEE_SLEEPY_END_DEVICE. If so, it leaves the network quietly,
 * sets the node type to SL_ZIGBEE_ROUTER, and attempts to rejoin the network as a router.
 *
 * @return SL_STATUS_OK if the operation was successful, or SL_STATUS_FAIL otherwise.
 */
sl_status_t sl_zigbee_switch_role_router(void);

/** @} END addtogroup */
#endif
