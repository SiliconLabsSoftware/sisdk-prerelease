/***************************************************************************/ /**
 * @file  sl_aws_mqtt_clock.h
 * @brief SiLabs clock for coreMQTT.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 ******************************************************************************/

#ifndef SL_AWS_MQTT_CLOCK_H
#define SL_AWS_MQTT_CLOCK_H

#include "cmsis_os2.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t sl_aws_mqtt_getTimeMs(void);

#ifdef __cplusplus
}
#endif

#endif /* SL_AWS_MQTT_CLOCK_H */
