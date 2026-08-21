/***************************************************************************//**
 * # License
 * <b> Copyright 2024 Silicon Laboratories Inc. www.silabs.com </b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * https://www.silabs.com/about-us/legal/master-software-license-agreement
 * By installing, copying or otherwise using this software, you agree to the
 * terms of the MSLA.
 *
 ******************************************************************************/

#ifndef _CC_BATTERY_CONFIG_H_
#define _CC_BATTERY_CONFIG_H_

// <<< Use Configuration Wizard in Context Menu >>>

// <h> Command Class Battery Configuration

// <o CC_BATTERY_REPORTING_DECREMENTS> Battery reporting decrements <1..255:1> <f.d>
// <i> Default: 10
#define CC_BATTERY_REPORTING_DECREMENTS  10

// <o CC_BATTERY_HEALTH_TEMPERATURE_PRECISION> Battery temperature precision <0..7:1> <f.d>
// <i> This field indicates how many decimal places are included in the Battery Temperature field.
// <i> Default: 0
#define CC_BATTERY_HEALTH_TEMPERATURE_PRECISION  0

// <o CC_BATTERY_HEALTH_TEMPERATURE_SCALE> Battery temperature scale <0..0:1> <f.d>
// <i> The scale of the battery temperature measurement. 0 = Celsius
// <i> Default: 0
#define CC_BATTERY_HEALTH_TEMPERATURE_SCALE  0

// <o CC_BATTERY_HEALTH_TEMPERATURE_VALUE_SIZE> Battery temperature value size <0..4:1> <f.d>
// <i> The size of the battery temperature value in bytes. 0 = unknown, 1 = 1 byte, 2 = 2 bytes, 4 = 4 bytes.
// <i> Default: 0
#define CC_BATTERY_HEALTH_TEMPERATURE_VALUE_SIZE  0

// <q CC_BATTERY_RECHARGEABLE> Rechargeable battery
// <i> Indicates whether the battery is rechargeable or not.
// <i> Default: 0
#define CC_BATTERY_RECHARGEABLE  0

// <q CC_BATTERY_BACKUP_BATTERY> Backup battery
// <i> Indicates whether the battery is a backup battery or not.
// <i> Default: 0
#define CC_BATTERY_BACKUP_BATTERY  0

// </h>

// <<< end of configuration section >>>

#endif /* _CC_BATTERY_CONFIG_H_ */
