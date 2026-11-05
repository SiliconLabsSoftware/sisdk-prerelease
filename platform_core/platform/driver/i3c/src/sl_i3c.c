/***************************************************************************//**
 * @file
 * @brief I3C High-Level Driver implementation
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

#include <string.h>
#include "sl_sleeptimer.h"
#include "sl_clock_manager.h"
#include "sl_interrupt_manager.h"
#include "sl_hal_i3c.h"
#include "sl_i3c.h"
#include "sli_i3c.h"

/*******************************************************************************
 *****************************  LOCAL VARIABLES  *******************************
 ******************************************************************************/

static sl_i3c_handle_t * i3c_handle_map[I3C_COUNT] = { NULL };

/*******************************************************************************
 *************************   LOCAL FUNCTION PROTOTYPES   ***********************
 ******************************************************************************/

static sli_i3c_context_t *handle_to_ctx(sl_i3c_handle_t *handle);
static void i3c_set_irq_enabled(I3C_TypeDef *i3c, uint32_t mask, bool enable);
static void i3c_apply_role_interrupts(const sli_i3c_context_t *ctx);
static int8_t i3c_find_device_slot(sli_i3c_context_t *ctx,
                                   const sl_i3c_device_desc_t *device);
static sl_status_t i3c_validate_fifo_thlds(I3C_TypeDef *i3c,
                                           const sl_hal_i3c_fifo_thld_t *fifo);

static void i3c_load_tx_fifo(sli_i3c_context_t *ctx, const uint8_t *buf, uint16_t len);
static void i3c_on_tx_threshold(sli_i3c_context_t *ctx);
static void i3c_arm_rx_buffer(sli_i3c_context_t *ctx, uint8_t *buf, uint16_t len);
static void i3c_on_rx_threshold(sli_i3c_context_t *ctx);
static uint32_t i3c_read_rx_fifo(I3C_TypeDef *i3c, uint8_t *buf, uint32_t len);
static void i3c_discard_rx_fifo(I3C_TypeDef *i3c, uint32_t len);

static void i3c_cancel_ext_tx(sli_i3c_context_t *ctx);
static void i3c_recover_after_error(sli_i3c_context_t *ctx);
static void i3c_resume_if_halted(I3C_TypeDef *i3c);
static void i3c_read_ibi_queue(sli_i3c_context_t *ctx);
static void i3c_discard_ibi_queue(I3C_TypeDef *i3c);

static void i3c_default_private_cmd(sli_i3c_context_t *ctx,
                                    sl_i3c_transfer_cmd_t *cmd,
                                    const sl_i3c_device_desc_t *device,
                                    bool is_read);
static sl_status_t i3c_post_write_command(sli_i3c_context_t *ctx,
                                          const uint8_t *buf,
                                          uint16_t len,
                                          sl_i3c_transfer_cmd_t *cmd,
                                          bool is_i2c);
static sl_status_t i3c_post_read_command(I3C_TypeDef *i3c,
                                         uint16_t len,
                                         sl_i3c_transfer_cmd_t *cmd,
                                         bool is_i2c,
                                         uint8_t defining_byte);
static sl_status_t i3c_post_ccc(sli_i3c_context_t *ctx,
                                const sl_i3c_ccc_payload_t *payload,
                                const sl_i3c_transfer_cmd_t *command,
                                uint8_t *rx_data,
                                size_t rx_len,
                                sli_i3c_transfer_callback_t callback);
static sl_status_t i3c_post_addr_assign(sli_i3c_context_t *ctx,
                                        uint8_t tid,
                                        uint8_t ccc_id,
                                        uint8_t dat_index,
                                        uint8_t dev_count,
                                        sli_i3c_transfer_callback_t callback);
static sl_i3c_transfer_event_t i3c_read_controller_response(sli_i3c_context_t *ctx);
static bool i3c_target_uses_ext_resp(const sli_i3c_context_t *ctx);

static sl_status_t i3c_blocking_callback(sl_i3c_handle_t *i3c_handle,
                                         sl_i3c_transfer_event_t event);
static sl_status_t i3c_wait_blocking(sli_i3c_context_t *ctx, uint32_t timeout_ms);
static void i3c_complete_transfer(sli_i3c_context_t *ctx,
                                  sl_i3c_transfer_event_t event);
static void i3c_notify_target(sli_i3c_context_t *ctx,
                              sl_i3c_target_event_t event);

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Initialize an I3C driver instance.
 ******************************************************************************/
sl_status_t sl_i3c_init(sl_i3c_handle_t *i3c_handle,
                        const sl_i3c_init_params_t *init_params)
{
  I3C_TypeDef *i3c_base_addr;
  int8_t i3c_instance_num;
  sl_status_t status;

  // Parameter validation
  if (i3c_handle == NULL || init_params == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Validate I3C peripheral instance
  i3c_base_addr = init_params->i3c_base_addr;
  if (!SL_HAL_I3C_REF_VALID(i3c_base_addr)) {
    return SL_STATUS_NOT_SUPPORTED;
  }
  i3c_instance_num = I3C_NUM(i3c_base_addr);
  // Check if the I3C instance is already initialized
  if ((i3c_handle_map[i3c_instance_num] != NULL) || i3c_handle->initialized) {
    return SL_STATUS_ALREADY_INITIALIZED;
  }

  memset(i3c_handle, 0, sizeof(*i3c_handle));
  i3c_handle->i3c = init_params->i3c_base_addr;
  i3c_handle->role = init_params->role;

  // Default DAT slots to accept SIR/MR (matches hardware reset).
  for (uint8_t i = 0; i < SL_HAL_I3C_MAX_TARGET_COUNT; i++) {
    i3c_handle->dat_cfg[i].sir_enable = true;
    i3c_handle->dat_cfg[i].mr_enable  = true;
  }

  status = sli_i3c_init_core(i3c_handle, init_params);
  if (status != SL_STATUS_OK) {
    return status;
  }

  i3c_handle->initialized = true;
  i3c_handle_map[i3c_instance_num] = i3c_handle;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Deinitialize an I3C driver instance.
 ******************************************************************************/
sl_status_t sl_i3c_deinit(sl_i3c_handle_t *i3c_handle)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  I3C_TypeDef *i3c_base_addr;
  int8_t i3c_instance_num;

  if (ctx == NULL) {
    return SL_STATUS_INVALID_HANDLE;
  }

  i3c_base_addr = ctx->i3c;
  if (!SL_HAL_I3C_REF_VALID(i3c_base_addr)) {
    return SL_STATUS_NOT_SUPPORTED;
  }
  i3c_instance_num = I3C_NUM(i3c_base_addr);
  i3c_set_irq_enabled(i3c_base_addr,
                      SLI_I3C_CONTROLLER_INTR_MASK
                      | SLI_I3C_TARGET_INTR_MASK
                      | SLI_I3C_IBI_INTR_MASK,
                      false);
  sl_hal_i3c_disable(i3c_base_addr);
  sl_hal_i3c_disable_peripheral_clock(i3c_base_addr);

  // Prevent stale IRQs after the handle is cleared.
  sl_interrupt_manager_disable_irq(I3C0_IRQn);
  ctx->transfer_callback = NULL;
  ctx->target_callback = NULL;
  ctx->ibi_callback = NULL;
  ctx->ibi_enabled_mask = 0U;
  ctx->xfer_state = SLI_I3C_XFER_IDLE;
  ctx->initialized = false;
  i3c_handle_map[i3c_instance_num] = NULL;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Start dynamic address assignment (ENTDAA / SETDASA / SETAASA / SETNEWDA).
 ******************************************************************************/
sl_status_t sl_i3c_start_address_assignment(sl_i3c_handle_t *i3c_handle,
                                            const sl_i3c_addr_assign_params_t *params,
                                            uint32_t timeout_ms)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  sl_status_t status;

  if (ctx == NULL || params == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_CONTROLLER) {
    return SL_STATUS_INVALID_MODE;
  }
  if (ctx->xfer_state != SLI_I3C_XFER_IDLE) {
    return SL_STATUS_BUSY;
  }
  if (params->tid > SLI_I3C_USER_TID_MAX) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  ctx->blocking_done = false;
  ctx->last_resp_error = SLI_I3C_RESP_ERR_SUCCESS;

  switch (params->mode) {
    case SL_I3C_ADDR_ASSIGN_SETAASA: {
      // SETAASA is a broadcast regular-transfer CCC without payload; every
      // target with a static address adopts it as its dynamic address.
      sl_i3c_ccc_payload_t payload = {
        .ccc = { .id = SL_I3C_CCC_BROADCAST_SETAASA },
        .roc = 1,
        .toc = 1,
      };
      sl_i3c_transfer_cmd_t cmd = {
        .tid = params->tid,
        .cmd = (uint8_t)SL_I3C_CCC_BROADCAST_SETAASA,
        .cp  = 1,
        .roc = 1,
        .toc = 1,
      };
      cmd.speed.i3c_speed = SL_I3C_SPEED_SDR0;
      status = i3c_post_ccc(ctx, &payload, &cmd, NULL, 0U, i3c_blocking_callback);
      break;
    }

    case SL_I3C_ADDR_ASSIGN_ENTDAA: {
      uint8_t start_index = params->dat_index;
      uint8_t dev_count = params->dev_count;
      if (dev_count == 0U) {
        dev_count = (uint8_t)(SL_HAL_I3C_MAX_TARGET_COUNT - start_index);
      }
      if (start_index >= SL_HAL_I3C_MAX_TARGET_COUNT
          || dev_count == 0U
          || ((uint16_t)start_index + (uint16_t)dev_count) > SL_HAL_I3C_MAX_TARGET_COUNT) {
        return SL_STATUS_INVALID_PARAMETER;
      }
      status = i3c_post_addr_assign(ctx,
                                    params->tid,
                                    (uint8_t)SL_I3C_CCC_BROADCAST_ENTDAA,
                                    start_index,
                                    dev_count,
                                    i3c_blocking_callback);
      break;
    }

    case SL_I3C_ADDR_ASSIGN_SETDASA: {
      uint8_t start_index = params->dat_index;
      uint8_t dev_count = (params->dev_count == 0U) ? 1U : params->dev_count;
      if (start_index >= SL_HAL_I3C_MAX_TARGET_COUNT
          || ((uint16_t)start_index + (uint16_t)dev_count) > SL_HAL_I3C_MAX_TARGET_COUNT) {
        return SL_STATUS_INVALID_PARAMETER;
      }
      /* DAT must hold SA and intended DA for each slot in the range. */
      for (uint8_t i = 0U; i < dev_count; i++) {
        uint8_t idx = (uint8_t)(start_index + i);
        if (ctx->dev_table[idx].static_addr == 0U
            || ctx->dev_table[idx].dynamic_addr == 0U) {
          return SL_STATUS_INVALID_PARAMETER;
        }
      }
      status = i3c_post_addr_assign(ctx,
                                    params->tid,
                                    (uint8_t)SL_I3C_CCC_DIRECT_SETDASA,
                                    start_index,
                                    dev_count,
                                    i3c_blocking_callback);
      break;
    }

    case SL_I3C_ADDR_ASSIGN_SETNEWDA: {
      uint8_t new_da = params->new_dynamic_address
                       & (uint8_t)_I3C_DEVICE_ADDR_STATIC_ADDR_MASK;
      if (params->dat_index >= SL_HAL_I3C_MAX_TARGET_COUNT || new_da == 0U) {
        return SL_STATUS_INVALID_PARAMETER;
      }
      if (ctx->dev_table[params->dat_index].dynamic_addr == 0U) {
        return SL_STATUS_INVALID_PARAMETER;
      }
      // SETNEWDA is a directed regular-transfer CCC. Its one data byte is the
      // new address with odd parity in bit 0, sent as a Short Data Argument.
      sl_i3c_transfer_cmd_t cmd = {
        .cmd_attr = (uint8_t)SLI_I3C_CMD_ATTR_TRANSFER_CMD,
        .tid      = params->tid,
        .cmd      = (uint8_t)SL_I3C_CCC_DIRECT_SETNEWDA,
        .dev_indx = params->dat_index,
        .cp       = 1,
        .sdap     = 1,
        .roc      = 1,
        .toc      = 1,
      };
      cmd.speed.i3c_speed = SL_I3C_SPEED_SDR0;

      // The DAT entry is updated in i3c_read_controller_response() on success.
      ctx->transfer_callback = i3c_blocking_callback;
      ctx->setnewda_dat_index = params->dat_index;
      ctx->setnewda_new_addr = new_da;
      ctx->setnewda_pending = true;
      ctx->xfer_state = SLI_I3C_XFER_CCC_PENDING;

      status = sl_hal_i3c_set_command_queue(
        ctx->i3c,
        sli_i3c_form_short_data_argument(0x1U, sli_i3c_encode_odd_parity_byte(new_da), 0U, 0U));
      if (status == SL_STATUS_OK) {
        status = sl_hal_i3c_set_command_queue(ctx->i3c,
                                              sli_i3c_form_transfer_command_from_struct(&cmd, false));
      }
      if (status != SL_STATUS_OK) {
        ctx->xfer_state = SLI_I3C_XFER_IDLE;
        ctx->transfer_callback = NULL;
        ctx->setnewda_pending = false;
      }
      break;
    }

    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  if (status != SL_STATUS_OK) {
    return status;
  }

  status = i3c_wait_blocking(ctx, timeout_ms);
  if (status != SL_STATUS_OK) {
    return status;
  }

  /*
   * After SETAASA CCC succeeds, application must set
   * DAT[Dynamic Address] = DAT[Static Address]. HW does not update DAT.
   * Only for SETAASA. Never for ENTDAA/SETDASA/SETNEWDA.
   */
  if (params->mode ==   SL_I3C_ADDR_ASSIGN_SETAASA) {
    for (uint8_t i = 0U; i < SL_HAL_I3C_MAX_TARGET_COUNT; i++) {
      uint8_t sa = ctx->dev_table[i].static_addr
                   & (uint8_t)_I3C_DEVICE_ADDR_STATIC_ADDR_MASK;
      if (sa == 0U) {
        continue;
      }
      ctx->dev_table[i].dynamic_addr = sa;
      ctx->dat_cfg[i].dynamic_address = sa;
      (void)sl_hal_i3c_configure_target(ctx->i3c, i, &ctx->dat_cfg[i]);
    }
  }

  return status;
}

/***************************************************************************//**
 * Look up a device descriptor by 7-bit address.
 ******************************************************************************/
sl_status_t sl_i3c_device_find(sl_i3c_handle_t *i3c_handle,
                               uint8_t address,
                               const sl_i3c_device_desc_t **device)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL || device == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  int8_t idx = sli_i3c_find_dev_index(ctx, address);
  if (idx < 0) {
    return SL_STATUS_NOT_FOUND;
  }

  *device = &ctx->dev_table[idx];
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Issue a CCC transferin controller mode.
 ******************************************************************************/
sl_status_t sl_i3c_do_ccc(sl_i3c_handle_t *i3c_handle,
                          const sl_i3c_ccc_payload_t *payload,
                          const sl_i3c_transfer_cmd_t *command,
                          uint8_t *rx_data,
                          size_t rx_len,
                          uint32_t timeout_ms)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL || payload == NULL || command == NULL
      || command->tid > SLI_I3C_USER_TID_MAX
      || payload->len > SLI_I3C_MAX_XFER_LEN || rx_len > SLI_I3C_MAX_XFER_LEN
      || ((rx_len > 0U) && (rx_data == NULL))
      || (!payload->is_read && (payload->len > 0U) && (payload->data == NULL))) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_CONTROLLER) {
    return SL_STATUS_INVALID_MODE;
  }
  if (ctx->xfer_state != SLI_I3C_XFER_IDLE) {
    return SL_STATUS_BUSY;
  }

  // Address-assignment CCCs need DAT bookkeeping; they go through
  // sl_i3c_start_address_assignment(). RSTDAA is allowed here.
  if ((payload->ccc.id == SL_I3C_CCC_BROADCAST_ENTDAA)
      || (payload->ccc.id == SL_I3C_CCC_BROADCAST_SETAASA)
      || (payload->ccc.id == SL_I3C_CCC_DIRECT_SETDASA)
      || (payload->ccc.id == SL_I3C_CCC_DIRECT_SETNEWDA)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  ctx->blocking_done = false;
  ctx->last_resp_error = SLI_I3C_RESP_ERR_SUCCESS;
  sl_status_t status = i3c_post_ccc(ctx, payload, command, rx_data, rx_len,
                                    i3c_blocking_callback);
  if (status != SL_STATUS_OK) {
    return status;
  }
  return i3c_wait_blocking(ctx, timeout_ms);
}

