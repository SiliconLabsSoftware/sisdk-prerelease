/***************************************************************************//**
 * @file
 * @brief An implementation of the interface described in
 * hal/micro/avr-atmega/spi-protocol.h for the simulator. See
 * tool/simulator/child/ezsp-serial.h for a description of the simulated
 * connection.
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
#include "stack/include/sl_zigbee.h"
#include "hal/hal.h"
#include "tool/simulator/child/ezsp-serial.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "hal/micro/unix/simulation/spi-protocol.h"

#define SPIP_BUFFER_SIZE              136
uint8_t halNcpSpipBuffer[SPIP_BUFFER_SIZE];

static uint8_t buffer[EZSP_SIM_BUFFER_SIZE];
uint8_t *halNcpFrame = buffer + EZSP_SIM_LENGTH_INDEX;
uint8_t halNcpSpipErrorByte;

static bool dataPending = false;

static void incomingResponseHandler(uint8_t *contents, uint8_t length);
extern void simulatedNodeResetMyNcp(void); // in child-main.c

enum {
  IDLE,
  WAIT_FOR_RESPONSE
};

uint8_t state = IDLE;

void simulatedNodeMyNcpHasCallbackPending(bool pending)
{
  dataPending = pending;
}

sl_zigbee_ezsp_status_t halNcpHardReset(void)
{
  simulatedNodeResetMyNcp();

  setEzspSerialHandler(incomingResponseHandler);
  memset(buffer, 0, EZSP_SIM_BUFFER_SIZE);
  return SL_ZIGBEE_EZSP_SUCCESS;
}

void halNcpWakeUp(void)
{
  uint8_t wakeUpMessage[2];
  wakeUpMessage[EZSP_SIM_MESSAGE_TYPE_INDEX] = EZSP_SIM_MESSAGE_TYPE_WAKE_UP;
  wakeUpMessage[EZSP_SIM_LENGTH_INDEX] = 0;
  writeEzspSerialOutput(wakeUpMessage, 2);
}

void halNcpSendCommand(void)
{
  assert(state == IDLE);
  buffer[EZSP_SIM_MESSAGE_TYPE_INDEX] = EZSP_SIM_MESSAGE_TYPE_EZSP;
  writeEzspSerialOutput(buffer,
                        buffer[EZSP_SIM_LENGTH_INDEX]
                        + EZSP_SIM_OVERHEAD);
  state = WAIT_FOR_RESPONSE;
}

sl_zigbee_ezsp_status_t halNcpPollForResponse(void)
{
  simulatedTimePasses();
  if (state == WAIT_FOR_RESPONSE) {
    return SL_ZIGBEE_EZSP_SPI_WAITING_FOR_RESPONSE;
  } else {
    return SL_ZIGBEE_EZSP_SUCCESS;
  }
}

bool halNcpHasData(void)
{
  return dataPending;
}

static void incomingResponseHandler(uint8_t *contents, uint8_t length)
{
  if (contents[EZSP_SIM_MESSAGE_TYPE_INDEX] == EZSP_SIM_MESSAGE_TYPE_CALLBACK) {
    dataPending = contents[EZSP_SIM_LENGTH_INDEX + 1];
    return;
  }
  if (contents[EZSP_SIM_MESSAGE_TYPE_INDEX] == EZSP_SIM_MESSAGE_TYPE_AWAKE) {
    halNcpIsAwakeIsr(true);
    return;
  }
  assert(state == WAIT_FOR_RESPONSE);
  assert(contents[EZSP_SIM_MESSAGE_TYPE_INDEX] == EZSP_SIM_MESSAGE_TYPE_EZSP);
  memcpy(buffer, contents, length);
  state = IDLE;
}

// Functions required for OTA bootload
bool halNcpVerifySpiProtocolActive(void)
{
  return false;
}

bool halNcpVerifySpiProtocolVersion(void)
{
  return false;
}

void halNcpSendRawCommand(void)
{
  assert(false);
}

int halNcpGetIntFd(void)
{
  return -1;
}
