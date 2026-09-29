/**
 * @file
 * @brief CPC Echo Example — Primary (Host MCU).
 *
 * Connects to endpoint 90 on the secondary, then ping-pongs a fixed payload for
 * ECHO_ITERATION_COUNT exchanges, closes the endpoint and reports the result.
 *
 * NOTICE: This is a minimal CPC API demo (short callback + polled
 * app_process_action). It is not a FreeRTOS application template. With a
 * kernel, the start task busy-polls and must run below the CPC task priority;
 * do not copy that as production design. Prefer a dedicated task
 * blocked on a semaphore/queue signaled from the CPC callback. See examples/cpc_echo/README.md.
 *
 * sl_main: app_init / app_process_action; with a kernel,
 * sl_main_start_task_should_continue() keeps the start task alive.
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

#include <inttypes.h>
#include <stdalign.h>
#include <stdio.h>
#include <string.h>

#include "sl_assert.h"
#include "sl_cpc_buf.h"
#include "sl_cpc_bus_instances.h"
#include "sl_cpc_ep.h"
#include "sl_cpc_frame.h"
#include "sl_cpc_msgq.h"

/// Application endpoint ID shared with the peer. IDs 0-89 are reserved for
/// Silicon Labs use; pick a higher ID to avoid collisions with stack services.
#define ECHO_EP_ID 90

/// Number of echo exchanges to perform before completing.
#define ECHO_ITERATION_COUNT 10U

typedef enum {
  ECHO_STATE_CONNECT,
  ECHO_STATE_WAIT_CONNECTED,
  ECHO_STATE_WAIT_ECHO,
  ECHO_STATE_WAIT_CLOSED,
  ECHO_STATE_DONE,
} echo_state_t;

alignas(SL_CPC_BUF_MIN_ALIGNMENT) static uint8_t tx_data[] = {
  'c', 'p', 'c', '_', 'e', 'c', 'h', 'o',
};
alignas(SL_CPC_BUF_MIN_ALIGNMENT) static uint8_t rx_data[sizeof(tx_data)];

static sl_cpc_ep_t ep;

// Shared by the CPC callback and app_process_action(); push/pop are Thread-safe
static sl_cpc_msgq_t rx_msgq;

// Written from the CPC callback, read from app_process_action().
static volatile bool ep_connected;
static volatile bool ep_closed;
static volatile echo_state_t echo_state = ECHO_STATE_CONNECT;

static void on_ep_event(sl_cpc_ep_t *endpoint, sl_cpc_ep_event_type_t type, const sl_cpc_ep_event_t *event, void *arg)
{
  (void)arg;

  switch (type) {
    case SL_CPC_EP_EVENT_CONNECTED:
      ep_connected = true;
      break;
    case SL_CPC_EP_EVENT_RECV:
      // Push RX buffer to a queue for post-processing; buffer work is prohibited in the CPC callback.
      sl_cpc_msgq_push(&rx_msgq, event->recv.buf);
      break;
    case SL_CPC_EP_EVENT_ERROR:
      EFM_ASSERT(!ep_connected); // Post-connect errors are fatal for this example
      // Close so CLOSED is delivered and connect can be retried.
      sl_cpc_ep_close(endpoint);
      break;
    case SL_CPC_EP_EVENT_CLOSED:
      if (!ep_connected) {
        echo_state = ECHO_STATE_CONNECT;
      } else {
        ep_closed = true;
      }
      break;
    default:
      break;
  }
}

void app_init(void)
{
  static sl_cpc_buf_t rx_buff;
  sl_status_t status;

  setvbuf(stdout, NULL, _IONBF, 0);
  printf("cpc_echo: primary start, %u echoes on ep %u\n", ECHO_ITERATION_COUNT, ECHO_EP_ID);

  // Local queue for RX buffers deferred from on_ep_event().
  sl_cpc_msgq_init(&rx_msgq);

  // Create endpoint 90; events arrive on on_ep_event().
  status = sl_cpc_ep_init(&ep, ECHO_EP_ID, sizeof(rx_data), on_ep_event, NULL);
  EFM_ASSERT(status == SL_STATUS_OK);

  // Provide one RX buffer so the endpoint can receive data after connect.
  sl_cpc_buf_init(&rx_buff, rx_data, sizeof(rx_data));
  sl_cpc_ep_push_recv_buf(&ep, &rx_buff);
}

void app_process_action(void)
{
  // Not a production kernel pattern.
  // Busy-polls with no yield. Under a kernel, keep this task's priority below the
  // CPC task or it will starve CPC; otherwise yield (e.g. osDelay) when idle.

  static sl_cpc_buf_t tx_buff;
  static sl_cpc_frame_t tx_frame;
  static uint32_t echo_count;
  sl_cpc_buf_t *buf;
  sl_status_t status;

  switch (echo_state) {
    case ECHO_STATE_CONNECT:
      printf("cpc_echo: connect\n");

      echo_state = ECHO_STATE_WAIT_CONNECTED;
      // Async connect on g_bus; outcome is delivered to on_ep_event()
      // (CONNECTED on success, ERROR if the peer is not listening yet).
      status = sl_cpc_ep_connect(&ep, g_bus);
      EFM_ASSERT(status == SL_STATUS_OK);

      // Bind the fixed echo payload to the TX buffer for later sends.
      sl_cpc_buf_init(&tx_buff, tx_data, sizeof(tx_data));
      break;

    case ECHO_STATE_WAIT_CONNECTED:
      if (!ep_connected) {
        return;
      }

      printf("cpc_echo: exchange\n");

      // Send the first echo frame.
      status = sl_cpc_ep_send(&ep, &tx_buff, &tx_frame, NULL);
      EFM_ASSERT(status == SL_STATUS_OK);
      echo_state = ECHO_STATE_WAIT_ECHO;
      break;

    case ECHO_STATE_WAIT_ECHO:

      // Pop a buffer from the local RX queue (filled in the CPC callback).
      if (!sl_cpc_msgq_pop(&rx_msgq, &buf)) {
        return;
      }

      EFM_ASSERT(buf->len == sizeof(tx_data));
      EFM_ASSERT(memcmp(buf->ptr, tx_data, sizeof(tx_data)) == 0);

      // Give the buffer back to the CPC endpoint so it can receive again.
      sl_cpc_ep_push_recv_buf(&ep, buf);

      echo_count++;
      printf("cpc_echo: echo %" PRIu32 "/%u\n", echo_count, ECHO_ITERATION_COUNT);

      if (echo_count >= ECHO_ITERATION_COUNT) {
        printf("cpc_echo: close\n");
        echo_state = ECHO_STATE_WAIT_CLOSED;
        // Async close; wait for CLOSED before reporting PASS.
        sl_cpc_ep_close(&ep);
        break;
      }

      // Next ping after a validated echo.
      status = sl_cpc_ep_send(&ep, &tx_buff, &tx_frame, NULL);
      EFM_ASSERT(status == SL_STATUS_OK);
      break;

    case ECHO_STATE_WAIT_CLOSED:
      if (!ep_closed) {
        return;
      }

      printf("cpc_echo: PASS %" PRIu32 "/%u echoes\n", echo_count, ECHO_ITERATION_COUNT);
      printf("TEST(cpc_echo, echo_exchange) PASS\n");
      printf("-----------------------\n");
      printf("1 Tests 0 Failures 0 Ignored\n");
      printf("OK\n");
      echo_state = ECHO_STATE_DONE;
      break;

    case ECHO_STATE_DONE:
      break;

    default:
      EFM_ASSERT(false);
      break;
  }
}

// With a kernel, sl_main exits the start task after app_init() unless this
// returns true. Baremetal: unused / GC'd.
bool sl_main_start_task_should_continue(void)
{
  return true;
}
