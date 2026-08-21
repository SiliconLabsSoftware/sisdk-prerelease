/***************************************************************************//**
 * @file
 * @brief Thermistor conversion functions provided to the customer as source for
 *   highest level of customization.
 * @details This file contains the logic that converts thermistor impedance
 * to temperature in Celsius.
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#include "sl_rail.h"
#include "rail.h"

#if SL_RAIL_SUPPORTS_EXTERNAL_THERMISTOR

#include "sl_rail_util_thermistor.h"
#include <math.h>

#if (_SILICON_LABS_32B_SERIES_3_CONFIG == 353)
// Defaults from RF_temperature_compensation_Rev_1p9 slide 44.
static sl_rail_util_thermistor_coefficients_t thermistorCoefficients = {
  .j = 0.002310296,
  .k = 0.000215353,
  .l = 0.0000024537,
  .m = 0.0,
};

// Local fallback used if the RFHAL coefficient resolver is unavailable.
static const sl_rail_hfxo_temp_compensation_coefficients_t hfxoTempCompDefaultCoefficients = {
  .force_coefficients = false,
  .a = 1.05e-4f,
  .b = 0.0f,
  .c = 2.82e-4f,
  .d = 0.0f,
  .e = -0.66f,
  .f = 0.0f,
  .t0 = 30.0f,
  .df85g8 = 0.0f,
  .f30mg8 = 0.0f,
};
#endif

#ifndef SLI_LIBRAIL_BUILD
// Do not put an sl_rail_util_thermistor_init() implementation into librail.
void sl_rail_util_thermistor_init(void)
{
#if (defined(GPIO_THMSW_EN_PORT) && defined(GPIO_THMSW_EN_PIN))
  sl_rail_hfxo_thermistor_config_t hfxo_thermistor_config = {
    .port = GPIO_THMSW_EN_PORT,
    .pin = GPIO_THMSW_EN_PIN
  };
  (void)sl_rail_config_hfxo_thermistor(SL_RAIL_EFR32_HANDLE, &hfxo_thermistor_config);
#endif// (defined(GPIO_THMSW_EN_PORT) && defined(GPIO_THMSW_EN_PIN))
}
#endif//SLI_LIBRAIL_BUILD

/* This macro is defined when Silicon Labs builds this into the library as WEAK
   to ensure it can be overriden by customer versions of these functions. The macro
   should *not* be defined in a customer build. */
#if (_SILICON_LABS_32B_SERIES_3_CONFIG == 353)
SLI_LIBRAIL_WEAK
sl_rail_status_t sl_rail_util_thermistor_set_coefficients(
  const sl_rail_util_thermistor_coefficients_t *coefficients)
{
  if (coefficients == NULL) {
    return SL_RAIL_STATUS_INVALID_PARAMETER;
  }

  thermistorCoefficients = *coefficients;
  return SL_RAIL_STATUS_NO_ERROR;
}

SLI_LIBRAIL_WEAK
void sl_rail_util_thermistor_get_coefficients(
  sl_rail_util_thermistor_coefficients_t *coefficients)
{
  if (coefficients != NULL) {
    *coefficients = thermistorCoefficients;
  }
}
#endif

SLI_LIBRAIL_WEAK
RAIL_Status_t RAIL_ConvertThermistorImpedance(RAIL_Handle_t railHandle,
                                              uint32_t thermistorImpedance,
                                              int16_t *thermistorTemperatureC)
{
  (void) railHandle;
  if ((thermistorTemperatureC == NULL) || (thermistorImpedance == 0U)) {
    return RAIL_STATUS_INVALID_PARAMETER;
  }

#if (_SILICON_LABS_32B_SERIES_3_CONFIG == 353)
  // Temperature(C) = 1 / (J + K*ln(R/1000) + L*ln(R/1000)^2
  //                    + M*ln(R/1000)^3) - 273.15
  // Use float/logf so soft-float and DP-hard builds avoid double log() and its
  // large libm tables. Public Steinhart coeffs remain double; cast at use.
  float logParam = (float)thermistorImpedance / 1000.0f;
  float impedanceLn = logf(logParam);
  float impedanceLn2 = impedanceLn * impedanceLn;
  float impedanceLn3 = impedanceLn2 * impedanceLn;
  float denominator = (float)thermistorCoefficients.j
                      + ((float)thermistorCoefficients.k * impedanceLn)
                      + ((float)thermistorCoefficients.l * impedanceLn2)
                      + ((float)thermistorCoefficients.m * impedanceLn3);
  if ((denominator == 0.0f) || !isfinite(denominator)) {
    return RAIL_STATUS_INVALID_PARAMETER;
  }

  float temperatureC = (1.0f / denominator) - 273.15f;
#else
  // T = 4200 / (log(Rtherm/100000) + 4200/(273.15+25)) - 273.15
  float logParam = (float)thermistorImpedance / 100000.0f;
  float impedanceLn = logf(logParam);
  float temperatureC =
    4200.0f / (impedanceLn + 4200.0f / (273.15f + 25.0f)) - 273.15f;
#endif

  if (!isfinite(temperatureC)) {
    return RAIL_STATUS_INVALID_PARAMETER;
  }

  // Multiply by 8 to convert to eighth of Celsius degrees
  *thermistorTemperatureC = (int16_t)roundf(temperatureC * 8.0f);
  return RAIL_STATUS_NO_ERROR;
}

