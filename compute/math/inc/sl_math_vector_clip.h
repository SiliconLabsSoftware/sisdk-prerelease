/***************************************************************************//**
 * @file
 * @brief Math vector clip function.
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
#ifndef SL_MATH_VECTOR_CLIP_H
#define SL_MATH_VECTOR_CLIP_H

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
 *   Element-by-element clipping of a value.
 *
 * @details
 *   Each element is constrained to the inclusive range [`low`, `high`].
 *   Both vectors must be of equal length.
 *
 * @param[in] input Input vector, input A.
 * @param[out] output Output vector, output Z.
 * @param[in] low Lower bound.
 * @param[in] high Higher bound.
 * @param[in] num_elements Length of input and output vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_clip_f16(const float16_t *input,
                                    float16_t *output,
                                    float16_t low,
                                    float16_t high,
                                    size_t num_elements);

/**
 * @brief
 *   Clip a vector of 32 bit floats to a range.
 *
 * @details
 *   Every element of A is limited to [low, high] and written to Z.
 *
 * @param[in] input Input vector, input A.
 * @param[out] output Output vector, output Z.
 * @param[in] low Lower bound.
 * @param[in] high Upper bound.
 * @param[in] num_elements The number of elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_clip_f32(const float32_t *input,
                                    float32_t *output,
                                    float32_t low,
                                    float32_t high,
                                    size_t num_elements);

/**
 * @brief
 *   Clip a vector of Q7 fixed-point values to a range.
 *
 * @details
 *   Every element of A is limited to [low, high] and written to Z.
 *
 * @param[in] input Input vector, input A.
 * @param[out] output Output vector, output Z.
 * @param[in] low Lower bound.
 * @param[in] high Upper bound.
 * @param[in] num_elements The number of elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_clip_q7(const q7_t *input,
                                   q7_t *output,
                                   q7_t low,
                                   q7_t high,
                                   size_t num_elements);

/**
 * @brief
 *   Clip a vector of Q15 fixed-point values to a range.
 *
 * @details
 *   Every element of A is limited to [low, high] and written to Z.
 *
 * @param[in] input Input vector, input A.
 * @param[out] output Output vector, output Z.
 * @param[in] low Lower bound.
 * @param[in] high Upper bound.
 * @param[in] num_elements The number of elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_clip_q15(const q15_t *input,
                                    q15_t *output,
                                    q15_t low,
                                    q15_t high,
                                    size_t num_elements);

/**
 * @brief
 *   Clip a vector of Q31 fixed-point values to a range.
 *
 * @details
 *   Every element of A is limited to [low, high] and written to Z.
 *
 * @param[in] input Input vector, input A.
 * @param[out] output Output vector, output Z.
 * @param[in] low Lower bound.
 * @param[in] high Upper bound.
 * @param[in] num_elements The number of elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_clip_q31(const q31_t *input,
                                    q31_t *output,
                                    q31_t low,
                                    q31_t high,
                                    size_t num_elements);

/** @} (end addtogroup math_vector) */

#ifdef __cplusplus
}
#endif

#endif // SL_MATH_VECTOR_CLIP_H
