#ifndef BTL_INTERNAL_FLASH_H
#define BTL_INTERNAL_FLASH_H

#include "core/btl_util.h"
MISRAC_DISABLE
#include "em_device.h"
MISRAC_ENABLE

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SL_GBL_MSC_LDMA_CHANNEL     2

bool flash_erasePage(uint32_t address);

bool flash_writeBuffer_dma(uint32_t       address,
                           void           *data,
                           size_t         length,
                           int            ch);

bool flash_writeBuffer(uint32_t       address,
                       void           *data,
                       size_t         length);

#endif // BTL_INTERNAL_FLASH_H
