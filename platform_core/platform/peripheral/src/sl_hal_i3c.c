/***************************************************************************//**
 * @file
 * @brief I3C - Improved Inter-Integrated Circuit peripheral API
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
 #include "sl_hal_i3c.h"
 #if defined(I3C_PRESENT)

 #include <stddef.h>

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/
static uint8_t i3c_calculate_parity_for_address(uint8_t addr_7bit);

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

extern __INLINE sl_status_t sl_hal_i3c_enable(I3C_TypeDef *i3c);

extern __INLINE sl_status_t sl_hal_i3c_disable(I3C_TypeDef *i3c);

extern __INLINE sl_status_t sl_hal_i3c_enable_adaptive_mode(I3C_TypeDef *i3c);

extern __INLINE sl_status_t sl_hal_i3c_enable_dma(I3C_TypeDef *i3c);

extern __INLINE sl_status_t sl_hal_i3c_disable_dma(I3C_TypeDef *i3c);

extern __INLINE sl_status_t sl_hal_i3c_is_dma_enabled(I3C_TypeDef *i3c,
                                                      bool *enabled);

extern __INLINE sl_status_t sl_hal_i3c_controller_configure_hot_join(I3C_TypeDef *i3c,
                                                                     bool enable);

extern __INLINE sl_status_t sl_hal_i3c_set_command_queue(I3C_TypeDef *i3c,
                                                         uint32_t command_value);

extern __INLINE sl_status_t sl_hal_i3c_get_command_queue_response(I3C_TypeDef *i3c,
                                                                  uint32_t *response);

extern __INLINE sl_status_t sl_hal_i3c_controller_get_ibi_response(I3C_TypeDef *i3c,
                                                                   uint32_t *ibi_resp);

extern __INLINE sl_status_t sl_hal_i3c_get_queue_threshold(I3C_TypeDef *i3c,
                                                           uint32_t *reg_val);

extern __INLINE sl_status_t sl_hal_i3c_get_fifo_threshold(I3C_TypeDef *i3c,
                                                          uint32_t *reg_val);

extern __INLINE sl_status_t sl_hal_i3c_ibi_reject_notify(I3C_TypeDef *i3c,
                                                         uint8_t notify_flags);

extern __INLINE sl_status_t sl_hal_i3c_soft_reset(I3C_TypeDef *i3c);

extern __INLINE sl_status_t sl_hal_i3c_set_reset_control(I3C_TypeDef *i3c,
                                                         uint32_t reset_mask);

extern __INLINE sl_status_t sl_hal_i3c_get_reset_control(I3C_TypeDef *i3c,
                                                         uint32_t *reg_val);

extern __INLINE sl_status_t sl_hal_i3c_target_set_hot_join_ibi(I3C_TypeDef *i3c,
                                                               bool enable);

extern __INLINE sl_status_t sl_hal_i3c_clear_interrupt_status(I3C_TypeDef *i3c,
                                                              uint32_t interrupt_mask);

extern __INLINE sl_status_t sl_hal_i3c_get_pending_interrupts(I3C_TypeDef *i3c,
                                                              uint32_t *pending_intr);

extern __INLINE sl_status_t sl_hal_i3c_enable_interrupt(I3C_TypeDef *i3c,
                                                        uint32_t interrupt_mask);

extern __INLINE sl_status_t sl_hal_i3c_disable_interrupt(I3C_TypeDef *i3c,
                                                         uint32_t interrupt_mask);

extern __INLINE sl_status_t sl_hal_i3c_get_enabled_interrupts(I3C_TypeDef *i3c,
                                                              uint32_t *enabled_intr);

extern __INLINE sl_status_t sl_hal_i3c_enable_interrupt_signal(I3C_TypeDef *i3c,
                                                               uint32_t interrupt_mask);

extern __INLINE sl_status_t sl_hal_i3c_disable_interrupt_signal(I3C_TypeDef *i3c,
                                                                uint32_t interrupt_mask);

extern __INLINE sl_status_t sl_hal_i3c_get_enabled_interrupt_signals(I3C_TypeDef *i3c,
                                                                     uint32_t *enabled_intr);

extern __INLINE sl_status_t sl_hal_i3c_get_queue_status_level(I3C_TypeDef *i3c,
                                                              uint32_t *queue_status_level);

extern __INLINE sl_status_t sl_hal_i3c_get_resp_buf_level(I3C_TypeDef *i3c,
                                                          uint8_t *resp_blr);

extern __INLINE sl_status_t sl_hal_i3c_get_ibi_queue_levels(I3C_TypeDef *i3c,
                                                            uint8_t *sts_cnt,
                                                            uint8_t *data_blr);

extern __INLINE sl_status_t sl_hal_i3c_get_data_buffer_level(I3C_TypeDef *i3c,
                                                             uint32_t *data_buffer_level);

extern __INLINE sl_status_t sl_hal_i3c_is_controller_idle(I3C_TypeDef *i3c,
                                                          bool *is_idle);

extern __INLINE sl_status_t sl_hal_i3c_is_current_controller(I3C_TypeDef *i3c,
                                                             bool *is_controller);

extern __INLINE sl_status_t sl_hal_i3c_get_ccc_device_status(I3C_TypeDef *i3c,
                                                             uint32_t *ccc_device_status);

extern __INLINE sl_status_t sl_hal_i3c_get_device_addr_register(I3C_TypeDef *i3c,
                                                                uint8_t target_index,
                                                                uint32_t **register_ptr);

extern __INLINE sl_status_t sl_hal_i3c_set_slave_pid(I3C_TypeDef *i3c,
                                                     uint32_t reg_value);

extern __INLINE sl_status_t sl_hal_i3c_trigger_master_request(I3C_TypeDef *i3c);

extern __INLINE sl_status_t sl_hal_i3c_trigger_slave_interrupt_request(I3C_TypeDef *i3c);

extern __INLINE sl_status_t sl_hal_i3c_set_target_sir_data(I3C_TypeDef *i3c,
                                                           uint32_t data);

extern __INLINE sl_status_t sl_hal_i3c_slave_get_ibi_response(I3C_TypeDef *i3c,
                                                              uint32_t *ibi_resp);

extern __INLINE sl_status_t sl_hal_i3c_get_status_register(I3C_TypeDef *i3c,
                                                           uint32_t *status_reg);

extern __INLINE sl_status_t sl_hal_i3c_enable_i2c_mode(I3C_TypeDef *i3c);

extern __INLINE sl_status_t sl_hal_i3c_disable_i2c_mode(I3C_TypeDef *i3c);

extern __INLINE sl_status_t sl_hal_i3c_enable_peripheral_clock(I3C_TypeDef *i3c);

extern __INLINE sl_status_t sl_hal_i3c_disable_peripheral_clock(I3C_TypeDef *i3c);

/***************************************************************************//**
 * Calculate odd parity for a 7-bit dynamic address.
 ******************************************************************************/
