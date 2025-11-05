/***************************************************************************//**
 * @file fault_catching_test.h
 * @brief Fault catching test header file
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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

#include <stdint.h>
#include "sli_tz_util.h"

uint32_t sli_tz_fault_catching_init(uint32_t arg);
uint32_t sli_tz_fault_handler(uint32_t error_code);

// NS side tests identifiers.
typedef enum {
  FAULT_CATCHING_TEST_USAGE,
  FAULT_CATCHING_TEST_HARD,
  FAULT_CATCHING_TEST_BUS,
  FAULT_CATCHING_TEST_SECURE,
  FAULT_CATCHING_TEST_EXIT,
} fault_catching_test_id_t;

// Possible faults to catch.
typedef enum {
  FAULT_CATCHING_NS_HARDFAULT,
  FAULT_CATCHING_NS_MEM_MANAGE,
  FAULT_CATCHING_NS_USAGEFAULT,
  FAULT_CATCHING_NS_BUSFAULT,
  FAULT_CATCHING_S_SMU_SECURE_IRQ,
  FAULT_CATCHING_S_HARDFAULT,
  FAULT_CATCHING_S_SECUREFAULT,
  FAULT_CATCHING_S_USAGEFAULT,
  FAULT_CATCHING_S_BUSFAULT,
  FAULT_CATCHING_S_MPU_FAULT,
} fault_catching_fault_id_t;

typedef enum {
  SLI_TZ_FAULT_CATCHING_INIT_SID,
  SLI_TZ_FAULT_CATCHING_FAULT_HANDLER_SID,
  SLI_TZ_FAULT_CATCHING_MAX_SID,
} fault_catching_service_id_t;

SLI_TZ_CMSE_NONSECURE_ENTRY
int32_t sli_tz_s_interface_dispatch_fault_catching(uint32_t sid, uint32_t arg);

#if defined(SL_TRUSTZONE_SECURE)
//Function pointer conforming to syscfg simple function signature.
typedef uint32_t (*sli_tz_simple_fn) (uint32_t arg);

static const sli_tz_simple_fn fault_catching_function_table[] = {
  (sli_tz_simple_fn)sli_tz_fault_catching_init,
  (sli_tz_simple_fn)sli_tz_fault_handler,
};
#endif
