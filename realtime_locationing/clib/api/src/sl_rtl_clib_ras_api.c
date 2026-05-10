/***************************************************************************//**
 * @file
 * @brief Legacy RAS API wrapper
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sl_rtl_clib_api.h"
#include "sli_rtl_clib_ras_api.h"

enum sl_rtl_error_code sl_rtl_ras_process(
  sl_rtl_cs_libitem *item,
  const uint8_t num_procedures,
  const sl_rtl_ras_procedure *procedure_data)
{
#if defined(SL_CATALOG_KERNEL_PRESENT)
  (void)item;
  (void)num_procedures;
  (void)procedure_data;
  return SL_RTL_ERROR_FEATURE_NOT_SUPPORTED;
#else
  return sli_rtl_ras_process(item, num_procedures, procedure_data);
#endif
}