static uint8_t i3c_calculate_parity_for_address(uint8_t addr_7bit)
{
  uint8_t bit_count = 0U;
  uint8_t temp = addr_7bit & SL_HAL_I3C_MAX_7BIT_ADDRESS;

  for (uint8_t i = 0U; i < 7U; i++) {
    if ((temp & 0x01U) != 0U) {
      bit_count++;
    }
    temp >>= 1;
  }

  uint8_t parity_bit = (bit_count % 2U == 0U) ? 1U : 0U;

  return (uint8_t)((parity_bit << 7) | (addr_7bit & SL_HAL_I3C_MAX_7BIT_ADDRESS));
}
/***************************************************************************//**
 * Sets the I3C peripheral to operate in controller mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_controller_mode(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Clear and set device operation mode to Controller.
  i3c->DEVICE_CTRL_EXTENDED &= ~_I3C_DEVICE_CTRL_EXTENDED_DEV_OPERATION_MODE_MASK;
  i3c->DEVICE_CTRL_EXTENDED |= (0x0 << _I3C_DEVICE_CTRL_EXTENDED_DEV_OPERATION_MODE_SHIFT);

  SL_PRINT_STRING_DEBUG("i3c controller mode set\r\n");

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Sets the I3C peripheral to operate in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_target_mode(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Clear and set device operation mode to Target.
  i3c->DEVICE_CTRL_EXTENDED &= ~_I3C_DEVICE_CTRL_EXTENDED_DEV_OPERATION_MODE_MASK;
  i3c->DEVICE_CTRL_EXTENDED |= (0x1 << _I3C_DEVICE_CTRL_EXTENDED_DEV_OPERATION_MODE_SHIFT);

  SL_PRINT_STRING_DEBUG("i3c target mode set\r\n");

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get the capabilities of the I3C peripheral.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_hw_capability(I3C_TypeDef *i3c,
                                         sl_hal_i3c_ip_capability_t *capability)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (capability == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  uint32_t reg = i3c->HW_CAPABILITY;
  capability->raw                          = reg;
  capability->device_role_config           = (uint8_t)((reg & _I3C_HW_CAPABILITY_DEVICE_ROLE_CONFIG_MASK) >> _I3C_HW_CAPABILITY_DEVICE_ROLE_CONFIG_SHIFT);
  capability->hdr_ddr_supported            = ((reg & I3C_HW_CAPABILITY_HDR_DDR_EN) != 0U);
  capability->hdr_ts_supported             = ((reg & I3C_HW_CAPABILITY_HDR_TS_EN) != 0U);
  capability->clock_period                 = (uint8_t)((reg & _I3C_HW_CAPABILITY_CLOCK_PERIOD_MASK) >> _I3C_HW_CAPABILITY_CLOCK_PERIOD_SHIFT);
  capability->hdr_tx_clock_period          = (uint8_t)((reg & _I3C_HW_CAPABILITY_HDR_TX_CLOCK_PERIOD_MASK) >> _I3C_HW_CAPABILITY_HDR_TX_CLOCK_PERIOD_SHIFT);
  capability->dma_supported                = ((reg & I3C_HW_CAPABILITY_DMA_EN) != 0U);
  capability->slave_hot_join_supported     = ((reg & I3C_HW_CAPABILITY_SLV_HJ_CAP) != 0U);
  capability->slave_ibi_supported          = ((reg & I3C_HW_CAPABILITY_SLV_IBI_CAP) != 0U);
  capability->target_group_addr_supported  = ((reg & I3C_HW_CAPABILITY_TGT_GRP_ADDR_CAP) != 0U);
  capability->virtual_target_supported     = ((reg & I3C_HW_CAPABILITY_TGT_VIRTUAL_TGT_CAP) != 0U);
  capability->d2d_tunneling_supported      = ((reg & I3C_HW_CAPABILITY_D2D_TUNNELING_CAP) != 0U);
  capability->hdr_bt_supported             = ((reg & I3C_HW_CAPABILITY_HDR_BT_EN) != 0U);

  return SL_STATUS_OK;
}
/***************************************************************************//**
 * Configures whether I2C slaves are present on the bus.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_i2c_slave_present(I3C_TypeDef *i3c,
                                             bool present)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  if (present) {
    i3c->DEVICE_CTRL |= I3C_DEVICE_CTRL_I2C_SLAVE_PRESENT;
  } else {
    i3c->DEVICE_CTRL &= ~I3C_DEVICE_CTRL_I2C_SLAVE_PRESENT;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Abort an ongoing I3C bus transfer in controller mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_abort_controller_transfer(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Set the ABORT bit.
  i3c->DEVICE_CTRL |= I3C_DEVICE_CTRL_ABORT;

  // Wait for the ABORT bit to auto-clear.
  while (i3c->DEVICE_CTRL & I3C_DEVICE_CTRL_ABORT) {
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Request resume after halt of controller.
 *
 * @note controller mode clears RESUME when the next command is initiated,
 * target mode clears when ready after error handling.
 ******************************************************************************/