/***************************************************************************//**
 * Controller: assign a group address (SETGRPA).
 ******************************************************************************/
sl_status_t sl_i3c_set_group_address(sl_i3c_handle_t *i3c_handle,
                                     const sl_i3c_device_desc_t *device,
                                     uint8_t group_addr_7bit,
                                     uint32_t timeout_ms)
{
  uint8_t grpa_byte;

  if (i3c_handle == NULL || device == NULL
      || group_addr_7bit > SL_HAL_I3C_MAX_7BIT_ADDRESS) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  /* MIPI: bits[7:1] = group address, bit[0] = 0. */
  grpa_byte = (uint8_t)(group_addr_7bit << 1);

  sl_i3c_ccc_payload_t payload = {
    .ccc = {
      .id = SL_I3C_CCC_DIRECT_SETGRPA,
      .addr = device->dynamic_addr,
    },
    .data = &grpa_byte,
    .len = 1U,
    .is_read = 0,
    .roc = 1,
    .toc = 1,
  };
  sl_i3c_transfer_cmd_t command = {
    .tid = 0,
    .cp = 1,
    .roc = 1,
    .toc = 1,
    .sdap = 1,
  };
  command.speed.i3c_speed = SL_I3C_SPEED_SDR0;

  return sl_i3c_do_ccc(i3c_handle, &payload, &command, NULL, 0U, timeout_ms);
}

/***************************************************************************//**
 * Controller: reset group address (RSTGRPA directed or broadcast).
 ******************************************************************************/
sl_status_t sl_i3c_reset_group_address(sl_i3c_handle_t *i3c_handle,
                                       const sl_i3c_device_desc_t *device,
                                       uint32_t timeout_ms)
{
  sl_i3c_ccc_payload_t payload;
  sl_i3c_transfer_cmd_t command = {
    .tid = 0,
    .cp = 1,
    .roc = 1,
    .toc = 1,
  };

  if (i3c_handle == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  command.speed.i3c_speed = SL_I3C_SPEED_SDR0;
  memset(&payload, 0, sizeof(payload));
  payload.roc = 1;
  payload.toc = 1;
  payload.is_read = 0;

  if (device != NULL) {
    payload.ccc.id = SL_I3C_CCC_DIRECT_RSTGRPA;
    payload.ccc.addr = device->dynamic_addr;
  } else {
    payload.ccc.id = SL_I3C_CCC_BROADCAST_RSTGRPA;
  }

  return sl_i3c_do_ccc(i3c_handle, &payload, &command, NULL, 0U, timeout_ms);
}

/***************************************************************************//**
 * Target: read assigned group-address slot.
 ******************************************************************************/
sl_status_t sl_i3c_get_group_address(sl_i3c_handle_t *i3c_handle,
                                     uint8_t grp_addr_index,
                                     uint8_t *group_addr_7bit,
                                     uint8_t *target_valid_mask)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);

  if (ctx == NULL || group_addr_7bit == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_SLAVE) {
    return SL_STATUS_INVALID_MODE;
  }

  return sl_hal_i3c_get_group_address(ctx->i3c,
                                      grp_addr_index,
                                      group_addr_7bit,
                                      target_valid_mask);
}

/***************************************************************************//**
 * Controller: combined write-then-read private transfer (blocking).
 ******************************************************************************/
sl_status_t sl_i3c_controller_transfer_blocking(sl_i3c_handle_t *i3c_handle,
                                                const sl_i3c_device_desc_t *device,
                                                const uint8_t *wr_buf,
                                                size_t wr_len,
                                                uint8_t *rd_buf,
                                                size_t rd_len,
                                                uint32_t timeout_ms)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL || device == NULL
      || (wr_len > SLI_I3C_MAX_XFER_LEN) || (rd_len > SLI_I3C_MAX_XFER_LEN)
      || ((wr_len > 0U) && (wr_buf == NULL))
      || ((rd_len > 0U) && (rd_buf == NULL))) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_CONTROLLER) {
    return SL_STATUS_INVALID_MODE;
  }
  if (ctx->xfer_state != SLI_I3C_XFER_IDLE) {
    return SL_STATUS_BUSY;
  }

  int8_t dev_idx = i3c_find_device_slot(ctx, device);
  if (dev_idx < 0) {
    return SL_STATUS_NOT_FOUND;
  }

  // Write without STOP (TOC=0) followed by a read with STOP: the read starts
  // with a repeated START. Fixed TIDs tell the two responses apart.
  sl_i3c_transfer_cmd_t cmd_w = {
    .tid      = SLI_I3C_WRITE_READ_WR_TID,
    .dev_indx = (uint8_t)dev_idx,
    .roc      = 1,
    .toc      = 0,
  };
  sl_i3c_transfer_cmd_t cmd_r = {
    .tid      = SLI_I3C_WRITE_READ_RD_TID,
    .dev_indx = (uint8_t)dev_idx,
    .roc      = 1,
    .toc      = 1,
    .rnw      = 1,
  };
  if (device->is_i2c) {
    cmd_w.speed.i2c_speed = SL_I2C_SPEED_FM;
    cmd_r.speed.i2c_speed = SL_I2C_SPEED_FM;
  } else {
    cmd_w.speed.i3c_speed = SL_I3C_SPEED_SDR0;
    cmd_r.speed.i3c_speed = SL_I3C_SPEED_SDR0;
  }

  ctx->blocking_done = false;
  ctx->last_resp_error = SLI_I3C_RESP_ERR_SUCCESS;
  ctx->transfer_callback = i3c_blocking_callback;
  ctx->xfer_state = SLI_I3C_XFER_WRITE_READ_PENDING;
  i3c_arm_rx_buffer(ctx, rd_buf, (uint16_t)rd_len);

  sl_status_t status = i3c_post_write_command(ctx, wr_buf, (uint16_t)wr_len, &cmd_w,
                                              device->is_i2c != 0U);
  if (status == SL_STATUS_OK) {
    status = i3c_post_read_command(ctx->i3c, (uint16_t)rd_len, &cmd_r,
                                   device->is_i2c != 0U, 0U);
  }
  if (status != SL_STATUS_OK) {
    // Part of the sequence may already be queued; flush it so it cannot run.
    i3c_recover_after_error(ctx);
    ctx->transfer_callback = NULL;
    return status;
  }

  status = i3c_wait_blocking(ctx, timeout_ms);
  // The target may end a read early; the buffer tail would be undefined.
  if ((status == SL_STATUS_OK) && (ctx->rx_offset < rd_len)) {
    status = SL_STATUS_INVALID_COUNT;
  }
  return status;
}

/***************************************************************************//**
 * Controller: private write to a device (blocking).
 ******************************************************************************/
sl_status_t sl_i3c_controller_send_blocking(sl_i3c_handle_t *i3c_handle,
                                            const sl_i3c_device_desc_t *device,
                                            const uint8_t *buf,
                                            size_t len,
                                            const sl_i3c_transfer_cmd_t *command,
                                            uint32_t timeout_ms)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  sl_i3c_transfer_cmd_t cmd;

  if (ctx == NULL || device == NULL || (len > SLI_I3C_MAX_XFER_LEN)
      || ((len > 0U) && (buf == NULL))
      || ((command != NULL) && (command->tid > SLI_I3C_USER_TID_MAX))) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_CONTROLLER) {
    return SL_STATUS_INVALID_MODE;
  }
  if (ctx->xfer_state != SLI_I3C_XFER_IDLE) {
    return SL_STATUS_BUSY;
  }

  int8_t dev_idx = i3c_find_device_slot(ctx, device);
  if (dev_idx < 0) {
    return SL_STATUS_NOT_FOUND;
  }
  if (command != NULL) {
    cmd = *command;
  } else {
    i3c_default_private_cmd(ctx, &cmd, device, false);
  }
  cmd.dev_indx = (uint8_t)dev_idx;

  ctx->blocking_done = false;
  ctx->last_resp_error = SLI_I3C_RESP_ERR_SUCCESS;
  ctx->transfer_callback = i3c_blocking_callback;
  ctx->xfer_state = SLI_I3C_XFER_WRITE_PENDING;
  // A write returns no data: clear any RX buffer left from a previous read.
  i3c_arm_rx_buffer(ctx, NULL, 0U);

  sl_status_t status = i3c_post_write_command(ctx, buf, (uint16_t)len, &cmd,
                                              device->is_i2c != 0U);
  if (status != SL_STATUS_OK) {
    ctx->xfer_state = SLI_I3C_XFER_IDLE;
    ctx->transfer_callback = NULL;
    return status;
  }

  return i3c_wait_blocking(ctx, timeout_ms);
}

/***************************************************************************//**
 * Controller: private read from a device (blocking).
 ******************************************************************************/
sl_status_t sl_i3c_controller_receive_blocking(sl_i3c_handle_t *i3c_handle,
                                               const sl_i3c_device_desc_t *device,
                                               uint8_t *buf,
                                               size_t len,
                                               const sl_i3c_transfer_cmd_t *command,
                                               uint32_t timeout_ms)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  sl_i3c_transfer_cmd_t cmd;

  if (ctx == NULL || device == NULL || (len > SLI_I3C_MAX_XFER_LEN)
      || ((len > 0U) && (buf == NULL))
      || ((command != NULL) && (command->tid > SLI_I3C_USER_TID_MAX))) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_CONTROLLER) {
    return SL_STATUS_INVALID_MODE;
  }
  if (ctx->xfer_state != SLI_I3C_XFER_IDLE) {
    return SL_STATUS_BUSY;
  }

  int8_t dev_idx = i3c_find_device_slot(ctx, device);
  if (dev_idx < 0) {
    return SL_STATUS_NOT_FOUND;
  }
  if (command != NULL) {
    cmd = *command;
  } else {
    i3c_default_private_cmd(ctx, &cmd, device, true);
  }
  cmd.dev_indx = (uint8_t)dev_idx;

  ctx->blocking_done = false;
  ctx->last_resp_error = SLI_I3C_RESP_ERR_SUCCESS;
  ctx->transfer_callback = i3c_blocking_callback;
  ctx->xfer_state = SLI_I3C_XFER_READ_PENDING;
  i3c_arm_rx_buffer(ctx, buf, (uint16_t)len);

  sl_status_t status = i3c_post_read_command(ctx->i3c, (uint16_t)len, &cmd,
                                             device->is_i2c != 0U, 0U);
  if (status != SL_STATUS_OK) {
    ctx->xfer_state = SLI_I3C_XFER_IDLE;
    ctx->transfer_callback = NULL;
    i3c_arm_rx_buffer(ctx, NULL, 0U);
    return status;
  }

  status = i3c_wait_blocking(ctx, timeout_ms);
  // The target may end a read early; the buffer tail would be undefined.
  if ((status == SL_STATUS_OK) && (ctx->rx_offset < len)) {
    status = SL_STATUS_INVALID_COUNT;
  }
  return status;
}

/***************************************************************************//**
 * Target: wait for a controller private write (blocking).
 ******************************************************************************/
sl_status_t sl_i3c_target_receive_blocking(sl_i3c_handle_t *i3c_handle,
                                           uint8_t *buf,
                                           size_t len,
                                           uint32_t timeout_ms)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL || (len > SLI_I3C_MAX_XFER_LEN)
      || ((len > 0U) && (buf == NULL))) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_SLAVE) {
    return SL_STATUS_INVALID_MODE;
  }
  // TX may already be armed (sl_i3c_target_send / sl_i3c_virtual_target_send)
  // for a controller that reads first and then writes.
  if (ctx->xfer_state != SLI_I3C_XFER_IDLE
      && ctx->xfer_state != SLI_I3C_XFER_TARGET_TX_PENDING) {
    return SL_STATUS_BUSY;
  }

  // A target posts no command for RX: arm the buffer and wait for the
  // controller's write to produce a response.
  ctx->blocking_done = false;
  ctx->last_resp_error = SLI_I3C_RESP_ERR_SUCCESS;
  ctx->transfer_callback = i3c_blocking_callback;
  i3c_arm_rx_buffer(ctx, buf, (uint16_t)len);
  ctx->xfer_state = SLI_I3C_XFER_TARGET_RX_PENDING;

  return i3c_wait_blocking(ctx, timeout_ms);
}

