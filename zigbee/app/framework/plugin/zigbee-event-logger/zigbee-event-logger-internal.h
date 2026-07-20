/***************************************************************************//**
 * @file
 * @brief Internal definitions for the Zigbee Event Logger plugin.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef _ZIGBEE_EVENT_LOGGER_INTERNAL_H_
#define _ZIGBEE_EVENT_LOGGER_INTERNAL_H_

#ifdef SL_ZIGBEE_SCRIPTED_TEST
 #include "app/framework/plugin/zigbee-event-logger/config/zigbee-event-logger-config.h"
#else // SL_ZIGBEE_SCRIPTED_TEST
 #include "zigbee-event-logger-config.h"
 #if (SL_ZIGBEE_AF_PLUGIN_ZIGBEE_EVENT_LOGGER_INCLUDE_STRINGS_IN_LOGS == 1)
  #define INCLUDE_STRINGS_IN_LOGS
 #endif // SL_ZIGBEE_AF_PLUGIN_ZIGBEE_EVENT_LOGGER_INCLUDE_STRINGS_IN_LOGS == 1
#endif // SL_ZIGBEE_SCRIPTED_TEST

/**
 * @defgroup zigbee-event-logger Zigbee Event Logger
 * @ingroup component
 * @brief API and Callbacks for the Zigbee Event Logger Component
 *
 * This component provides an interface to add system event data into the
 * zigbee-event-logger's circular buffer.  It also provides hooks for the
 * application to be able to print and decode the logged event data.
 *
 */

/**
 * @addtogroup zigbee-event-logger
 * @{
 */

/** @} */ // end of comp-name

uint8_t* addUint8_t(uint8_t* data, uint8_t itemToAdd);
uint8_t* addUint16_t(uint8_t* data, uint16_t itemToAdd);
uint8_t* addUint32_t(uint8_t* data, uint32_t itemToAdd);
uint8_t* addArray(uint8_t* data, uint8_t *itemToAdd, uint8_t itemToAddLength);

uint8_t  parseUint8_t(uint8_t** data);
uint16_t parseUint16_t(uint8_t** data);
uint32_t parseUint32_t(uint8_t** data);
void parseArray(uint8_t *value, uint8_t** data, uint8_t dataLength);

#endif  // #ifndef _ZIGBEE_EVENT_LOGGER_INTERNAL_H_
