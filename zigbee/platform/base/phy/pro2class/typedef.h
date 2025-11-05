/***************************************************************************//**
 * @file
 * @brief Type definitions to map PRO2+ API to ember PHY
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
#ifndef __PHY_PRO2PLUS_TYPEDEF_H__
#define __PHY_PRO2PLUS_TYPEDEF_H__

#define  xdata
#define  code

typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint32_t u32;
typedef int32_t i32;

// N.B. This include file does not address endianness differences between
//      PRO2+ big-endian API and host environment if little-endian.

#endif//__PHY_PRO2PLUS_TYPEDEF_H__
