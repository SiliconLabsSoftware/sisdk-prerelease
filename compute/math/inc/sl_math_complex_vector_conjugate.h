/***************************************************************************//**
 * @file
 * @brief Math complex vector conjugate function.
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
#ifndef SL_MATH_COMPLEX_VECTOR_CONJUGATE_H
#define SL_MATH_COMPLEX_VECTOR_CONJUGATE_H

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
 *   Conjugates the elements of a complex data vector.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `float16_t` pairs.
 *
 * @param[in] input Input vector.
 * @param[out] output Output vector.
 * @param[in] num_elements The number of complex elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_conjugate_f16(float16_t *input,
                                                 float16_t *output,
                                                 size_t num_elements);

/**
 * @brief
 *   Conjugate a complex vector of 32 bit floats.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `float32_t` pairs.
 *
 * @param[in] input Input vector.
 * @param[out] output Output vector.
 * @param[in] num_elements The number of complex elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_conjugate_f32(const float32_t *input,
                                                 float32_t *output,
                                                 size_t num_elements);

/**
 * @brief
 *   Conjugate a complex vector of Q15 fixed-point values.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `q15_t` pairs.
 *   Negating the most negative imaginary part saturates.
 *
 * @param[in] input Input vector.
 * @param[out] output Output vector.
 * @param[in] num_elements The number of complex elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_conjugate_q15(const q15_t *input,
                                                 q15_t *output,
                                                 size_t num_elements);

/**
 * @brief
 *   Conjugate a complex vector of Q31 fixed-point values.
 *
 * @details
 *   Complex elements are interleaved real and imaginary `q31_t` pairs.
 *   Negating the most negative imaginary part saturates.
 *
 * @param[in] input Input vector.
 * @param[out] output Output vector.
 * @param[in] num_elements The number of complex elements in the vectors.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_complex_vector_conjugate_q31(const q31_t *input,
                                                 q31_t *output,
                                                 size_t num_elements);

/** @} (end addtogroup math_vector) */

#ifdef __cplusplus
}
#endif

#endif // SL_MATH_COMPLEX_VECTOR_CONJUGATE_H
