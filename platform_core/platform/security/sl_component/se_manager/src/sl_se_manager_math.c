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
#include "sl_se_manager_math.h"

#if defined(SLI_SE_COMMAND_BIGINT_MATH)

#include "sli_se_manager_internal.h"
#include "sli_se_manager_mailbox.h"

// -----------------------------------------------------------------------------
// Helpers

/// Command option for modular inverse. Not part of sl_se_bigint_operation_t.
#define SLI_SE_BIGINT_OP_MOD_INV  4u

static bool operand_size_ok(size_t size)
{
  return (size >= SL_SE_BIGINT_MATH_MIN_OPERAND_SIZE)
         && (size <= SL_SE_BIGINT_MATH_MAX_OPERAND_SIZE);
}

// The integer is big-endian, so the least-significant byte is last.
static bool modulus_is_odd(const uint8_t *modulus, size_t modulus_size)
{
  return (modulus[modulus_size - 1u] & 1u) != 0u;
}

static sl_status_t execute_bigint_math(sl_se_command_context_t *cmd_ctx,
                                       uint8_t operation,
                                       const uint8_t *x,
                                       size_t x_size,
                                       const uint8_t *y,
                                       size_t y_size,
                                       const uint8_t *modulus,
                                       uint8_t *result)
{
  uint32_t command_word = SLI_SE_COMMAND_BIGINT_MATH | ((uint32_t)operation << 8);
  sli_se_command_init(cmd_ctx, command_word);

  sli_se_mailbox_command_t *se_cmd = &cmd_ctx->command;
  sli_se_datatransfer_t in_x = SLI_SE_DATATRANSFER_DEFAULT(x, x_size);
  sli_se_datatransfer_t in_y = SLI_SE_DATATRANSFER_DEFAULT(y, y_size);
  sli_se_datatransfer_t in_n = SLI_SE_DATATRANSFER_DEFAULT(modulus, x_size);
  sli_se_datatransfer_t out_r = SLI_SE_DATATRANSFER_DEFAULT(result, x_size);

  sli_se_mailbox_command_add_input(se_cmd, &in_n);
  sli_se_mailbox_command_add_input(se_cmd, &in_x);
  if (y != NULL) {
    sli_se_mailbox_command_add_input(se_cmd, &in_y);
  }
  sli_se_mailbox_command_add_output(se_cmd, &out_r);
  sli_se_mailbox_command_add_parameter(se_cmd, (uint32_t)x_size);
  sli_se_mailbox_command_add_parameter(se_cmd, (uint32_t)y_size);

  return sli_se_execute_and_wait(cmd_ctx);
}

/***************************************************************************//**
 * @brief
 *   Execute a modular bigint operation with a caller-supplied modulus.
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
                                           size_t result_size)
{
  if ((cmd_ctx == NULL) || (x == NULL) || (y == NULL)
      || (modulus == NULL) || (result == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  if (operation > SL_SE_BIGINT_OP_MOD_DIV) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (!operand_size_ok(x_size) || !operand_size_ok(y_size)
      || !operand_size_ok(modulus_size)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if ((x_size != modulus_size) || (y_size != x_size) || (result_size < x_size)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Multiply and divide are defined only for an odd modulus.
  if (((operation == SL_SE_BIGINT_OP_MOD_MUL) || (operation == SL_SE_BIGINT_OP_MOD_DIV))
      && !modulus_is_odd(modulus, modulus_size)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  return execute_bigint_math(cmd_ctx, (uint8_t)operation, x, x_size, y, x_size,
                             modulus, result);
}

/***************************************************************************//**
 * @brief
 *   Compute the modular inverse of an operand.
 ******************************************************************************/
sl_status_t sl_se_bigint_mod_inv(sl_se_command_context_t *cmd_ctx,
                                 const uint8_t *x,
                                 size_t x_size,
                                 const uint8_t *modulus,
                                 size_t modulus_size,
                                 uint8_t *result,
                                 size_t result_size)
{
  if ((cmd_ctx == NULL) || (x == NULL) || (modulus == NULL) || (result == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!operand_size_ok(x_size) || !operand_size_ok(modulus_size)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if ((x_size != modulus_size) || (result_size < x_size)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (!modulus_is_odd(modulus, modulus_size)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  return execute_bigint_math(cmd_ctx, SLI_SE_BIGINT_OP_MOD_INV, x, x_size,
                             NULL, 0u, modulus, result);
}

// TODO: Time the test results as well!
// To begin with, we only need one function that inputs modulus and we can look at the
// presets later.
// TODO: How time consuming is sending the data to the command?
#endif // defined(SLI_SE_COMMAND_BIGINT_MATH)
