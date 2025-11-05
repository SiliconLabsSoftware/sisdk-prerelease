#include "btl_internal_flash.h"

bool flash_erasePage(uint32_t address)
{
  (void) address;
  return true;
}

bool flash_writeBuffer_dma(uint32_t       address,
                           void           *data,
                           size_t         length,
                           int            ch)
{
  (void) address;
  (void) *data;
  (void) length;
  (void) ch;
  return true;
}

bool flash_writeBuffer(uint32_t       address,
                       void           *data,
                       size_t         length)
{
  (void)address;
  (void)*data;
  (void)length;
  return true;
}
