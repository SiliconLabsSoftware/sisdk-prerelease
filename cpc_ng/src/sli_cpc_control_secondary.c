/***************************************************************************/ /**
 * @file
 * @brief CPC control secondary: bus init sequence responders.
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sl_hal_emu.h"
#include "sl_status.h"

#include "sli_cpc.h"
#include "sli_cpc_bus.h"
#include "sli_cpc_control.h"
#include "sli_cpc_endianness.h"
#include "sli_cpc_ep.h"
#include "sli_cpc_log.h"
#include "sli_cpc_utils.h"

static sl_cpc_bus_t *to_bus(sli_cpc_control_t *ctrl)
{
  return container_of(ctrl, sl_cpc_bus_t, ctrl);
}

static sl_status_t secondary_init(sli_cpc_control_t *ctrl)
{
  sl_cpc_bus_t *bus = to_bus(ctrl);
  sl_status_t status;

  status = sli_cpc_control_init(ctrl);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sli_cpc_ep_attach(&ctrl->ep, bus);
  if (status != SL_STATUS_OK) {
    sli_cpc_control_deinit(ctrl);
    return status;
  }

  status = sli_cpc_ep_listen(&ctrl->ep);
  if (status != SL_STATUS_OK) {
    sli_cpc_control_close(ctrl);
    sli_cpc_control_deinit(ctrl);
    return status;
  }

  SLI_CPC_LOG_DEBUG("secondary listening");

  sli_cpc_send_reset_frame(bus, ctrl->ep.id);

  SLI_CPC_LOG_DEBUG("reset frame sent");

  return SL_STATUS_OK;
}

static void secondary_deinit(sli_cpc_control_t *ctrl)
{
  ctrl->deinitializing = true;

  sli_cpc_control_close(ctrl);
  sli_cpc_control_deinit(ctrl);
}

/**
 * @brief Read the system reset cause and map it to an sli_cpc_reset_reason_t.
 */
static sli_cpc_reset_reason_t cpc_get_reset_reason(void)
{
  uint32_t reset_cause = sl_hal_emu_get_reset_cause();

#if defined(_EMU_RSTCTRL_WDOG0RMODE_SHIFT)
  if (reset_cause & SL_HAL_EMU_RESET_WDOG0) {
    return SLI_CPC_RESET_REASON_WATCHDOG;
  }
#endif
#if defined(_EMU_RSTCTRL_WDOG1RMODE_SHIFT)
  if (reset_cause & SL_HAL_EMU_RESET_WDOG1) {
    return SLI_CPC_RESET_REASON_WATCHDOG;
  }
#endif
#if defined(_EMU_RSTCTRL_SYSRMODE_MASK)
  if (reset_cause & SL_HAL_EMU_RESET_SYS) {
    return SLI_CPC_RESET_REASON_SOFTWARE;
  }
#endif
#if defined(_EMU_RSTCTRL_LOCKUPRMODE_SHIFT)
  if (reset_cause & SL_HAL_EMU_RESET_CORE_LOCKUP) {
    return SLI_CPC_RESET_REASON_CRASH;
  }
#endif
#if defined(_EMU_RSTCTRL_AVDDBODRMODE_SHIFT)
  if (reset_cause & SL_HAL_EMU_RESET_AVDD) {
    return SLI_CPC_RESET_REASON_FAULT;
  }
#endif
#if defined(_EMU_RSTCTRL_IOVDD0BODRMODE_SHIFT)
  if (reset_cause & SL_HAL_EMU_RESET_IOVDD0) {
    return SLI_CPC_RESET_REASON_FAULT;
  }
#endif
#if defined(_EMU_RSTCTRL_IOVDD1BODRMODE_SHIFT)
  if (reset_cause & SL_HAL_EMU_RESET_IOVDD1) {
    return SLI_CPC_RESET_REASON_FAULT;
  }
#endif
#if defined(_EMU_RSTCTRL_DECBODRMODE_SHIFT)
  if (reset_cause & SL_HAL_EMU_RESET_DECOUPLE) {
    return SLI_CPC_RESET_REASON_FAULT;
  }
#endif
#if defined(_EMU_RSTCTRL_FLBODRMODE_SHIFT)
  if (reset_cause & SL_HAL_EMU_RESET_FLASH) {
    return SLI_CPC_RESET_REASON_FAULT;
  }
#endif
#if defined(_EMU_RSTCTRL_SEM0SYSRMODE_SHIFT)
  if (reset_cause & SL_HAL_EMU_RESET_SE_SYS) {
    return SLI_CPC_RESET_REASON_SOFTWARE;
  }
#endif
#if defined(_EMU_RSTCTRL_SEM0LOCKUPRMODE_SHIFT)
  if (reset_cause & SL_HAL_EMU_RESET_SE_LOCKUP) {
    return SLI_CPC_RESET_REASON_CRASH;
  }
#endif

  return SLI_CPC_RESET_REASON_UNKNOWN;
}

