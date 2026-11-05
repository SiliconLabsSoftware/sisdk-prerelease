/***************************************************************************//**
 * @file
 * @brief Math matrix transpose functions.
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
#ifndef SL_MATH_MATRIX_TRANSPOSE_H
#define SL_MATH_MATRIX_TRANSPOSE_H

#include "sl_status.h"
#include "sl_math_matrix.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup math_matrix
 * @{
 ******************************************************************************/

/**
 * @brief
 *   Transpose a matrix.
 *
 * @details
 *   This function fills the output matrix with the transposed version of
 *   the input matrix. The output row count must equal the input column
 *   count, and the output column count must equal the input row count.
 *
 * @param[in] input Input matrix.
 * @param[out] output Output matrix.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_transpose_f16(const sl_math_matrix_f16_t *input,
                                         sl_math_matrix_f16_t *output);

/**
 * @brief
 *   Transpose a complex float16 matrix.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `float16_t` pairs.
 *   This function fills the output matrix with the transposed version of
 *   the input matrix. The output row count must equal the input column
 *   count, and the output column count must equal the input row count.
 *
 * @param[in] input Input matrix.
 * @param[out] output Output matrix.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_matrix_transpose_f16(const sl_math_matrix_f16_t *input,
                                                 sl_math_matrix_f16_t *output);

/**
 * @brief
 *   Transpose a matrix of 32 bit floats.
 *
 * @details
 *   This function fills the output matrix with the transposed version of
 *   the input matrix. The output row count must equal the input column
 *   count, and the output column count must equal the input row count.
 *
 * @param[in] input Input matrix.
 * @param[out] output Output matrix.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_transpose_f32(const sl_math_matrix_f32_t *input,
                                         sl_math_matrix_f32_t *output);

/**
 * @brief
 *   Transpose a matrix of Q7 fixed-point values.
 *
 * @details
 *   This function fills the output matrix with the transposed version of
 *   the input matrix. The output row count must equal the input column
 *   count, and the output column count must equal the input row count.
 *
 * @param[in] input Input matrix.
 * @param[out] output Output matrix.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_transpose_q7(const sl_math_matrix_q7_t *input,
                                        sl_math_matrix_q7_t *output);

/**
 * @brief
 *   Transpose a matrix of Q15 fixed-point values.
 *
 * @details
 *   This function fills the output matrix with the transposed version of
 *   the input matrix. The output row count must equal the input column
 *   count, and the output column count must equal the input row count.
 *
 * @param[in] input Input matrix.
 * @param[out] output Output matrix.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_transpose_q15(const sl_math_matrix_q15_t *input,
                                         sl_math_matrix_q15_t *output);

/**
 * @brief
 *   Transpose a matrix of Q31 fixed-point values.
 *
 * @details
 *   This function fills the output matrix with the transposed version of
 *   the input matrix. The output row count must equal the input column
 *   count, and the output column count must equal the input row count.
 *
 * @param[in] input Input matrix.
 * @param[out] output Output matrix.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_transpose_q31(const sl_math_matrix_q31_t *input,
                                         sl_math_matrix_q31_t *output);

/**
 * @brief
 *   Transpose a complex matrix of 32 bit floats.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `float32_t` pairs, and the
 *   row and column counts are in complex elements. The output row count must
 *   equal the input column count, and the output column count must equal the
 *   input row count.
 *
 * @param[in] input Input matrix.
 * @param[out] output Output matrix.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_matrix_transpose_f32(const sl_math_matrix_f32_t *input,
                                                 sl_math_matrix_f32_t *output);

/**
 * @brief
 *   Transpose a complex matrix of Q15 fixed-point values.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `q15_t` pairs, and the
 *   row and column counts are in complex elements. The output row count must
 *   equal the input column count, and the output column count must equal the
 *   input row count.
 *
 * @param[in] input Input matrix.
 * @param[out] output Output matrix.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_matrix_transpose_q15(const sl_math_matrix_q15_t *input,
                                                 sl_math_matrix_q15_t *output);

/**
 * @brief
 *   Transpose a complex matrix of Q31 fixed-point values.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `q31_t` pairs, and the
 *   row and column counts are in complex elements. The output row count must
 *   equal the input column count, and the output column count must equal the
 *   input row count.
 *
 * @param[in] input Input matrix.
 * @param[out] output Output matrix.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_matrix_transpose_q31(const sl_math_matrix_q31_t *input,
                                                 sl_math_matrix_q31_t *output);

/** @} (end addtogroup math_matrix) */

#ifdef __cplusplus
}
#endif

#endif // SL_MATH_MATRIX_TRANSPOSE_H
