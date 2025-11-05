/***************************************************************************//**
 * @file
 * @brief Duty-Cycle stubs
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
#if     (PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)

// If SLC builds this source file without being included, make it empty

#else//!(PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)

#ifdef  SL_ZIGBEE_TEST
#include "core/sl_zigbee_stack.h"
#include "hal/hal.h"
#else//!SL_ZIGBEE_TEST
#define LEGACY_PHY_BUILD 1
#include "phy/hal.h"
#include "core/sl_zigbee_stack.h"
#endif//SL_ZIGBEE_TEST
#include "phy/phy.h"

#if     (PHY_DUAL && (PHY_THIS == PHY_THIS_UNSPECIFIED))

// Let the phy/dual/phy.c Mux layer implementation replace this one

#else//!(PHY_DUAL && (PHY_THIS == PHY_THIS_UNSPECIFIED))

#include "duty-cycle.h"                // Public API
#include "duty-cycle-test.h"           // Test API

#if     (PHY_PRO2PLUS || defined(SL_ZIGBEE_TEST))
// PTA is not supported by these PHYs or in Connect
#define issuePtaStackEvent(ptaEvent, supplement) /*no-op*/
#else//!(PHY_PRO2PLUS || ...)
#include "coexistence/protocol/ieee802154_uc/coexistence-802154.h"
#define issuePtaStackEvent(ptaStackEvent, supplement) \
  ((void) sl_rail_util_ieee802154_on_event(ptaStackEvent, supplement))
#endif//(PHY_PRO2PLUS || ...)

static void transmitComplete(sl_status_t status)
{
  extern uint8_t sli_legacy_mfglib_mode;
  if (sli_legacy_mfglib_mode) {
#ifndef SL_ZIGBEE_TEST
    extern volatile bool sli_legacy_mfglib_transmit_complete;
    sli_legacy_mfglib_transmit_complete = true;
#endif // SL_ZIGBEE_TEST
  } else {
    sli_802154phy_radio_transmit_complete_callback(status, 0, 0);
  }
}

//-- Public APIs

sl_status_t emPhyDcSetMode(EmPhyDcMode dcMode)
{
  return (dcMode == EM_PHY_DC_MODE_OFF)
         ? SL_STATUS_OK
         : SL_STATUS_INVALID_PARAMETER;
}

EmPhyDcMode emPhyDcGetMode(void)
{
  return EM_PHY_DC_MODE_OFF;
}

EmPhyDcMode emPhyDcGetPktMode(void)
{
  return EM_PHY_DC_MODE_OFF;
}

EmPhyDcState emPhyDcGetState(void)
{
  return EM_PHY_DC_STATE_OFF;
}

EmPhyDcState emPhyDcGetActualState(void)
{
  return EM_PHY_DC_STATE_OFF;
}

EmPhyDcHectoPct emPhyDcGetDcAndLimits(EmPhyDcLimits* limitPtr)
{
  if (limitPtr != NULL) {
    limitPtr->talkLimit  = EM_PHY_DC_LIMIT_UNLIMITED;
    limitPtr->limiThresh = EM_PHY_DC_LIMIT_UNLIMITED;
    limitPtr->critThresh = EM_PHY_DC_LIMIT_UNLIMITED;
    limitPtr->suspLimit  = EM_PHY_DC_LIMIT_UNLIMITED;
  }
  return 0; // DC is infinite so none used
}

const EmPhyDcLimits* emPhyDcGetLimitsConfig(void)
{
  return NULL;
}

sl_status_t emPhyDcSetLimitsConfig(const EmPhyDcLimits* limitsConfig)
{
  UNUSED_VAR(limitsConfig);
  return SL_STATUS_OK;
}

sl_status_t sli_802154phy_dc_send_pkt_asap(uint8_t* packet,
                                           uint32_t limitMs,
                                           bool callbackOnErrorStatus)
{
  UNUSED_VAR(limitMs);
  issuePtaStackEvent(SL_RAIL_UTIL_IEEE802154_STACK_EVENT_TX_PENDED_PHY,
                     (uint32_t)radioTransmitConfig.checkCca);
  sl_status_t status = emRadioTransmit(packet);

  if (callbackOnErrorStatus && status != SL_STATUS_OK) {
    transmitComplete(status);
  }

  return status;
}

void sli_802154phy_dc_send_pkt_failed(void)
{
}

void emPhyDcAckSent(void)
{
}

void emPhyDcTick(void)
{
}

static EmPhyDcExtendedStatus emPhyDcExtendedStatus;

/** @brief Helper function to get the status appropriately.
 */
EmPhyDcExtendedStatus emPhyDcGetExtendedStatus(void)
{
  return emPhyDcExtendedStatus;
}

/** @brief Helper function to set the status appropriately.
 */
void emPhyDcSetExtendedStatus(EmPhyDcExtendedStatus dcStatus)
{
  emPhyDcExtendedStatus = dcStatus;
}

//-- Events
sli_zigbee_event_t emPhyDcTxEvent;
void emPhyDcTxEventHandler(sli_zigbee_event_t *event)
{
  UNUSED_VAR(event);
}

//-- Private Test APIs

EmPhyDutyCycle* emPhyDcEntry(void)
{
  return NULL;
}

uint8_t emPhyDcBucketIndex(uint32_t timeMsTick)
{
  UNUSED_VAR(timeMsTick);
  return 0;
}

void sli_802154phy_dc_set_time_accel(uint16_t timeAccel)
{
  UNUSED_VAR(timeAccel);
}

uint16_t emPhyDcGetTimeAccel(void)
{
  return 1;
}

#endif//(PHY_DUAL && (PHY_THIS != PHY_THIS_UNSPECIFIED))

#endif//(PHY_DUALRAIL && !PHY_THIS && !PHY_INCLUDE_SOURCE)
