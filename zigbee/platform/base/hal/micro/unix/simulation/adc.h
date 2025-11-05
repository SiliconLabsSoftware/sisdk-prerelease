/***************************************************************************//**
 * @file
 * @brief Header for Atmel AVR A/D converter.
 *
 * See @ref adc for documentation.
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
#ifndef __AVR_ADC_H__
#define __AVR_ADC_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  #ifndef __ADC_H__
    #error do not include this file directly - include plugin/adc/adc.h
  #endif
#endif

/** @addtogroup adc
 *@{
 */

/** @brief ADCReferenceType enumeration for the AVR Atmega microcontrollers.
 */
enum {
  /** AREF pin reference. */
  ADC_REF_AREF = 0x00,

  /** AVCC pin reference. */
  ADC_REF_AVCC = 0x40,

  /** Internal reference. */
  ADC_REF_INT  = 0xC0
};

/** @brief ADCRateType enumeration.
 *         Note: AVR only supports one rate: ADC_CONVERSION_TIME_US_256
 */
enum {
  /** Internal reference. */
  ADC_CONVERSION_TIME_US_256  = 42
};

/** @brief Atmega single-ended ADC channel values for ::ADCChannelType.
 */
enum {
  /** Channel 0 : ADC0 */
  ADC_SOURCE_ADC0     = 0x0,

  /** Channel 1 : ADC1 */
  ADC_SOURCE_ADC1     = 0x1,

  /** Channel 2 : ADC2 */
  ADC_SOURCE_ADC2     = 0x2,

  /** Channel 3 : ADC3 */
  ADC_SOURCE_ADC3     = 0x3,

  /** Channel 4 : ADC4 */
  ADC_SOURCE_ADC4     = 0x4,

  /** Channel 5 : ADC5 */
  ADC_SOURCE_ADC5     = 0x5,

  /** Channel 5 : ADC6 */
  ADC_SOURCE_ADC6     = 0x6,

  /** Channel 7 : ADC7 */
  ADC_SOURCE_ADC7     = 0x7,
};

/** @} END addtogroup
 */

#endif //__AVR_ADC_H__
