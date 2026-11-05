/***************************************************************************//**
 * @file
 * @brief Silicon Labs Secure Engine Manager BigInt Math API.
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
#ifndef SL_SE_MANAGER_MATH_H
#define SL_SE_MANAGER_MATH_H

#include "sli_se_manager_features.h"
#include "sli_se_manager_mailbox.h"

#if defined(SLI_SE_COMMAND_BIGINT_MATH)

/// @addtogroup sl_se_manager
/// @{

/***************************************************************************//**
 * @addtogroup sl_se_manager_math BigInt math
 *
 * @brief
 *   Modular big-integer arithmetic using the Secure Engine.
 *
 * @details
 *   These functions execute modular add, subtract, multiply, divide, and
 *   modular inverse on the SE. Operands are unsigned big-endian integers
 *   of one shared length, at least #SL_SE_BIGINT_MATH_MIN_OPERAND_SIZE
 *   bytes. The result is that same length in bytes.
 *
 *   Available only with runtime Secure Engine firmware (HSE mailbox).
 *
 * @{
 ******************************************************************************/

#include "sl_se_manager_types.h"
#include "sl_status.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Minimum operand and modulus size accepted by the BigInt math APIs.
#define SL_SE_BIGINT_MATH_MIN_OPERAND_SIZE  8

/// Maximum operand, modulus, and result size accepted by the BigInt math APIs.
#define SL_SE_BIGINT_MATH_MAX_OPERAND_SIZE  512

/// Modular operation executed by the SE bigint math command.
typedef enum {
  SL_SE_BIGINT_OP_MOD_ADD = 0, ///< r = (x + y) mod n
  SL_SE_BIGINT_OP_MOD_SUB = 1, ///< r = (x - y) mod n
  SL_SE_BIGINT_OP_MOD_MUL = 2, ///< r = (x * y) mod n (n must be odd)
  SL_SE_BIGINT_OP_MOD_DIV = 3, ///< r = (x / y) mod n (n odd, y and n coprime)
} sl_se_bigint_operation_t;

/***************************************************************************//**
 * @brief
 *   Execute a modular bigint operation with a caller-supplied modulus.
 *
 * @details
 *   Computes one of add, subtract, multiply, or divide modulo \p modulus.
 *   \p x, \p y, and \p modulus must have the same length. The result
 *   occupies that many bytes. Modular inverse is
 *   @ref sl_se_bigint_mod_inv.
 *
 * @note
 *   Multiply and divide require an odd modulus. Divide also requires
 *   \p y to be coprime with the modulus.
 *
 * @param[in] cmd_ctx
 *   Pointer to an SE command context object.
 * @param[in] operation
 *   Modular operation to execute.
 * @param[in] x
 *   First operand, big-endian.
 * @param[in] x_size
 *   Length of \p x in bytes. Must be at least
 *   #SL_SE_BIGINT_MATH_MIN_OPERAND_SIZE, at most
 *   #SL_SE_BIGINT_MATH_MAX_OPERAND_SIZE, and equal to \p y_size and
 *   \p modulus_size.
 * @param[in] y
 *   Second operand, big-endian.
 * @param[in] y_size
 *   Length of \p y in bytes. Must equal \p x_size.
 * @param[in] modulus
 *   Modulus n, big-endian.
 * @param[in] modulus_size
 *   Length of \p modulus in bytes. Must equal \p x_size.
 * @param[out] result
 *   Buffer for the big-endian remainder, \p x_size bytes.
 * @param[in] result_size
 *   Size of \p result in bytes. Must be at least \p x_size.
 *
 * @return
 *   Status code, @ref sl_status.h.
 ******************************************************************************/
sl_status_t sl_se_bigint_math_with_modulus(sl_se_command_context_t *cmd_ctx,
                                           sl_se_bigint_operation_t operation,
                                           const uint8_t *x,
                                           size_t x_size,
                                           const uint8_t *y,
                                           size_t y_size,
                                           const uint8_t *modulus,
                                           size_t modulus_size,
                                           uint8_t *result,
                                           size_t result_size);

/***************************************************************************//**
 * @brief
 *   Compute the modular inverse of an operand.
 *
 * @details
 *   Computes r = (1 / x) mod n. \p x and \p modulus must have the same
 *   length. The result occupies that many bytes.
 *
 * @note
 *   The modulus must be odd. \p x must be coprime with the modulus.
 *
 * @param[in] cmd_ctx
 *   Pointer to an SE command context object.
 * @param[in] x
 *   Value to invert, big-endian.
 * @param[in] x_size
 *   Length of \p x in bytes. Must be at least
 *   #SL_SE_BIGINT_MATH_MIN_OPERAND_SIZE, at most
 *   #SL_SE_BIGINT_MATH_MAX_OPERAND_SIZE, and equal to \p modulus_size.
 * @param[in] modulus
 *   Modulus n, big-endian.
 * @param[in] modulus_size
 *   Length of \p modulus in bytes. Must equal \p x_size.
 * @param[out] result
 *   Buffer for the big-endian inverse, \p x_size bytes.
 * @param[in] result_size
 *   Size of \p result in bytes. Must be at least \p x_size.
 *
 * @return
 *   Status code, @ref sl_status.h.
 ******************************************************************************/
sl_status_t sl_se_bigint_mod_inv(sl_se_command_context_t *cmd_ctx,
                                 const uint8_t *x,
                                 size_t x_size,
                                 const uint8_t *modulus,
                                 size_t modulus_size,
                                 uint8_t *result,
                                 size_t result_size);

#ifdef __cplusplus
}
#endif

/// @} (end addtogroup sl_se_manager_math)
/// @} (end addtogroup sl_se_manager)

#endif // defined(SLI_SE_COMMAND_BIGINT_MATH)

#endif // SL_SE_MANAGER_MATH_H
