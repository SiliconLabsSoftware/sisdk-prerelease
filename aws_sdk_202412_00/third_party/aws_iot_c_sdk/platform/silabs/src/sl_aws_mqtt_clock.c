/***************************************************************************/ /**
 * @file  sli_aws_mqtt_clock.c
 * @brief CMSIS-RTOS2 clock for coreMQTT.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 ******************************************************************************/

#include "sl_aws_mqtt_clock.h"
#include "cmsis_os2.h"

uint32_t sl_aws_mqtt_getTimeMs(void)
{
  return (uint32_t)osKernelGetTickCount();
}
