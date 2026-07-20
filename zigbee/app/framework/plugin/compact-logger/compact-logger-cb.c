/***************************************************************************//**
 * @file
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "app/framework/include/af.h"

/** @brief UTC Time Set
 *
 * This function is called when the UTC time has been set. It returns the
 * current UTC time. This callback is fired after all applicable compact-logger
 * events have had their UTC time updated.
 *
 * @param currentUtcTimeSeconds The current UTC time in seconds.
 * Ver.: always
 */
WEAK(void sl_zigbee_af_compact_logger_utc_time_set_cb(uint32_t currentUtcTimeSeconds))
{
  UNUSED_VAR(currentUtcTimeSeconds);
}
