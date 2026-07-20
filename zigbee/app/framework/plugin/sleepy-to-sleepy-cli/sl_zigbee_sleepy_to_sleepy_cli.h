/***************************************************************************//**
 * @file
 * @brief Handles commands related to the Sleepy-to-Sleepy feature.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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
#ifndef __SL_ZIGBEE_SLEEPY_TO_SLEEPY_CLI_H__
#define __SL_ZIGBEE_SLEEPY_TO_SLEEPY_CLI_H__

#include PLATFORM_HEADER
#include "hal.h"
#include "sl_zigbee.h"
#include "app/framework/include/af.h"

void sli_zigbee_sleepy_to_sleepy_init_nwk_key(void);

#endif //__SL_ZIGBEE_SLEEPY_TO_SLEEPY_CLI_H__
