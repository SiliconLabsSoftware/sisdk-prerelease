/***************************************************************************//**
 * @file
 * @brief CoreMark timing abstraction.
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

#ifndef COREMARK_TIMING_H
#define COREMARK_TIMING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void coremark_timing_init(void);
void coremark_timing_start(void);
void coremark_timing_stop(void);
uint32_t coremark_timing_get_elapsed_ticks(void);

#ifdef __cplusplus
}
#endif

#endif
