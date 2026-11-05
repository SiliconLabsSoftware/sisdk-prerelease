/***************************************************************************//**
 * @file
 * @brief Math complex vector multiply functions.
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
#ifndef SL_MATH_COMPLEX_VECTOR_MULT_H
#define SL_MATH_COMPLEX_VECTOR_MULT_H

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
 *   Multiply a vector of complex float16 values by a vector of real float16 values.
 *
 * @details
 *   This function performs the following operation: Z = A * B.
 *   Complex elements are interleaved real and imaginary `float16_t` pairs.
 *   Both vectors must have the same number of elements.
 *
 * @param[in] input_a The complex vector, input A.
 * @param[in] input_b The real vector, input B.
 * @param[out] output The complex output vector, output Z.
 * @param[in] num_elements The number of elements in the input and output vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_mult_real_f16(const float16_t *input_a,
                                                 const float16_t *input_b,
                                                 float16_t *output,
                                                 size_t num_elements);

/**
 * @brief
 *   Multiply complex float16 vectors.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `float16_t` pairs.
 *   Both input vectors and the output vector have the same length.
 *
 * @param[in] input_a Complex input vector A.
 * @param[in] input_b Complex input vector B.
 * @param[out] output Complex output vector.
 * @param[in] num_elements The number of complex elements in the input and output vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_mult_f16(const float16_t *input_a,
                                            const float16_t *input_b,
                                            float16_t *output,
                                            size_t num_elements);

/**
 * @brief
 *   Multiply complex vectors of 32 bit floats.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `float32_t` pairs.
 *   Both input vectors and the output vector have the same length.
 *
 * @param[in] input_a Complex input vector A.
 * @param[in] input_b Complex input vector B.
 * @param[out] output Complex output vector.
 * @param[in] num_elements The number of complex elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_mult_f32(const float32_t *input_a,
                                            const float32_t *input_b,
                                            float32_t *output,
                                            size_t num_elements);

/**
 * @brief
 *   Multiply a complex vector of 32 bit floats by a real vector.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `float32_t` pairs. Each
 *   complex element of A is multiplied by the real element of B with the same index.
 *
 * @param[in] input_a The complex vector, input A.
 * @param[in] input_b The real vector, input B.
 * @param[out] output The complex output vector, output Z.
 * @param[in] num_elements The number of elements in the input and output vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_mult_real_f32(const float32_t *input_a,
                                                 const float32_t *input_b,
                                                 float32_t *output,
                                                 size_t num_elements);

/**
 * @brief
 *   Multiply complex vectors of Q15 fixed-point values.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `q15_t` pairs.
 *   Both input vectors and the output vector have the same length.
 *   The output is in 3.13 format.
 *
 * @param[in] input_a Complex input vector A.
 * @param[in] input_b Complex input vector B.
 * @param[out] output Complex output vector.
 * @param[in] num_elements The number of complex elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_mult_q15(const q15_t *input_a,
                                            const q15_t *input_b,
                                            q15_t *output,
                                            size_t num_elements);

/**
 * @brief
 *   Multiply a complex vector of Q15 fixed-point values by a real vector.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `q15_t` pairs. Each
 *   complex element of A is multiplied by the real element of B with the same index.
 *
 * @param[in] input_a The complex vector, input A.
 * @param[in] input_b The real vector, input B.
 * @param[out] output The complex output vector, output Z.
 * @param[in] num_elements The number of elements in the input and output vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_mult_real_q15(const q15_t *input_a,
                                                 const q15_t *input_b,
                                                 q15_t *output,
                                                 size_t num_elements);

/**
 * @brief
 *   Multiply complex vectors of Q31 fixed-point values.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `q31_t` pairs.
 *   Both input vectors and the output vector have the same length.
 *   The output is in 3.29 format.
 *
 * @param[in] input_a Complex input vector A.
 * @param[in] input_b Complex input vector B.
 * @param[out] output Complex output vector.
 * @param[in] num_elements The number of complex elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_mult_q31(const q31_t *input_a,
                                            const q31_t *input_b,
                                            q31_t *output,
                                            size_t num_elements);

/**
 * @brief
 *   Multiply a complex vector of Q31 fixed-point values by a real vector.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `q31_t` pairs. Each
 *   complex element of A is multiplied by the real element of B with the same index.
 *
 * @param[in] input_a The complex vector, input A.
 * @param[in] input_b The real vector, input B.
 * @param[out] output The complex output vector, output Z.
 * @param[in] num_elements The number of elements in the input and output vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_mult_real_q31(const q31_t *input_a,
                                                 const q31_t *input_b,
                                                 q31_t *output,
                                                 size_t num_elements);

/** @} (end addtogroup math_vector) */

#ifdef __cplusplus
}
#endif

#endif // SL_MATH_COMPLEX_VECTOR_MULT_H
