/***************************************************************************//**
 * @file sli_token_manager_hal_flash.c
 * @brief HAL API implementation for handling flash read/write operation.
 * @version 1.0.0
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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

#include <string.h>
#include "sli_token_manager_hal_flash.h"
#include "em_msc.h"
#if defined(SL_TOKEN_MANAGER_BACKEND_INT_FLASH_SE)
#include "sl_se_manager.h"
#include "sl_se_manager_util.h"
#endif // (SL_TOKEN_MANAGER_BACKEND_INT_FLASH_SE)

#define MFB_MASS_ERASE 0x01

/***************************************************************************//**
 * Validates that [@p flash_adr, @p flash_adr + @p flash_size) lies inside the
 * device main flash array.
 *
 * Series 2 has a uniform main flash region (no SE data-region demarcation).
 ******************************************************************************/
static sl_status_t sli_hal_internal_flash_open(uint32_t flash_adr, size_t flash_size)
{
  uint64_t flash_end = 0U;
  uint64_t range_end = 0U;

  if (flash_size == 0U) {
    return SL_STATUS_INVALID_RANGE;
  }

  flash_end = (uint64_t)FLASH_BASE + (uint64_t)FLASH_SIZE;
  range_end = (uint64_t)flash_adr + (uint64_t)flash_size;
  // On devices where FLASH_BASE is 0, an unsigned lower-bound check is always
  // false. Skip it in that case.
  if (
#if (FLASH_BASE > 0x0UL)
    ((uint64_t)flash_adr < (uint64_t)FLASH_BASE) ||
#endif
    (range_end > flash_end)) {
    return SL_STATUS_INVALID_RANGE;
  }

  return SL_STATUS_OK;
}

static void sli_hal_internal_flash_close(void)
{
}

// The parameter 'flash_adr' defines the starting flash_adr of where the
// programming will occur - this parameter MUST be word aligned since all
// programming operations are word aligned.  The parameter 'data' is a pointer to a buffer
// containing the 32bit words to be written.  Length is the number of 32bit
// words contained in 'data' to be written to flash.
// NOTE: This function can NOT write the option bytes and will throw an error
// if that is attempted.
static sl_status_t sli_hal_internal_flash_write(uint32_t flash_adr, const void *data, size_t length)
{
  if ((flash_adr & 3U) != 0U) {
    return SL_STATUS_FAIL;
  }

  MSC_Init();

  const uint32_t *p_src = data;
  uint32_t *p_dst = (uint32_t *)flash_adr;

  if (MSC_WriteWord(p_dst, p_src, length) != mscReturnOk) {
    MSC_Deinit();
    return SL_STATUS_FLASH_PROGRAM_FAILED;
  }

  MSC_Deinit();

  return SL_STATUS_OK;
}

static sl_status_t sli_hal_internal_flash_read(uint32_t flash_adr, void *dst, size_t length)
{
  uint32_t *p_src = (uint32_t *)flash_adr;
  uint32_t *p_dst = (uint32_t *)dst;

  (void)memcpy(p_dst, p_src, length);

  return SL_STATUS_OK;
}

static sl_status_t sli_hal_internal_flash_page_erase(uint32_t flash_adr)
{
  MSC_Init();

  msc_Return_TypeDef ret = MSC_ErasePage((uint32_t *)flash_adr);
  if (ret != mscReturnOk) {
    MSC_Deinit();
    return SL_STATUS_FAIL;
  }

  MSC_Deinit();
  return SL_STATUS_OK;
}

#if defined(SL_TOKEN_MANAGER_BACKEND_INT_FLASH_SE)
static sl_status_t sli_hal_internal_flash_write_SE(uint32_t flash_adr, const void *data, size_t len)
{
  sl_status_t status = SL_STATUS_OK;
  if ((flash_adr & USERDATA_BASE) == USERDATA_BASE) {
    sl_se_command_context_t cmd_ctx;
    sl_se_init_command_context(&cmd_ctx);
    status = sl_se_write_user_data(&cmd_ctx, (flash_adr & 0x0FFF), (void *)data, len);
  } else {
    status = sli_hal_internal_flash_write(flash_adr, data, len);
  }

  return status;
}
#endif // (SL_TOKEN_MANAGER_BACKEND_INT_FLASH_SE)

const sli_token_hal_flash_handle_t sli_token_manager_hal_nvm_handle = {
  .open = sli_hal_internal_flash_open,
  .close = sli_hal_internal_flash_close,
  .read = sli_hal_internal_flash_read,
#if defined(SL_TOKEN_MANAGER_BACKEND_INT_FLASH_SE)
  .write = sli_hal_internal_flash_write_SE,
#else
  .write = sli_hal_internal_flash_write,
#endif
  .erase = sli_hal_internal_flash_page_erase,
};
