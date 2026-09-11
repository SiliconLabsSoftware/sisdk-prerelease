/***************************************************************************//**
 * @file
 * @brief Zigbee - SoC Empty application configuration file.
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef ZIGBEE_SOC_EMPTY_CONFIG_H
#define ZIGBEE_SOC_EMPTY_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h>Zigbee - SoC Empty configuration

// <q SL_ZIGBEE_EMPTY_JOIN_ON_BOOT> Join on boot (testing only)
// <i> Default: FALSE
// <i> On boot, force a join attempt (leave any restored NVM network if needed, then start network steering). When disabled, the device joins once after flash or mass erase and keeps its network across reset.
#define SL_ZIGBEE_EMPTY_JOIN_ON_BOOT   0

// </h>

// <<< end of configuration section >>>

#endif // ZIGBEE_SOC_EMPTY_CONFIG_H
