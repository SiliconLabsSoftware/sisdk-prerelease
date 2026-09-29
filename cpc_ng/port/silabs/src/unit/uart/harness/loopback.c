/***************************************************************************/ /**
 * @file
 * @brief Loopback transport for CPC RX injection.
 *
 * Wire bytes are written on a secondary UART whose TX must be wired to the
 * CPC UART RX line (board-level loopback or a UART link between devices).
 * Frames then traverse the normal peripheral TX -> RX path.
 ******************************************************************************/

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "cmsis_os2.h"
#endif

#include <unity_fixture.h>

#include "sl_cpc_drv_instances.h"
#include "sl_cpc_drv_uart.h"
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
  if (CPC_DRV_UART_TEST_INJECTOR_IOSTREAM_HANDLE == NULL || sl_cpc_drv_uart_instances[0].iostream == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  s_loopback.injector = CPC_DRV_UART_TEST_INJECTOR_IOSTREAM_HANDLE;

  sl_iostream_uart_set_read_block(CPC_DRV_UART_TEST_INJECTOR_UART_HANDLE, false);

  return SL_STATUS_OK;
}

void loopback_inject_bytes_at(unsigned int lineno, const uint8_t *data, size_t length)
{
  if (s_loopback.injector == NULL || data == NULL || length == 0U) {
    return;
  }

  for (size_t i = 0; i < length; i++) {
    sl_status_t status = sl_iostream_write(s_loopback.injector, &data[i], 1U);

    UNITY_TEST_ASSERT_EQUAL_INT(SL_STATUS_OK, (int)status, lineno, "loopback_inject_bytes: iostream write failed");

#if defined(SL_CATALOG_KERNEL_PRESENT)
    osDelay(1);
#endif
  }
}
