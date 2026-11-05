/***************************************************************************//**
 * @file
 * @brief Math functions.
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
#ifndef SL_MATH_H
#define SL_MATH_H

#include "sl_math_types.h"
#include "sl_math_matrix.h"

// Matrix
#include "sl_math_complex_matrix_mult.h"
#include "sl_math_matrix_add.h"
#include "sl_math_matrix_mult.h"
#include "sl_math_matrix_scale.h"
#include "sl_math_matrix_sub.h"
#include "sl_math_matrix_transpose.h"
#include "sl_math_matrix_vector_mult.h"

// Vector
#include "sl_math_complex_vector_conjugate.h"
#include "sl_math_complex_vector_dot_product.h"
#include "sl_math_complex_vector_magnitude_squared.h"
#include "sl_math_complex_vector_mult.h"
#include "sl_math_vector_abs.h"
#include "sl_math_vector_add.h"
#include "sl_math_vector_clamp.h"
#include "sl_math_vector_clip.h"
#include "sl_math_vector_copy.h"
#include "sl_math_vector_dot_product.h"
#include "sl_math_vector_fill.h"
#include "sl_math_vector_mult.h"
#include "sl_math_vector_negate.h"
#include "sl_math_vector_offset.h"
#include "sl_math_vector_scale.h"
#include "sl_math_vector_sub.h"

/* *INDENT-OFF* */
/************ THIS SECTION IS FOR DOCUMENTATION ONLY !**********************//**
 *
 * @defgroup math Math functions
 * @defgroup math_vector Vector functions
 * @ingroup math
 * @defgroup math_matrix Matrix functions
 * @ingroup math
 *
 * @note
 *   There is no `sl_math_init()`. Driver initialization, such as
 *   `sli_mvp_init()`, stays in the driver component.
 *
 */

#endif // SL_MATH_H
