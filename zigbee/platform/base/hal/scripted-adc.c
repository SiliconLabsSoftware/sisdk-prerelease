/***************************************************************************//**
 * @file
 * @brief stubs for the ADC (Analog->Digital Converter) driver.
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
#include "stack/core/sl_zigbee_stack.h"
#include "hal/hal.h"

// Nothing actually scripted at this point.

void halInternalInitAdc(void)
{
}

void halInternalSleepAdc(void)
{
}

EmberStatus halStartAdcConversion(ADCUser id,
                                  ADCReferenceType reference,
                                  ADCChannelType reqChannel,
                                  ADCRateType rate)
{
  return SL_ZIGBEE_SUCCESS;
}

EmberStatus halRequestAdcData(ADCUser id, uint16_t *value)
{
  return SL_ZIGBEE_SUCCESS;
}

EmberStatus halReadAdcBlocking(ADCUser id, uint16_t *value)
{
  return SL_ZIGBEE_SUCCESS;
}

EmberStatus halAdcCalibrate(ADCUser id)
{
  return SL_ZIGBEE_ADC_CONVERSION_DONE;
}

int32_t halConvertValueToVolts(uint16_t data)
{
  // v = d / 100;
  return (int32_t)data * 100;
}
