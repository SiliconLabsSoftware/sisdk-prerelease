/***************************************************************************//**
 * @file
 * @brief Internal routines for the Zigbee Event Logger plugin.
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

#include "../../include/af.h"
#include "zigbee-event-logger-internal.h"

//-----------------------------------------------------------------------------
// Internal API

uint8_t* addUint32_t(uint8_t* data, uint32_t itemToAdd)
{
  data = addUint16_t(data, (uint16_t)itemToAdd);
  data = addUint16_t(data, (uint16_t)(itemToAdd >> 16));
  return data;
}

uint8_t* addUint16_t(uint8_t* data, uint16_t itemToAdd)
{
  data = addUint8_t(data, (uint8_t)itemToAdd);
  data = addUint8_t(data, (uint8_t)(itemToAdd >> 8));
  return data;
}

uint8_t* addUint8_t(uint8_t* data, uint8_t itemToAdd)
{
  *data = itemToAdd;
  data++;
  return data;
}

uint8_t* addArray(uint8_t* data, uint8_t *itemToAdd, uint8_t itemToAddLength)
{
  while ( itemToAddLength > 0 ) {
    *data = *itemToAdd++;
    data++;
    itemToAddLength--;
  }
  return data;
}

uint32_t parseUint32_t(uint8_t** data)
{
  uint32_t value = parseUint16_t(data);
  value += (parseUint16_t(data) << 16);
  return value;
}

uint16_t parseUint16_t(uint8_t** data)
{
  uint16_t value = parseUint8_t(data);
  value += (parseUint8_t(data) << 8);
  return value;
}

uint8_t parseUint8_t(uint8_t** data)
{
  uint8_t value = **data;
  // NOTE:  "*data = *data+1" is not the same as "*data++".
  *data = *data + 1;
  return value;
}

void parseArray(uint8_t *value, uint8_t** data, uint8_t dataLength)
{
  while ( dataLength > 0 ) {
    *value++ = **data;
    *data = *data + 1;
    dataLength--;
  }
}
