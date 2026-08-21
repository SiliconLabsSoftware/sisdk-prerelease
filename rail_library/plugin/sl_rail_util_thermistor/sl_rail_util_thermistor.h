/***************************************************************************//**
 * @file
 * @brief
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

#ifndef SL_RAIL_UTIL_THERMISTOR_H
#define SL_RAIL_UTIL_THERMISTOR_H

#if (_SILICON_LABS_32B_SERIES_3_CONFIG == 353)
#include "sl_rail_types.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Configure the thermistor and crystal compensation settings for use with RAIL.
 */
void sl_rail_util_thermistor_init(void);

#if (_SILICON_LABS_32B_SERIES_3_CONFIG == 353)
/**
 * @struct sl_rail_util_thermistor_coefficients_t
 * @brief Thermistor impedance-to-temperature polynomial coefficients.
 */
typedef struct sl_rail_util_thermistor_coefficients {
  /**
   * Constant term J in the denominator.
   */
  double j;
  /**
   * Linear coefficient K for ln(R / 1000).
   */
  double k;
  /**
   * Quadratic coefficient L for ln(R / 1000)^2.
   */
  double l;
  /**
   * Cubic coefficient M for ln(R / 1000)^3.
   */
  double m;
} sl_rail_util_thermistor_coefficients_t;

/**
 * Set the thermistor polynomial coefficients used by the default converter.
 *
 * @param[in] coefficients New coefficients.
 * @return Status code indicating success of the function call.
 */
sl_rail_status_t sl_rail_util_thermistor_set_coefficients(
  const sl_rail_util_thermistor_coefficients_t *coefficients);

/**
 * Get the thermistor polynomial coefficients used by the default converter.
 *
 * @param[out] coefficients Current coefficients.
 */
void sl_rail_util_thermistor_get_coefficients(
  sl_rail_util_thermistor_coefficients_t *coefficients);
#endif // (_SILICON_LABS_32B_SERIES_3_CONFIG == 353)

#ifdef __cplusplus
}
#endif

#endif // SL_RAIL_UTIL_THERMISTOR_H
