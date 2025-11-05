/***************************************************************************//**
 * @file
 * @brief backdoors for unix simulation files
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
#define MAC_ACK_WAIT_DURATION 54 // 54 symbols = 864 microseconds

void setRadioGetCCAHandler(uint8_t (*handler)(void));
void setRadioTransmitHandler(void (*handler)(uint8_t* memptr));
void setRadioSettingHandler(void (*handler)(uint8_t channelpage, uint8_t channel, int8_t power));
bool sli_simulator_set_energy_level(uint8_t channelpage, uint8_t channel, uint8_t level);

#ifdef SIMULATED_TRANSCEIVER
void sendPhyAck(bool framePending, uint8_t sequenceNumber);

#endif
