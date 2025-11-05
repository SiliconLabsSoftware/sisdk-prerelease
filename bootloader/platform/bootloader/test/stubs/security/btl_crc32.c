#include "btl_crc32.h"
#include <stdio.h>
#include <stdint.h>

#define CRC32_POLYNOMIAL 0xEDB88320

uint32_t btl_crc32(const uint8_t newByte, uint32_t prevResult)
{
  uint32_t previous = (prevResult >> 8) & 0x00FFFFFF;
  uint32_t oper = (prevResult ^ newByte) & 0xFF;

  for (uint8_t jj = 0; jj < 8; jj++) {
    if (oper & 1) {
      oper = (oper >> 1) ^ CRC32_POLYNOMIAL;
    } else {
      oper >>= 1;
    }
  }

  return previous ^ oper;
}

uint32_t btl_crc32Stream(const uint8_t *buffer, size_t length, uint32_t prevResult)
{
  for (size_t position = 0; position < length; position++) {
    prevResult = btl_crc32(buffer[position], prevResult);
  }

  return prevResult;
}
