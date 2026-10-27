/***************************************************************************/ /**
 * @file loopback.c
 * @brief Loopback transport for CPC RX injection.
 *
 * Wire bytes are written on a secondary UART whose TX must be wired to the
 * CPC UART RX line (board-level loopback or a UART link between devices).
 * Frames then traverse the normal peripheral TX -> RX path.
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

#include <unity_fixture.h>

#include "sl_iostream.h"
#include "sl_iostream_handles.h"
#include "sl_iostream_init_eusart_instances.h"
#include "sl_iostream_uart.h"

#include "loopback.h"

#define CPC_DRV_UART_TEST_INJECTOR_IOSTREAM_HANDLE sl_iostream_exp_2_handle
#define CPC_DRV_UART_TEST_INJECTOR_UART_HANDLE sl_iostream_uart_exp_2_handle

typedef struct {
  sl_iostream_t *injector;
} loopback_ctx_t;

static loopback_ctx_t s_loopback;

sl_status_t loopback_bind(void)
{
  if (CPC_DRV_UART_TEST_INJECTOR_IOSTREAM_HANDLE == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  s_loopback.injector = CPC_DRV_UART_TEST_INJECTOR_IOSTREAM_HANDLE;

  sl_iostream_uart_set_read_block(CPC_DRV_UART_TEST_INJECTOR_UART_HANDLE, false);

  return SL_STATUS_OK;
}

void loopback_inject_bytes_at(unsigned int lineno, const uint8_t *data, size_t length)
{
  if (length == 0U) {
    return;
  }

  UNITY_TEST_ASSERT_NOT_NULL(data, lineno, "loopback_inject_bytes: data is NULL");

  for (size_t i = 0; i < length; i++) {
    sl_status_t status = sl_iostream_write(s_loopback.injector, &data[i], 1U);

    UNITY_TEST_ASSERT_EQUAL_INT(SL_STATUS_OK, (int)status, lineno, "loopback_inject_bytes: iostream write failed");
  }
}