/***************************************************************************//**
 * Target: arm physical TX for an upcoming controller private read.
 ******************************************************************************/
sl_status_t sl_i3c_target_send(sl_i3c_handle_t *i3c_handle,
                               const uint8_t *buf,
                               size_t len,
                               uint8_t tid)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  sl_status_t status;

  if (ctx == NULL || (len > SLI_I3C_MAX_XFER_LEN) || tid > 0x0FU
      || ((len > 0U) && (buf == NULL))) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_SLAVE) {
    return SL_STATUS_INVALID_MODE;
  }
  if (ctx->xfer_state != SLI_I3C_XFER_IDLE
      && ctx->xfer_state != SLI_I3C_XFER_TARGET_TX_PENDING) {
    return SL_STATUS_BUSY;
  }

  status = sl_hal_i3c_buffer_reset(ctx->i3c, SL_HAL_I3C_RESET_TX_FIFO);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // The physical target uses the regular command queue (extended commands are
  // for virtual targets only): preload the TX FIFO, then post a Transmit
  // Command. The data goes out when the controller reads.
  i3c_load_tx_fifo(ctx, buf, (uint16_t)len);
  status = sl_hal_i3c_set_command_queue(ctx->i3c,
                                        sli_i3c_form_target_tx_command((uint16_t)len, tid));
  if (status != SL_STATUS_OK) {
    return status;
  }

  // The response for this TX has no RX data; the target dispatcher uses this
  // flag to tell it apart from a controller write.
  ctx->await_phys_tx_done = true;
  ctx->await_ext_tx_done = false;
  ctx->expect_target_id_mask = 0U;
  // Arm only, no callback: the application may call target_receive_blocking next.
  if (ctx->xfer_state == SLI_I3C_XFER_IDLE) {
    ctx->xfer_state = SLI_I3C_XFER_TARGET_TX_PENDING;
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Target: arm virtual-target TX for an upcoming controller private read.
 ******************************************************************************/
sl_status_t sl_i3c_virtual_target_send(sl_i3c_handle_t *i3c_handle,
                                       uint8_t virtual_target_index,
                                       uint8_t ext_cmd_index,
                                       const uint8_t *buf,
                                       size_t len)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  const uint32_t words = (uint32_t)((len + 3U) / 4U);
  uint8_t empty_before = 0U;
  sl_status_t status;

  if (ctx == NULL || buf == NULL || len > SLI_I3C_MAX_XFER_LEN
      || virtual_target_index < 1U
      || virtual_target_index > SL_HAL_I3C_MAX_VIRTUAL_TARGET_COUNT
      || ext_cmd_index >= SL_HAL_I3C_MAX_EXT_REG_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_SLAVE) {
    return SL_STATUS_INVALID_MODE;
  }
  if (ctx->xfer_state != SLI_I3C_XFER_IDLE
      && ctx->xfer_state != SLI_I3C_XFER_TARGET_TX_PENDING) {
    return SL_STATUS_BUSY;
  }

  // Virtual targets transmit through an extended command: data goes into the
  // extended TX queue, then EXT_CMD_REG_2 (length) and EXT_CMD_REG (CMD_VLD,
  // written last) arm it for the virtual target's address.
  status = sl_hal_i3c_reset_ext_tx_queue(ctx->i3c, ext_cmd_index);
  if (status != SL_STATUS_OK) {
    return status;
  }
  status = sl_hal_i3c_get_ext_data_buf_status_level(ctx->i3c, ext_cmd_index, &empty_before);

  for (size_t offset = 0U; (status == SL_STATUS_OK) && (offset < len); ) {
    uint32_t word = 0U;
    // Pack up to 4 bytes little-endian per queue word.
    for (uint32_t b = 0U; (b < 4U) && (offset < len); b++, offset++) {
      word |= ((uint32_t)buf[offset]) << (b * 8U);
    }
    status = sl_hal_i3c_set_ext_tx_data(ctx->i3c, ext_cmd_index, word);
  }

  // Do not arm the command until the queue level shows every word has landed.
  if (status == SL_STATUS_OK) {
    uint32_t start_ms = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());
    status = SL_STATUS_TIMEOUT;
    while ((sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count()) - start_ms)
           < SLI_I3C_EXT_TX_BLR_TIMEOUT_MS) {
      uint8_t empty_now = 0U;
      sl_status_t level_status = sl_hal_i3c_get_ext_data_buf_status_level(ctx->i3c,
                                                                          ext_cmd_index,
                                                                          &empty_now);
      if (level_status != SL_STATUS_OK) {
        status = level_status;
        break;
      }
      if ((empty_before >= words) && (empty_now <= (uint8_t)(empty_before - words))) {
        status = SL_STATUS_OK;
        break;
      }
    }
  }

  if (status == SL_STATUS_OK) {
    status = sl_hal_i3c_set_ext_cmd_reg_2(ctx->i3c, ext_cmd_index,
                                          sli_i3c_form_ext_cmd_word2((uint16_t)len, 0U, 0U));
  }
  if (status == SL_STATUS_OK) {
    status = sl_hal_i3c_set_ext_cmd_reg(ctx->i3c, ext_cmd_index,
                                        sli_i3c_form_ext_cmd_word1(virtual_target_index,
                                                                   true,
                                                                   false));
  }
  if (status != SL_STATUS_OK) {
    (void)sl_hal_i3c_reset_ext_tx_queue(ctx->i3c, ext_cmd_index);
    return status;
  }

  // The dispatcher matches the extended response by its TARGET_ID bit.
  ctx->await_ext_tx_done = true;
  ctx->ext_cmd_index = ext_cmd_index;
  ctx->expect_target_id_mask = (uint8_t)SLI_I3C_EXT_RESP_TARGET_ID_BIT(virtual_target_index);
  // Leave a halt (e.g. after SETMRL) so the armed TX can run.
  i3c_resume_if_halted(ctx->i3c);
  if (ctx->xfer_state == SLI_I3C_XFER_IDLE) {
    ctx->xfer_state = SLI_I3C_XFER_TARGET_TX_PENDING;
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configure (and optionally enable) a virtual target.
 ******************************************************************************/
sl_status_t sl_i3c_configure_virtual_target(sl_i3c_handle_t *i3c_handle,
                                            uint8_t virtual_target_index,
                                            const sl_i3c_virtual_target_config_t *config)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  sl_status_t status;

  if (ctx == NULL || config == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_SLAVE) {
    return SL_STATUS_INVALID_MODE;
  }
  if (virtual_target_index < 1U
      || virtual_target_index > SL_HAL_I3C_MAX_VIRTUAL_TARGET_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = sl_hal_i3c_virtual_target_set_pid(ctx->i3c, virtual_target_index,
                                             config->pid_value);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sl_hal_i3c_virtual_target_set_mipi_id(ctx->i3c, virtual_target_index,
                                                 config->mipi_mfg_id,
                                                 config->prov_id_sel);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sl_hal_i3c_virtual_target_set_dcr(ctx->i3c, virtual_target_index,
                                             config->dcr);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sl_hal_i3c_virtual_target_set_static_addr(ctx->i3c, virtual_target_index,
                                                     config->static_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (config->clear_dynamic_address) {
    status = sl_hal_i3c_virtual_target_clear_dynamic_addr(ctx->i3c,
                                                          virtual_target_index);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  if (config->enable) {
    status = sl_hal_i3c_virtual_target_enable(ctx->i3c, virtual_target_index);
    if (status == SL_STATUS_OK) {
      /* IC_VIRT_TGT_EN switches RESPONSE_QUEUE to Format-2, including T0. */
      ctx->virt_tgt_enabled = true;
    }
  }

  return status;
}

/***************************************************************************//**
 * Read the dynamic address assigned to a virtual target.
 ******************************************************************************/
sl_status_t sl_i3c_get_virtual_target_dynamic_addr(sl_i3c_handle_t *i3c_handle,
                                                   uint8_t virtual_target_index,
                                                   uint8_t *dynamic_addr)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  volatile uint32_t *ctrl_reg = NULL;
  sl_status_t status;

  if (ctx == NULL || dynamic_addr == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_SLAVE) {
    return SL_STATUS_INVALID_MODE;
  }

  status = sl_hal_i3c_virtual_target_get_ctrl_register(ctx->i3c,
                                                       virtual_target_index,
                                                       &ctrl_reg);
  if (status != SL_STATUS_OK) {
    return status;
  }
  if ((ctrl_reg == NULL)
      || ((*ctrl_reg & I3C_VIRT_TGT_1_CTRL_DYNAMIC_ADDR_VALID) == 0U)) {
    *dynamic_addr = 0U;
    return SL_STATUS_OK;
  }

  *dynamic_addr = (uint8_t)((*ctrl_reg & _I3C_VIRT_TGT_1_CTRL_DYNAMIC_ADDR_MASK)
                            >> _I3C_VIRT_TGT_1_CTRL_DYNAMIC_ADDR_SHIFT);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Program a controller-mode Device Address Table (DAT) entry.
 ******************************************************************************/
sl_status_t sl_i3c_configure_target(sl_i3c_handle_t *i3c_handle,
                                    uint8_t device_index,
                                    const sl_hal_i3c_target_dat_config_t *cfg)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL || cfg == NULL || device_index >= SL_HAL_I3C_MAX_TARGET_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_CONTROLLER) {
    return SL_STATUS_INVALID_MODE;
  }

  sl_status_t status = sl_hal_i3c_configure_target(ctx->i3c,
                                                   device_index,
                                                   cfg);
  if (status != SL_STATUS_OK) {
    return status;
  }

  ctx->dat_cfg[device_index] = *cfg;
  ctx->dev_table[device_index].device_index = device_index;
  ctx->dev_table[device_index].static_addr  = cfg->static_address;
  ctx->dev_table[device_index].dynamic_addr =
    cfg->dynamic_address & (uint8_t)_I3C_DEVICE_ADDR_STATIC_ADDR_MASK;
  ctx->dev_table[device_index].is_i2c       = cfg->is_i2c_device ? 1U : 0U;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set I3C open-drain and push-pull SCL timing (controller mode).
 ******************************************************************************/
sl_status_t sl_i3c_set_i3c_timing(sl_i3c_handle_t *i3c_handle,
                                  uint8_t od_high_count, uint8_t od_low_count,
                                  uint8_t pp_high_count, uint8_t pp_low_count)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  I3C_TypeDef *i3c = ctx->i3c;
  sl_status_t status = sl_hal_i3c_set_od_timing(i3c, od_high_count, od_low_count);
  if (status != SL_STATUS_OK) {
    return status;
  }
  return sl_hal_i3c_set_pp_timing(i3c, pp_high_count, pp_low_count);
}

/***************************************************************************//**
 * Set legacy I2C SCL timing for mixed-bus operation (controller mode).
 ******************************************************************************/
sl_status_t sl_i3c_set_i2c_timing(sl_i3c_handle_t *i3c_handle,
                                  bool is_fmp_mode, uint16_t high_count, uint16_t low_count)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (is_fmp_mode) {
    return sl_hal_i3c_set_i2c_fmp_timing(ctx->i3c, high_count, low_count);
  }
  return sl_hal_i3c_set_i2c_fm_timing(ctx->i3c, high_count, low_count);
}

/***************************************************************************//**
 * Set queue and FIFO threshold levels.
 ******************************************************************************/
sl_status_t sl_i3c_set_thresholds(sl_i3c_handle_t *i3c_handle,
                                  const sl_hal_i3c_queue_thld_t *queue_thld,
                                  const sl_hal_i3c_fifo_thld_t *fifo_thld)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  I3C_TypeDef *i3c = ctx->i3c;
  sl_status_t status;

  if (queue_thld != NULL) {
    status = sl_hal_i3c_set_queue_threshold(i3c, queue_thld);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  if (fifo_thld != NULL) {
    status = i3c_validate_fifo_thlds(i3c, fifo_thld);
    if (status != SL_STATUS_OK) {
      return status;
    }
    status = sl_hal_i3c_set_fifo_threshold(i3c, fifo_thld);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Register a target-mode event callback.
 ******************************************************************************/
sl_status_t sl_i3c_set_target_callback(sl_i3c_handle_t *i3c_handle,
                                       sl_i3c_target_callback_t callback)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  ctx->target_callback = callback;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Store application context pointer on the handle.
 ******************************************************************************/
sl_status_t sl_i3c_set_user_data(sl_i3c_handle_t *i3c_handle,
                                 void *user_data)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL) {
    return SL_STATUS_INVALID_HANDLE;
  }

  ctx->user_data = user_data;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Read application context pointer from the handle.
 ******************************************************************************/
sl_status_t sl_i3c_get_user_data(sl_i3c_handle_t *i3c_handle,
                                 void **user_data)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL) {
    return SL_STATUS_INVALID_HANDLE;
  }
  if (user_data == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *user_data = ctx->user_data;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configure In-Band Interrupt (IBI) acceptance and callback (controller).
 ******************************************************************************/
sl_status_t sl_i3c_ibi_configure(sl_i3c_handle_t *i3c_handle,
                                 const sl_i3c_device_desc_t *device,
                                 sl_hal_i3c_ibi_type_t type,
                                 sl_i3c_ibi_callback_t callback)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  const bool enable = (callback != NULL);
  uint8_t type_bit;
  sl_status_t status;

  if (ctx == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_CONTROLLER) {
    return SL_STATUS_INVALID_MODE;
  }

  switch (type) {
    case SL_HAL_I3C_IBI_TYPE_HJ:
      type_bit = SLI_I3C_IBI_EN_HJ;
      status = sl_hal_i3c_controller_configure_hot_join(ctx->i3c, enable);
      if (status != SL_STATUS_OK) {
        return status;
      }
      break;

    case SL_HAL_I3C_IBI_TYPE_SIR:
    case SL_HAL_I3C_IBI_TYPE_MR: {
      if (device == NULL) {
        return SL_STATUS_INVALID_PARAMETER;
      }
      int8_t idx = i3c_find_device_slot(ctx, device);
      if (idx < 0) {
        return SL_STATUS_NOT_FOUND;
      }

      sl_hal_i3c_target_dat_config_t dat_cfg = ctx->dat_cfg[idx];
      type_bit = (type == SL_HAL_I3C_IBI_TYPE_SIR) ? SLI_I3C_IBI_EN_SIR : SLI_I3C_IBI_EN_MR;
      if (type == SL_HAL_I3C_IBI_TYPE_SIR) {
        dat_cfg.sir_enable = enable;
      } else {
        dat_cfg.mr_enable = enable;
      }

      status = sl_hal_i3c_configure_target(ctx->i3c,
                                           (uint8_t)idx,
                                           &dat_cfg);
      if (status != SL_STATUS_OK) {
        return status;
      }
      ctx->dat_cfg[idx] = dat_cfg;
      if (type == SL_HAL_I3C_IBI_TYPE_SIR) {
        ctx->dev_table[idx].ibi_enabled = enable ? 1U : 0U;
      }
      break;
    }

    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  if (enable) {
    ctx->ibi_enabled_mask |= type_bit;
    ctx->ibi_callback = callback;
  } else {
    ctx->ibi_enabled_mask = (uint8_t)(ctx->ibi_enabled_mask & (uint8_t)(~type_bit));
    if (ctx->ibi_enabled_mask == 0U) {
      ctx->ibi_callback = NULL;
    }
  }
  i3c_apply_role_interrupts(ctx);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Trigger an IBI from target mode (SIR or MR).
 ******************************************************************************/
sl_status_t sl_i3c_ibi_trigger(sl_i3c_handle_t *i3c_handle,
                               sl_hal_i3c_ibi_type_t type)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_SLAVE) {
    return SL_STATUS_INVALID_MODE;
  }

  switch (type) {
    case SL_HAL_I3C_IBI_TYPE_SIR:
      return sl_hal_i3c_trigger_slave_interrupt_request(ctx->i3c);
    case SL_HAL_I3C_IBI_TYPE_MR: {
      sl_hal_i3c_device_role_t role = SL_HAL_I3C_DEVICE_ROLE_TARGET;
      sl_status_t status = sl_hal_i3c_get_device_role(ctx->i3c, &role);
      if (status != SL_STATUS_OK) {
        return status;
      }
      /* MIPI BCR Device Role must be Controller capable to request mastership. */
      if (role != SL_HAL_I3C_DEVICE_ROLE_CONTROLLER_CAPABLE) {
        return SL_STATUS_INVALID_MODE;
      }
      return sl_hal_i3c_trigger_master_request(ctx->i3c);
    }
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }
}

