/***************************************************************************//**
 * @file
 * @brief USBD Hardware Configuration
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

#ifndef SL_USBD_DRIVER_CONFIG_H
#define SL_USBD_DRIVER_CONFIG_H

#include <stdint.h>

// <<< sl:start pin_tool >>>
// <gpio> SL_USBD_DRIVER_VBUS_SENSE
// $[GPIO_SL_USBD_DRIVER_VBUS_SENSE]
// TODO #warning "USB Device VBUS GPIO pin not configured"
 #define SL_USBD_DRIVER_VBUS_SENSE_PORT       SL_GPIO_PORT_C
 #define SL_USBD_DRIVER_VBUS_SENSE_PIN        2
// [GPIO_SL_USBD_DRIVER_VBUS_SENSE]$
// <<< sl:end pin_tool >>>

// Default above is Series 2 (port C). Series 3 boards use port D; override here
// (outside pin_tool) so Pin Tool does not remove this block.
#if defined(_SILICON_LABS_32B_SERIES_3)
#undef SL_USBD_DRIVER_VBUS_SENSE_PORT
#undef SL_USBD_DRIVER_VBUS_SENSE_PIN
#define SL_USBD_DRIVER_VBUS_SENSE_PORT       SL_GPIO_PORT_D
#define SL_USBD_DRIVER_VBUS_SENSE_PIN        2
#endif

#endif // SL_USBD_DRIVER_CONFIG_H
