/***************************************************************************//**
 * @file
 * @brief Math matrix definitions.
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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
#ifndef SL_MATH_MATRIX_H
#define SL_MATH_MATRIX_H

#include <stddef.h>
#include "sl_status.h"
#include "sl_math_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief
 *  Matrix data type.
 */
typedef struct {
  size_t    num_rows;   ///< The number of rows in the matrix.
  size_t    num_cols;   ///< The number of columns in the matrix.
  float16_t *data;      ///< Pointer to the matrix data.
} sl_math_matrix_f16_t;

/**
 * @brief
 *  Matrix data type for `float32_t` elements.
 */
typedef struct {
  size_t    num_rows;   ///< The number of rows in the matrix.
  size_t    num_cols;   ///< The number of columns in the matrix.
  float32_t *data;      ///< Pointer to the matrix data.
} sl_math_matrix_f32_t;

/**
 * @brief
 *  Matrix data type for `q7_t` elements.
 */
typedef struct {
  size_t num_rows;      ///< The number of rows in the matrix.
  size_t num_cols;      ///< The number of columns in the matrix.
  q7_t   *data;         ///< Pointer to the matrix data.
} sl_math_matrix_q7_t;

/**
 * @brief
 *  Matrix data type for `q15_t` elements.
 */
typedef struct {
  size_t num_rows;      ///< The number of rows in the matrix.
  size_t num_cols;      ///< The number of columns in the matrix.
  q15_t  *data;         ///< Pointer to the matrix data.
} sl_math_matrix_q15_t;

/**
 * @brief
 *  Matrix data type for `q31_t` elements.
 */
typedef struct {
  size_t num_rows;      ///< The number of rows in the matrix.
  size_t num_cols;      ///< The number of columns in the matrix.
  q31_t  *data;         ///< Pointer to the matrix data.
} sl_math_matrix_q31_t;

/***************************************************************************//**
 * @addtogroup math_matrix
 * @{
 ******************************************************************************/

/**
 * @brief
 *  Matrix initialization.
 *
 * @param[in] matrix Pointer to a matrix.
 * @param[in] num_rows The number of rows in the matrix.
 * @param[in] num_cols The number of cols in the matrix.
 * @param[in] data A pointer to the matrix data.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_init_f16(sl_math_matrix_f16_t *matrix, size_t num_rows, size_t num_cols, float16_t *data);

/**
 * @brief
 *  Initialize a `float32_t` matrix descriptor.
 *
 * @param[in] matrix Pointer to a matrix.
 * @param[in] num_rows The number of rows in the matrix.
 * @param[in] num_cols The number of cols in the matrix.
 * @param[in] data A pointer to the matrix data.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_init_f32(sl_math_matrix_f32_t *matrix, size_t num_rows, size_t num_cols, float32_t *data);

/**
 * @brief
 *  Initialize a `q7_t` matrix descriptor.
 *
 * @param[in] matrix Pointer to a matrix.
 * @param[in] num_rows The number of rows in the matrix.
 * @param[in] num_cols The number of cols in the matrix.
 * @param[in] data A pointer to the matrix data.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_init_q7(sl_math_matrix_q7_t *matrix, size_t num_rows, size_t num_cols, q7_t *data);

/**
 * @brief
 *  Initialize a `q15_t` matrix descriptor.
 *
 * @param[in] matrix Pointer to a matrix.
 * @param[in] num_rows The number of rows in the matrix.
 * @param[in] num_cols The number of cols in the matrix.
 * @param[in] data A pointer to the matrix data.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_init_q15(sl_math_matrix_q15_t *matrix, size_t num_rows, size_t num_cols, q15_t *data);

/**
 * @brief
 *  Initialize a `q31_t` matrix descriptor.
 *
 * @param[in] matrix Pointer to a matrix.
 * @param[in] num_rows The number of rows in the matrix.
 * @param[in] num_cols The number of cols in the matrix.
 * @param[in] data A pointer to the matrix data.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_matrix_init_q31(sl_math_matrix_q31_t *matrix, size_t num_rows, size_t num_cols, q31_t *data);

/** @} (end addtogroup math_matrix) */

#ifdef __cplusplus
}
#endif

#endif // SL_MATH_MATRIX_H
