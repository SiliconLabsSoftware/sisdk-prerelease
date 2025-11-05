/***************************************************************************//**
 * @file
 * @brief Code common to all PHYs.
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
#include "core/sl_zigbee_stack.h"
#include "hal.h"
#include "phy.h"
#include "ieee802154mac.h"

void sli_802154phy_radio_seed_random(void)
{
  uint32_t seed;
  sli_802154phy_radio_get_random_numbers((uint16_t *)(&seed), 2);
  halStackSeedRandom(seed);
}