/***************************************************************************//**
 * Configure SIR (Slave Interrupt Request) settings for target mode.
 ******************************************************************************/
sl_status_t sl_i3c_configure_sir(sl_i3c_handle_t *i3c_handle,
                                 const sl_i3c_sir_params_t *config)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  sl_status_t status;

  if (ctx == NULL || config == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_SLAVE) {
    return SL_STATUS_INVALID_MODE;
  }

  status = sl_hal_i3c_configure_sir(ctx->i3c,
                                    config->sir_config.mdb,
                                    config->sir_config.payload_length,
                                    config->sir_config.virtual_target_index);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return sl_hal_i3c_set_target_sir_data(ctx->i3c, config->sir_payload);
}

/***************************************************************************//**
 * Report whether this instance is the current bus controller.
 ******************************************************************************/
sl_status_t sl_i3c_is_current_controller(sl_i3c_handle_t *i3c_handle,
                                         bool *is_controller)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL) {
    return SL_STATUS_INVALID_HANDLE;
  }
  if (is_controller == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  return sl_hal_i3c_is_current_controller(ctx->i3c, is_controller);
}

/***************************************************************************//**
 * Accept Master Request via directed GETACCMST (controller hands over bus).
 ******************************************************************************/
sl_status_t sl_i3c_accept_mastership(sl_i3c_handle_t *i3c_handle,
                                     const sl_i3c_device_desc_t *device,
                                     uint32_t timeout_ms)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  uint8_t new_master_da = 0U;
  sl_status_t status;
  int8_t dev_idx;

  if (ctx == NULL || device == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_CONTROLLER) {
    return SL_STATUS_INVALID_MODE;
  }

  dev_idx = i3c_find_device_slot(ctx, device);
  if (dev_idx < 0) {
    return SL_STATUS_NOT_FOUND;
  }

  sl_i3c_ccc_payload_t payload = {
    .ccc = {
      .id = SL_I3C_CCC_DIRECT_GETACCMST,
      .addr = device->dynamic_addr,
    },
    .data = &new_master_da,
    .len = 1U,
    .tid = 0U,
    .is_read = 1U,
    .roc = 1U,
    .toc = 1U,
  };
  sl_i3c_transfer_cmd_t command = {
    .tid = 0U,
    .cmd = (uint8_t)SL_I3C_CCC_DIRECT_GETACCMST,
    .cp = 1U,
    .rnw = 1U,
    .roc = 1U,
    .toc = 1U,
    .dev_indx = (uint8_t)dev_idx,
  };
  command.speed.i3c_speed = SL_I3C_SPEED_SDR0;

  status = sl_i3c_do_ccc(i3c_handle, &payload, &command, &new_master_da, 1U, timeout_ms);
  if (status != SL_STATUS_OK) {
    return status;
  }

  /*
   * Successful GETACCMST: this controller becomes a target. Sync software role
   * and interrupt masks; BUSOWNER_UPDATED also updates role when it fires.
   */
  ctx->role = SL_I3C_ROLE_SLAVE;
  i3c_apply_role_interrupts(ctx);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set target SLV_CHAR_CTRL writable fields (target mode).
 ******************************************************************************/
sl_status_t sl_i3c_set_characteristics(sl_i3c_handle_t *i3c_handle,
                                       const sl_hal_i3c_slave_char_config_t *cfg)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL || cfg == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_SLAVE) {
    return SL_STATUS_INVALID_MODE;
  }

  return sl_hal_i3c_set_slave_characteristics(ctx->i3c, cfg);
}

/***************************************************************************//**
 * Configure target provisional ID and MIPI manufacturer ID (target mode).
 ******************************************************************************/
sl_status_t sl_i3c_set_target_id(sl_i3c_handle_t *i3c_handle,
                                 const sl_i3c_device_id_config_t *config)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL || config == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_SLAVE) {
    return SL_STATUS_INVALID_MODE;
  }

  I3C_TypeDef *i3c = ctx->i3c;
  sl_status_t status = SL_STATUS_OK;

  if (config->set_mipi_id) {
    status = sl_hal_i3c_set_slave_mipi_id(i3c, config->mipi_id.mfg_id, config->mipi_id.is_random);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }
  if (config->set_pid) {
    status = sl_hal_i3c_set_slave_pid(i3c, config->pid.part_id);
  }
  return status;
}

/***************************************************************************//**
 * Get the last RESPONSE_QUEUE ERR_STATUS seen by the driver.
 ******************************************************************************/
sl_status_t sl_i3c_get_last_response_error(sl_i3c_handle_t *i3c_handle,
                                           sl_i3c_response_error_t *error)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL) {
    return SL_STATUS_INVALID_HANDLE;
  }
  if (error == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *error = (sl_i3c_response_error_t)ctx->last_resp_error;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Abort the active transfer.
 ******************************************************************************/
sl_status_t sl_i3c_abort_transfer(sl_i3c_handle_t *i3c_handle)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  sl_status_t status = SL_STATUS_OK;

  if (ctx == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (ctx->role == SL_I3C_ROLE_CONTROLLER) {
    status = sl_hal_i3c_abort_controller_transfer(ctx->i3c);
  }
  /* Flush queues / Format-2 arm; resume if PRESENT_STATE shows halt. */
  i3c_recover_after_error(ctx);
  ctx->transfer_callback = NULL;
  return status;
}

/***************************************************************************//**
 * Request DEVICE_CTRL.RESUME after a halt.
 ******************************************************************************/
sl_status_t sl_i3c_resume(sl_i3c_handle_t *i3c_handle)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);

  if (ctx == NULL) {
    return SL_STATUS_INVALID_HANDLE;
  }

  return sl_hal_i3c_resume(ctx->i3c);
}

/***************************************************************************//**
 * Reset a command queue, response queue, FIFO, or IBI queue.
 ******************************************************************************/
sl_status_t sl_i3c_buffer_reset(sl_i3c_handle_t *i3c_handle, sl_hal_i3c_buffer_reset_t buffer_type)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);

  if (ctx == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  return sl_hal_i3c_buffer_reset(ctx->i3c, buffer_type);
}

/***************************************************************************//**
 * Read command/response queue and FIFO fill levels.
 ******************************************************************************/
sl_status_t sl_i3c_get_buffer_levels(sl_i3c_handle_t *i3c_handle, sl_i3c_buffer_levels_t *levels)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL || levels == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sl_status_t status = sl_hal_i3c_get_queue_status_level(ctx->i3c, &levels->queue_status_level);
  if (status != SL_STATUS_OK) {
    return status;
  }
  return sl_hal_i3c_get_data_buffer_level(ctx->i3c, &levels->data_buffer_level);
}

/***************************************************************************//**
 * Issue a bus reset pattern from controller mode.
 ******************************************************************************/
sl_status_t sl_i3c_bus_reset(sl_i3c_handle_t *i3c_handle, sl_hal_i3c_bus_reset_type_t type)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_CONTROLLER) {
    return SL_STATUS_INVALID_MODE;
  }
  return sl_hal_i3c_set_bus_reset_type(ctx->i3c, type);
}

/***************************************************************************//**
 * Read PID, BCR, and DCR for a DAT entry (controller mode).
 ******************************************************************************/
sl_status_t sl_i3c_get_target_characteristics(sl_i3c_handle_t *i3c_handle,
                                              uint8_t target_index,
                                              sl_i3c_target_characteristics_t *characteristics)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  const uint32_t *dct_base = NULL;
  sl_status_t status;

  if (ctx == NULL || characteristics == NULL || target_index >= SL_HAL_I3C_MAX_TARGET_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (ctx->role != SL_I3C_ROLE_CONTROLLER) {
    return SL_STATUS_INVALID_MODE;
  }

  status = sl_hal_i3c_get_dev_char_table_base(ctx->i3c, &dct_base);
  if (status != SL_STATUS_OK || dct_base == NULL) {
    return (status != SL_STATUS_OK) ? status : SL_STATUS_FAIL;
  }

  // Each DCT entry is four consecutive 32-bit words (PID / BCR / DCR layout).
  uint32_t dct0 = dct_base[target_index * 4U + 0U];
  uint32_t dct1 = dct_base[target_index * 4U + 1U];
  uint32_t dct2 = dct_base[target_index * 4U + 2U];
  uint32_t dct3 = dct_base[target_index * 4U + 3U];

  sli_i3c_parse_dct_entry(dct0,
                          dct1,
                          dct2,
                          dct3,
                          characteristics->pid,
                          &characteristics->bcr,
                          &characteristics->dcr,
                          NULL);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Read target device status (target mode).
 ******************************************************************************/
sl_status_t sl_i3c_get_target_status(sl_i3c_handle_t *i3c_handle,
                                     sl_i3c_target_status_t *status_info)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL || status_info == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  uint32_t raw_status;
  sl_status_t result = sl_hal_i3c_get_ccc_device_status(ctx->i3c, &raw_status);
  if (result == SL_STATUS_OK) {
    status_info->pending_intr =
      (uint16_t)((raw_status & _I3C_CCC_DEVICE_STATUS_PENDING_INTR_MASK)
                 >> _I3C_CCC_DEVICE_STATUS_PENDING_INTR_SHIFT);
    status_info->activity_mode =
      (uint8_t)((raw_status & _I3C_CCC_DEVICE_STATUS_ACTIVITY_MODE_MASK)
                >> _I3C_CCC_DEVICE_STATUS_ACTIVITY_MODE_SHIFT);
    /* Bit0 = underflow, bit1 = overflow (CCC_DEVICE_STATUS bits 8 and 10). */
    status_info->overflow_underflow =
      (uint8_t)(((raw_status & I3C_CCC_DEVICE_STATUS_UNDERFLOW_ERR) != 0U ? 1U : 0U)
                | ((raw_status & I3C_CCC_DEVICE_STATUS_OVERFLOW_ERR) != 0U ? 2U : 0U));
  }
  return result;
}

/***************************************************************************//**
 * Read static and dynamic addresses configured for target mode.
 ******************************************************************************/
sl_status_t sl_i3c_get_target_addresses(sl_i3c_handle_t *i3c_handle,
                                        sl_i3c_target_addresses_t *addresses)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL || addresses == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sl_status_t status = sl_hal_i3c_get_static_address(ctx->i3c, &addresses->static_address);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = sl_hal_i3c_get_dynamic_address(ctx->i3c, &addresses->dynamic_address);
  if (status == SL_STATUS_FAIL) {
    /* DA not assigned yet — normal before ENTDAA / SETDASA. */
    addresses->dynamic_address = 0U;
    return SL_STATUS_OK;
  }
  return status;
}

/***************************************************************************//**
 * I3C interrupt service routine entry point.
 ******************************************************************************/
void sl_i3c_irq_handler(sl_i3c_handle_t *i3c_handle)
{
  sli_i3c_context_t *ctx = handle_to_ctx(i3c_handle);
  if (ctx == NULL) {
    return;
  }
  if (ctx->role == SL_I3C_ROLE_CONTROLLER) {
    sli_i3c_controller_dispatch_interrupt(ctx);
  } else {
    sli_i3c_target_dispatch_interrupt(ctx);
  }
}

/*******************************************************************************
 **************************   INTERNAL FUNCTIONS   *****************************
 ******************************************************************************/

/***************************************************************************//**
 * Configure hardware and interrupts after context setup.
 ******************************************************************************/