static sl_status_t on_protocol_version_request(sli_cpc_control_t *ctrl, const sl_cpc_buf_t *buf)
{
  sli_cpc_protocol_version_response_t *resp = NULL;
  const sli_cpc_protocol_version_request_t *req;
  sli_cpc_ctrl_status_t resp_status;
  uint16_t resp_len = 0;
  const void *req_data;
  sl_status_t status;
  uint16_t req_len;

  sli_cpc_control_get_data_from_buf(buf, &req_data, &req_len);
  req = req_data;

  if (req == NULL) {
    SLI_CPC_LOG_WARN("version request with no payload");
    resp_status = SLI_CPC_CTRL_STATUS_INVALID_PARAMETER;
    goto respond;
  }

  if (req_len != sizeof(*req)) {
    SLI_CPC_LOG_WARN("invalid version request length: %d", req_len);
    resp_status = SLI_CPC_CTRL_STATUS_INVALID_PARAMETER;
    goto respond;
  }

  resp = calloc(1, sizeof(*resp));
  if (resp == NULL) {
    SLI_CPC_LOG_ERROR("failed to allocate version response");
    resp_status = SLI_CPC_CTRL_STATUS_INTERNAL_ERROR;
    goto respond;
  }

  resp->version_major = SL_CPC_VERSION_MAJOR;
  resp->version_minor = SL_CPC_VERSION_MINOR;

  resp_len = sizeof(*resp);

  resp_status = SLI_CPC_CTRL_STATUS_OK;

respond:
  status = sli_cpc_control_respond(ctrl, buf, resp, resp_len, resp_status, NULL);
  SLI_CPC_FREE(resp);

  return status;
}

static sl_status_t on_reset_reason_request(sli_cpc_control_t *ctrl, const sl_cpc_buf_t *buf)
{
  const sli_cpc_control_header_t *hdr = sli_cpc_control_header_from_buf(buf);
  uint32_t reset_reason = (uint32_t)cpc_get_reset_reason();
  sli_cpc_reset_reason_response_t *resp = NULL;
  sli_cpc_ctrl_status_t resp_status;
  uint16_t resp_len = 0;
  sl_status_t status;

  if (sli_cpc_control_header_get_payload_size(hdr) != 0) {
    SLI_CPC_LOG_WARN("reset reason request with payload");
    resp_status = SLI_CPC_CTRL_STATUS_INVALID_PARAMETER;
    goto respond;
  }

  resp = calloc(1, sizeof(*resp));
  if (resp == NULL) {
    SLI_CPC_LOG_ERROR("failed to allocate reset reason response");
    resp_status = SLI_CPC_CTRL_STATUS_INTERNAL_ERROR;
    goto respond;
  }

  sli_cpc_u32_to_le(reset_reason, resp->reset_reason_le);

  resp_len = sizeof(*resp);

  resp_status = SLI_CPC_CTRL_STATUS_OK;

respond:
  status = sli_cpc_control_respond(ctrl, buf, resp, resp_len, resp_status, NULL);
  SLI_CPC_FREE(resp);

  return status;
}

static sl_status_t on_phy_capabilities_request(sli_cpc_control_t *ctrl, const sl_cpc_buf_t *buf)
{
  sli_cpc_ctrl_status_t resp_status;
  sl_cpc_bus_t *bus = to_bus(ctrl);
  const void *device_cap_local;
  uint8_t *device_cap = NULL;
  uint16_t resp_len = 0;
  uint16_t host_cap_len;
  uint16_t dev_cap_len;
  const void *host_cap;
  sl_status_t status;

  if (bus->drv_ops->set_remote_capabilities) {
    sli_cpc_control_get_data_from_buf(buf, &host_cap, &host_cap_len);

    if (host_cap_len != 0) {
      status = bus->drv_ops->set_remote_capabilities(bus, host_cap, host_cap_len);
      if (status != SL_STATUS_OK) {
        SLI_CPC_LOG_ERROR("failed to set remote capabilities: %lx", (unsigned long)status);
        resp_status = SLI_CPC_CTRL_STATUS_INTERNAL_ERROR;
        goto respond;
      }
    }
  }

  if (bus->drv_ops->get_local_capabilities) {
    bus->drv_ops->get_local_capabilities(bus, &device_cap_local, &dev_cap_len);
    if (dev_cap_len != 0) {
      device_cap = malloc(dev_cap_len);
      if (device_cap == NULL) {
        SLI_CPC_LOG_ERROR("failed to allocate local capabilities");
        resp_status = SLI_CPC_CTRL_STATUS_INTERNAL_ERROR;
        goto respond;
      }
      memcpy(device_cap, device_cap_local, dev_cap_len);
      resp_len = dev_cap_len;
    }
  }

  resp_status = SLI_CPC_CTRL_STATUS_OK;

respond:
  status = sli_cpc_control_respond(ctrl, buf, device_cap, resp_len, resp_status, NULL);
  SLI_CPC_FREE(device_cap);

  return status;
}

