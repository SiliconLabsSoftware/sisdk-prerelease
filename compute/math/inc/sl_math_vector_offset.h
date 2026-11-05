/***************************************************************************//**
 * @file
 * @brief Math vector offset function.
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
#ifndef SL_MATH_VECTOR_OFFSET_H
#define SL_MATH_VECTOR_OFFSET_H

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
 *   Adds a constant offset to each element of a vector.
 *
 * @details
 *   This function performs the following operation: Z[i] = A[i] + offset.
 *   The input and output vectors must be of equal length.
 *
 * @param[in] input Input vector.
 * @param[in] offset Offset value.
 * @param[out] output Output vector.
 * @param[in] num_elements The number of elements in the input and output vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_offset_f16(const float16_t *input,
                                      const float16_t offset,
                                      float16_t *output,
                                      size_t num_elements);

/**
 * @brief
 *   Add an offset to a vector of 32 bit floats.
 *
 * @details
 *   This function performs the following operation: Z = A + offset.
 *
 * @param[in] input Input vector, input A.
 * @param[in] offset Offset added to every element.
 * @param[out] output Output vector, output Z.
 * @param[in] num_elements The number of elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_offset_f32(const float32_t *input,
                                      const float32_t offset,
                                      float32_t *output,
                                      size_t num_elements);

/**
 * @brief
 *   Add an offset to a vector of Q7 fixed-point values.
 *
 * @details
 *   This function performs the following operation: Z = A + offset.
 *   Results saturate to the `q7_t` range.
 *
 * @param[in] input Input vector, input A.
 * @param[in] offset Offset added to every element.
 * @param[out] output Output vector, output Z.
 * @param[in] num_elements The number of elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_offset_q7(const q7_t *input,
                                     const q7_t offset,
                                     q7_t *output,
                                     size_t num_elements);

/**
 * @brief
 *   Add an offset to a vector of Q15 fixed-point values.
 *
 * @details
 *   This function performs the following operation: Z = A + offset.
 *   Results saturate to the `q15_t` range.
 *
 * @param[in] input Input vector, input A.
 * @param[in] offset Offset added to every element.
 * @param[out] output Output vector, output Z.
 * @param[in] num_elements The number of elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_offset_q15(const q15_t *input,
                                      const q15_t offset,
                                      q15_t *output,
                                      size_t num_elements);

/**
 * @brief
 *   Add an offset to a vector of Q31 fixed-point values.
 *
 * @details
 *   This function performs the following operation: Z = A + offset.
 *   Results saturate to the `q31_t` range.
 *
 * @param[in] input Input vector, input A.
 * @param[in] offset Offset added to every element.
 * @param[out] output Output vector, output Z.
 * @param[in] num_elements The number of elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_vector_offset_q31(const q31_t *input,
                                      const q31_t offset,
                                      q31_t *output,
                                      size_t num_elements);

/** @} (end addtogroup math_vector) */

#ifdef __cplusplus
}
#endif

#endif // SL_MATH_VECTOR_OFFSET_H
