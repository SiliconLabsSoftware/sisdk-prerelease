/**************************************************************************//**
 * @file
 * @brief Configuration for the protocol-agnostic cpc-ng message link.
 ******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 ******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 *****************************************************************************/
#ifndef SL_BT_LINK_CONFIG_H
#define SL_BT_LINK_CONFIG_H

/***********************************************************************************************//**
 * @addtogroup sl_bt_link
 * @{
 **************************************************************************************************/

// <<< Use Configuration Wizard in Context Menu >>>

// <h> cpc-ng message link configuration

// <o SL_BT_LINK_COUNT> Link object pool size <1-8>
// <i> One object is required per endpoint. BGAPI uses one;
// <i> increase this value if additional protocols are added.
// <i> Default: 1
#define SL_BT_LINK_COUNT                              1

// <o SL_BT_LINK_RUNTIME_QUEUE_SIZE> Runtime event queue length <5-64>
// <i> Must hold the combined receive depth of all enabled link bindings
// <i> plus the derived control-event capacity of the link object pool.
// <i> Default: 5
#define SL_BT_LINK_RUNTIME_QUEUE_SIZE                 5

// <o SL_BT_LINK_RUNTIME_TASK_PRIO> Runtime context priority
// <APP_RTA_PRIORITY_LOW=> Low
// <APP_RTA_PRIORITY_BELOW_NORMAL=> Below normal
// <APP_RTA_PRIORITY_NORMAL=> Normal
// <APP_RTA_PRIORITY_ABOVE_NORMAL=> Above normal
// <APP_RTA_PRIORITY_HIGH=> High
// <i> Default: Normal
#define SL_BT_LINK_RUNTIME_TASK_PRIO                  APP_RTA_PRIORITY_NORMAL

// <o SL_BT_LINK_RUNTIME_TASK_STACK> Runtime stack size [bytes] <512-4096>
// <i> Default: 1024
#define SL_BT_LINK_RUNTIME_TASK_STACK                 1024

// <o SL_BT_LINK_RUNTIME_GUARD_TIMEOUT> Guard timeout [ticks] <1-1000>
// <i> Default: 10
#define SL_BT_LINK_RUNTIME_GUARD_TIMEOUT              10

// </h>

// <<< end of configuration section >>>

/**
 * Runtime queue entries reserved for non-receive events.
 *
 * Each link can contribute at most three non-receive events concurrently: an
 * error, an active transmit completion, and the subsequent closed event. This
 * correctness limit is derived from the configured link pool size and is
 * intentionally not configurable.
 */
#define SL_BT_LINK_RUNTIME_CONTROL_EVENTS  (3u * SL_BT_LINK_COUNT)

/** @} (end addtogroup sl_bt_link) */

#endif // SL_BT_LINK_CONFIG_H
