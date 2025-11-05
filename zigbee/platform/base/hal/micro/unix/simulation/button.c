/***************************************************************************//**
 * @file
 * @brief simulation of button functions.
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
#include PLATFORM_HEADER
#include "stack/include/sl_zigbee_types.h"
#include "hal/hal.h"

static uint8_t button0State = BUTTON_RELEASED;
static uint8_t button1State = BUTTON_RELEASED;

void halInternalInitButton(void)
{
}

uint8_t halButtonState(uint8_t button)
{
  switch (button) {
    case BUTTON0:
      return button0State;
    case BUTTON1:
      return button1State;
    default:
      return BUTTON_RELEASED;
  }
}

void simulatedButtonIsr(uint8_t button, bool isPress)
{
  switch (button) {
    case BUTTON0:
    {
      uint8_t bs = (isPress ? BUTTON_PRESSED : BUTTON_RELEASED);
      if (bs != button0State) {
        button0State = bs;
        halButtonIsr(button, button0State);
      }
    }
    break;
    case BUTTON1:
    {
      uint8_t bs = (isPress ? BUTTON_PRESSED : BUTTON_RELEASED);
      if (bs != button1State) {
        button1State = bs;
        halButtonIsr(button, button1State);
      }
    }
    break;
    default:
      break;
  }
}
