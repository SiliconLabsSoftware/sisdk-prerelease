/***************************************************************************//**
 * @file
 * @brief Silicon Labs FreeRTOS heap_3 integration API.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 ******************************************************************************/

#ifndef SLI_FREERTOS_HEAP_3_H_
#define SLI_FREERTOS_HEAP_3_H_

#include <stdbool.h>
#include "em_device.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_SILICON_LABS_32B_SERIES_3)
void sli_freertos_heap_3_set_short_term_allocations(bool enabled);
#endif

#ifdef __cplusplus
}
#endif

#endif