static sl_status_t on_bus_enable_request(sli_cpc_control_t *ctrl, const sl_cpc_buf_t *buf)
{
  const sli_cpc_control_header_t *hdr = sli_cpc_control_header_from_buf(buf);
  sli_cpc_ctrl_status_t resp_status;
  void *resp_arg = NULL;

  if (sli_cpc_control_header_get_payload_size(hdr) != 0) {
    SLI_CPC_LOG_WARN("bus enable request with payload");
    resp_status = SLI_CPC_CTRL_STATUS_INVALID_PARAMETER;
    goto respond;
  }

  resp_status = SLI_CPC_CTRL_STATUS_OK;
  resp_arg = (void *)(uintptr_t)SLI_CPC_CTRL_TYPE_BUS_ENABLE;

respond:
  return sli_cpc_control_respond(ctrl, buf, NULL, 0, resp_status, resp_arg);
}

static void secondary_on_connected(sli_cpc_control_t *ctrl)
{
  (void)ctrl;
}

static sl_status_t secondary_on_response(sli_cpc_control_t *ctrl, const sl_cpc_buf_t *buf)
{
  (void)ctrl;
  (void)buf;

  SLI_CPC_LOG_WARN("unexpected control response: type=0x%02X response_op_id=%d status=%d",
                   sli_cpc_control_header_get_type(sli_cpc_control_header_from_buf(buf)),
                   sli_cpc_control_header_get_op_id(sli_cpc_control_header_from_buf(buf)),
                   sli_cpc_control_header_get_status(sli_cpc_control_header_from_buf(buf)));

  return SL_STATUS_OK;
}

static void secondary_on_send_done(sli_cpc_control_t *ctrl, const sl_cpc_ep_event_t *event)
{
  sl_cpc_bus_t *bus = to_bus(ctrl);
  sli_cpc_ctrl_type_t ctrl_type = (sli_cpc_ctrl_type_t)(uintptr_t)event->send_done.arg;

  if (ctrl_type == SLI_CPC_CTRL_TYPE_BUS_ENABLE) {
    SLI_CPC_LOG_DEBUG("secondary init sequence completed");
    ctrl->initialized = true;
    sli_cpc_bus_signal_event(bus, SLI_CPC_SIGNAL_SYSTEM);
  }
}

static sl_status_t secondary_on_request(sli_cpc_control_t *ctrl, const sl_cpc_buf_t *buf)
{
  uint8_t type = sli_cpc_control_header_get_type(sli_cpc_control_header_from_buf(buf));

  switch (type) {
    case SLI_CPC_CTRL_TYPE_PROTOCOL_VERSION:
      return on_protocol_version_request(ctrl, buf);
    case SLI_CPC_CTRL_TYPE_RESET_REASON:
      return on_reset_reason_request(ctrl, buf);
    case SLI_CPC_CTRL_TYPE_PHY_CAPABILITIES:
      return on_phy_capabilities_request(ctrl, buf);
    case SLI_CPC_CTRL_TYPE_BUS_ENABLE:
      return on_bus_enable_request(ctrl, buf);
    default:
      return sli_cpc_control_on_unknown_request(ctrl, buf);
  }
}

static void secondary_on_error(sli_cpc_control_t *ctrl, sl_status_t status)
{
  (void)status;

  SLI_CPC_LOG_DEBUG("on_error: status=0x%lx, closing to re-listen", (unsigned long)status);

  sl_cpc_ep_close(&ctrl->ep);
}

static void secondary_on_closed(sli_cpc_control_t *ctrl)
{
  sl_cpc_bus_t *bus = to_bus(ctrl);
  sl_cpc_ep_t *ep = &ctrl->ep;
  sl_status_t status;

  // Re-listen only — do not send another startup RESET (that would abort the
  // primary's recovery SYN in a loop).
  status = sli_cpc_ep_attach(ep, bus);
  if (status != SL_STATUS_OK) {
    SLI_CPC_PANIC("on_closed: attach failed: 0x%lx", (unsigned long)status);
  }

  status = sli_cpc_ep_listen(ep);
  if (status != SL_STATUS_OK) {
    SLI_CPC_PANIC("on_closed: listen failed: 0x%lx", (unsigned long)status);
  }
}

const sli_cpc_control_ops_t sli_cpc_control_secondary_ops = {
  .init = secondary_init,
  .deinit = secondary_deinit,
  .on_connected = secondary_on_connected,
  .on_error = secondary_on_error,
  .on_closed = secondary_on_closed,
  .on_request = secondary_on_request,
  .on_response = secondary_on_response,
  .on_send_done = secondary_on_send_done,
};
