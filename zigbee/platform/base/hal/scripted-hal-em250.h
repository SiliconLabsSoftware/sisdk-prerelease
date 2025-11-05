/***************************************************************************//**
 * @file
 * @brief stubs for writing test scripts that simulate the HAL.
 * This file should *NOT* be included in Ember release code to customers.
 * This is for internal testing only.
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
// callIsr() is called from processIncomingByte() at every latency interval.
// callIsr() is also called from the test-script-writing code after the last
// packet has been added to allow the Isr to process the last buffer unload(s).
// Since this final call is scripted, it should only be called when there are
// byte time ticks remaining in the last latency interval.  Otherwise the
// script will get "callIsr" when it is expecting
// "sli_802154phy_radio_receive_complete_callback".
void callIsr(void);

// processIncomingByte() is called from addIncomingPacket() for each byte in the
// packet.  Each call to processIncomingByte() represents two symbol times, or
// one byte time (32us).  At every latency interval, processIncomingByte() calls
// the Isr to process any pending interrupts.
void addIncomingPacket(sli_parcel_t *buffer,
                       bool crcPassed);

//void processIncomingByte(int elapsedByteTimeTicks, uint8_t byte);

#define CRC_PASSED  true
#define CRC_FAILED  false

#define SIM_RX_TIME 0x00012345
#define SIM_LQI_IN  0
#define SIM_LQI_OUT 255
#define SIM_RSSI    0x7f
#define SIM_CHANNEL 16
#define SIM_GAIN    0xab
