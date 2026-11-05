/***************************************************************************//**
 * @file
 * @brief Math complex vector dot product function.
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
#ifndef SL_MATH_COMPLEX_VECTOR_DOT_PRODUCT_H
#define SL_MATH_COMPLEX_VECTOR_DOT_PRODUCT_H

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
 *   Computes the dot product of two complex vectors.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `float16_t` pairs.
 *   The vectors are multiplied element-by-element and then summed.
 *
 * @param[in] input_a Input vector a.
 * @param[in] input_b Input vector b.
 * @param[in] num_elements The number of complex elements in the input vectors.
 * @param[out] output Dot product result.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_dot_product_f16(float16_t *input_a,
                                                   float16_t *input_b,
                                                   size_t num_elements,
                                                   float16_t *output);

/**
 * @brief
 *   Compute the dot product of two complex vectors of 32 bit floats.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `float32_t` pairs. The
 *   real part of the result is written to `output[0]` and the imaginary part
 *   to `output[1]`.
 *
 * @param[in] input_a Input vector a.
 * @param[in] input_b Input vector b.
 * @param[in] num_elements The number of complex elements in the input vectors.
 * @param[out] output Two-element dot product result.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_dot_product_f32(const float32_t *input_a,
                                                   const float32_t *input_b,
                                                   size_t num_elements,
                                                   float32_t *output);

/**
 * @brief
 *   Compute the dot product of two complex vectors of Q15 fixed-point values.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `q15_t` pairs. The
 *   real part of the result is written to `output[0]` and the imaginary part
 *   to `output[1]`.
 *   The results are in 8.24 format.
 *
 * @param[in] input_a Input vector a.
 * @param[in] input_b Input vector b.
 * @param[in] num_elements The number of complex elements in the input vectors.
 * @param[out] output Two-element dot product result.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_dot_product_q15(const q15_t *input_a,
                                                   const q15_t *input_b,
                                                   size_t num_elements,
                                                   q31_t *output);

/**
 * @brief
 *   Compute the dot product of two complex vectors of Q31 fixed-point values.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `q31_t` pairs. The
 *   real part of the result is written to `output[0]` and the imaginary part
 *   to `output[1]`.
 *   The results are in 16.48 format.
 *
 * @param[in] input_a Input vector a.
 * @param[in] input_b Input vector b.
 * @param[in] num_elements The number of complex elements in the input vectors.
 * @param[out] output Two-element dot product result.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_dot_product_q31(const q31_t *input_a,
                                                   const q31_t *input_b,
                                                   size_t num_elements,
                                                   q63_t *output);

/** @} (end addtogroup math_vector) */

#ifdef __cplusplus
}
#endif

#endif // SL_MATH_COMPLEX_VECTOR_DOT_PRODUCT_H
