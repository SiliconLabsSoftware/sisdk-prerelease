/***************************************************************************//**
 * @file
 * @brief Reset cause signalling for the Silicon Labs bootloader
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc.  Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement.  This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#ifndef BTL_RESET_H
#define BTL_RESET_H

#include <stdint.h>
#include <stdbool.h>

void reset_resetWithReason(uint16_t resetReason);
void reset_setResetReason(uint16_t resetReason);
void reset_enableResetCounter(void);
void reset_disableResetCounter(void);
bool reset_resetCounterEnabled(void);
void reset_incrementResetCounter(void);
uint8_t reset_getResetCounter(void);
uint16_t reset_getResetReason(void);
void reset_invalidateResetReason(void);
uint16_t reset_classifyReset(void);

#endif
