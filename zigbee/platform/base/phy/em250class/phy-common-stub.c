/***************************************************************************//**
 * @file
 * @brief Stubs for native legacy phy.
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
#ifdef PHY_SIMULATION_DUAL
#define PHY_THIS    1
#define PHY_EM250   1
#include "core/sl_zigbee_stack.h"
#include "phy/phy.h"
#endif

void sl_802154_calibrate_current_channel(void)
{
}

void emPhyCancelTransmit(void)
{
}

void emRadioTransmitIsr(void)
{
}
