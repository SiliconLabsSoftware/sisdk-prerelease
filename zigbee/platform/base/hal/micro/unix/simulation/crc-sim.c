/***************************************************************************//**
 * @file
 * @brief crc implementation for simulator.
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
#include STACK_CORE_HEADER

#include "hal/hal.h"

// the poly,  CCITT: g(X) = X^16 + X^12 + X^5 + 1
uint16_t halCommonCrc16(uint8_t newByte, uint16_t prevRemainder)
{
  unsigned int crc = prevRemainder;
  crc  = (unsigned char)(crc >> 8) | (crc << 8);
  crc ^= newByte;
  crc ^= (unsigned char)(crc & 0xff) >> 4;
  crc ^= (crc << 8) << 4;
  crc ^= ((crc & 0xff) << 4) << 1;
  return crc;
}

//--------------------------------------------------------------
// CRC-32
#define POLYNOMIAL              (0xEDB88320UL)

uint32_t halCommonCrc32(uint8_t newByte, uint32_t prevResult)
{
  uint8_t jj;
  uint32_t previous;
  uint32_t oper;

  previous = (prevResult >> 8) & 0x00FFFFFFL;
  oper = (prevResult ^ newByte) & 0xFF;
  for (jj = 0; jj < 8; jj++) {
    oper = ((oper & 0x01)
            ? ((oper >> 1) ^ POLYNOMIAL)
            : (oper >> 1));
  }

  return (previous ^ oper);
}
