/***************************************************************************//**
 * @file
 * @brief Stubs needed for phy/dual/phy-dual.c.
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
// This file require to build dual mac-phy application using dual/phy.c

#define PHY_THIS    1
#define PHY_EM250   1

#include "core/sl_zigbee_stack.h"
#include "phy/phy.h"

#if !defined(PHY_SIMULATION_DUAL)
  #error "This file is intended to use only with PHY_DUAL option"
#endif

bool emPhyIsTransmitting(void)
{
  return 0;
}

void emRadioEnableReceiveCompleteInt(bool enable)
{
  UNUSED_VAR(enable);
}
void emRadioEnableSfdRecvNotification(bool enable)
{
  UNUSED_VAR(enable);
}

bool emRadioReceiveCompleteIntPending(bool acknowledgePending)
{
  UNUSED_VAR(acknowledgePending);
  return 0;
}

uint32_t emPhySymbolsToUs(uint32_t symbols)
{
  return SL_ZIGBEE_PHY_SYMBOLS_TO_US(symbols);
}

bool emRadioHoldOffIsActive(void)
{
  return 0;
}

void emRadioHoldOffIsr(bool active)
{
  UNUSED_VAR(active);
}

bool emRadioSfdRecvNotificationEnabled(void)
{
  return 0;
}