SLI_LIBRAIL_WEAK
RAIL_Status_t RAIL_ComputeHFXOPPMError(RAIL_Handle_t railHandle,
                                       int16_t crystalTemperatureC,
                                       int8_t *crystalPPMError)
{
  if (crystalPPMError == NULL) {
    return RAIL_STATUS_INVALID_PARAMETER;
  }

#if (_SILICON_LABS_32B_SERIES_3_CONFIG == 353)
  // Ask RFHAL for the effective coefficients so this plugin does not need to
  // know whether they came from the application, PTE DEVINFO, or defaults.
  sl_rail_hfxo_temp_compensation_coefficients_t coefficients = hfxoTempCompDefaultCoefficients;
  if (sl_rail_get_hfxo_temp_compensation_coefficients(
        (sl_rail_handle_t)railHandle,
        &coefficients) != SL_RAIL_STATUS_NO_ERROR) {
    coefficients = hfxoTempCompDefaultCoefficients;
  }

  // Slide-45 polynomial:
  // A*(T-T0)^3 + (B*DF85G8+C)*(T-T0)^2
  // + (D*DF85G8+E)*(T-T0) + F + F30MG8.
  // float is enough for int8 ppm; avoid double poly math on M33.
  float deltaRefTempC = (float)crystalTemperatureC - coefficients.t0;
  float deltaRefTempC2 = deltaRefTempC * deltaRefTempC;
  float deltaRefTempC3 = deltaRefTempC2 * deltaRefTempC;
  float crystalErrorPpm =
    (coefficients.a * deltaRefTempC3)
    + (((coefficients.b * coefficients.df85g8) + coefficients.c) * deltaRefTempC2)
    + (((coefficients.d * coefficients.df85g8) + coefficients.e) * deltaRefTempC)
    + coefficients.f
    + coefficients.f30mg8;
#else
  (void) railHandle;
  // Equation is:
  // f(T) = 1.05*10^-4(T-T0)^3 + 1.0*10^-4(T-T0)^2 - 0.74(T-T0) - 0.35, Reference to 30C
  // Cache T-T0. Prefer multiplies over pow() so non-353 builds avoid libm pow.
  float deltaRefTempC = (float)crystalTemperatureC - 30.0f;
  float deltaRefTempC2 = deltaRefTempC * deltaRefTempC;
  float crystalErrorPpm = (1.05e-4f * deltaRefTempC2 * deltaRefTempC)
                          + (1.0e-4f * deltaRefTempC2)
                          - (0.74f * deltaRefTempC)
                          - 0.35f;
#endif

  if (!isfinite(crystalErrorPpm)) {
    return RAIL_STATUS_INVALID_PARAMETER;
  }

  // int8_t is the public ppm type, but casting an out-of-range rounded value is
  // undefined. Also reserve -128 for SL_RAIL_INVALID_PPM_VALUE so a real error
  // is never rejected by sli_rfhal_compensate_hfxo().
  float roundedPpm = roundf(crystalErrorPpm);
  *crystalPPMError = (int8_t)((roundedPpm > 127.0f)
                              ? 127.0f
                              : ((roundedPpm < -127.0f) ? -127.0f : roundedPpm));
  return RAIL_STATUS_NO_ERROR;
}

// Below needed for RAIL 3

SLI_LIBRAIL_WEAK
sl_rail_status_t sl_railcb_convert_thermistor_impedance(sl_rail_handle_t rail_handle,
                                                        uint32_t thermistor_impedance_ohms,
                                                        int16_t *p_thermistor_temperature_8c)
{
  return (sl_rail_status_t)RAIL_ConvertThermistorImpedance((RAIL_Handle_t)rail_handle,
                                                           thermistor_impedance_ohms,
                                                           p_thermistor_temperature_8c);
}

SLI_LIBRAIL_WEAK
sl_rail_status_t sl_railcb_compute_hfxo_error_ppm(sl_rail_handle_t rail_handle,
                                                  int16_t crystal_temperature_c,
                                                  int8_t *p_crystal_error_ppm)
{
  return (sl_rail_status_t)RAIL_ComputeHFXOPPMError((RAIL_Handle_t)rail_handle,
                                                    crystal_temperature_c,
                                                    p_crystal_error_ppm);
}

#endif // SL_RAIL_SUPPORTS_EXTERNAL_THERMISTOR
