/***************************************************************************//**
 * @file
 * @brief Math vector clamp function.
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
#ifndef SL_MATH_VECTOR_CLAMP_H
#define SL_MATH_VECTOR_CLAMP_H

#include "sl_status.h"
#include "sl_math_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup math_vector
 * @{
 ******************************************************************************/

/**
 * @brief
 *   Clamp all signed 8 bit integers in a vector to a range.
 *
 * @details
 *   Given a min/max value, this function makes sure that none of the elements
 *   in the input vector are < `min` or > `max`. Elements below `min` are set
 *   to `min`. Elements above `max` are set to `max`. The buffer is updated
 *   in place.
 *
 * @param[inout] data Vector with data values.
 * @param[in] num_elements The number of elements in the vector.
 * @param[in] min Minimum value. After the operation no element is < `min`.
 * @param[in] max Maximum value. After the operation no element is > `max`.
 *
 * @return
 *   @ref SL_STATUS_OK on success. On failure, an appropriate `sl_status_t`
 *   error code is returned.
 */
sl_status_t sl_math_clamp_i8(int8_t *data,
                             size_t num_elements,
                             int8_t min,
                             int8_t max);

/** @} (end addtogroup math_vector) */

#ifdef __cplusplus
}
#endif

#endif // SL_MATH_VECTOR_CLAMP_H
