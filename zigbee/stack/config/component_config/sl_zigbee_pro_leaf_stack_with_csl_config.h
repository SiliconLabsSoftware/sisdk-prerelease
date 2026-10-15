/***************************************************************************//**
 * @brief Zigbee PRO Leaf Stack with CSL component configuration header.
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

#include "sl_zigbee_pro_leaf_stack_config.h"

// Leaf stack defaults the neighbor table size to 1; CSL overrides it below.
#undef SL_ZIGBEE_NEIGHBOR_TABLE_SIZE

// <<< Use Configuration Wizard in Context Menu >>>

// <h>Zigbee PRO Leaf Stack with CSL configuration

// <o SL_ZIGBEE_NEIGHBOR_TABLE_SIZE> Neighbor Table Size for CSL devices <16-26>
// <i> Default: 16
// <i> The size of the neighbor table. For Coordinated Sample Listening (CSL)
// <i> devices, this sets how many sleepy-to-sleepy partners/initiators can be
// <i> tracked. Values below 16 are not supported for CSL; 16 is the recommended
// <i> default. Larger sizes use more RAM.
#define SL_ZIGBEE_NEIGHBOR_TABLE_SIZE   16

// </h>

// <<< end of configuration section >>>
