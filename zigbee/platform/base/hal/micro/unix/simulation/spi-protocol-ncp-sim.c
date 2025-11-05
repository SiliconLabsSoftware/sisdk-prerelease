/***************************************************************************//**
 * @file
 * @brief An implementation of the interface described in
 * hal/micro/xap2b/spi-protocol.h for the simulator. See
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
#include "tool/simulator/child/message.h"
#include "tool/simulator/child/ezsp-serial.h"
#include "hal/micro/unix/simulation/spi-protocol.h"
#include "app/util/ezsp/ezsp-protocol.h"

//------------------------------------------------------------------------------
// Extern Declarations

// in child-main.c
void simulatedNodeSetOutputGpio(const char *message, bool isHigh);

//------------------------------------------------------------------------------
// Forward Declarations

static void incomingCommandHandler(uint8_t *contents, uint8_t length);

//------------------------------------------------------------------------------
// Global Variables

static uint8_t buffer[EZSP_SIM_BUFFER_SIZE];
enum {
  IDLE,
  WAIT_FOR_RESPONSE
};

static uint8_t state = IDLE;

uint8_t *halHostFrame = buffer + EZSP_SIM_LENGTH_INDEX;
bool spipFlagWakeFallingEdge;

//------------------------------------------------------------------------------
// SPI Protocol Interface

void halHostSerialInit(void)
{
  setEzspSerialHandler(incomingCommandHandler);
  memset(buffer, 0, EZSP_SIM_BUFFER_SIZE);
  memset(buffer, 0, EZSP_SIM_BUFFER_SIZE);
  spipFlagWakeFallingEdge = false;
}

// If 'responseReady' is true we send up the current contents of the
// response buffer, with a prefix of EZSP_SIM_MESSAGE_TYPE_EZSP to indicate
// that this is a serial message, not a wire signal.
//
// This returns true if there is an incoming message waiting in the
// command buffer.
bool halHostSerialTick(bool responseReady)
{
  if (spipFlagWakeFallingEdge) {
    uint8_t awakeMessage[2];
    awakeMessage[EZSP_SIM_MESSAGE_TYPE_INDEX] = EZSP_SIM_MESSAGE_TYPE_AWAKE;
    awakeMessage[EZSP_SIM_LENGTH_INDEX] = 0;
    writeEzspSerialOutput(awakeMessage, 2);
    spipFlagWakeFallingEdge = false;
    return false;
  }
  if (responseReady) {
    assert(state == WAIT_FOR_RESPONSE);
    buffer[EZSP_SIM_MESSAGE_TYPE_INDEX] = EZSP_SIM_MESSAGE_TYPE_EZSP;
    writeEzspSerialOutput(buffer,
                          buffer[EZSP_SIM_LENGTH_INDEX]
                          + EZSP_SIM_OVERHEAD);
    state = IDLE;
    return false;
  } else {
    return (state == WAIT_FOR_RESPONSE);
  }
}

void halHostCallback(bool haveData)
{
  // This simulates the NCP setting the PIN for notifying the HOST that there is
  // EZSP data pending at the NCP. It causes the HOST to return from the
  // realSimulatedTimePasses() call.
  simulatedNodeSetOutputGpio("GPIO: NCP callback pending", haveData);

  // TODO: the above new GPIO mechanism should fully replace this simulation
  // only serial message. however, without the following, few app framework
  // tests fail. To be investigated.

  // Don't use buffer to construct the callback message since it
  // might contain a partially   constructed response that is about to be sent.
  uint8_t callbackMessage[3];
  callbackMessage[EZSP_SIM_MESSAGE_TYPE_INDEX] = EZSP_SIM_MESSAGE_TYPE_CALLBACK;
  callbackMessage[EZSP_SIM_LENGTH_INDEX] = 1;
  callbackMessage[EZSP_SIM_LENGTH_INDEX + 1] = haveData;
  writeEzspSerialOutput(callbackMessage, 3);
}

//------------------------------------------------------------------------------
// Incoming Commands

static void incomingCommandHandler(uint8_t *contents, uint8_t length)
{
  if (contents[EZSP_SIM_MESSAGE_TYPE_INDEX] == EZSP_SIM_MESSAGE_TYPE_WAKE_UP) {
    spipFlagWakeFallingEdge = true;
    return;
  }
  assert(state == IDLE);
  assert(contents[EZSP_SIM_MESSAGE_TYPE_INDEX] == EZSP_SIM_MESSAGE_TYPE_EZSP);
  memcpy(buffer, contents, length);
  state = WAIT_FOR_RESPONSE;
}
