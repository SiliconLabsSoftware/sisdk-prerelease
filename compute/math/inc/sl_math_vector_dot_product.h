/***************************************************************************//**
 * @file
 * @brief Math vector dot product function.
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
#ifndef SL_MATH_VECTOR_DOT_PRODUCT_H
#define SL_MATH_VECTOR_DOT_PRODUCT_H

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
 *   Computes the dot product of two vectors.
 *
 * @details
 *   The vectors are multiplied element-by-element and then summed.
 *   Both vectors must have the same length. The scalar result is written
 *   to `output`.
 *
 * @param[in] input_a Input vector a.
 * @param[in] input_b Input vector b.
 * @param[in] num_elements The number of elements in the input vectors.
 * @param[out] output The result.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_dot_product_f16(const float16_t *input_a,
                                           const float16_t *input_b,
                                           size_t num_elements,
                                           float16_t *output);

/**
 * @brief
 *   Compute the dot product of two vectors of 32 bit floats.
 *
 * @details
 *   The vectors are multiplied element-by-element and then summed.
 *
 * @param[in] input_a Input vector a.
 * @param[in] input_b Input vector b.
 * @param[in] num_elements The number of elements in the input vectors.
 * @param[out] output Dot product result.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_dot_product_f32(const float32_t *input_a,
                                           const float32_t *input_b,
                                           size_t num_elements,
                                           float32_t *output);

/**
 * @brief
 *   Compute the dot product of two vectors of Q7 fixed-point values.
 *
 * @details
 *   The vectors are multiplied element-by-element and then summed.
 *   The result is the full-precision sum of products in 18.14 format.
 *
 * @param[in] input_a Input vector a.
 * @param[in] input_b Input vector b.
 * @param[in] num_elements The number of elements in the input vectors.
 * @param[out] output Dot product result.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_dot_product_q7(const q7_t *input_a,
                                          const q7_t *input_b,
                                          size_t num_elements,
                                          q31_t *output);

/**
 * @brief
 *   Compute the dot product of two vectors of Q15 fixed-point values.
 *
 * @details
 *   The vectors are multiplied element-by-element and then summed.
 *   The result is the full-precision sum of products in 34.30 format.
 *
 * @param[in] input_a Input vector a.
 * @param[in] input_b Input vector b.
 * @param[in] num_elements The number of elements in the input vectors.
 * @param[out] output Dot product result.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_dot_product_q15(const q15_t *input_a,
                                           const q15_t *input_b,
                                           size_t num_elements,
                                           q63_t *output);

/**
 * @brief
 *   Compute the dot product of two vectors of Q31 fixed-point values.
 *
 * @details
 *   The vectors are multiplied element-by-element and then summed.
 *   Each product is shifted right by 14 and accumulated in 16.48 format.
 *
 * @param[in] input_a Input vector a.
 * @param[in] input_b Input vector b.
 * @param[in] num_elements The number of elements in the input vectors.
 * @param[out] output Dot product result.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_dot_product_q31(const q31_t *input_a,
                                           const q31_t *input_b,
                                           size_t num_elements,
                                           q63_t *output);

/** @} (end addtogroup math_vector) */

#ifdef __cplusplus
}
#endif

#endif // SL_MATH_VECTOR_DOT_PRODUCT_H
