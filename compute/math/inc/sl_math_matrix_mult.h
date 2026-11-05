/***************************************************************************//**
 * @file
 * @brief Math matrix multiply function.
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
#ifndef SL_MATH_MATRIX_MULT_H
#define SL_MATH_MATRIX_MULT_H

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
 *   Multiply two matrices of 16 bit floats.
 *
 * @details
 *   This function performs the following operation: Z = A * B
 *   (matrix multiplication). The number of columns of the first matrix must
 *   equal the number of rows of the second matrix. The output row count must
 *   match matrix A, and the output column count must match matrix B.
 *
 * @param[in] input_a First input matrix, input A.
 * @param[in] input_b Second input matrix, input B.
 * @param[out] output Output matrix, output Z.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_mult_f16(const sl_math_matrix_f16_t *input_a,
                                    const sl_math_matrix_f16_t *input_b,
                                    sl_math_matrix_f16_t *output);

/**
 * @brief
 *   Multiply two matrices of 32 bit floats.
 *
 * @details
 *   Performs Z = A * B. The number of columns of A must equal the number of rows
 *   of B. The output must not overlap the inputs.
 *
 * @param[in] input_a First input matrix, input A.
 * @param[in] input_b Second input matrix, input B.
 * @param[out] output Output matrix, output Z.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_mult_f32(const sl_math_matrix_f32_t *input_a,
                                    const sl_math_matrix_f32_t *input_b,
                                    sl_math_matrix_f32_t *output);

/**
 * @brief
 *   Multiply two matrices of Q15 fixed-point values.
 *
 * @details
 *   Performs Z = A * B. The number of columns of A must equal the number of rows
 *   of B. The output must not overlap the inputs.
 *   Products are accumulated in 64 bits; the result is shifted right by 15 and
 *   saturated to the `q15_t` range.
 *
 * @param[in] input_a First input matrix, input A.
 * @param[in] input_b Second input matrix, input B.
 * @param[out] output Output matrix, output Z.
 * @param[in] workspace Scratch buffer of `input_b->num_rows * input_b->num_cols` elements.
 *   Required by every backend so the contract does not change with the backend.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_mult_q15(const sl_math_matrix_q15_t *input_a,
                                    const sl_math_matrix_q15_t *input_b,
                                    sl_math_matrix_q15_t *output,
                                    q15_t *workspace);

/**
 * @brief
 *   Multiply two matrices of Q31 fixed-point values.
 *
 * @details
 *   Performs Z = A * B. The number of columns of A must equal the number of rows
 *   of B. The output must not overlap the inputs.
 *   Products are accumulated in 64 bits and the result is shifted right by 31.
 *
 * @param[in] input_a First input matrix, input A.
 * @param[in] input_b Second input matrix, input B.
 * @param[out] output Output matrix, output Z.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_mult_q31(const sl_math_matrix_q31_t *input_a,
                                    const sl_math_matrix_q31_t *input_b,
                                    sl_math_matrix_q31_t *output);

/** @} (end addtogroup math_matrix) */

#ifdef __cplusplus
}
#endif

#endif // SL_MATH_MATRIX_MULT_H
