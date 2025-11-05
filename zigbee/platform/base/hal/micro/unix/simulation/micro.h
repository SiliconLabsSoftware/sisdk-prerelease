/***************************************************************************//**
 * @file
 * @brief Interface definitions for HAL micro simulation.
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
#ifndef __UNIX_MICRO_H__
#define __UNIX_MICRO_H__

#ifndef __MICRO_H__
#error do not include this file directly - include micro/micro.h
#endif

#define EM_NUM_SERIAL_PORTS 4

typedef union UU16 {
  uint16_t U16;
  int16_t S16;
  uint8_t U8[2];
  int8_t S8[2];
} UU16;

typedef union UU32 {
  uint32_t U32;
  int32_t S32;
  UU16 UU16[2];
  uint16_t U16[2];
  int16_t S16[2];
  uint8_t U8[4];
  int8_t S8[4];
} UU32;

void microSetSystemTime(uint32_t time);
void setMicroRebootHandler(void (*handler)(void));

void halCommonDelayMilliseconds(uint16_t msec);
uint32_t halGetWakeInfo(void);

/**
 * @brief Change the CTUNE value. Involves switching to HFRCO and turning off
 * the HFXO temporarily. (Only valid on EFR32)
 *
 * @param tune   Value to set CTUNE to.
 *
 */
#define halInternalSetCtune(tune)

/**
 * @brief Get the CTUNE value. (Only valid on EFR32)
 *
 * @return Current CTUNE value.
 *
 */
#define halInternalGetCtune() 0

/**
 * @return A 16-bit code identifying the base and extended cause of the reset
 * Only returns base definition shifted up in simulation
 */
#define halGetExtendedResetInfo() ((uint16_t) halGetResetInfo() << 8)

/**
 * @brief Defines conditions representing possible reasons for a reset.
 * Should mirror reset-def.h base reset reasons
 */
#define RESET_BASE_TYPE(extendedType)   ((uint8_t)(((extendedType) >> 8) & 0xFF))
#define RESET_EXTENDED_FIELD(extendedType) ((uint8_t)((extendedType) & 0xFF))

#define RESET_UNKNOWN             0x00   // Underterminable cause
#define RESET_FIB                 0x01   // Reset originated from the FIB bootloader
#define RESET_BOOTLOADER          0x02   // Reset relates to an Ember bootloader
#define RESET_EXTERNAL            0x03   // External reset trigger
#define RESET_POWERON             0x04   // Poweron reset type, supply voltage < power-on threshold
#define RESET_WATCHDOG            0x05   // Watchdog reset occurred
#define RESET_SOFTWARE            0x06   // Software triggered reset
#define RESET_CRASH               0x07   // Software crash
#define RESET_FLASH               0x08   // Flash failure cause reset
#define RESET_FATAL               0x09   // A non-recoverable fatal error occurred
#define RESET_FAULT               0x0A   // A access fault occurred
#define RESET_BROWNOUT            0x0B   // Brown out
// A few extended reset reasons
#define RESET_FIB_GO              (RESET_FIB << 8 | 0x01) // FIB bootloader caused a reset to main flash
#define RESET_BOOTLOADER_GO       (RESET_BOOTLOADER << 8 | 0x01) // Bootloader caused reset telling app to run
#define RESET_SOFTWARE_EM4        (RESET_SOFTWARE << 8 | 0x03) // App has been in EM4
// Em2xx compatible reset reasons
#define RESET_2xx_UNKNOWN           0   // cause not known
#define RESET_2xx_EXTERNAL          1   // reset pin was asserted
#define RESET_2xx_POWERON           2   // supply voltage was < power-on threshold
#define RESET_2xx_WATCHDOG          3   // watchdog low water mark caused NMI
#define RESET_2xx_ASSERT            6   // a self-check within the code failed
#define RESET_2xx_BOOTLOADER        9   // requested by bootloader
#define RESET_2xx_SOFTWARE          11  // reset requested by software
#define RESET_2xx_SOFTWARE_EM4      16  // coming out of em4

// the number of ticks (as returned from halCommonGetInt32uMillisecondTick)
// that represent an actual second.
#define MILLISECOND_TICKS_PER_SECOND 1000UL

#endif //__UNIX_MICRO_H__
