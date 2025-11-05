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
// This file require to build dual mac-phy application using dual/phy.c

#define PHY_THIS 2
#define PHY_TRANSCEIVER_SIM 1

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

uint32_t emPhyGetChannelFreqHz(uint8_t macPgChan)
{
  UNUSED_VAR(macPgChan);
  return 0;
}

uint8_t sli_802154phy_radio_convert_rssi_to_ed(int8_t rssiDbm)
{
  UNUSED_VAR(rssiDbm);
  return 0;
}

void emRadioEnableOverflowNotification(bool enable)
{
  UNUSED_VAR(enable);
}

void emRadioEnablePacketTrace(bool enable)
{
  UNUSED_VAR(enable);
}

void emRadioEnableSfdSentNotification(bool enable)
{
  UNUSED_VAR(enable);
}

int8_t emRadioGetEdCcaThreshold(void)
{
  return 0;
}

bool emRadioReceiveCompleteIntEnabled(void)
{
  return 0;
}

sl_status_t emRadioSetChannelAndForceCalibration(uint8_t macPgChan)
{
  UNUSED_VAR(macPgChan);
  return 0;
}

void emPhyCancelTransmit(void)
{
}