sl_status_t sli_i3c_init_core(sli_i3c_context_t *ctx,
                              const sl_i3c_init_params_t *init_params)
{
  I3C_TypeDef *i3c = ctx->i3c;
  sl_status_t status;

  sl_hal_i3c_enable_peripheral_clock(i3c);
  /* Soft-reset while ENABLE=1 can hang; disable first (matches HAL bring-up). */
  (void)sl_hal_i3c_disable(i3c);
  (void)sl_hal_i3c_soft_reset(i3c);

  sl_hal_i3c_disable_interrupt(i3c, _I3C_INTR_STATUS_EN_MASK);
  sl_hal_i3c_disable_interrupt_signal(i3c, _I3C_INTR_SIGNAL_EN_MASK);
  sl_hal_i3c_clear_interrupt_status(i3c, _I3C_INTR_STATUS_MASK);

  if (ctx->role == SL_I3C_ROLE_CONTROLLER) {
    const sl_i3c_controller_config_t *cfg = init_params->controller_cfg;
    if (cfg == NULL) {
      return SL_STATUS_INVALID_PARAMETER;
    }

    status = sl_hal_i3c_set_controller_mode(i3c);
    if (status != SL_STATUS_OK) {
      return status;
    }

    sl_hal_i3c_set_od_timing(i3c, cfg->i3c_timing.od_high_count, cfg->i3c_timing.od_low_count);
    sl_hal_i3c_set_pp_timing(i3c, cfg->i3c_timing.pp_high_count, cfg->i3c_timing.pp_low_count);

    if (cfg->i2c_timing.is_fmp_mode) {
      sl_hal_i3c_set_i2c_fmp_timing(i3c, cfg->i2c_timing.high_count, cfg->i2c_timing.low_count);
    } else {
      sl_hal_i3c_set_i2c_fm_timing(i3c, cfg->i2c_timing.high_count, cfg->i2c_timing.low_count);
    }

    sl_hal_i3c_set_i2c_slave_present(i3c, cfg->i2c_slave_present);

    status = i3c_validate_fifo_thlds(i3c, &cfg->fifo_thlds);
    if (status != SL_STATUS_OK) {
      return status;
    }

    status = sl_hal_i3c_set_queue_threshold(i3c, &cfg->queue_thlds);
    if (status != SL_STATUS_OK) {
      return status;
    }

    status = sl_hal_i3c_set_fifo_threshold(i3c, &cfg->fifo_thlds);
    if (status != SL_STATUS_OK) {
      return status;
    }

    if (cfg->dynamic_address != 0) {
      sl_hal_i3c_set_dynamic_address(i3c, cfg->dynamic_address);
    }

    sl_hal_i3c_set_bus_free_time(i3c, cfg->bus_free_time);
    (void)sl_hal_i3c_buffer_reset(i3c, SL_HAL_I3C_RESET_ALL_QUEUES);
  } else {
    const sl_i3c_slave_config_t *cfg = init_params->slave_cfg;
    if (cfg == NULL) {
      return SL_STATUS_INVALID_PARAMETER;
    }

    status = sl_hal_i3c_set_target_mode(i3c);
    if (status != SL_STATUS_OK) {
      return status;
    }

    switch (cfg->address_type) {
      case SL_I3C_STATIC_ADDRESS:
        sl_hal_i3c_set_static_address(i3c, cfg->address);
        break;
      case SL_I3C_DYNAMIC_ADDRESS:
        sl_hal_i3c_set_dynamic_address(i3c, cfg->address);
        break;
      case SL_I3C_NO_ADDRESS:
      default:
        break;
    }

    status = i3c_validate_fifo_thlds(i3c, &cfg->fifo_thlds);
    if (status != SL_STATUS_OK) {
      return status;
    }

    {
      sl_hal_i3c_target_queue_thld_t tq = {
        .cmd_empty_buf_thld = cfg->queue_thlds.cmd_empty_buf_thld,
        .resp_buf_thld = cfg->queue_thlds.resp_buf_thld,
      };
      status = sl_hal_i3c_set_target_queue_threshold(i3c, &tq);
      if (status != SL_STATUS_OK) {
        return status;
      }
    }

    status = sl_hal_i3c_set_fifo_threshold(i3c, &cfg->fifo_thlds);
    if (status != SL_STATUS_OK) {
      return status;
    }

    status = sl_hal_i3c_set_max_read_speed(i3c, cfg->max_read_speed);
    if (status != SL_STATUS_OK) {
      return status;
    }

    status = sl_hal_i3c_set_clock_data_turnaround(i3c, cfg->clk_data_turnaround);
    if (status != SL_STATUS_OK) {
      return status;
    }

    status = sl_hal_i3c_set_idle_count_multiplier(i3c, cfg->idle_cnt_multiplier);
    if (status != SL_STATUS_OK) {
      return status;
    }

    status = sl_hal_i3c_set_bus_available_time(i3c, (uint16_t)cfg->bus_free_avail_timing);
    if (status != SL_STATUS_OK) {
      return status;
    }

    status = sl_hal_i3c_set_bus_idle_time(i3c, cfg->bus_idle_timing);
    if (status != SL_STATUS_OK) {
      return status;
    }

    status = sl_hal_i3c_set_release_sda_timing(i3c, cfg->release_sda_timing);
    if (status != SL_STATUS_OK) {
      return status;
    }

    if (cfg->mode_i2c) {
      status = sl_hal_i3c_enable_i2c_mode(i3c);
    } else {
      status = sl_hal_i3c_disable_i2c_mode(i3c);
    }
    if (status != SL_STATUS_OK) {
      return status;
    }

    if (cfg->adaptive_i2c_i3c) {
      status = sl_hal_i3c_enable_adaptive_mode(i3c);
      if (status != SL_STATUS_OK) {
        return status;
      }
    }

    status = sl_hal_i3c_target_set_hot_join_ibi(i3c, cfg->hot_join_enable);
    if (status != SL_STATUS_OK) {
      return status;
    }

    (void)sl_hal_i3c_buffer_reset(i3c, SL_HAL_I3C_RESET_ALL_QUEUES);
  }

  i3c_apply_role_interrupts(ctx);
  sl_hal_i3c_enable(i3c);

  IRQn_Type irqn = I3C0_IRQn;
  sl_interrupt_manager_clear_irq_pending(irqn);
  sl_interrupt_manager_enable_irq(irqn);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Dispatch interrupt handler for the I3C controller.
 ******************************************************************************/
void sli_i3c_controller_dispatch_interrupt(sli_i3c_context_t *ctx)
{
  I3C_TypeDef *i3c = ctx->i3c;
  uint32_t pending = 0;

  sl_hal_i3c_get_pending_interrupts(i3c, &pending);

  if (pending & I3C_INTR_STATUS_TX_THLD_STS) {
    i3c_on_tx_threshold(ctx);
  }

  if (pending & I3C_INTR_STATUS_RX_THLD_STS) {
    i3c_on_rx_threshold(ctx);
  }

  // Service IBIs before transfer completion in the same IRQ.
  if (pending & I3C_INTR_STATUS_IBI_THLD_STS) {
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_IBI_THLD_STS);
    i3c_read_ibi_queue(ctx);
  }

  if (pending & I3C_INTR_STATUS_RESP_READY_STS) {
    sli_i3c_xfer_state_t prev_state = (sli_i3c_xfer_state_t)ctx->xfer_state;

    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_RESP_READY_STS);
    sl_i3c_transfer_event_t event = i3c_read_controller_response(ctx);

    /* Combined write-then-read: first RESP is write; keep waiting for read. */
    if (prev_state == SLI_I3C_XFER_WRITE_READ_PENDING
        && event == SL_I3C_EVENT_TRANSFER_COMPLETED) {
      ctx->xfer_state = SLI_I3C_XFER_READ_PENDING;
      return;
    }

    ctx->xfer_state = SLI_I3C_XFER_IDLE;

    if (event != SL_I3C_EVENT_TRANSFER_COMPLETED) {
      /* Clear co-pending TRANSFER_ERR so it does not re-fire on the next transfer. */
      sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_TRANSFER_ERR_STS);
      (void)sl_hal_i3c_buffer_reset(i3c, SL_HAL_I3C_RESET_ALL_QUEUES);
      i3c_resume_if_halted(i3c);
    }

    i3c_complete_transfer(ctx, event);
    return;
  }

  if (pending & I3C_INTR_STATUS_TRANSFER_ERR_STS) {
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_TRANSFER_ERR_STS);
    /* Pop the response queue when available to capture the real ERR_STATUS;
     * i3c_recover_after_error() flushes queues, so the pop must come first. */
    uint8_t resp_level = 0U;
    (void)sl_hal_i3c_get_resp_buf_level(i3c, &resp_level);
    if (resp_level > 0U) {
      (void)i3c_read_controller_response(ctx); /* sets ctx->last_resp_error */
    } else {
      ctx->last_resp_error = SLI_I3C_RESP_ERR_UNSPECIFIED;
    }
    i3c_recover_after_error(ctx);
    i3c_complete_transfer(ctx, SL_I3C_EVENT_TRANSFER_ERROR);
    return;
  }

  if (pending & I3C_INTR_STATUS_TRANSFER_ABORT_STS) {
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_TRANSFER_ABORT_STS);
    ctx->last_resp_error = SLI_I3C_RESP_ERR_TRANSFER_ABORT;
    i3c_recover_after_error(ctx);
    i3c_complete_transfer(ctx, SL_I3C_EVENT_TRANSFER_ABORTED);
    return;
  }

  if (pending & I3C_INTR_STATUS_BUSOWNER_UPDATED_STS) {
    bool is_controller = false;
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_BUSOWNER_UPDATED_STS);
    (void)sl_hal_i3c_is_current_controller(i3c, &is_controller);
    if (is_controller) {
      ctx->role = SL_I3C_ROLE_CONTROLLER;
    } else {
      ctx->role = SL_I3C_ROLE_SLAVE;
    }
    i3c_apply_role_interrupts(ctx);
    i3c_notify_target(ctx, SL_I3C_TARGET_EVENT_BUS_OWNER_UPDATED);
  }
}

/***************************************************************************//**
 * Dispatch interrupt handler for the I3C target.
 ******************************************************************************/
