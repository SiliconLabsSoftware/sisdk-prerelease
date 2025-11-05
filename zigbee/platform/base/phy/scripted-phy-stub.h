/***************************************************************************//**
 * @file
 * @brief stubs for writing test scripts that simulate the PHY.
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
// Supply the next 'length' random numbers to be returned by
// halCommonGetRandomTraced().

void setRandomNumberScript(uint16_t *numbers, uint16_t length);

// Check that no timers are currently running.

void phyVerifyNoActiveTimers(void);

extern ActionType radioOutCheckType;

#define addRadioOutCheck(packet) \
  addCheck(&radioOutCheckType, true, (packet))

#define addRadioOutNoCcaCheck(packet) \
  addCheck(&radioOutCheckType, false, (packet))

extern ActionType radioAckOutCheckType;

#define addRadioAckOutCheck(framePending) \
  addCheck(&radioAckOutCheckType, (framePending))

extern ActionType radioInActionType;

#define addRadioInAction(packet, receiveIsrStatus) \
  (addAction(&radioInActionType, (packet), (receiveIsrStatus)))
