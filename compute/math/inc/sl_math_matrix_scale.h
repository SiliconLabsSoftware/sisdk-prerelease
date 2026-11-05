/***************************************************************************//**
 * @file
 * @brief Math matrix scale function.
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
#ifndef SL_MATH_MATRIX_SCALE_H
#define SL_MATH_MATRIX_SCALE_H

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
 *   Scale each element in a float16 matrix by a float16 scale.
 *
 * @details
 *   This function multiplies each element in the input matrix by `scale`
 *   and writes the result to the output matrix. The input and output
 *   matrices must be the same size.
 *
 * @param[in] input Input matrix.
 * @param[in] scale Scale value.
 * @param[out] output Output matrix.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_scale_f16(const sl_math_matrix_f16_t *input,
                                     float16_t scale,
                                     sl_math_matrix_f16_t *output);

/**
 * @brief
 *   Scale a matrix of 32 bit floats.
 *
 * @details
 *   This function performs the following operation: Z = A * scale.
 *
 * @param[in] input Input matrix, input A.
 * @param[in] scale Scale factor.
 * @param[out] output Output matrix, output Z.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_scale_f32(const sl_math_matrix_f32_t *input,
                                     float32_t scale,
                                     sl_math_matrix_f32_t *output);

/**
 * @brief
 *   Scale a matrix of Q15 fixed-point values.
 *
 * @details
 *   This function performs the following operation: Z = A * scale_fract * 2^shift.
 *   `scale_fract` is in Q15 format. Results saturate to the `q15_t` range.
 *
 * @param[in] input Input matrix, input A.
 * @param[in] scale_fract Fractional part of the scale factor.
 * @param[in] shift Number of bits to shift the result by.
 * @param[out] output Output matrix, output Z.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_scale_q15(const sl_math_matrix_q15_t *input,
                                     q15_t scale_fract,
                                     int8_t shift,
                                     sl_math_matrix_q15_t *output);

/**
 * @brief
 *   Scale a matrix of Q31 fixed-point values.
 *
 * @details
 *   This function performs the following operation: Z = A * scale_fract * 2^shift.
 *   `scale_fract` is in Q31 format. Results saturate to the `q31_t` range.
 *
 * @param[in] input Input matrix, input A.
 * @param[in] scale_fract Fractional part of the scale factor.
 * @param[in] shift Number of bits to shift the result by.
 * @param[out] output Output matrix, output Z.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_scale_q31(const sl_math_matrix_q31_t *input,
                                     q31_t scale_fract,
                                     int8_t shift,
                                     sl_math_matrix_q31_t *output);

/** @} (end addtogroup math_matrix) */

#ifdef __cplusplus
}
#endif

#endif // SL_MATH_MATRIX_SCALE_H