void sli_i3c_target_dispatch_interrupt(sli_i3c_context_t *ctx)
{
  I3C_TypeDef *i3c = ctx->i3c;
  uint32_t pending = 0;

  sl_hal_i3c_get_pending_interrupts(i3c, &pending);

  if (pending & I3C_INTR_STATUS_TX_THLD_STS) {
    i3c_on_tx_threshold(ctx);
  }

  if (pending & I3C_INTR_STATUS_RX_THLD_STS) {
    i3c_on_rx_threshold(ctx);
  }

  if (pending & I3C_INTR_STATUS_RESP_READY_STS) {
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_RESP_READY_STS);

    /*
     * Physical private R/W uses COMMAND_QUEUE + regular RESP (16-bit
     * DATA_LENGTH). Format-2 EXT_CMD / extended RESP is only for virtual
     * targets and vendor-specific transfers.
     */
    // Bounded drain: at most 256 responses per interrupt.
    for (uint32_t drained = 0U; drained < 256U; drained++) {
      uint32_t resp_word = 0;
      uint8_t resp_blr;
      sli_i3c_response_entry_t resp;
      sl_i3c_transfer_event_t event;
      uint32_t remaining;
      uint32_t chunk;
      bool use_ext_resp;

      if (sl_hal_i3c_get_resp_buf_level(i3c, &resp_blr) != SL_STATUS_OK) {
        break;
      }
      if (resp_blr == 0U) {
        break;
      }

      if (sl_hal_i3c_get_command_queue_response(i3c, &resp_word) != SL_STATUS_OK) {
        break;
      }
      use_ext_resp = i3c_target_uses_ext_resp(ctx);
      if (use_ext_resp) {
        sli_i3c_parse_target_ext_response(resp_word, &resp);
      } else {
        sli_i3c_parse_response(resp_word, &resp);
      }
      ctx->last_resp_error = resp.err_status;

      if (!use_ext_resp) {
        uint32_t level = 0U;
        uint32_t rx_blr = 0U;

        (void)sl_hal_i3c_get_data_buffer_level(i3c, &level);
        rx_blr = (level & _I3C_DATA_BUFFER_STATUS_LEVEL_RX_BUF_BLR_MASK)
                 >> _I3C_DATA_BUFFER_STATUS_LEVEL_RX_BUF_BLR_SHIFT;

        /* COMMAND_QUEUE TX RESP has no RX payload (Read-then-Write: this fires first). */
        if (ctx->await_phys_tx_done && (ctx->rx_offset == 0U) && (rx_blr == 0U)) {
          ctx->await_phys_tx_done = false;
          if (ctx->xfer_state == SLI_I3C_XFER_TARGET_TX_PENDING) {
            ctx->xfer_state = SLI_I3C_XFER_IDLE;
          }
          continue;
        }

        event = sli_i3c_resp_err_to_event(resp.err_status);
        if ((ctx->xfer_state != SLI_I3C_XFER_TARGET_RX_PENDING)
            || (ctx->rx_buf == NULL)
            || (ctx->rx_len == 0U)) {
          i3c_discard_rx_fifo(i3c, resp.data_length);
          continue;
        }
        if (event != SL_I3C_EVENT_TRANSFER_COMPLETED) {
          i3c_discard_rx_fifo(i3c, resp.data_length);
          (void)sl_hal_i3c_buffer_reset(i3c, SL_HAL_I3C_RESET_ALL_QUEUES);
          i3c_resume_if_halted(i3c);
          i3c_complete_transfer(ctx, event);
          continue;
        }

        // RX threshold copied part of the data already; read the rest up to
        // the buffer size and drop any surplus.
        remaining = (uint32_t)ctx->rx_len - (uint32_t)ctx->rx_offset;
        chunk = (resp.data_length < remaining) ? resp.data_length : remaining;
        if (chunk > 0U) {
          uint32_t got = i3c_read_rx_fifo(i3c, ctx->rx_buf + ctx->rx_offset, chunk);
          ctx->rx_offset = (uint16_t)(ctx->rx_offset + got);
        }
        if (resp.data_length > chunk) {
          i3c_discard_rx_fifo(i3c, resp.data_length - chunk);
        }
        if (ctx->rx_offset >= ctx->rx_len) {
          i3c_set_irq_enabled(i3c, SLI_I3C_RX_THLD_INTR_MASK, false);
          i3c_complete_transfer(ctx, event);
        }
        continue;
      }

      /*
       * Format-2: EXT_CMD VT / vendor-specific. TX completions report
       * data_length == 0. Writes may be chunked by RSP_DATA_THLD.
       */
      if ((resp.data_length == 0U) || resp.deftgts) {
        if (ctx->xfer_state == SLI_I3C_XFER_TARGET_RX_PENDING) {
          /* 2048-byte write: Format-2 DATA_LENGTH is 8-bit (2048→0). RX_THLD
           * may already have the payload; LAST with DL=0 still completes RX. */
          if ((!resp.deftgts) && resp.last
              && (ctx->rx_buf != NULL)
              && (ctx->rx_offset >= ctx->rx_len) && (ctx->rx_len > 0U)) {
            i3c_set_irq_enabled(i3c, SLI_I3C_RX_THLD_INTR_MASK, false);
            i3c_complete_transfer(ctx, sli_i3c_resp_err_to_event(resp.err_status));
          }
          continue;
        }
        if ((resp.data_length == 0U)
            && (ctx->xfer_state == SLI_I3C_XFER_TARGET_TX_PENDING)) {
          ctx->await_ext_tx_done = false;
          ctx->expect_target_id_mask = 0U;
          if (ctx->transfer_callback != NULL) {
            i3c_complete_transfer(ctx, sli_i3c_resp_err_to_event(resp.err_status));
          } else {
            ctx->xfer_state = SLI_I3C_XFER_IDLE;
          }
        }
        continue;
      }

      event = sli_i3c_resp_err_to_event(resp.err_status);

      if ((ctx->xfer_state != SLI_I3C_XFER_TARGET_RX_PENDING)
          || (ctx->rx_buf == NULL)
          || (ctx->rx_len == 0U)) {
        i3c_discard_rx_fifo(i3c, resp.data_length);
        continue;
      }

      // While the armed virtual-target TX is still pending (CMD_VLD set),
      // incoming write data is not for this RX request; drop it.
      if (ctx->await_ext_tx_done) {
        bool ext_tx_pending = false;
        if ((sl_hal_i3c_ext_cmd_is_valid(i3c, ctx->ext_cmd_index, &ext_tx_pending) == SL_STATUS_OK)
            && ext_tx_pending) {
          i3c_discard_rx_fifo(i3c, resp.data_length);
          continue;
        }
        ctx->await_ext_tx_done = false;
      }

      if ((ctx->expect_target_id_mask != 0U)
          && ((resp.tid & ctx->expect_target_id_mask) == 0U)) {
        i3c_discard_rx_fifo(i3c, resp.data_length);
        continue;
      }

      if (event != SL_I3C_EVENT_TRANSFER_COMPLETED) {
        i3c_discard_rx_fifo(i3c, resp.data_length);
        i3c_cancel_ext_tx(ctx);
        (void)sl_hal_i3c_buffer_reset(i3c, SL_HAL_I3C_RESET_ALL_QUEUES);
        i3c_resume_if_halted(i3c);
        ctx->xfer_state = SLI_I3C_XFER_IDLE;
        i3c_complete_transfer(ctx, event);
        continue;
      }

      // A first chunk with CMD_SIZE carries the command bytes ahead of the
      // data in the RX FIFO; skip them.
      if (resp.first && (resp.cmd_size != 0U)) {
        i3c_discard_rx_fifo(i3c, (uint32_t)resp.cmd_size);
      }

      remaining = (uint32_t)ctx->rx_len - (uint32_t)ctx->rx_offset;
      if (remaining > 0U) {
        chunk = i3c_read_rx_fifo(i3c, ctx->rx_buf + ctx->rx_offset, remaining);
        ctx->rx_offset = (uint16_t)(ctx->rx_offset + chunk);
      }

      // Buffer full on the last chunk: drop any surplus (up to a full FIFO).
      if (resp.last && (ctx->rx_offset >= ctx->rx_len)) {
        i3c_discard_rx_fifo(i3c, 128U);
      }

      if ((ctx->rx_offset >= ctx->rx_len) || resp.last) {
        ctx->expect_target_id_mask = 0U;
        i3c_set_irq_enabled(i3c, SLI_I3C_RX_THLD_INTR_MASK, false);
        i3c_complete_transfer(ctx, event);
      }
    }
    return;
  }

  if (pending & I3C_INTR_STATUS_CCC_UPDATED_STS) {
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_CCC_UPDATED_STS);
    i3c_notify_target(ctx, SL_I3C_TARGET_EVENT_CCC_UPDATED);
    i3c_resume_if_halted(i3c);
  }

  if (pending & I3C_INTR_STATUS_DYN_ADDR_ASSGN_STS) {
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_DYN_ADDR_ASSGN_STS);
    i3c_notify_target(ctx, SL_I3C_TARGET_EVENT_DYNAMIC_ADDR_ASSIGNED);
  }

  if (pending & I3C_INTR_STATUS_READ_REQ_RECV_STS) {
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_READ_REQ_RECV_STS);
    i3c_notify_target(ctx, SL_I3C_TARGET_EVENT_READ_REQUEST);
  }

  if (pending & I3C_INTR_STATUS_IBI_UPDATED_STS) {
    uint32_t ibi_resp = 0U;
    uint8_t ibi_sts = SLI_I3C_SLV_IBI_STS_NONE;

    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_IBI_UPDATED_STS);
    if (sl_hal_i3c_slave_get_ibi_response(i3c, &ibi_resp) == SL_STATUS_OK) {
      ibi_sts = (uint8_t)((ibi_resp & _I3C_SLV_IBI_RESP_IBI_STS_MASK)
                          >> _I3C_SLV_IBI_RESP_IBI_STS_SHIFT);
      if (ibi_sts == SLI_I3C_SLV_IBI_STS_EARLY_TERMINATE) {
        (void)sl_hal_i3c_buffer_reset(i3c, SL_HAL_I3C_RESET_TX_FIFO);
        i3c_resume_if_halted(i3c);
      }
    }
    if (ibi_sts == SLI_I3C_SLV_IBI_STS_SUCCESS) {
      i3c_notify_target(ctx, SL_I3C_TARGET_EVENT_IBI_SUCCESS);
    } else if (ibi_sts == SLI_I3C_SLV_IBI_STS_EARLY_TERMINATE) {
      i3c_notify_target(ctx, SL_I3C_TARGET_EVENT_IBI_EARLY_TERMINATE);
    } else if (ibi_sts == SLI_I3C_SLV_IBI_STS_NOT_ATTEMPTED) {
      i3c_notify_target(ctx, SL_I3C_TARGET_EVENT_IBI_NOT_ATTEMPTED);
    }
  }

  if (pending & I3C_INTR_STATUS_DEFSLV_STS) {
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_DEFSLV_STS);
    i3c_notify_target(ctx, SL_I3C_TARGET_EVENT_DEFSLV);
  }

  if (pending & I3C_INTR_STATUS_START_DET_STS) {
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_START_DET_STS);
    i3c_notify_target(ctx, SL_I3C_TARGET_EVENT_START);
  }

  if (pending & I3C_INTR_STATUS_TGT_RST_PATTERN_DET_STS) {
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_TGT_RST_PATTERN_DET_STS);
    i3c_notify_target(ctx, SL_I3C_TARGET_EVENT_RESET_PATTERN);
  }

  if (pending & I3C_INTR_STATUS_GRP_ADDR_ASSGN_STS) {
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_GRP_ADDR_ASSGN_STS);
    i3c_notify_target(ctx, SL_I3C_TARGET_EVENT_GROUP_ADDR_ASSIGNED);
  }

  if (pending & I3C_INTR_STATUS_SDA_RELEASED_STS) {
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_SDA_RELEASED_STS);
    i3c_notify_target(ctx, SL_I3C_TARGET_EVENT_SDA_RELEASED);
  }

  if (pending & I3C_INTR_STATUS_BUSOWNER_UPDATED_STS) {
    bool is_controller = false;
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_BUSOWNER_UPDATED_STS);
    (void)sl_hal_i3c_is_current_controller(i3c, &is_controller);
    /*
     * After MR + GETACCMST, HW flips CURRENT_MASTER. Keep driver role and IRQ
     * masks aligned so subsequent APIs match the new bus ownership.
     */
    if (is_controller) {
      ctx->role = SL_I3C_ROLE_CONTROLLER;
    } else {
      ctx->role = SL_I3C_ROLE_SLAVE;
    }
    i3c_apply_role_interrupts(ctx);
    i3c_notify_target(ctx, SL_I3C_TARGET_EVENT_BUS_OWNER_UPDATED);
    return;
  }

  if (pending & I3C_INTR_STATUS_TRANSFER_ERR_STS) {
    sl_hal_i3c_clear_interrupt_status(i3c, I3C_INTR_STATUS_TRANSFER_ERR_STS);
    /* Pop the response queue when available to capture the real ERR_STATUS;
     * i3c_recover_after_error() flushes queues and the RX FIFO, so the pop
     * must come first. Decode it with the same format as RESP_READY. */
    uint8_t resp_level = 0U;
    (void)sl_hal_i3c_get_resp_buf_level(i3c, &resp_level);
    if (resp_level > 0U) {
      uint32_t resp_word = 0U;
      sli_i3c_response_entry_t resp;

      (void)sl_hal_i3c_get_command_queue_response(i3c, &resp_word);
      if (i3c_target_uses_ext_resp(ctx)) {
        sli_i3c_parse_target_ext_response(resp_word, &resp);
      } else {
        sli_i3c_parse_response(resp_word, &resp);
      }
      ctx->last_resp_error = resp.err_status;
    } else {
      ctx->last_resp_error = SLI_I3C_RESP_ERR_UNSPECIFIED;
    }
    i3c_recover_after_error(ctx);
    i3c_complete_transfer(ctx, SL_I3C_EVENT_TRANSFER_ERROR);
  }
}

/***************************************************************************//**
 * Find a device DAT index by 7-bit static or dynamic address.
 ******************************************************************************/
int8_t sli_i3c_find_dev_index(sli_i3c_context_t *ctx, uint8_t address)
{
  // Address 0 is not a valid I2C static address nor I3C dynamic address, and
  // unpopulated DAT slots have both fields zero. Treating 0 as searchable would
  // make a legacy I2C device (dynamic_addr == 0) match the first empty slot,
  // pointing transfers at the wrong DAT entry. Reject 0 so the caller's
  // static-address fallback is used instead.
  if (address == 0) {
    return -1;
  }
  for (uint8_t i = 0; i < SL_HAL_I3C_MAX_TARGET_COUNT; i++) {
    if (ctx->dev_table[i].dynamic_addr == address
        || ctx->dev_table[i].static_addr == address) {
      return (int8_t)i;
    }
  }
  return -1;
}

/***************************************************************************//**
 * Write bytes from a buffer into the TX FIFO.
 ******************************************************************************/
uint32_t sli_i3c_write_tx_fifo(I3C_TypeDef *i3c, const uint8_t *buf, uint32_t len)
{
  uint32_t offset = 0U;

  // Write one 32-bit word per free slot; stop when the FIFO is full so it
  // never overruns. The TX threshold interrupt writes the rest later.
  while (offset < len) {
    uint32_t level = 0U;
    uint32_t data = 0U;

    if (sl_hal_i3c_get_data_buffer_level(i3c, &level) != SL_STATUS_OK) {
      break;
    }
    if (((level & _I3C_DATA_BUFFER_STATUS_LEVEL_TX_BUF_EMPTY_LOC_MASK)
         >> _I3C_DATA_BUFFER_STATUS_LEVEL_TX_BUF_EMPTY_LOC_SHIFT) == 0U) {
      break;
    }
    // Pack up to 4 bytes little-endian (a partial last word is zero-padded).
    for (uint32_t b = 0U; (b < 4U) && (offset < len); b++, offset++) {
      data |= ((uint32_t)buf[offset]) << (b * 8U);
    }
    (void)sl_hal_i3c_set_transfer_data(i3c, data);
  }

  return offset;
}

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Return the handle as a context, or NULL if it is not initialized.
 ******************************************************************************/
static sli_i3c_context_t *handle_to_ctx(sl_i3c_handle_t *handle)
{
  if (handle == NULL || !handle->initialized) {
    return NULL;
  }
  return handle;
}

/***************************************************************************//**
 * Enable or disable interrupt sources in both INTR_STATUS_EN (latch the status)
 * and INTR_SIGNAL_EN (route it to the IRQ line).
 ******************************************************************************/
static void i3c_set_irq_enabled(I3C_TypeDef *i3c, uint32_t mask, bool enable)
{
  if (enable) {
    (void)sl_hal_i3c_enable_interrupt(i3c, mask);
    (void)sl_hal_i3c_enable_interrupt_signal(i3c, mask);
  } else {
    (void)sl_hal_i3c_disable_interrupt(i3c, mask);
    (void)sl_hal_i3c_disable_interrupt_signal(i3c, mask);
  }
}

/***************************************************************************//**
 * Enable the interrupt set of the current role and disable the other role's
 * set. Called at init, on IBI configuration, and on bus-ownership change; not
 * per transfer.
 ******************************************************************************/
static void i3c_apply_role_interrupts(const sli_i3c_context_t *ctx)
{
  if (ctx->role == SL_I3C_ROLE_CONTROLLER) {
    i3c_set_irq_enabled(ctx->i3c, SLI_I3C_TARGET_INTR_MASK, false);
    i3c_set_irq_enabled(ctx->i3c, SLI_I3C_CONTROLLER_INTR_MASK, true);
    // IBI threshold interrupt only while at least one IBI type is accepted.
    i3c_set_irq_enabled(ctx->i3c, I3C_INTR_STATUS_EN_IBI_THLD_STS_EN,
                        ctx->ibi_enabled_mask != 0U);
  } else {
    i3c_set_irq_enabled(ctx->i3c,
                        SLI_I3C_CONTROLLER_INTR_MASK | I3C_INTR_STATUS_EN_IBI_THLD_STS_EN,
                        false);
    i3c_set_irq_enabled(ctx->i3c, SLI_I3C_TARGET_INTR_MASK, true);
  }
}

/***************************************************************************//**
 * Find the DAT slot of @p device: by dynamic address first, then by static
 * address (legacy I2C devices and targets before address assignment).
 ******************************************************************************/
static int8_t i3c_find_device_slot(sli_i3c_context_t *ctx,
                                   const sl_i3c_device_desc_t *device)
{
  int8_t idx = sli_i3c_find_dev_index(ctx, device->dynamic_addr);

  if (idx < 0) {
    idx = sli_i3c_find_dev_index(ctx, device->static_addr);
  }
  return idx;
}

/***************************************************************************//**
 * Reject FIFO thresholds deeper than the FIFO itself; such a threshold would
 * never be reached and the transfer would stall.
 ******************************************************************************/
