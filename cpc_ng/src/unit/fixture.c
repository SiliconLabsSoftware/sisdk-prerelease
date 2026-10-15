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

#include "../sli_cpc_control.h"
#include "../sli_cpc_memory.h"
#include "../sli_cpc_timer.h"
#include "sl_cpc_drv_instances.h"

#include "../../port/mock/src/mock_cpc_timer.h"
#include "unity_assert.h"

#define CPC_TEST_RETX_TIMER_WAIT_ITERATIONS 16

int cpc_test_event_mask;

static const sl_cpc_drv_mock_config_t s_test_drv_cfg = {};
static const sl_cpc_bus_config_t s_test_bus_cfg = {
  .is_secondary = false,
  .rx_frame_pool_count = SL_CPC_DRV_MOCK_DEFAULT_RX_FRAME_POOL_COUNT,
  .tx_frame_pool_count = SL_CPC_DRV_MOCK_DEFAULT_TX_FRAME_POOL_COUNT,
};

/// True for each @c sl_cpc_drv_mock_instances[i] started via the fixture API.
static bool s_mock_bus_started[SL_CPC_DRV_MOCK_INSTANCES_COUNT];

/// True once mocks left by @c sl_cpc_drv_instances_init have been torn down.
static bool s_service_mocks_reclaimed;

static void mock_bus_started_reset(void)
{
  memset(s_mock_bus_started, 0, sizeof(s_mock_bus_started));
}

static size_t mock_instance_index(unsigned int lineno, const sl_cpc_drv_mock_t *drv)
{
  for (size_t i = 0; i < SL_CPC_DRV_MOCK_INSTANCES_COUNT; i++) {
    if (&sl_cpc_drv_mock_instances[i] == drv) {
      return i;
    }
  }

  UNITY_TEST_FAIL(lineno, "cpc_test_bus: bus is not a configured mock instance");
  return 0;
}

/// Reclaim mocks left live by @c sl_cpc_drv_instances_init.
static void reclaim_service_init_mocks(void)
{
  if (s_service_mocks_reclaimed) {
    return;
  }

  for (size_t i = 0; i < SL_CPC_DRV_MOCK_INSTANCES_COUNT; i++) {
    mock_cpc_drv_deinit(&sl_cpc_drv_mock_instances[i]);
  }

  s_service_mocks_reclaimed = true;
}

static void init_mock_instance(unsigned int lineno, sl_cpc_drv_mock_t *drv, bool initialized)
{
  sl_status_t status = sl_cpc_drv_mock_init(drv, &s_test_drv_cfg, &s_test_bus_cfg);

  UNITY_TEST_ASSERT_EQUAL_INT(SL_STATUS_OK, status, lineno, NULL);
  sl_cpc_drv_mock_get_bus(drv)->initialized = initialized;
}

void cpc_test_pump(sl_cpc_bus_t *bus)
{
#if defined(SL_CATALOG_KERNEL_PRESENT)
  (void)bus;
  osDelay(10);
#else
  sli_cpc_bus_process_action(bus);
#endif
}

void cpc_test_wait_retx_timer(sl_cpc_bus_t *bus)
{
#if defined(SL_CATALOG_KERNEL_PRESENT)
  (void)bus;
  for (int i = 0; i < CPC_TEST_RETX_TIMER_WAIT_ITERATIONS && !mock_cpc_timer_has_pending(); i++) {
    osDelay(1);
  }
#else
  for (int i = 0; i < CPC_TEST_RETX_TIMER_WAIT_ITERATIONS && !mock_cpc_timer_has_pending(); i++) {
    sli_cpc_bus_process_action(bus);
  }
#endif
}

void cpc_test_setup(unsigned int lineno, bool initialized, bool all)
{
  const size_t mock_count = all ? SL_CPC_DRV_MOCK_INSTANCES_COUNT : 1U;

  // sl_cpc_drv_instances_init already initialized the mocks; tear them down once here.
  reclaim_service_init_mocks();

  if (all && mock_count < 2U) {
    UNITY_TEST_FAIL(lineno, "multi-bus setup requires >= 2 mock instances");
  }

  for (size_t i = 0; i < mock_count; i++) {
    sl_cpc_drv_mock_t *drv = &sl_cpc_drv_mock_instances[i];

    init_mock_instance(lineno, drv, initialized);
    cpc_frame_pools_start_test(lineno, sl_cpc_drv_mock_get_bus(drv));
  }
}

void cpc_test_tear_down(unsigned int lineno, bool all)
{
  const size_t mock_count = all ? SL_CPC_DRV_MOCK_INSTANCES_COUNT : 1U;

  for (size_t i = 0; i < mock_count; i++) {
    sl_cpc_drv_mock_t *drv = &sl_cpc_drv_mock_instances[i];

    cpc_frame_pools_end_test(lineno, sl_cpc_drv_mock_get_bus(drv));
    cpc_test_bus_stop(lineno, sl_cpc_drv_mock_get_bus(drv));
    mock_cpc_drv_deinit(drv);
  }

  cpc_test_event_reset();
  mock_bus_started_reset();
  mock_cpc_timer_reset();
}

sl_cpc_bus_t *cpc_test_mock_bus(unsigned int lineno, size_t index)
{
  if (index >= SL_CPC_DRV_MOCK_INSTANCES_COUNT) {
    UNITY_TEST_FAIL(lineno, "cpc_test_mock_bus: mock index out of range");
    return NULL;
  }

  return sl_cpc_drv_mock_get_bus(&sl_cpc_drv_mock_instances[index]);
}

sl_status_t cpc_test_bus_start(unsigned int lineno, sl_cpc_bus_t *bus)
{
  sl_cpc_drv_mock_t *drv;
  sl_status_t status;
  size_t index;

  UNITY_TEST_ASSERT_NOT_NULL(bus, lineno, "cpc_test_bus_start: bus is NULL");

  drv = sl_cpc_drv_mock_from_bus(bus);
  index = mock_instance_index(lineno, drv);

  status = sl_cpc_bus_start(bus);
  if (status == SL_STATUS_OK) {
    s_mock_bus_started[index] = true;
  }

  return status;
}

void cpc_test_bus_stop(unsigned int lineno, sl_cpc_bus_t *bus)
{
  size_t index;

  UNITY_TEST_ASSERT_NOT_NULL(bus, lineno, "cpc_test_bus_stop: bus is NULL");

  index = mock_instance_index(lineno, sl_cpc_drv_mock_from_bus(bus));

  if (!s_mock_bus_started[index]) {
    return;
  }

  sl_cpc_bus_stop(bus);
  s_mock_bus_started[index] = false;
}

void cpc_test_event_reset(void)
{
  cpc_test_event_mask = 0;
}

void cpc_test_set_role_primary(sl_cpc_bus_t *bus)
{
  bus->ctrl.ops = &sli_cpc_control_primary_ops;
}

void cpc_test_set_role_secondary(sl_cpc_bus_t *bus)
{
  bus->ctrl.ops = &sli_cpc_control_secondary_ops;
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
  sl_status_t status;
  sl_cpc_buf_t *buf;

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