sl_status_t sl_hal_i3c_resume(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Request resume; hardware clears the bit when it leaves halt.
  i3c->DEVICE_CTRL |= I3C_DEVICE_CTRL_RESUME;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Sets the dynamic address for the I3C device.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_dynamic_address(I3C_TypeDef *i3c,
                                           uint8_t dynamic_address)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (dynamic_address > SL_HAL_I3C_MAX_7BIT_ADDRESS) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Clear the current Dynamic Address and set the new one.
  i3c->DEVICE_ADDR &= ~(_I3C_DEVICE_ADDR_DYNAMIC_ADDR_MASK | I3C_DEVICE_ADDR_DYNAMIC_ADDR_VALID);
  i3c->DEVICE_ADDR |= (dynamic_address << _I3C_DEVICE_ADDR_DYNAMIC_ADDR_SHIFT) & _I3C_DEVICE_ADDR_DYNAMIC_ADDR_MASK;

  // Mark the Dynamic Address as valid.
  i3c->DEVICE_ADDR |= I3C_DEVICE_ADDR_DYNAMIC_ADDR_VALID;

  SL_PRINT_STRING_DEBUG("i3c dynamic addr set\r\n");

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Gets the current dynamic address of the I3C device.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_dynamic_address(I3C_TypeDef *i3c,
                                           uint8_t *dynamic_address)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (dynamic_address == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Check if the dynamic address is valid.
  if (!(i3c->DEVICE_ADDR & I3C_DEVICE_ADDR_DYNAMIC_ADDR_VALID)) {
    SL_PRINT_STRING_WARN("i3c dynamic addr not valid\r\n");
    return SL_STATUS_FAIL;
  }

  // Read the dynamic address from the DEVICE_ADDR register.
  *dynamic_address = (i3c->DEVICE_ADDR & _I3C_DEVICE_ADDR_DYNAMIC_ADDR_MASK) >> _I3C_DEVICE_ADDR_DYNAMIC_ADDR_SHIFT;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Sets the static address for the I3C device and marks it as valid.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_static_address(I3C_TypeDef *i3c,
                                          uint8_t static_address)
{
  // Validate parameters
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (static_address > SL_HAL_I3C_MAX_7BIT_ADDRESS) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Clear the current Static Address and set the new one.
  i3c->DEVICE_ADDR &= ~(_I3C_DEVICE_ADDR_STATIC_ADDR_MASK | I3C_DEVICE_ADDR_STATIC_ADDR_VALID);
  i3c->DEVICE_ADDR |= (static_address & _I3C_DEVICE_ADDR_STATIC_ADDR_MASK);

  // Mark the Static Address as valid.
  i3c->DEVICE_ADDR |= I3C_DEVICE_ADDR_STATIC_ADDR_VALID;

  SL_PRINT_STRING_DEBUG("i3c static addr set\r\n");

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Gets the current static address of the I3C device.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_static_address(I3C_TypeDef *i3c,
                                          uint8_t *static_address)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (static_address == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Check if the Static Address is valid.
  if (!(i3c->DEVICE_ADDR & I3C_DEVICE_ADDR_STATIC_ADDR_VALID)) {
    SL_PRINT_STRING_WARN("i3c static addr not valid\r\n");
    return SL_STATUS_FAIL;
  }

  *static_address = (i3c->DEVICE_ADDR & _I3C_DEVICE_ADDR_STATIC_ADDR_MASK) >> _I3C_DEVICE_ADDR_STATIC_ADDR_SHIFT;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Writes data to the I3C transfer data port.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_transfer_data(I3C_TypeDef *i3c,
                                         uint32_t data)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Write data to the XFER_DATA_PORT register.
  i3c->XFER_DATA_PORT = data;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Reads data from the I3C transfer data port.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_transfer_data(I3C_TypeDef *i3c,
                                         uint32_t *data)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (data == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Read data from the XFER_DATA_PORT register.
  *data = i3c->XFER_DATA_PORT;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get IBI type from IBI ID.
 ******************************************************************************/
sl_hal_i3c_ibi_type_t sl_hal_i3c_get_ibi_type(I3C_TypeDef *i3c,
                                              uint8_t ibi_id)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_HAL_I3C_IBI_TYPE_UNKNOWN;
  }

  // Hot-Join uses reserved 7-bit address 0x02 (IBI_ID byte 0x04 with RnW = 0).
  if (((ibi_id >> 1) == 0x02U) && ((ibi_id & 0x01U) == 0U)) {
    return SL_HAL_I3C_IBI_TYPE_HJ;
  }

  // Classify based on R/W bit (bit 0).
  // R/W = 1 indicates SIR (Slave Interrupt Request).
  // R/W = 0 indicates MR (Master Request).
  if ((ibi_id & 0x01) == 1) {
    return SL_HAL_I3C_IBI_TYPE_SIR;
  } else {
    return SL_HAL_I3C_IBI_TYPE_MR;
  }
}

/***************************************************************************//**
 * Set queue thresholds for command empty buffer, response buffer,
 * IBI data buffer, and IBI status buffer.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_queue_threshold(I3C_TypeDef *i3c,
                                           const sl_hal_i3c_queue_thld_t *thld)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (thld == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  //The minimum supported segment size is 1 (4 bytes).
  if (thld->ibi_data_thld < 1) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Construct and write the complete queue threshold register value.
  i3c->QUEUE_THLD_CTRL = ((thld->cmd_empty_buf_thld << _I3C_QUEUE_THLD_CTRL_CMD_EMPTY_BUF_THLD_SHIFT)
                          & _I3C_QUEUE_THLD_CTRL_CMD_EMPTY_BUF_THLD_MASK)
                         | ((thld->resp_buf_thld << _I3C_QUEUE_THLD_CTRL_RESP_BUF_THLD_SHIFT)
                            & _I3C_QUEUE_THLD_CTRL_RESP_BUF_THLD_MASK)
                         | ((thld->ibi_data_thld << _I3C_QUEUE_THLD_CTRL_IBI_DATA_THLD_SHIFT)
                            & _I3C_QUEUE_THLD_CTRL_IBI_DATA_THLD_MASK)
                         | ((thld->ibi_status_thld << _I3C_QUEUE_THLD_CTRL_IBI_STATUS_THLD_SHIFT)
                            & _I3C_QUEUE_THLD_CTRL_IBI_STATUS_THLD_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set FIFO thresholds for transmit and receive buffers.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_fifo_threshold(I3C_TypeDef *i3c,
                                          const sl_hal_i3c_fifo_thld_t *thld)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (thld == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (!SL_HAL_I3C_IS_DATA_THLD_VALID(thld->tx_empty_buf_thld)
      || !SL_HAL_I3C_IS_DATA_THLD_VALID(thld->rx_buf_thld)
      || !SL_HAL_I3C_IS_DATA_THLD_VALID(thld->tx_start_thld)
      || !SL_HAL_I3C_IS_DATA_THLD_VALID(thld->rx_start_thld)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  // Set the FIFO thresholds in the DATA_BUFFER_THLD_CTRL register.
  i3c->DATA_BUFFER_THLD_CTRL = ((thld->tx_empty_buf_thld << _I3C_DATA_BUFFER_THLD_CTRL_TX_EMPTY_BUF_THLD_SHIFT)
                                & _I3C_DATA_BUFFER_THLD_CTRL_TX_EMPTY_BUF_THLD_MASK)
                               | ((thld->rx_buf_thld << _I3C_DATA_BUFFER_THLD_CTRL_RX_BUF_THLD_SHIFT)
                                  & _I3C_DATA_BUFFER_THLD_CTRL_RX_BUF_THLD_MASK)
                               | ((thld->tx_start_thld << _I3C_DATA_BUFFER_THLD_CTRL_TX_START_THLD_SHIFT)
                                  & _I3C_DATA_BUFFER_THLD_CTRL_TX_START_THLD_MASK)
                               | ((thld->rx_start_thld << _I3C_DATA_BUFFER_THLD_CTRL_RX_START_THLD_SHIFT)
                                  & _I3C_DATA_BUFFER_THLD_CTRL_RX_START_THLD_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Read QUEUE_SIZE_CAPABILITY and decode dword depths.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_queue_size_capability(I3C_TypeDef *i3c,
                                                 sl_hal_i3c_queue_size_t *size)
{
  uint32_t reg;

  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (size == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  reg = i3c->QUEUE_SIZE_CAPABILITY;
  size->tx_buf_dwords = ((reg & _I3C_QUEUE_SIZE_CAPABILITY_TX_BUF_SIZE_MASK)
                         >> _I3C_QUEUE_SIZE_CAPABILITY_TX_BUF_SIZE_SHIFT);
  size->rx_buf_dwords = ((reg & _I3C_QUEUE_SIZE_CAPABILITY_RX_BUF_SIZE_MASK)
                         >> _I3C_QUEUE_SIZE_CAPABILITY_RX_BUF_SIZE_SHIFT);
  size->cmd_buf_dwords = ((reg & _I3C_QUEUE_SIZE_CAPABILITY_CMD_BUF_SIZE_MASK)
                          >> _I3C_QUEUE_SIZE_CAPABILITY_CMD_BUF_SIZE_SHIFT);
  size->resp_buf_dwords = ((reg & _I3C_QUEUE_SIZE_CAPABILITY_RESP_BUF_SIZE_MASK)
                           >> _I3C_QUEUE_SIZE_CAPABILITY_RESP_BUF_SIZE_SHIFT);
  size->ibi_buf_dwords = ((reg & _I3C_QUEUE_SIZE_CAPABILITY_IBI_BUF_SIZE_MASK)
                          >> _I3C_QUEUE_SIZE_CAPABILITY_IBI_BUF_SIZE_SHIFT);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Program target command/response thresholds.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_target_queue_threshold(I3C_TypeDef *i3c,
                                                  const sl_hal_i3c_target_queue_thld_t *thld)
{
  uint32_t reg;

  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (thld == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  reg = i3c->QUEUE_THLD_CTRL;
  reg &= ~(_I3C_QUEUE_THLD_CTRL_CMD_EMPTY_BUF_THLD_MASK
           | _I3C_QUEUE_THLD_CTRL_RESP_BUF_THLD_MASK);
  reg |= ((uint32_t)thld->cmd_empty_buf_thld << _I3C_QUEUE_THLD_CTRL_CMD_EMPTY_BUF_THLD_SHIFT)
         & _I3C_QUEUE_THLD_CTRL_CMD_EMPTY_BUF_THLD_MASK;
  reg |= ((uint32_t)thld->resp_buf_thld << _I3C_QUEUE_THLD_CTRL_RESP_BUF_THLD_SHIFT)
         & _I3C_QUEUE_THLD_CTRL_RESP_BUF_THLD_MASK;
  i3c->QUEUE_THLD_CTRL = reg;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Gets the current status of the slave event status register.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_slave_event_status(I3C_TypeDef *i3c,
                                              uint32_t *reg_val)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (reg_val == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Read the SLV_EVENT_STATUS register.
  *reg_val = i3c->SLV_EVENT_STATUS;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get current master transfer state.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_controller_transfer_status(I3C_TypeDef *i3c,
                                                      uint32_t *reg_val)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (reg_val == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Read the transfer state from PRESENT_STATE register.
  *reg_val = (i3c->PRESENT_STATE & _I3C_PRESENT_STATE_CM_TFR_ST_STS_MASK)
             >> _I3C_PRESENT_STATE_CM_TFR_ST_STS_SHIFT;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get current transfer type of the I3C device.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_transfer_type(I3C_TypeDef *i3c,
                                         uint32_t *transfer_type)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (transfer_type == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Read the transfer type from PRESENT_STATE register.
  *transfer_type = (i3c->PRESENT_STATE & _I3C_PRESENT_STATE_CM_TFR_STS_MASK)
                   >> _I3C_PRESENT_STATE_CM_TFR_STS_SHIFT;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Provision a complete Device Address Table (DAT) entry with one write.
 ******************************************************************************/
sl_status_t sl_hal_i3c_configure_target(I3C_TypeDef *i3c,
                                        uint8_t target_index,
                                        const sl_hal_i3c_target_dat_config_t *cfg)
{
  sl_status_t status;
  uint32_t *device_addr_reg = NULL;
  uint32_t reg_value;

  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  if (cfg == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Validate address fields fit in 7 bits.
  if (cfg->static_address > SL_HAL_I3C_MAX_7BIT_ADDRESS) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (cfg->dynamic_address > SL_HAL_I3C_MAX_7BIT_ADDRESS) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Resolve the target's DAT register (also validates target_index).
  status = sl_hal_i3c_get_device_addr_register(i3c, target_index, &device_addr_reg);
  if (status != SL_STATUS_OK) {
    SL_PRINT_STRING_ERROR("i3c invalid target index\r\n");
    return status;
  }

  // clear the fields owned by this configuration.
  reg_value = *device_addr_reg
              & ~(_I3C_DEV_ADDR_TABLE1_LOC1_STATIC_ADDRESS_MASK
                  | _I3C_DEV_ADDR_TABLE1_LOC1_DEV_DYNAMIC_ADDR_MASK
                  | I3C_DEV_ADDR_TABLE1_LOC1_IBI_WITH_DATA
                  | I3C_DEV_ADDR_TABLE1_LOC1_SIR_REJECT
                  | I3C_DEV_ADDR_TABLE1_LOC1_MR_REJECT
                  | I3C_DEV_ADDR_TABLE1_LOC1_DEVICE);

  // set the static address.
  reg_value |= (cfg->static_address & _I3C_DEV_ADDR_TABLE1_LOC1_STATIC_ADDRESS_MASK);

  // set the dynamic address with odd parity in the 8-bit DAT field.
  if (cfg->dynamic_address != 0U) {
    uint8_t address_with_parity = i3c_calculate_parity_for_address(cfg->dynamic_address);

    reg_value |= ((uint32_t)address_with_parity << _I3C_DEV_ADDR_TABLE1_LOC1_DEV_DYNAMIC_ADDR_SHIFT)
                 & _I3C_DEV_ADDR_TABLE1_LOC1_DEV_DYNAMIC_ADDR_MASK;
  }

  // set the device type.
  if (cfg->is_i2c_device) {
    reg_value |= I3C_DEV_ADDR_TABLE1_LOC1_DEVICE;
  }

  // IBI with data in the Device Address Table
  if (cfg->ibi_with_data) {
    reg_value |= I3C_DEV_ADDR_TABLE1_LOC1_IBI_WITH_DATA;
  }

  // configure SIR and MR bits in the Device Address Table
  if (!cfg->sir_enable) {
    reg_value |= I3C_DEV_ADDR_TABLE1_LOC1_SIR_REJECT;
  }
  if (!cfg->mr_enable) {
    reg_value |= I3C_DEV_ADDR_TABLE1_LOC1_MR_REJECT;
  }

  *device_addr_reg = reg_value;

  SL_PRINT_STRING_DEBUG("i3c configure target\r\n");

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get DEV_CHAR_TABLE base register address.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_dev_char_table_base(I3C_TypeDef *i3c,
                                               const uint32_t **base_addr)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Check for null pointer.
  if (base_addr == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Return the base register address of the first DEV_CHAR_TABLE1_LOC1 register.
  // Cast volatile pointer to const pointer to match the function signature.
  *base_addr = (const uint32_t *)&i3c->DEV_CHAR_TABLE1_LOC1;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Sets the MIPI Manufacturer ID and the Provisional ID Type in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_slave_mipi_id(I3C_TypeDef *i3c,
                                         uint16_t mfg_id,
                                         bool prov_id_sel)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  // Validate mfg_id fits in 15 bits (bits 15:1 in register).
  if (mfg_id > (_I3C_SLV_MIPI_ID_VALUE_SLV_MIPI_MFG_ID_MASK
                >> _I3C_SLV_MIPI_ID_VALUE_SLV_MIPI_MFG_ID_SHIFT)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Construct the register value:
  // bits [15:1] = SLV_MIPI_MFG_ID (15 bits)
  // bit  [0]    = SLV_PROV_ID_SEL (0 = Vendor Fixed, 1 = Random)
  i3c->SLV_MIPI_ID_VALUE = ((uint32_t)mfg_id << _I3C_SLV_MIPI_ID_VALUE_SLV_MIPI_MFG_ID_SHIFT)
                           | (prov_id_sel ? I3C_SLV_MIPI_ID_VALUE_SLV_PROV_ID_SEL : 0);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set slave characteristics of I3C device in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_slave_characteristics(I3C_TypeDef *i3c,
                                                 const sl_hal_i3c_slave_char_config_t *cfg)
{
  uint32_t mask;
  uint32_t value;
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (cfg == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (!SL_HAL_I3C_IS_DEVICE_ROLE_VALID(cfg->device_role)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  mask = (_I3C_SLV_CHAR_CTRL_DCR_MASK
          | _I3C_SLV_CHAR_CTRL_DEVICE_ROLE_MASK
          | _I3C_SLV_CHAR_CTRL_ADVANCED_CAPABILITIES_MASK
          | _I3C_SLV_CHAR_CTRL_MAX_DATA_SPEED_LIMIT_MASK);

  value = ((uint32_t)cfg->dcr << _I3C_SLV_CHAR_CTRL_DCR_SHIFT)
          | (((uint32_t)cfg->device_role << _I3C_SLV_CHAR_CTRL_DEVICE_ROLE_SHIFT)
             & _I3C_SLV_CHAR_CTRL_DEVICE_ROLE_MASK)
          | ((cfg->hdr_capable ? 1UL : 0UL) << _I3C_SLV_CHAR_CTRL_ADVANCED_CAPABILITIES_SHIFT)
          | ((cfg->max_data_speed_limit ? 1UL : 0UL) << _I3C_SLV_CHAR_CTRL_MAX_DATA_SPEED_LIMIT_SHIFT);

  i3c->SLV_CHAR_CTRL = (i3c->SLV_CHAR_CTRL & ~mask) | value;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get slave characteristics register.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_slave_characteristics(I3C_TypeDef *i3c,
                                                 uint32_t *value)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (value == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Read the entire SLV_CHAR_CTRL register value.
  *value = i3c->SLV_CHAR_CTRL;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get the device role from the Slave Characteristics register.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_device_role(I3C_TypeDef *i3c,
                                       sl_hal_i3c_device_role_t *role)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (role == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *role = (sl_hal_i3c_device_role_t)((i3c->SLV_CHAR_CTRL
                                      & _I3C_SLV_CHAR_CTRL_DEVICE_ROLE_MASK)
                                     >> _I3C_SLV_CHAR_CTRL_DEVICE_ROLE_SHIFT);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set clock to data turnaround time for I3C device in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_clock_data_turnaround(I3C_TypeDef *i3c,
                                                 sl_hal_i3c_clock_data_turnaround_t turn_time)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Validate turnaround time (3-bit field: 0-7).
  if (!SL_HAL_I3C_IS_CLK_DATA_TURN_VALID(turn_time)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Clear MXDS_CLK_DATA_TURN field.
  i3c->MAX_DATA_SPEED &= ~_I3C_MAX_DATA_SPEED_MXDS_CLK_DATA_TURN_MASK;
  // Set MXDS_CLK_DATA_TURN field with new value.
  i3c->MAX_DATA_SPEED |= ((turn_time << _I3C_MAX_DATA_SPEED_MXDS_CLK_DATA_TURN_SHIFT)
                          & _I3C_MAX_DATA_SPEED_MXDS_CLK_DATA_TURN_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set maximum read speed for I3C device in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_max_read_speed(I3C_TypeDef *i3c,
                                          sl_hal_i3c_max_read_speed_t max_speed)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Validate max read speed (3-bit field: 0-7).
  if (!SL_HAL_I3C_IS_MAX_RD_SPEED_VALID(max_speed)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Clear MXDS_MAX_RD_SPEED field.
  i3c->MAX_DATA_SPEED &= ~_I3C_MAX_DATA_SPEED_MXDS_MAX_RD_SPEED_MASK;
  // Set MXDS_MAX_RD_SPEED field with new value.
  i3c->MAX_DATA_SPEED |= ((max_speed << _I3C_MAX_DATA_SPEED_MXDS_MAX_RD_SPEED_SHIFT)
                          & _I3C_MAX_DATA_SPEED_MXDS_MAX_RD_SPEED_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configure a Slave Interrupt Request (SIR) in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_configure_sir(I3C_TypeDef *i3c,
                                     uint8_t mdb,
                                     uint8_t payload_length,
                                     uint8_t virtual_target_index)
{
  uint32_t slv_intr_req;

  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (payload_length > SL_HAL_I3C_SIR_PAYLOAD_MAX
      || virtual_target_index >= SL_HAL_I3C_MAX_VIRTUAL_TARGET_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  slv_intr_req = i3c->SLV_INTR_REQ;
  if ((slv_intr_req & I3C_SLV_INTR_REQ_SIR) != 0U) {
    return SL_STATUS_BUSY;
  }

  slv_intr_req &= ~(_I3C_SLV_INTR_REQ_SIR_TGT_INDEX_MASK
                    | _I3C_SLV_INTR_REQ_SIR_DATA_LENGTH_MASK
                    | _I3C_SLV_INTR_REQ_MDB_MASK
                    | _I3C_SLV_INTR_REQ_SIR_CTRL_MASK
                    | I3C_SLV_INTR_REQ_TS);
  slv_intr_req |= ((uint32_t)virtual_target_index
                   << _I3C_SLV_INTR_REQ_SIR_TGT_INDEX_SHIFT);
  slv_intr_req |= ((uint32_t)payload_length
                   << _I3C_SLV_INTR_REQ_SIR_DATA_LENGTH_SHIFT);
  slv_intr_req |= ((uint32_t)mdb << _I3C_SLV_INTR_REQ_MDB_SHIFT);

  i3c->SLV_INTR_REQ = slv_intr_req;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configures SCL open-drain  for I3C transfers.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_od_timing(I3C_TypeDef *i3c,
                                     uint8_t high_count,
                                     uint8_t low_count)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Clear the timing value fields.
  i3c->SCL_I3C_OD_TIMING &= ~(_I3C_SCL_I3C_OD_TIMING_I3C_OD_HCNT_MASK | _I3C_SCL_I3C_OD_TIMING_I3C_OD_LCNT_MASK);

  // Set the new timing values.
  i3c->SCL_I3C_OD_TIMING |= ((high_count << _I3C_SCL_I3C_OD_TIMING_I3C_OD_HCNT_SHIFT) & _I3C_SCL_I3C_OD_TIMING_I3C_OD_HCNT_MASK)
                            | ((low_count << _I3C_SCL_I3C_OD_TIMING_I3C_OD_LCNT_SHIFT) & _I3C_SCL_I3C_OD_TIMING_I3C_OD_LCNT_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configures SCL push-pull timing for I3C transfers .
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_pp_timing(I3C_TypeDef *i3c,
                                     uint8_t high_count,
                                     uint8_t low_count)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Clear the timing value fields.
  i3c->SCL_I3C_PP_TIMING &= ~(_I3C_SCL_I3C_PP_TIMING_I3C_PP_HCNT_MASK | _I3C_SCL_I3C_PP_TIMING_I3C_PP_LCNT_MASK);

  // Set the new timing values.
  i3c->SCL_I3C_PP_TIMING |= ((high_count << _I3C_SCL_I3C_PP_TIMING_I3C_PP_HCNT_SHIFT) & _I3C_SCL_I3C_PP_TIMING_I3C_PP_HCNT_MASK)
                            | ((low_count << _I3C_SCL_I3C_PP_TIMING_I3C_PP_LCNT_SHIFT) & _I3C_SCL_I3C_PP_TIMING_I3C_PP_LCNT_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configures SCL timing for I2C fast mode transfers.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_i2c_fm_timing(I3C_TypeDef *i3c,
                                         uint16_t high_count,
                                         uint16_t low_count)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Clear the timing value fields.
  i3c->SCL_I2C_FM_TIMING &= ~(_I3C_SCL_I2C_FM_TIMING_I2C_FM_HCNT_MASK | _I3C_SCL_I2C_FM_TIMING_I2C_FM_LCNT_MASK);

  // Set the new timing values.
  i3c->SCL_I2C_FM_TIMING |= ((high_count << _I3C_SCL_I2C_FM_TIMING_I2C_FM_HCNT_SHIFT) & _I3C_SCL_I2C_FM_TIMING_I2C_FM_HCNT_MASK)
                            | ((low_count << _I3C_SCL_I2C_FM_TIMING_I2C_FM_LCNT_SHIFT) & _I3C_SCL_I2C_FM_TIMING_I2C_FM_LCNT_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configures SCL timing for I2C fast mode plus transfers.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_i2c_fmp_timing(I3C_TypeDef *i3c,
                                          uint8_t high_count,
                                          uint16_t low_count)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Clear the timing value fields.
  i3c->SCL_I2C_FMP_TIMING &= ~(_I3C_SCL_I2C_FMP_TIMING_I2C_FMP_HCNT_MASK | _I3C_SCL_I2C_FMP_TIMING_I2C_FMP_LCNT_MASK);

  // Set the new timing values.
  i3c->SCL_I2C_FMP_TIMING |= ((high_count << _I3C_SCL_I2C_FMP_TIMING_I2C_FMP_HCNT_SHIFT) & _I3C_SCL_I2C_FMP_TIMING_I2C_FMP_HCNT_MASK)
                             | ((low_count << _I3C_SCL_I2C_FMP_TIMING_I2C_FMP_LCNT_SHIFT) & _I3C_SCL_I2C_FMP_TIMING_I2C_FMP_LCNT_MASK);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Sets idle count multiplier for the I3C device in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_idle_count_multiplier(I3C_TypeDef *i3c,
                                                 sl_hal_i3c_idle_cnt_multiplier_t multiplier)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  // Validate multiplier (2-bit field: 0-3).
  if (!SL_HAL_I3C_IS_IDLE_MULTIPLIER_VALID(multiplier)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Set DEVICE_CTRL.IDLE_CNT_MULTIPLIER.
  i3c->DEVICE_CTRL &= ~_I3C_DEVICE_CTRL_IDLE_CNT_MULTPLIER_MASK;
  i3c->DEVICE_CTRL |= ((multiplier << _I3C_DEVICE_CTRL_IDLE_CNT_MULTPLIER_SHIFT) & _I3C_DEVICE_CTRL_IDLE_CNT_MULTPLIER_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Sets the Bus Available Time for the I3C device in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_bus_available_time(I3C_TypeDef *i3c,
                                              uint16_t bus_available_time)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Clear BUS_AVAILABLE_TIME field (bits 31:16).
  i3c->BUS_FREE_AVAIL_TIMING &= ~_I3C_BUS_FREE_AVAIL_TIMING_BUS_AVAILABLE_TIME_MASK;
  // Set BUS_AVAILABLE_TIME field (bits 31:16) with new value.
  i3c->BUS_FREE_AVAIL_TIMING |= ((bus_available_time << _I3C_BUS_FREE_AVAIL_TIMING_BUS_AVAILABLE_TIME_SHIFT)
                                 & _I3C_BUS_FREE_AVAIL_TIMING_BUS_AVAILABLE_TIME_MASK);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Sets the Bus Free Time for the I3C device in controller mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_bus_free_time(I3C_TypeDef *i3c,
                                         uint16_t bus_free_time)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Clear BUS_FREE_TIME field (bits 15:0).
  i3c->BUS_FREE_AVAIL_TIMING &= ~_I3C_BUS_FREE_AVAIL_TIMING_BUS_FREE_TIME_MASK;
  // Set BUS_FREE_TIME field (bits 15:0).
  i3c->BUS_FREE_AVAIL_TIMING |= (bus_free_time & _I3C_BUS_FREE_AVAIL_TIMING_BUS_FREE_TIME_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set the bus idle time for the I3C target device.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_bus_idle_time(I3C_TypeDef *i3c,
                                         uint32_t bus_idle_time)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  // Ensure the bus_idle_time does not exceed the valid range (20 bits).
  if (bus_idle_time > _I3C_BUS_IDLE_TIMING_BUS_IDLE_TIME_MASK) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Clear the BUS_IDLE_TIME field.
  i3c->BUS_IDLE_TIMING &= ~_I3C_BUS_IDLE_TIMING_BUS_IDLE_TIME_MASK;
  // Set the new BUS_IDLE_TIME value.
  i3c->BUS_IDLE_TIMING |= (bus_idle_time & _I3C_BUS_IDLE_TIMING_BUS_IDLE_TIME_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get the bus idle time for the I3C target device.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_bus_idle_time(I3C_TypeDef *i3c,
                                         uint32_t *bus_idle_time)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (bus_idle_time == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Read the bus idle time from register.
  *bus_idle_time = i3c->BUS_IDLE_TIMING & _I3C_BUS_IDLE_TIMING_BUS_IDLE_TIME_MASK;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set release SDA timing for I3C device in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_release_sda_timing(I3C_TypeDef *i3c,
                                              uint32_t release_sda_time)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  if (release_sda_time > _I3C_RELEASE_SDA_TIMING_RELEASE_SDA_TIME_MASK) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Clear and set RELEASE_SDA_TIME field (bits 19:0).
  i3c->RELEASE_SDA_TIMING = ((i3c->RELEASE_SDA_TIMING & ~_I3C_RELEASE_SDA_TIMING_RELEASE_SDA_TIME_MASK)
                             | (release_sda_time & _I3C_RELEASE_SDA_TIMING_RELEASE_SDA_TIME_MASK));

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set subscriber ID of the I3C Device.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_subscriber_id(I3C_TypeDef *i3c,
                                         uint8_t subscriber_id)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  // Validate subscriber_id (7-bit field in DEVICE_ADDR).
  if (subscriber_id > SL_HAL_I3C_MAX_SUBSCRIBER_ID) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Clear the current Subscriber ID and set the new one.
  i3c->DEVICE_ADDR &= ~_I3C_DEVICE_ADDR_SUBSCRIBER_ID_MASK;
  i3c->DEVICE_ADDR |= ((subscriber_id << _I3C_DEVICE_ADDR_SUBSCRIBER_ID_SHIFT)
                       & _I3C_DEVICE_ADDR_SUBSCRIBER_ID_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get subscriber ID of the I3C Device.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_subscriber_id(I3C_TypeDef *i3c,
                                         uint8_t *subscriber_id)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (subscriber_id == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Retrieve the Subscriber ID
  *subscriber_id = ((i3c->DEVICE_ADDR & _I3C_DEVICE_ADDR_SUBSCRIBER_ID_MASK)
                    >> _I3C_DEVICE_ADDR_SUBSCRIBER_ID_SHIFT);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Program one Subscriber-ID map table entry in controller mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_subscriber_map_entry(I3C_TypeDef *i3c,
                                                uint8_t map_index,
                                                uint8_t subscriber_id,
                                                uint8_t dynamic_addr_7bit)
{
  volatile uint32_t *loc;
  uint32_t value;

  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (map_index >= SL_HAL_I3C_MAX_SUBSCRIBER_MAP_COUNT
      || subscriber_id > SL_HAL_I3C_MAX_SUBSCRIBER_ID
      || dynamic_addr_7bit > SL_HAL_I3C_MAX_7BIT_ADDRESS) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  switch (map_index) {
    case 0: loc = &i3c->SUBSCRIBER_ID_MAP_TABLE_LOC1; break;
    case 1: loc = &i3c->SUBSCRIBER_ID_MAP_TABLE_LOC2; break;
    case 2: loc = &i3c->SUBSCRIBER_ID_MAP_TABLE_LOC3; break;
    case 3: loc = &i3c->SUBSCRIBER_ID_MAP_TABLE_LOC4; break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  value = ((uint32_t)(subscriber_id & _I3C_SUBSCRIBER_ID_MAP_TABLE_LOC1_SUBSCRIBER_ID_MASK))
          | (((uint32_t)dynamic_addr_7bit << _I3C_SUBSCRIBER_ID_MAP_TABLE_LOC1_DEV_DYNAMIC_ADDR_SHIFT)
             & _I3C_SUBSCRIBER_ID_MAP_TABLE_LOC1_DEV_DYNAMIC_ADDR_MASK);
  *loc = value;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Read one Subscriber-ID map table entry in controller mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_subscriber_map_entry(I3C_TypeDef *i3c,
                                                uint8_t map_index,
                                                uint8_t *subscriber_id,
                                                uint8_t *dynamic_addr_7bit)
{
  volatile uint32_t *loc;
  uint32_t reg;

  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (map_index >= SL_HAL_I3C_MAX_SUBSCRIBER_MAP_COUNT
      || subscriber_id == NULL
      || dynamic_addr_7bit == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  switch (map_index) {
    case 0: loc = &i3c->SUBSCRIBER_ID_MAP_TABLE_LOC1; break;
    case 1: loc = &i3c->SUBSCRIBER_ID_MAP_TABLE_LOC2; break;
    case 2: loc = &i3c->SUBSCRIBER_ID_MAP_TABLE_LOC3; break;
    case 3: loc = &i3c->SUBSCRIBER_ID_MAP_TABLE_LOC4; break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  reg = *loc;
  *subscriber_id = (uint8_t)(reg & _I3C_SUBSCRIBER_ID_MAP_TABLE_LOC1_SUBSCRIBER_ID_MASK);
  *dynamic_addr_7bit = (uint8_t)((reg & _I3C_SUBSCRIBER_ID_MAP_TABLE_LOC1_DEV_DYNAMIC_ADDR_MASK)
                                 >> _I3C_SUBSCRIBER_ID_MAP_TABLE_LOC1_DEV_DYNAMIC_ADDR_SHIFT);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Write transmit data to extended TX data port in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_ext_tx_data(I3C_TypeDef *i3c,
                                       uint8_t ext_reg_index,
                                       uint32_t data)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (ext_reg_index >= SL_HAL_I3C_MAX_EXT_REG_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Write to the appropriate EXT_TX_DATA_PORT based on queue index.
  switch (ext_reg_index) {
    case 0:
      i3c->EXT_TX_DATA_PORT_0 = data;
      break;
    case 1:
      i3c->EXT_TX_DATA_PORT_1 = data;
      break;
    case 2:
      i3c->EXT_TX_DATA_PORT_2 = data;
      break;
    case 3:
      i3c->EXT_TX_DATA_PORT_3 = data;
      break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Program extended command register in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_ext_cmd_reg(I3C_TypeDef *i3c,
                                       uint8_t ext_reg_index,
                                       uint32_t command_value)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (ext_reg_index >= SL_HAL_I3C_MAX_EXT_REG_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Write to the appropriate EXT_CMD_REG based on queue index.
  switch (ext_reg_index) {
    case 0:
      i3c->EXT_CMD_REG_0 = command_value;
      break;
    case 1:
      i3c->EXT_CMD_REG_1 = command_value;
      break;
    case 2:
      i3c->EXT_CMD_REG_2 = command_value;
      break;
    case 3:
      i3c->EXT_CMD_REG_3 = command_value;
      break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Program extended command register (Word 2) in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_ext_cmd_reg_2(I3C_TypeDef *i3c,
                                         uint8_t ext_reg_index,
                                         uint32_t command_value
                                         )
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (ext_reg_index >= SL_HAL_I3C_MAX_EXT_REG_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Write to the appropriate EXT_CMD_REG_x_2 based on queue index.
  switch (ext_reg_index) {
    case 0:
      i3c->EXT_CMD_REG_0_2 = command_value;
      break;
    case 1:
      i3c->EXT_CMD_REG_1_2 = command_value;
      break;
    case 2:
      i3c->EXT_CMD_REG_2_2 = command_value;
      break;
    case 3:
      i3c->EXT_CMD_REG_3_2 = command_value;
      break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get extended data buffer status level in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_ext_data_buf_status_level(I3C_TypeDef *i3c,
                                                     uint8_t ext_reg_index,
                                                     uint8_t *level)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (level == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (ext_reg_index >= SL_HAL_I3C_MAX_EXT_REG_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  switch (ext_reg_index) {
    case 0:
      *level = (uint8_t)((i3c->EXT_0_1_DATA_BUF_STS_LEVEL
                          & _I3C_EXT_0_1_DATA_BUF_STS_LEVEL_EXT_0_TX_BUF_EMPTY_LOC_MASK)
                         >> _I3C_EXT_0_1_DATA_BUF_STS_LEVEL_EXT_0_TX_BUF_EMPTY_LOC_SHIFT);
      break;
    case 1:
      *level = (uint8_t)((i3c->EXT_0_1_DATA_BUF_STS_LEVEL
                          & _I3C_EXT_0_1_DATA_BUF_STS_LEVEL_EXT_1_TX_BUF_EMPTY_LOC_MASK)
                         >> _I3C_EXT_0_1_DATA_BUF_STS_LEVEL_EXT_1_TX_BUF_EMPTY_LOC_SHIFT);
      break;
    case 2:
      *level = (uint8_t)((i3c->EXT_2_3_DATA_BUF_STS_LEVEL
                          & _I3C_EXT_2_3_DATA_BUF_STS_LEVEL_EXT_2_TX_BUF_EMPTY_LOC_MASK)
                         >> _I3C_EXT_2_3_DATA_BUF_STS_LEVEL_EXT_2_TX_BUF_EMPTY_LOC_SHIFT);
      break;
    case 3:
      *level = (uint8_t)((i3c->EXT_2_3_DATA_BUF_STS_LEVEL
                          & _I3C_EXT_2_3_DATA_BUF_STS_LEVEL_EXT_3_TX_BUF_EMPTY_LOC_MASK)
                         >> _I3C_EXT_2_3_DATA_BUF_STS_LEVEL_EXT_3_TX_BUF_EMPTY_LOC_SHIFT);
      break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * This function is used to reset (flush) the extended TX buffer in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_reset_ext_tx_queue(I3C_TypeDef *i3c,
                                          uint8_t ext_reg_index)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (ext_reg_index >= SL_HAL_I3C_MAX_EXT_REG_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Set the reset bit for the specified queue in EXT_TX_QUEUE_RESET_CTRL.
  i3c->EXT_TX_QUEUE_RESET_CTRL |= (1UL << ext_reg_index);

  // Wait for reset to complete (bit auto-clears).
  while (i3c->EXT_TX_QUEUE_RESET_CTRL & (1UL << ext_reg_index)) {
    // Wait for reset to complete.
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get extended command TX buffer threshold status in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_ext_cmd_tx_thld_status(I3C_TypeDef *i3c,
                                                  uint8_t *status)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (status == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *status = (uint8_t)i3c->EXT_CMD_TX_THLD_STAT;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get extended command valid status in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_ext_cmd_valid_status(I3C_TypeDef *i3c,
                                                uint8_t *valid_status)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (valid_status == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *valid_status = (uint8_t)(i3c->EXT_CMD_VLD_STAT & SL_HAL_I3C_EXT_CMD_VLD_STAT_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Check whether Format-2 EXT_CMD_REG_x.CMD_VLD is still set.
 ******************************************************************************/
sl_status_t sl_hal_i3c_ext_cmd_is_valid(I3C_TypeDef *i3c,
                                        uint8_t ext_reg_index,
                                        bool *is_valid)
{
  uint32_t reg;

  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (is_valid == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (ext_reg_index >= SL_HAL_I3C_MAX_EXT_REG_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  switch (ext_reg_index) {
    case 0:
      reg = i3c->EXT_CMD_REG_0;
      break;
    case 1:
      reg = i3c->EXT_CMD_REG_1;
      break;
    case 2:
      reg = i3c->EXT_CMD_REG_2;
      break;
    case 3:
      reg = i3c->EXT_CMD_REG_3;
      break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  *is_valid = (reg & SL_HAL_I3C_EXT_CMD_REG_VLD) != 0U;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set extended command response threshold control in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_ext_cmd_rsp_threshold(I3C_TypeDef *i3c,
                                                 const sl_hal_i3c_ext_cmd_rsp_thld_t *thld)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (thld == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  // Validate thresholds using enum max values.
  if (thld->ext_tx_buf_thld > SL_HAL_I3C_EXT_TX_BUF_THLD_8) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (thld->recv_data_thld > SL_HAL_I3C_RECV_DATA_THLD_252) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Build and write the EXT_TX_EMPTY_RX_RSP_THLD_CTRL register.
  i3c->EXT_TX_EMPTY_RX_RSP_THLD_CTRL = ((uint8_t)thld->ext_tx_buf_thld << _I3C_EXT_TX_EMPTY_RX_RSP_THLD_CTRL_EXT_CMD_TX_BUF_THLD_SHIFT)
                                       | ((uint8_t)thld->recv_data_thld << _I3C_EXT_TX_EMPTY_RX_RSP_THLD_CTRL_RSP_DATA_THLD_SHIFT);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable a virtual target in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_enable(I3C_TypeDef *i3c,
                                             uint8_t virt_tgt_index)
{
  volatile uint32_t *ctrl_reg;
  sl_status_t status;
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (virt_tgt_index < 1 || virt_tgt_index > SL_HAL_I3C_MAX_VIRTUAL_TARGET_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = sl_hal_i3c_virtual_target_get_ctrl_register(i3c, virt_tgt_index, &ctrl_reg);
  if (status != SL_STATUS_OK) {
    SL_PRINT_STRING_ERROR("Failed to get virtual target\r\n");
    return status;
  }

  // Set ENABLE bit and write back to register.
  *ctrl_reg |= _I3C_VIRT_TGT_1_CTRL_ENABLE_MASK;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Disable a virtual target in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_disable(I3C_TypeDef *i3c,
                                              uint8_t virt_tgt_index)
{
  volatile uint32_t *ctrl_reg;
  sl_status_t status;
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (virt_tgt_index < 1 || virt_tgt_index > SL_HAL_I3C_MAX_VIRTUAL_TARGET_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  status = sl_hal_i3c_virtual_target_get_ctrl_register(i3c, virt_tgt_index, &ctrl_reg);
  if (status != SL_STATUS_OK) {
    SL_PRINT_STRING_ERROR("Failed to get virtual target\r\n");
    return status;
  }

  // Clear ENABLE bit and write back to register.
  *ctrl_reg &= ~(_I3C_VIRT_TGT_1_CTRL_ENABLE_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set static address for a virtual target.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_set_static_addr(I3C_TypeDef *i3c,
                                                      uint8_t virt_tgt_index,
                                                      uint8_t static_addr)
{
  volatile uint32_t *ctrl_reg;
  sl_status_t status;
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  if (static_addr > (_I3C_VIRT_TGT_1_CTRL_STATIC_ADDR_MASK >> _I3C_VIRT_TGT_1_CTRL_STATIC_ADDR_SHIFT)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = sl_hal_i3c_virtual_target_get_ctrl_register(i3c, virt_tgt_index, &ctrl_reg);
  if (status != SL_STATUS_OK) {
    SL_PRINT_STRING_ERROR("Failed to get virtual target\r\n");
    return status;
  }

  // Clear static address field and set new static address and valid bit.
  *ctrl_reg &= ~(_I3C_VIRT_TGT_1_CTRL_STATIC_ADDR_MASK | I3C_VIRT_TGT_1_CTRL_STATIC_ADDR_VALID);
  *ctrl_reg |= ((uint32_t)static_addr << _I3C_VIRT_TGT_1_CTRL_STATIC_ADDR_SHIFT)
               | I3C_VIRT_TGT_1_CTRL_STATIC_ADDR_VALID;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get dynamic address of a virtual target.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_get_dynamic_addr(I3C_TypeDef *i3c,
                                                       uint8_t virt_tgt_index,
                                                       uint8_t *dynamic_addr)
{
  volatile uint32_t *ctrl_reg;
  sl_status_t status;
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (dynamic_addr == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  status = sl_hal_i3c_virtual_target_get_ctrl_register(i3c, virt_tgt_index, &ctrl_reg);
  if (status != SL_STATUS_OK) {
    SL_PRINT_STRING_ERROR("Failed to get virtual target\r\n");
    return status;
  }

  *dynamic_addr = (uint8_t)((*ctrl_reg & _I3C_VIRT_TGT_1_CTRL_DYNAMIC_ADDR_MASK)
                            >> _I3C_VIRT_TGT_1_CTRL_DYNAMIC_ADDR_SHIFT);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set PID value for a virtual target.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_set_pid(I3C_TypeDef *i3c,
                                              uint8_t virt_tgt_index,
                                              uint32_t pid_value)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (virt_tgt_index < 1 || virt_tgt_index > SL_HAL_I3C_MAX_VIRTUAL_TARGET_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Write to the appropriate PID register directly.
  switch (virt_tgt_index) {
    case 1:
      i3c->VIRT_TGT_1_PID_VALUE = pid_value;
      break;
    case 2:
      i3c->VIRT_TGT_2_PID_VALUE = pid_value;
      break;
    case 3:
      i3c->VIRT_TGT_3_PID_VALUE = pid_value;
      break;
    case 4:
      i3c->VIRT_TGT_4_PID_VALUE = pid_value;
      break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set MIPI ID value for a virtual target.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_set_mipi_id(I3C_TypeDef *i3c,
                                                  uint8_t virt_tgt_index,
                                                  uint16_t mipi_mfg_id,
                                                  bool prov_id_sel)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (virt_tgt_index < 1 || virt_tgt_index > SL_HAL_I3C_MAX_VIRTUAL_TARGET_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (mipi_mfg_id > (_I3C_VIRT_TGT_1_MIPI_ID_VALUE_MIPI_MFG_ID_MASK
                     >> _I3C_VIRT_TGT_1_MIPI_ID_VALUE_MIPI_MFG_ID_SHIFT)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Build register value: bits[15:1]=MIPI_MFG_ID, bit[0]=PROV_ID_SEL.
  uint32_t reg_value = ((uint32_t)mipi_mfg_id << _I3C_VIRT_TGT_1_MIPI_ID_VALUE_MIPI_MFG_ID_SHIFT)
                       | (uint32_t)prov_id_sel;

  switch (virt_tgt_index) {
    case 1:
      i3c->VIRT_TGT_1_MIPI_ID_VALUE = reg_value;
      break;
    case 2:
      i3c->VIRT_TGT_2_MIPI_ID_VALUE = reg_value;
      break;
    case 3:
      i3c->VIRT_TGT_3_MIPI_ID_VALUE = reg_value;
      break;
    case 4:
      i3c->VIRT_TGT_4_MIPI_ID_VALUE = reg_value;
      break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set DCR (Device Characteristic Register) for a virtual target.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_set_dcr(I3C_TypeDef *i3c,
                                              uint8_t virt_tgt_index,
                                              uint8_t dcr)
{
  volatile uint32_t *char_ctrl_reg;
  uint32_t reg_value;
  sl_status_t status;

  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  status = sl_hal_i3c_virtual_target_get_char_ctrl_register(i3c, virt_tgt_index, &char_ctrl_reg);
  if (status != SL_STATUS_OK) {
    SL_PRINT_STRING_ERROR("Failed to get virtual target\r\n");
    return status;
  }

  reg_value = *char_ctrl_reg;
  reg_value &= ~_I3C_VIRT_TGT_1_CHAR_CTRL_DCR_MASK;
  reg_value |= ((uint32_t)dcr << _I3C_VIRT_TGT_1_CHAR_CTRL_DCR_SHIFT)
               & _I3C_VIRT_TGT_1_CHAR_CTRL_DCR_MASK;
  *char_ctrl_reg = reg_value;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get control register pointer for a virtual target.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_get_ctrl_register(I3C_TypeDef *i3c,
                                                        uint8_t virt_tgt_index,
                                                        volatile uint32_t **ctrl_reg)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (ctrl_reg == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (virt_tgt_index < 1 || virt_tgt_index > SL_HAL_I3C_MAX_VIRTUAL_TARGET_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  switch (virt_tgt_index) {
    case 1:
      *ctrl_reg = &i3c->VIRT_TGT_1_CTRL;
      break;
    case 2:
      *ctrl_reg = &i3c->VIRT_TGT_2_CTRL;
      break;
    case 3:
      *ctrl_reg = &i3c->VIRT_TGT_3_CTRL;
      break;
    case 4:
      *ctrl_reg = &i3c->VIRT_TGT_4_CTRL;
      break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Clear dynamic address and VALID bit for a virtual target.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_clear_dynamic_addr(I3C_TypeDef *i3c,
                                                         uint8_t virt_tgt_index)
{
  volatile uint32_t *ctrl_reg = NULL;
  sl_status_t status;

  status = sl_hal_i3c_virtual_target_get_ctrl_register(i3c, virt_tgt_index, &ctrl_reg);
  if (status != SL_STATUS_OK || ctrl_reg == NULL) {
    return (status != SL_STATUS_OK) ? status : SL_STATUS_FAIL;
  }

  *ctrl_reg &= ~(_I3C_VIRT_TGT_1_CTRL_DYNAMIC_ADDR_MASK
                 | I3C_VIRT_TGT_1_CTRL_DYNAMIC_ADDR_VALID);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get characteristics control register pointer for a virtual target.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_get_char_ctrl_register(I3C_TypeDef *i3c,
                                                             uint8_t virt_tgt_index,
                                                             volatile uint32_t **char_ctrl_reg)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (char_ctrl_reg == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (virt_tgt_index < 1 || virt_tgt_index > SL_HAL_I3C_MAX_VIRTUAL_TARGET_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  switch (virt_tgt_index) {
    case 1:
      *char_ctrl_reg = &i3c->VIRT_TGT_1_CHAR_CTRL;
      break;
    case 2:
      *char_ctrl_reg = &i3c->VIRT_TGT_2_CHAR_CTRL;
      break;
    case 3:
      *char_ctrl_reg = &i3c->VIRT_TGT_3_CHAR_CTRL;
      break;
    case 4:
      *char_ctrl_reg = &i3c->VIRT_TGT_4_CHAR_CTRL;
      break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get group address status register pointer.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_group_address_stat_register(I3C_TypeDef *i3c,
                                                       uint8_t grp_addr_index,
                                                       volatile uint32_t **stat_reg)
{
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (stat_reg == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (grp_addr_index >= SL_HAL_I3C_MAX_GROUP_ADDRESS_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  switch (grp_addr_index) {
    case 0:
      *stat_reg = &i3c->GRP_ADDR_0_STAT;
      break;
    case 1:
      *stat_reg = &i3c->GRP_ADDR_1_STAT;
      break;
    case 2:
      *stat_reg = &i3c->GRP_ADDR_2_STAT;
      break;
    case 3:
      *stat_reg = &i3c->GRP_ADDR_3_STAT;
      break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set group address for a specified group address index (secondary controller mode).
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_group_address(I3C_TypeDef *i3c,
                                         uint8_t grp_addr_index,
                                         uint8_t grp_addr)
{
  volatile uint32_t *reg_ptr;
  sl_status_t status;

  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (grp_addr > SL_HAL_I3C_MAX_7BIT_ADDRESS) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = sl_hal_i3c_get_group_address_stat_register(i3c, grp_addr_index, &reg_ptr);
  if (status != SL_STATUS_OK) {
    SL_PRINT_STRING_ERROR("Failed to get group address\r\n");
    return status;
  }

  *reg_ptr &= ~_I3C_GRP_ADDR_0_STAT_GRP_ADDR_MASK;
  *reg_ptr |= (grp_addr << _I3C_GRP_ADDR_0_STAT_GRP_ADDR_SHIFT);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get group address and target valid mask in target mode.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_group_address(I3C_TypeDef *i3c,
                                         uint8_t grp_addr_index,
                                         uint8_t *grp_addr,
                                         uint8_t *target_valid_mask)
{
  volatile uint32_t *reg_ptr;
  uint32_t reg_value;
  sl_status_t status;

  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (grp_addr == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  status = sl_hal_i3c_get_group_address_stat_register(i3c, grp_addr_index, &reg_ptr);
  if (status != SL_STATUS_OK) {
    SL_PRINT_STRING_ERROR("Failed to get group address\r\n");
    return status;
  }

  reg_value = *reg_ptr;

  *grp_addr = (uint8_t)((reg_value & _I3C_GRP_ADDR_0_STAT_GRP_ADDR_MASK)
                        >> _I3C_GRP_ADDR_0_STAT_GRP_ADDR_SHIFT);

  if (target_valid_mask != NULL) {
    *target_valid_mask = (uint8_t)((reg_value >> _I3C_GRP_ADDR_0_STAT_T0_GRP_ADDR_VLD_SHIFT)
                                   & SL_HAL_I3C_GROUP_ADDRESS_TARGET_VALID_MASK);
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable a target for a group address (secondary controller mode).
 ******************************************************************************/
sl_status_t sl_hal_i3c_group_address_enable_target(I3C_TypeDef *i3c,
                                                   uint8_t grp_addr_index,
                                                   uint8_t virt_tgt_index)
{
  volatile uint32_t *reg_ptr;
  sl_status_t status;

  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (virt_tgt_index >= SL_HAL_I3C_GROUP_ADDRESS_TARGET_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = sl_hal_i3c_get_group_address_stat_register(i3c, grp_addr_index, &reg_ptr);
  if (status != SL_STATUS_OK) {
    SL_PRINT_STRING_ERROR("Failed to get group address\r\n");
    return status;
  }

  *reg_ptr |= (1UL << (_I3C_GRP_ADDR_0_STAT_T0_GRP_ADDR_VLD_SHIFT + virt_tgt_index));

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Disable a target for a group address (secondary controller mode).
 ******************************************************************************/
sl_status_t sl_hal_i3c_group_address_disable_target(I3C_TypeDef *i3c,
                                                    uint8_t grp_addr_index,
                                                    uint8_t virt_tgt_index)
{
  volatile uint32_t *reg_ptr;
  sl_status_t status;

  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (virt_tgt_index >= SL_HAL_I3C_GROUP_ADDRESS_TARGET_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = sl_hal_i3c_get_group_address_stat_register(i3c, grp_addr_index, &reg_ptr);
  if (status != SL_STATUS_OK) {
    SL_PRINT_STRING_ERROR("Failed to get group address\r\n");
    return status;
  }

  *reg_ptr &= ~(1UL << (_I3C_GRP_ADDR_0_STAT_T0_GRP_ADDR_VLD_SHIFT + virt_tgt_index));

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Reset one or all I3C queues / FIFOs.
 ******************************************************************************/
sl_status_t sl_hal_i3c_buffer_reset(I3C_TypeDef *i3c,
                                    sl_hal_i3c_buffer_reset_t buffer_type)
{
  uint32_t reset_mask;

  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (!SL_HAL_I3C_BUFFER_RESET_IS_VALID(buffer_type)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  switch (buffer_type) {
    case SL_HAL_I3C_RESET_CMD_QUEUE:
      reset_mask = I3C_RESET_CTRL_CMD_QUEUE_RST;
      break;
    case SL_HAL_I3C_RESET_RESP_QUEUE:
      reset_mask = I3C_RESET_CTRL_RESP_QUEUE_RST;
      break;
    case SL_HAL_I3C_RESET_TX_FIFO:
      reset_mask = I3C_RESET_CTRL_TX_FIFO_RST;
      break;
    case SL_HAL_I3C_RESET_RX_FIFO:
      reset_mask = I3C_RESET_CTRL_RX_FIFO_RST;
      break;
    case SL_HAL_I3C_RESET_IBI_QUEUE:
      reset_mask = I3C_RESET_CTRL_IBI_QUEUE_RST;
      break;
    case SL_HAL_I3C_RESET_ALL_QUEUES:
      reset_mask = (I3C_RESET_CTRL_CMD_QUEUE_RST
                    | I3C_RESET_CTRL_RESP_QUEUE_RST
                    | I3C_RESET_CTRL_TX_FIFO_RST
                    | I3C_RESET_CTRL_RX_FIFO_RST
                    | I3C_RESET_CTRL_IBI_QUEUE_RST);
      break;
    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  return sl_hal_i3c_set_reset_control(i3c, reset_mask);
}

/***************************************************************************//**
 * Issue an I3C bus reset pattern.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_bus_reset_type(I3C_TypeDef *i3c,
                                          sl_hal_i3c_bus_reset_type_t type)
{
  uint32_t reset_mask;

  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (!SL_HAL_I3C_BUS_RESET_TYPE_IS_VALID(type)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  reset_mask = (((uint32_t)type << _I3C_RESET_CTRL_BUS_RESET_TYPE_SHIFT)
                & _I3C_RESET_CTRL_BUS_RESET_TYPE_MASK)
               | I3C_RESET_CTRL_BUS_RESET;

  /* TYPE bits are sticky; clear them before ORing the new pattern + BUS_RESET. */
  i3c->RESET_CTRL &= ~_I3C_RESET_CTRL_BUS_RESET_TYPE_MASK;

  return sl_hal_i3c_set_reset_control(i3c, reset_mask);
}

#endif /* defined(I3C_PRESENT) */