static sl_status_t i3c_validate_fifo_thlds(I3C_TypeDef *i3c,
                                           const sl_hal_i3c_fifo_thld_t *fifo)
{
  const sl_hal_i3c_data_thld_t thlds[] = {
    fifo->tx_empty_buf_thld, fifo->tx_start_thld,  // TX FIFO
    fifo->rx_buf_thld, fifo->rx_start_thld,        // RX FIFO
  };
  sl_hal_i3c_queue_size_t cap;

  if (sl_hal_i3c_get_queue_size_capability(i3c, &cap) != SL_STATUS_OK) {
    return SL_STATUS_FAIL;
  }
  for (uint32_t i = 0U; i < (sizeof(thlds) / sizeof(thlds[0])); i++) {
    // Encoding: THLD_1 = 1 entry, otherwise code n = 2^(n+1) entries.
    uint32_t entries = (thlds[i] == SL_HAL_I3C_THLD_1) ? 1U : (2UL << (uint32_t)thlds[i]);
    uint32_t depth = (i < 2U) ? cap.tx_buf_dwords : cap.rx_buf_dwords;
    if (entries > depth) {
      return SL_STATUS_INVALID_PARAMETER;
    }
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Start a TX payload: write as much as fits into the TX FIFO now and, if bytes
 * remain, enable the TX threshold interrupt so i3c_on_tx_threshold() keeps the
 * FIFO filled while the bus drains it (payloads larger than the FIFO).
 ******************************************************************************/
static void i3c_load_tx_fifo(sli_i3c_context_t *ctx, const uint8_t *buf, uint16_t len)
{
  ctx->tx_buf = buf;
  ctx->tx_len = len;
  ctx->tx_offset = (uint16_t)sli_i3c_write_tx_fifo(ctx->i3c, buf, len);
  i3c_set_irq_enabled(ctx->i3c, SLI_I3C_TX_THLD_INTR_MASK, ctx->tx_offset < ctx->tx_len);
}

/***************************************************************************//**
 * TX threshold interrupt: top up the TX FIFO from the pending payload and stop
 * the interrupt once the whole payload has been written.
 ******************************************************************************/
static void i3c_on_tx_threshold(sli_i3c_context_t *ctx)
{
  sl_hal_i3c_clear_interrupt_status(ctx->i3c, I3C_INTR_STATUS_TX_THLD_STS);
  if ((ctx->tx_buf != NULL) && (ctx->tx_offset < ctx->tx_len)) {
    uint32_t written = sli_i3c_write_tx_fifo(ctx->i3c,
                                             &ctx->tx_buf[ctx->tx_offset],
                                             (uint32_t)(ctx->tx_len - ctx->tx_offset));
    ctx->tx_offset = (uint16_t)(ctx->tx_offset + written);
  }
  if ((ctx->tx_buf == NULL) || (ctx->tx_offset >= ctx->tx_len)) {
    i3c_set_irq_enabled(ctx->i3c, SLI_I3C_TX_THLD_INTR_MASK, false);
  }
}

/***************************************************************************//**
 * Set the RX destination buffer. With a buffer, the RX threshold interrupt is
 * enabled so i3c_on_rx_threshold() empties the FIFO during the transfer
 * (payloads larger than the FIFO). With NULL / 0 the RX state is cleared.
 ******************************************************************************/
static void i3c_arm_rx_buffer(sli_i3c_context_t *ctx, uint8_t *buf, uint16_t len)
{
  ctx->rx_buf = buf;
  ctx->rx_len = len;
  ctx->rx_offset = 0U;
  i3c_set_irq_enabled(ctx->i3c, SLI_I3C_RX_THLD_INTR_MASK, (buf != NULL) && (len > 0U));
}

/***************************************************************************//**
 * RX threshold interrupt: copy the received words into the RX buffer. Bytes
 * left below the threshold are read when the response arrives.
 ******************************************************************************/
static void i3c_on_rx_threshold(sli_i3c_context_t *ctx)
{
  sl_hal_i3c_clear_interrupt_status(ctx->i3c, I3C_INTR_STATUS_RX_THLD_STS);
  if ((ctx->rx_buf != NULL) && (ctx->rx_offset < ctx->rx_len)) {
    uint32_t got = i3c_read_rx_fifo(ctx->i3c,
                                    &ctx->rx_buf[ctx->rx_offset],
                                    (uint32_t)(ctx->rx_len - ctx->rx_offset));
    ctx->rx_offset = (uint16_t)(ctx->rx_offset + got);
  }
  if ((ctx->rx_buf == NULL) || (ctx->rx_offset >= ctx->rx_len)) {
    i3c_set_irq_enabled(ctx->i3c, SLI_I3C_RX_THLD_INTR_MASK, false);
  }
}

/***************************************************************************//**
 * Copy up to @p len bytes from the RX FIFO, reading only words that are
 * present (RX_BUF_BLR > 0). Returns the number of bytes copied.
 ******************************************************************************/
static uint32_t i3c_read_rx_fifo(I3C_TypeDef *i3c, uint8_t *buf, uint32_t len)
{
  uint32_t offset = 0U;

  while (offset < len) {
    uint32_t level = 0U;
    uint32_t data = 0U;

    if (sl_hal_i3c_get_data_buffer_level(i3c, &level) != SL_STATUS_OK) {
      break;
    }
    if (((level & _I3C_DATA_BUFFER_STATUS_LEVEL_RX_BUF_BLR_MASK)
         >> _I3C_DATA_BUFFER_STATUS_LEVEL_RX_BUF_BLR_SHIFT) == 0U) {
      break;
    }
    (void)sl_hal_i3c_get_transfer_data(i3c, &data);
    // Unpack little-endian bytes; a partial last word carries padding.
    for (uint32_t b = 0U; (b < 4U) && (offset < len); b++, offset++) {
      buf[offset] = (uint8_t)(data >> (b * 8U));
    }
  }

  return offset;
}

/***************************************************************************//**
 * Drop up to @p len bytes from the RX FIFO (data with no destination buffer,
 * or left after an error) so it cannot block the next transfer.
 ******************************************************************************/
static void i3c_discard_rx_fifo(I3C_TypeDef *i3c, uint32_t len)
{
  uint32_t word_count = (len + 3U) / 4U;
  uint32_t data;

  for (uint32_t i = 0U; i < word_count; i++) {
    uint32_t level = 0U;

    if (sl_hal_i3c_get_data_buffer_level(i3c, &level) != SL_STATUS_OK) {
      break;
    }
    if (((level & _I3C_DATA_BUFFER_STATUS_LEVEL_RX_BUF_BLR_MASK)
         >> _I3C_DATA_BUFFER_STATUS_LEVEL_RX_BUF_BLR_SHIFT) == 0U) {
      break;
    }
    (void)sl_hal_i3c_get_transfer_data(i3c, &data);
  }
}

/***************************************************************************//**
 * Target: drop an armed virtual-target (extended command) TX.
 ******************************************************************************/
static void i3c_cancel_ext_tx(sli_i3c_context_t *ctx)
{
  if (ctx->await_ext_tx_done) {
    (void)sl_hal_i3c_reset_ext_tx_queue(ctx->i3c, ctx->ext_cmd_index);
  }
  ctx->await_ext_tx_done = false;
  ctx->expect_target_id_mask = 0U;
}

/***************************************************************************//**
 * Return the driver to idle after an error, timeout, or abort: stop FIFO
 * servicing, flush all queues and FIFOs, and leave the halt state.
 ******************************************************************************/
static void i3c_recover_after_error(sli_i3c_context_t *ctx)
{
  i3c_cancel_ext_tx(ctx);
  i3c_set_irq_enabled(ctx->i3c,
                      SLI_I3C_TX_THLD_INTR_MASK | SLI_I3C_RX_THLD_INTR_MASK,
                      false);
  (void)sl_hal_i3c_buffer_reset(ctx->i3c, SL_HAL_I3C_RESET_ALL_QUEUES);
  i3c_resume_if_halted(ctx->i3c);

  ctx->xfer_state = SLI_I3C_XFER_IDLE;
  ctx->rx_buf = NULL;
  ctx->rx_len = 0;
  ctx->rx_offset = 0;
  ctx->await_phys_tx_done = false;
  ctx->setnewda_pending = false;
}

/***************************************************************************//**
 * The controller halts after an error and the target halts after some CCCs;
 * both ignore new commands until RESUME. If the halt state cannot be read,
 * RESUME is requested anyway.
 ******************************************************************************/
static void i3c_resume_if_halted(I3C_TypeDef *i3c)
{
  bool is_controller = false;
  bool halted = false;

  if ((sl_hal_i3c_is_current_controller(i3c, &is_controller) != SL_STATUS_OK)
      || (sli_i3c_is_halted(i3c, is_controller, &halted) != SL_STATUS_OK)
      || halted) {
    (void)sl_hal_i3c_resume(i3c);
  }
}

/***************************************************************************//**
 * Drain the IBI queue. Each IBI is one status word followed by its payload
 * words; long payloads are split into several status+data segments ending
 * with LAST. Every loop is bounded by SLI_I3C_IBI_QUEUE_POP_MAX.
 ******************************************************************************/
static void i3c_read_ibi_queue(sli_i3c_context_t *ctx)
{
  I3C_TypeDef *i3c = ctx->i3c;

  for (uint32_t ibi_count = 0U; ibi_count < SLI_I3C_IBI_QUEUE_POP_MAX; ibi_count++) {
    uint8_t sts_cnt = 0U;
    uint8_t blr = 0U;
    uint32_t status_word = 0U;
    sl_i3c_ibi_info_t info;

    (void)sl_hal_i3c_get_ibi_queue_levels(i3c, &sts_cnt, &blr);
    if ((sts_cnt == 0U) && (blr == 0U)) {
      break;
    }
    if (sl_hal_i3c_controller_get_ibi_response(i3c, &status_word) != SL_STATUS_OK) {
      break;
    }

    memset(&info, 0, sizeof(info));
    // IBI ID = target address << 1 | RnW; Hot-Join has no target address.
    const uint8_t ibi_id = SLI_I3C_IBI_STS_ID(status_word);
    info.type = sl_hal_i3c_get_ibi_type(i3c, ibi_id);
    info.address = (info.type == SL_HAL_I3C_IBI_TYPE_HJ)
                   ? 0U : (uint8_t)((ibi_id >> 1) & _I3C_DEVICE_ADDR_STATIC_ADDR_MASK);
    info.error = (SLI_I3C_IBI_STS_ERROR(status_word) != 0U);
    info.rejected = (SLI_I3C_IBI_STS_REJECT(status_word) != 0U);

    if (info.rejected || info.error) {
      i3c_discard_ibi_queue(i3c);
      if (ctx->ibi_callback != NULL) {
        ctx->ibi_callback(ctx, &info);
      }
      break;
    }

    uint32_t segment_status = status_word;
    for (uint32_t segment = 0U; segment < SLI_I3C_IBI_QUEUE_POP_MAX; segment++) {
      uint8_t remaining = SLI_I3C_IBI_STS_DATA_LEN(segment_status);

      // Payload words follow the status word; keep at most SL_I3C_IBI_PAYLOAD_MAX bytes.
      while (remaining > 0U) {
        uint32_t data_word = 0U;
        uint8_t bytes_in_word = (remaining < 4U) ? remaining : 4U;

        (void)sl_hal_i3c_controller_get_ibi_response(i3c, &data_word);
        for (uint8_t b = 0U; b < bytes_in_word; b++) {
          if (info.len < SL_I3C_IBI_PAYLOAD_MAX) {
            info.data[info.len++] = (uint8_t)(data_word >> (b * 8U));
          }
        }
        remaining = (uint8_t)(remaining - bytes_in_word);
      }

      if (SLI_I3C_IBI_STS_LAST(segment_status) != 0U) {
        break;
      }
      // Not LAST: the next status word continues the same IBI.
      if (sl_hal_i3c_controller_get_ibi_response(i3c, &segment_status) != SL_STATUS_OK) {
        break;
      }
      if ((SLI_I3C_IBI_STS_ERROR(segment_status) != 0U)
          || (SLI_I3C_IBI_STS_REJECT(segment_status) != 0U)) {
        info.error = (SLI_I3C_IBI_STS_ERROR(segment_status) != 0U);
        info.rejected = (SLI_I3C_IBI_STS_REJECT(segment_status) != 0U);
        // Drop the rest of the failed IBI so the next one starts on a status word.
        i3c_discard_ibi_queue(i3c);
        break;
      }
    }

    if (ctx->ibi_callback != NULL) {
      ctx->ibi_callback(ctx, &info);
    }
  }
}

/***************************************************************************//**
 * Drop what is left in the IBI queue after a failed (error/rejected) IBI.
 * Bounded by SLI_I3C_IBI_QUEUE_POP_MAX.
 ******************************************************************************/
static void i3c_discard_ibi_queue(I3C_TypeDef *i3c)
{
  for (uint32_t pops = 0U; pops < SLI_I3C_IBI_QUEUE_POP_MAX; pops++) {
    uint8_t sts_cnt = 0U;
    uint8_t blr = 0U;
    uint32_t discard = 0U;

    (void)sl_hal_i3c_get_ibi_queue_levels(i3c, &sts_cnt, &blr);
    if ((sts_cnt == 0U) && (blr == 0U)) {
      break;
    }
    (void)sl_hal_i3c_controller_get_ibi_response(i3c, &discard);
  }
}

/***************************************************************************//**
 * Fill a default private-transfer command: next TID, response and STOP on
 * completion, FM for I2C devices, SDR0 for I3C devices.
 ******************************************************************************/
static void i3c_default_private_cmd(sli_i3c_context_t *ctx,
                                    sl_i3c_transfer_cmd_t *cmd,
                                    const sl_i3c_device_desc_t *device,
                                    bool is_read)
{
  memset(cmd, 0, sizeof(*cmd));
  // TID ties the response to its command; rotate it per transfer.
  cmd->tid = (uint8_t)(ctx->next_tid & SLI_I3C_USER_TID_MASK);
  ctx->next_tid = (uint8_t)((ctx->next_tid + 1U) & SLI_I3C_USER_TID_MASK);
  cmd->roc = 1;
  cmd->toc = 1;
  cmd->rnw = is_read ? 1U : 0U;
  if (device->is_i2c != 0U) {
    cmd->speed.i2c_speed = SL_I2C_SPEED_FM;
  } else {
    cmd->speed.i3c_speed = SL_I3C_SPEED_SDR0;
  }
}

/***************************************************************************//**
 * Controller: post a write as argument word + command word. Payloads of 1..3
 * bytes fit in the Short Data Argument (no FIFO). Longer payloads use a
 * Transfer Argument with the byte count, and the TX FIFO is loaded before the
 * command so data is ready when the controller starts the write.
 ******************************************************************************/
static sl_status_t i3c_post_write_command(sli_i3c_context_t *ctx,
                                          const uint8_t *buf,
                                          uint16_t len,
                                          sl_i3c_transfer_cmd_t *cmd,
                                          bool is_i2c)
{
  uint32_t arg;
  sl_status_t status;

  if ((len > 0U) && (len <= SLI_I3C_SHORT_DATA_MAX_LEN)) {
    cmd->sdap = 1;
    // BYTE_STRB has one bit per valid byte: 1 -> 0b001, 2 -> 0b011, 3 -> 0b111.
    arg = sli_i3c_form_short_data_argument((uint8_t)((1U << len) - 1U),
                                           buf[0],
                                           (len > 1U) ? buf[1] : 0U,
                                           (len > 2U) ? buf[2] : 0U);
  } else {
    cmd->sdap = 0;
    if (len > 0U) {
      i3c_load_tx_fifo(ctx, buf, len);
    }
    arg = sli_i3c_form_transfer_argument(len, 0U);
  }

  status = sl_hal_i3c_set_command_queue(ctx->i3c, arg);
  if (status != SL_STATUS_OK) {
    return status;
  }
  return sl_hal_i3c_set_command_queue(ctx->i3c,
                                      sli_i3c_form_transfer_command_from_struct(cmd, is_i2c));
}

/***************************************************************************//**
 * Controller: post a read as Transfer Argument (byte count, optional defining
 * byte) + command word. The data arrives in the RX FIFO.
 ******************************************************************************/
static sl_status_t i3c_post_read_command(I3C_TypeDef *i3c,
                                         uint16_t len,
                                         sl_i3c_transfer_cmd_t *cmd,
                                         bool is_i2c,
                                         uint8_t defining_byte)
{
  cmd->sdap = 0;
  cmd->rnw = 1;

  sl_status_t status = sl_hal_i3c_set_command_queue(
    i3c,
    sli_i3c_form_transfer_argument(len, cmd->dbp ? defining_byte : 0U));
  if (status != SL_STATUS_OK) {
    return status;
  }
  return sl_hal_i3c_set_command_queue(i3c,
                                      sli_i3c_form_transfer_command_from_struct(cmd, is_i2c));
}

/***************************************************************************//**
 * Controller: post a CCC as a regular transfer command (CP=1). Directed CCCs
 * (ID bit 7 set) are sent to the DAT slot of payload->ccc.addr; broadcast CCCs
 * ignore the slot. Lengths are validated by the caller (<= 65535).
 ******************************************************************************/
static sl_status_t i3c_post_ccc(sli_i3c_context_t *ctx,
                                const sl_i3c_ccc_payload_t *payload,
                                const sl_i3c_transfer_cmd_t *command,
                                uint8_t *rx_data,
                                size_t rx_len,
                                sli_i3c_transfer_callback_t callback)
{
  I3C_TypeDef *i3c = ctx->i3c;
  uint8_t ccc_id = (uint8_t)payload->ccc.id;
  uint8_t dev_index = command->dev_indx;
  sl_status_t status;

  if ((ccc_id & 0x80U) != 0U) {
    int8_t idx = sli_i3c_find_dev_index(ctx, payload->ccc.addr);
    if (idx < 0) {
      return SL_STATUS_NOT_FOUND;
    }
    dev_index = (uint8_t)idx;
  }

  ctx->transfer_callback = callback;
  i3c_arm_rx_buffer(ctx, rx_data, (uint16_t)rx_len);
  ctx->xfer_state = SLI_I3C_XFER_CCC_PENDING;

  sl_i3c_transfer_cmd_t cmd = *command;
  cmd.dev_indx = dev_index;
  cmd.cmd = ccc_id;
  cmd.cp = 1;
  cmd.rnw = payload->is_read ? 1U : 0U;

  if (payload->is_read) {
    status = i3c_post_read_command(i3c, (uint16_t)rx_len, &cmd, false,
                                   payload->defining_byte);
  } else if ((payload->len > 0U) && (payload->data != NULL)) {
    if (command->dbp) {
      // The defining byte occupies the Transfer Argument, so the data must
      // go through the TX FIFO (Short Data cannot be used).
      status = sl_hal_i3c_set_command_queue(
        i3c,
        sli_i3c_form_transfer_argument((uint16_t)payload->len, payload->defining_byte));
      if (status == SL_STATUS_OK) {
        i3c_load_tx_fifo(ctx, payload->data, (uint16_t)payload->len);
        cmd.sdap = 0;
        status = sl_hal_i3c_set_command_queue(
          i3c,
          sli_i3c_form_transfer_command_from_struct(&cmd, false));
      }
    } else {
      status = i3c_post_write_command(ctx, payload->data, (uint16_t)payload->len,
                                      &cmd, false);
    }
  } else if (command->dbp) {
    // No data but a defining byte: it still needs a Transfer Argument.
    status = sl_hal_i3c_set_command_queue(
      i3c,
      sli_i3c_form_transfer_argument(0U, payload->defining_byte));
    if (status == SL_STATUS_OK) {
      status = sl_hal_i3c_set_command_queue(
        i3c,
        sli_i3c_form_transfer_command(cmd.tid, ccc_id, dev_index,
                                      (uint8_t)cmd.speed.i3c_speed,
                                      false, cmd.roc, cmd.toc, true, true));
    }
  } else {
    // No data, no defining byte: the command word alone.
    status = sl_hal_i3c_set_command_queue(
      i3c,
      sli_i3c_form_transfer_command(cmd.tid, ccc_id, dev_index,
                                    (uint8_t)cmd.speed.i3c_speed,
                                    false, cmd.roc, cmd.toc, true, false));
  }

  if (status != SL_STATUS_OK) {
    ctx->xfer_state = SLI_I3C_XFER_IDLE;
    ctx->transfer_callback = NULL;
    i3c_arm_rx_buffer(ctx, NULL, 0U);
  }
  return status;
}

/***************************************************************************//**
 * Controller: post an Address Assignment command (CMD_ATTR=3) for ENTDAA or
 * SETDASA covering @p dev_count DAT slots from @p dat_index. Arguments are
 * validated by sl_i3c_start_address_assignment().
 ******************************************************************************/
static sl_status_t i3c_post_addr_assign(sli_i3c_context_t *ctx,
                                        uint8_t tid,
                                        uint8_t ccc_id,
                                        uint8_t dat_index,
                                        uint8_t dev_count,
                                        sli_i3c_transfer_callback_t callback)
{
  ctx->transfer_callback = callback;
  ctx->xfer_state = SLI_I3C_XFER_DAA_PENDING;

  sl_status_t status = sl_hal_i3c_set_command_queue(
    ctx->i3c,
    sli_i3c_form_address_assignment_command(tid, ccc_id, dat_index, dev_count, true, true));
  if (status != SL_STATUS_OK) {
    ctx->xfer_state = SLI_I3C_XFER_IDLE;
    ctx->transfer_callback = NULL;
  }
  return status;
}

/***************************************************************************//**
 * Controller: pop one RESPONSE_QUEUE entry, copy the read data still in the RX
 * FIFO into the RX buffer, and map ERR_STATUS to a transfer event.
 ******************************************************************************/
static sl_i3c_transfer_event_t i3c_read_controller_response(sli_i3c_context_t *ctx)
{
  I3C_TypeDef *i3c = ctx->i3c;
  uint32_t resp_word = 0;
  sli_i3c_response_entry_t resp;

  sl_hal_i3c_get_command_queue_response(i3c, &resp_word);
  sli_i3c_parse_response(resp_word, &resp);
  ctx->last_resp_error = resp.err_status;
  sl_i3c_transfer_event_t event = sli_i3c_resp_err_to_event(resp.err_status);

  // For address assignment DATA_LENGTH is the count of unassigned devices, not
  // RX bytes. The write half of a write-then-read has no RX data, and the read
  // that follows must not be consumed here.
  bool skip_rx = (ctx->xfer_state == SLI_I3C_XFER_DAA_PENDING)
                 || (ctx->xfer_state == SLI_I3C_XFER_WRITE_READ_PENDING);

  if (!skip_rx && (resp.data_length > 0U)) {
    i3c_set_irq_enabled(i3c, SLI_I3C_RX_THLD_INTR_MASK, false);
    if ((event == SL_I3C_EVENT_TRANSFER_COMPLETED)
        && (ctx->rx_buf != NULL) && (ctx->rx_len > 0U)) {
      // The RX threshold interrupt copied most of the data already; read the
      // rest, up to the buffer size, and drop anything beyond it.
      uint32_t want = (resp.data_length < ctx->rx_len) ? resp.data_length : ctx->rx_len;
      if (want > ctx->rx_offset) {
        uint32_t got = i3c_read_rx_fifo(i3c, ctx->rx_buf + ctx->rx_offset,
                                        want - ctx->rx_offset);
        ctx->rx_offset = (uint16_t)(ctx->rx_offset + got);
      }
      if (resp.data_length > want) {
        i3c_discard_rx_fifo(i3c, resp.data_length - want);
      }
    } else if (resp.data_length > ctx->rx_offset) {
      // Error, or no buffer: drop the data so the FIFO does not stall.
      i3c_discard_rx_fifo(i3c, resp.data_length - ctx->rx_offset);
    }
  }

  // SETNEWDA success: the target now answers at the new address; update the DAT.
  if (ctx->setnewda_pending) {
    if (event == SL_I3C_EVENT_TRANSFER_COMPLETED) {
      uint8_t idx = ctx->setnewda_dat_index;
      ctx->dev_table[idx].dynamic_addr = ctx->setnewda_new_addr;
      ctx->dat_cfg[idx].dynamic_address = ctx->setnewda_new_addr;
      (void)sl_hal_i3c_configure_target(i3c, idx, &ctx->dat_cfg[idx]);
    }
    ctx->setnewda_pending = false;
  }

  return event;
}

/***************************************************************************//**
 * Target: true when RESPONSE_QUEUE entries use the extended (Format-2) layout.
 * With virtual targets enabled the hardware posts Format-2 words for T0 too;
 * parsing them as regular responses reads DATA_LENGTH[7:4] as ERR_STATUS.
 ******************************************************************************/
static bool i3c_target_uses_ext_resp(const sli_i3c_context_t *ctx)
{
  return ctx->virt_tgt_enabled
         || ctx->await_ext_tx_done
         || (ctx->expect_target_id_mask != 0U);
}

/***************************************************************************//**
 * Transfer callback used by the blocking APIs: record the event and release
 * i3c_wait_blocking().
 ******************************************************************************/
static sl_status_t i3c_blocking_callback(sl_i3c_handle_t *i3c_handle,
                                         sl_i3c_transfer_event_t event)
{
  if (i3c_handle != NULL) {
    i3c_handle->blocking_event = event;
    i3c_handle->blocking_done = true;
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Wait for the transfer started by a blocking API. A timeout of 0 selects
 * SL_I3C_BLOCKING_TIMEOUT_DEFAULT_MS. On any failure the transfer is aborted
 * so the driver is idle again when this returns.
 ******************************************************************************/
static sl_status_t i3c_wait_blocking(sli_i3c_context_t *ctx, uint32_t timeout_ms)
{
  uint32_t timeout = (timeout_ms == 0U) ? SL_I3C_BLOCKING_TIMEOUT_DEFAULT_MS : timeout_ms;
  uint32_t start_ms = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());
  sl_status_t status;

  while (!ctx->blocking_done) {
    if ((sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count()) - start_ms) >= timeout) {
      break;
    }
    if ((ctx->role == SL_I3C_ROLE_SLAVE)
        && (ctx->xfer_state == SLI_I3C_XFER_TARGET_RX_PENDING)) {
      uint8_t ext_vld_stat = 0U;
      // A target halted by a CCC NACKs the controller's write; keep resuming.
      i3c_resume_if_halted(ctx->i3c);
      // Reading EXT_CMD_VLD_STAT consumes it (read-to-clear).
      (void)sl_hal_i3c_get_ext_cmd_valid_status(ctx->i3c, &ext_vld_stat);
    }
  }

  if (!ctx->blocking_done) {
    status = SL_STATUS_TIMEOUT;
  } else if (ctx->blocking_event == SL_I3C_EVENT_TRANSFER_COMPLETED) {
    status = SL_STATUS_OK;
  } else if (ctx->blocking_event == SL_I3C_EVENT_TRANSFER_ABORTED) {
    status = SL_STATUS_ABORT;
  } else if (ctx->last_resp_error != SLI_I3C_RESP_ERR_SUCCESS) {
    status = sli_i3c_resp_err_to_status(ctx->last_resp_error);
  } else {
    status = SL_STATUS_FAIL;
  }

  if (status != SL_STATUS_OK) {
    (void)sl_i3c_abort_transfer(ctx);
  }
  ctx->transfer_callback = NULL;
  return status;
}

/***************************************************************************//**
 * Finish the active transfer: go idle, then call its one-shot callback.
 ******************************************************************************/
static void i3c_complete_transfer(sli_i3c_context_t *ctx,
                                  sl_i3c_transfer_event_t event)
{
  sli_i3c_transfer_callback_t cb = ctx->transfer_callback;

  // Clear before calling so the callback may start a new transfer.
  ctx->xfer_state = SLI_I3C_XFER_IDLE;
  ctx->transfer_callback = NULL;
  if (cb != NULL) {
    (void)cb(ctx, event);
  }
}

/***************************************************************************//**
 * Report a target-mode event to the application callback, if registered.
 ******************************************************************************/
static void i3c_notify_target(sli_i3c_context_t *ctx,
                              sl_i3c_target_event_t event)
{
  if (ctx->target_callback != NULL) {
    ctx->target_callback(ctx, event);
  }
}

#if defined(I3C0)
/***************************************************************************//**
 * I3C0 NVIC vector: dispatch to the handle registered for instance 0.
 ******************************************************************************/
void I3C0_IRQHandler(void)
{
  sl_i3c_irq_handler(i3c_handle_map[0]);
}
#endif
