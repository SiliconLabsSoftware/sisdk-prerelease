/**
 * @file
 * @brief CPC Echo Example — Secondary (co-processor / MCU under test).
 *
 * Listens on endpoint 90 and echoes every received payload back to the primary.
 *
 * NOTICE: Minimal CPC API demo (short callback + polled app_process_action),
 * not a FreeRTOS template. With a kernel, the start task busy-polls and must
 * run below the CPC task priority (see .slcp)—do not copy that as production
 * design. See examples/cpc_echo/README.md.
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

#include <stdalign.h>

#include "sl_assert.h"
#include "sl_cpc_buf.h"
#include "sl_cpc_bus_instances.h"
#include "sl_cpc_ep.h"
#include "sl_cpc_frame.h"
#include "sl_cpc_msgq.h"

/// Application endpoint ID shared with the peer. IDs 0-89 are reserved for
/// Silicon Labs use; pick a higher ID to avoid collisions with stack services.
#define ECHO_EP_ID 90
#define ECHO_BUFFER_SIZE 8

alignas(SL_CPC_BUF_MIN_ALIGNMENT) static uint8_t rx_data[ECHO_BUFFER_SIZE];
static sl_cpc_ep_t ep;
static sl_cpc_buf_t rx_buff;
static sl_cpc_frame_t tx_frame;
// Shared by the CPC callback and app_process_action(); push/pop are IRQ-safe
// (MCU_ENTER_ATOMIC inside sl_cpc_msgq_*).
static sl_cpc_msgq_t rx_msgq;

// CPC callback: keep it short — defer RX, recycle buffers on SEND_DONE.
static void on_ep_event(sl_cpc_ep_t *endpoint, sl_cpc_ep_event_type_t type, const sl_cpc_ep_event_t *event, void *arg)
{
  (void)endpoint;
  (void)arg;

  switch (type) {
    case SL_CPC_EP_EVENT_RECV:
      // Push RX to a queue for post-processing; buffer work is prohibited in the CPC callback.
      sl_cpc_msgq_push(&rx_msgq, event->recv.buf);
      break;
    case SL_CPC_EP_EVENT_SEND_DONE:
      // Echo reused the RX buffer for TX; return it so the endpoint can receive again.
      sl_cpc_ep_push_recv_buf(&ep, event->send_done.buf);
      break;
    default:
      break;
  }
}

void app_init(void)
{
  sl_status_t status;

  // Local queue for RX buffers deferred from on_ep_event().
  sl_cpc_msgq_init(&rx_msgq);

  // Create endpoint 90; events arrive on on_ep_event().
  status = sl_cpc_ep_init(&ep, ECHO_EP_ID, sizeof(rx_data), on_ep_event, NULL);
  EFM_ASSERT(status == SL_STATUS_OK);

  // Provide one RX buffer so the endpoint can receive data after listen.
  sl_cpc_buf_init(&rx_buff, rx_data, sizeof(rx_data));
  sl_cpc_ep_push_recv_buf(&ep, &rx_buff);

  // Passive open: accept an incoming connect from the primary on g_bus.
  status = sl_cpc_ep_listen(&ep, g_bus);
  EFM_ASSERT(status == SL_STATUS_OK);
}

void app_process_action(void)
{
  // Busy-poll; see file header / README — not a production kernel pattern.
  sl_cpc_buf_t *buf;
  sl_status_t status;

  // Pop a buffer from the local RX queue (filled in the CPC callback).
  if (!sl_cpc_msgq_pop(&rx_msgq, &buf)) {
    return;
  }

  // Echo the same buffer back; ownership returns on SEND_DONE.
  status = sl_cpc_ep_send(&ep, buf, &tx_frame, NULL);
  EFM_ASSERT(status == SL_STATUS_OK);
}

// With a kernel, sl_main exits the start task after app_init() unless this
// returns true. Baremetal: unused / GC'd.
bool sl_main_start_task_should_continue(void)
{
  return true;
}
