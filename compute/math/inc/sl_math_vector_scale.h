/***************************************************************************//**
 * @file
 * @brief Math vector scale function.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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
#ifndef SL_MATH_VECTOR_SCALE_H
#define SL_MATH_VECTOR_SCALE_H

#include "sl_status.h"
#include "sl_math_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup math_vector
 * @{
 ******************************************************************************/

/**
 * @brief
 *   Scale a vector of 16 bit floats by a float16 scale.
 *
 * @details
 *   This function performs the following operation: Z[i] = A[i] * scale.
 *   The input and output vectors must be of equal length.
 *
 * @param[in] input The input vector.
 * @param[in] scale The value by which to scale the vector.
 * @param[out] output Output vector, output Z.
 * @param[in] num_elements Length of input and output vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_scale_f16(const float16_t *input,
                                     float16_t scale,
                                     float16_t *output,
                                     size_t num_elements);

/**
 * @brief
 *   Scale a vector of 32 bit floats.
 *
 * @details
 *   This function performs the following operation: Z = A * scale.
 *
 * @param[in] input Input vector, input A.
 * @param[in] scale Scale factor.
 * @param[out] output Output vector, output Z.
 * @param[in] num_elements The number of elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_scale_f32(const float32_t *input,
                                     float32_t scale,
                                     float32_t *output,
                                     size_t num_elements);

/**
 * @brief
 *   Scale a vector of Q7 fixed-point values.
 *
 * @details
 *   This function performs the following operation: Z = A * scale_fract * 2^shift.
 *   `scale_fract` is in Q7 format. Results saturate to the `q7_t` range.
 *
 * @param[in] input Input vector, input A.
 * @param[in] scale_fract Fractional part of the scale factor.
 * @param[in] shift Number of bits to shift the result by.
 * @param[out] output Output vector, output Z.
 * @param[in] num_elements The number of elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_scale_q7(const q7_t *input,
                                    q7_t scale_fract,
                                    int8_t shift,
                                    q7_t *output,
                                    size_t num_elements);

/**
 * @brief
 *   Scale a vector of Q15 fixed-point values.
 *
 * @details
 *   This function performs the following operation: Z = A * scale_fract * 2^shift.
 *   `scale_fract` is in Q15 format. Results saturate to the `q15_t` range.
 *
 * @param[in] input Input vector, input A.
 * @param[in] scale_fract Fractional part of the scale factor.
 * @param[in] shift Number of bits to shift the result by.
 * @param[out] output Output vector, output Z.
 * @param[in] num_elements The number of elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_scale_q15(const q15_t *input,
                                     q15_t scale_fract,
                                     int8_t shift,
                                     q15_t *output,
                                     size_t num_elements);

/**
 * @brief
 *   Scale a vector of Q31 fixed-point values.
 *
 * @details
 *   This function performs the following operation: Z = A * scale_fract * 2^shift.
 *   `scale_fract` is in Q31 format. Results saturate to the `q31_t` range.
 *
 * @param[in] input Input vector, input A.
 * @param[in] scale_fract Fractional part of the scale factor.
 * @param[in] shift Number of bits to shift the result by.
 * @param[out] output Output vector, output Z.
 * @param[in] num_elements The number of elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_scale_q31(const q31_t *input,
                                     q31_t scale_fract,
                                     int8_t shift,
                                     q31_t *output,
                                     size_t num_elements);

/** @} (end addtogroup math_vector) */

#ifdef __cplusplus
}
#endif

#endif // SL_MATH_VECTOR_SCALE_H
