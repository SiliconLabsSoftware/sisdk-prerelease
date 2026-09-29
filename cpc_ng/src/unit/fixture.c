/***************************************************************************/ /**
 * @file
 * @brief Shared helpers for CPC unit-test fixtures.
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <unity_fixture.h>
#include "sl_cpc_buf.h"

#include "fixture.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "cmsis_os2.h"
#endif

#include "sl_status.h"

#include "sl_cpc_bus.h"
#include "sl_cpc_drv_mock_default_config.h"

#include "../sli_cpc.h"
#include "../sli_cpc_control.h"
#include "../sli_cpc_memory.h"
#include "../sli_cpc_timer.h"
#include "sl_cpc_bus_instances.h"
#include "sl_cpc_drv_mock.h"

#include "../../port/mock/src/mock_cpc_timer.h"
#include "unity_assert.h"

#define CPC_TEST_RETX_TIMER_WAIT_ITERATIONS 16

int cpc_test_event_mask;
bool cpc_test_bus_started;

void cpc_test_pump(void)
{
#if defined(SL_CATALOG_KERNEL_PRESENT)
  osDelay(10);
#else
  sli_cpc_bus_process_action(g_bus);
#endif
}

void cpc_test_wait_retx_timer(void)
{
#if defined(SL_CATALOG_KERNEL_PRESENT)
  for (int i = 0; i < CPC_TEST_RETX_TIMER_WAIT_ITERATIONS && !mock_cpc_timer_has_pending(); i++) {
    osDelay(1);
  }
#else
  for (int i = 0; i < CPC_TEST_RETX_TIMER_WAIT_ITERATIONS && !mock_cpc_timer_has_pending(); i++) {
    sli_cpc_bus_process_action(g_bus);
  }
#endif
}

void cpc_test_setup(unsigned int lineno, bool initialized)
{
  static const sl_cpc_drv_mock_config_t drv_cfg = {};
  static const sl_cpc_bus_config_t bus_cfg = {
    .is_secondary = false,
    .rx_frame_pool_count = SL_CPC_DRV_MOCK_DEFAULT_RX_FRAME_POOL_COUNT,
    .tx_frame_pool_count = SL_CPC_DRV_MOCK_DEFAULT_TX_FRAME_POOL_COUNT,
  };
  sl_status_t status;

  cpc_test_event_reset();
  cpc_test_bus_started = false;

  status = sl_cpc_drv_mock_init(sl_cpc_drv_mock_default, &drv_cfg, &bus_cfg);
  UNITY_TEST_ASSERT_EQUAL_INT(SL_STATUS_OK, status, lineno, NULL);

  // Must be set after mock init: sl_cpc_drv_mock_init() zeros the whole driver
  // (including the embedded bus that g_bus aliases).
  g_bus->initialized = initialized;

  mock_cpc_timer_reset();

  cpc_frame_pools_start_test(lineno, g_bus);
}

void cpc_test_tear_down(unsigned int lineno)
{
  cpc_frame_pools_end_test(lineno, g_bus);

  cpc_test_bus_stop(lineno);

  g_bus->initialized = false;

  mock_cpc_drv_deinit(sl_cpc_drv_mock_default);
}

sl_status_t cpc_test_bus_start(unsigned int lineno)
{
  sl_status_t status = sl_cpc_bus_start(g_bus);

  (void)lineno;

  if (status == SL_STATUS_OK) {
    cpc_test_bus_started = true;
  }

  return status;
}

void cpc_test_bus_stop(unsigned int lineno)
{
  (void)lineno;

  if (!cpc_test_bus_started) {
    return;
  }

  sl_cpc_bus_stop(g_bus);
  cpc_test_bus_started = false;
}

void cpc_test_event_reset(void)
{
  cpc_test_event_mask = 0;
}

void cpc_test_set_role_primary(void)
{
  g_bus->ctrl.ops = &sli_cpc_control_primary_ops;
}

void cpc_test_set_role_secondary(void)
{
  g_bus->ctrl.ops = &sli_cpc_control_secondary_ops;
}

void cpc_test_event_cb(sl_cpc_ep_t *ep, sl_cpc_ep_event_type_t type, const sl_cpc_ep_event_t *event, void *arg)
{
  (void)ep;
  (void)event;
  (void)arg;

  cpc_test_event_mask |= 1 << type;

  if (type == SL_CPC_EP_EVENT_SEND_DONE) {
    free(event->send_done.frame);
    sl_cpc_buf_free(event->send_done.buf);
  }
}

void cpc_test_write_payload(unsigned int lineno, sl_cpc_ep_t *ep, const uint8_t *payload, uint16_t len)
{
  sl_cpc_frame_t *frame;
  sl_cpc_buf_t *buf;
  sl_status_t status;

  frame = malloc(sizeof(*frame));
  UNITY_TEST_ASSERT_NOT_NULL(frame, lineno, "cpc_test_write_payload: frame alloc failed");

  buf = sl_cpc_buf_alloc(len);
  UNITY_TEST_ASSERT_NOT_NULL(buf, lineno, "cpc_test_write_payload: buffer alloc failed");

  memcpy(buf->ptr, payload, len);

  status = sl_cpc_ep_send(ep, buf, frame, NULL);
  UNITY_TEST_ASSERT_EQUAL_INT((int)SL_STATUS_OK, (int)status, lineno, "cpc_test_write_payload: sl_cpc_ep_send failed");
}

sl_status_t cpc_test_set_ep_connected(sl_cpc_ep_t *ep, sl_cpc_bus_t *bus, uint8_t local_seq, uint8_t remote_seq,
                                      uint16_t local_mtu, uint16_t remote_mtu, uint8_t remote_rx_wnd)
{
  sl_status_t status;

  if (ep == NULL || bus == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  status = sl_cpc_ep_listen(ep, bus);
  if (status != SL_STATUS_OK) {
    return status;
  }

  ep->mtu = local_mtu;
  ep->remote_mtu = remote_mtu;
  ep->send_nxt = local_seq;
  ep->send_una = local_seq;
  ep->ack = remote_seq;
  ep->send_wnd = remote_rx_wnd;
  ep->state = SLI_CPC_STATE_CONNECTED;

  return SL_STATUS_OK;
}

void cpc_test_ep_discard(sl_cpc_ep_t *ep)
{
  sli_cpc_timer_stop(&ep->re_transmit_timer);

  sl_cpc_ep_deinit(ep);
}
