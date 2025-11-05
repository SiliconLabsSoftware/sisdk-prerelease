/***************************************************************************//**
 * @file
 * @brief For serial communication between Transceiver and Mcu, we are using
 * the same set of messages that are used between embernode (Client) and java
 * simulator (Parent). Thus, ParentToChild messages used for transceiver to mcu
 * communication and ChildToParent for viceversa.
 * These messages are wrapped into the serial frame and send over the mapped
 * port (TRX_PORT 3) between trx and mcu.
 *
 * transceiver serial packet format:
 *  -----------------------------------------
 *  messageType | Length of payload | payload
 *  -----------------------------------------
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
#include "tool/simulator/child/message.h"
#include "app/test/transceiver/transceiver-simulator.h"
#include "phy/phy.h"

void incomingTransceiverCommandHandler(uint8_t *contents, uint8_t length);

static uint8_t trxFrame[TRX_MCU_SERIAL_BUFFER_SIZE];

void incomingTransceiverCommandHandler(uint8_t *contents, uint8_t length)
{
  memcpy(trxFrame, contents, length);
  processIncomingCommands(trxFrame);
}

#if defined(PHY_TRANSCEIVER_SIM) || defined(PHY_SIMULATION_DUAL)

// APIs to send  messages to transceiver.
void trxSendCommandRadioSetting(sli_trx_radio_setting_events_t event, uint8_t length, void *content)
{
  trxFrame[0] = RADIO_SETTINGS; //message type
  trxFrame[1] = length + 1; //1 for event length
  trxFrame[2] = event;

  if (content != NULL) {
    memcpy(&trxFrame[3], (uint8_t *)content, length);
  }
  writeTrxSerialOutput(trxFrame, trxFrame[1] + TRX_MCU_SERIAL_OVERHEAD);   //2 bytes (messageType+length)
}

void trxSendCommandRadioOut(uint8_t *content)
{
  uint8_t length = content[0];

  //make sure length is non-zero
  if (length < 1) {
    assert(false);
  }
  trxFrame[0] = RADIO_OUT; //message type
  memcpy(&trxFrame[1], content, length);
  //Lenght is already embedded in transmit packet
  writeTrxSerialOutput(trxFrame, trxFrame[1] + TRX_MCU_SERIAL_OVERHEAD);
}

void trxSendCommandTxAck(bool framePending, uint8_t seqNum)
{
  trxFrame[0] = TX_ACK;  //message type
  trxFrame[1] = sizeof(framePending) + sizeof(seqNum);  //length
  trxFrame[2] = (uint8_t) (framePending & 0x01);
  trxFrame[3] = seqNum;
  writeTrxSerialOutput(trxFrame, trxFrame[1] + TRX_MCU_SERIAL_OVERHEAD);
}

void trxSendCcaRequest(void)
{
  trxFrame[0] = REQUEST_CCA;
  trxFrame[1] = 0;  //length
  writeTrxSerialOutput(trxFrame, trxFrame[1] + TRX_MCU_SERIAL_OVERHEAD);
}
#endif  //PHY_TRANSCEIVER_SIM

#ifdef SIMULATED_TRANSCEIVER
// APIs to send  messages to MCU
void trxSendCommandRadioIn(uint8_t *content, uint8_t length)
{
  //make sure length is non-zero
  if (length < 1) {
    assert(false);
  }
  trxFrame[0] = RADIO_IN; //message type
  memcpy(&trxFrame[1], content, length);
  //Length is already embedded in the packet
  writeTrxSerialOutput(trxFrame, trxFrame[1] + TRX_MCU_SERIAL_OVERHEAD);
}

void trxSendCommandTxComplete(sl_status_t status,
                              uint32_t sfdSentTime,
                              uint8_t framePending)
{
  trxFrame[0] = TX_COMPLETE;
  //length
  trxFrame[1] = sizeof(status) + sizeof(sfdSentTime) + sizeof(framePending);
  trxFrame[2] = status & 0xFF;
  trxFrame[3] = (status >> 8) & 0xFF;
  trxFrame[4] = (status >> 16) & 0xFF;
  trxFrame[5] = (status >> 24) & 0xFF;
  trxFrame[6] = sfdSentTime & 0xFF;
  trxFrame[7] = (sfdSentTime >> 8) & 0xFF;
  trxFrame[8] = (sfdSentTime >> 16) & 0xFF;
  trxFrame[9] = (sfdSentTime >> 24) & 0xFF;
  trxFrame[10] = framePending;
  writeTrxSerialOutput(trxFrame, trxFrame[1] + TRX_MCU_SERIAL_OVERHEAD);
}

void trxSendCommandTxAckComplete(void)
{
  trxFrame[0] = TX_ACK_COMPLETE;
  trxFrame[1] = 0;  //length
  writeTrxSerialOutput(trxFrame, trxFrame[1] + TRX_MCU_SERIAL_OVERHEAD);
}

void trxSendCCAResult(uint8_t ccaValue)
{
  trxFrame[0] = CCA;
  trxFrame[1] = 1;  //length
  trxFrame[2] = ccaValue;
  writeTrxSerialOutput(trxFrame, trxFrame[1] + TRX_MCU_SERIAL_OVERHEAD);
}
#endif  //SIMULATED_TRANSCEIVER
