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
#ifndef SL_HAL_I3C_H
#define SL_HAL_I3C_H
#include "em_device.h"
#if defined(I3C_PRESENT)
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stddef.h>
#include "sl_log_helper.h"
#include "sl_status.h"
#include "sl_enum.h"

/***************************************************************************//**
 * @addtogroup i3c I3C - Improved Inter-Integrated Circuit
 *
 * @note "Target mode" refers to both "target only" and "secondary controller A" roles.
 * @{
 ******************************************************************************/

/*******************************************************************************
 *********************************   DEFINES   *********************************
 ******************************************************************************/

// I3C instance validation.
#define SL_HAL_I3C_REF_VALID(ref)    (I3C_NUM(ref) != -1)

/// Maximum 7-bit I3C/I2C address value.
#define SL_HAL_I3C_MAX_7BIT_ADDRESS  0x7FU

/// Maximum Device Address Table (DAT) entries (controller).
#define SL_HAL_I3C_MAX_TARGET_COUNT \
        _I3C_DEVICE_ADDR_TABLE_POINTER_DEV_ADDR_TABLE_DEPTH_DEFAULT

/// Number of virtual targets supported (IC_VIRT_TGT_CNT).
#define SL_HAL_I3C_MAX_VIRTUAL_TARGET_COUNT  4U

/// Number of extended command queues (IC_EXT_QUEUE_CNT).
#define SL_HAL_I3C_MAX_EXT_REG_COUNT         4U

/// Maximum SLV_INTR_REQ.SIR_DATA_LENGTH (total IBI bytes excluding MDB).
#define SL_HAL_I3C_SIR_PAYLOAD_MAX 4U

/// Number of group addresses supported (IC_GRP_ADDR_CNT).
#define SL_HAL_I3C_MAX_GROUP_ADDRESS_COUNT 4U

/// Targets that can be associated with a group address (T0–T4).
#define SL_HAL_I3C_GROUP_ADDRESS_TARGET_COUNT 5U

/// Subscriber-ID map table slots (SUBSCRIBER_ID_MAP_TABLE_LOC1..4).
#define SL_HAL_I3C_MAX_SUBSCRIBER_MAP_COUNT 4U

/// Max 7-bit D2D subscriber ID (DEVICE_ADDR.SUBSCRIBER_ID).
#define SL_HAL_I3C_MAX_SUBSCRIBER_ID 0x7FU

// Mask for valid extended command valid status bits (bits 0-3: CMD_0_VLD through CMD_3_VLD).
#define SL_HAL_I3C_EXT_CMD_VLD_STAT_MASK  0x0FUL

/** Format-2 EXT_CMD_REG Word1 CMD_VLD bit (set while command is armed). */
#define SL_HAL_I3C_EXT_CMD_REG_VLD        (1UL << 16)

// Mask for group address target valid bits (bits 0-4: T0 through T4).
// 5 bits total for the 5 targets that can be associated with a group address.
#define SL_HAL_I3C_GROUP_ADDRESS_TARGET_VALID_MASK  0x1FUL

/*******************************************************************************
 ********************************   ENUMS   ************************************
 ******************************************************************************/

// Enum for Threshold Levels.
SL_ENUM(sl_hal_i3c_data_thld_t) {
  SL_HAL_I3C_THLD_1  = 0,    ///< Threshold Level 1.
  SL_HAL_I3C_THLD_4  = 1,    ///< Threshold Level 4.
  SL_HAL_I3C_THLD_8  = 2,    ///< Threshold Level 8.
  SL_HAL_I3C_THLD_16 = 3,    ///< Threshold Level 16.
  SL_HAL_I3C_THLD_32 = 4,    ///< Threshold Level 32.
  SL_HAL_I3C_THLD_64 = 5     ///< Threshold Level 64.
};

// Enum for Idle Count Multiplier.
SL_ENUM(sl_hal_i3c_idle_cnt_multiplier_t) {
  SL_HAL_I3C_IDLE_MULTIPLIER_1 = 0,    ///< BUS_AVAILABLE_TIME * 1.
  SL_HAL_I3C_IDLE_MULTIPLIER_2 = 1,    ///< BUS_AVAILABLE_TIME * 2.
  SL_HAL_I3C_IDLE_MULTIPLIER_4 = 2,    ///< BUS_AVAILABLE_TIME * 4.
  SL_HAL_I3C_IDLE_MULTIPLIER_8 = 3     ///< BUS_AVAILABLE_TIME * 8.
};

// Enum for Clock to Data Turnaround Time (values 5-7 are reserved).
SL_ENUM(sl_hal_i3c_clock_data_turnaround_t) {
  SL_HAL_I3C_CLK_DATA_TURN_8NS  = 0,    ///< 8 ns.
  SL_HAL_I3C_CLK_DATA_TURN_9NS  = 1,    ///< 9 ns.
  SL_HAL_I3C_CLK_DATA_TURN_10NS = 2,    ///< 10 ns.
  SL_HAL_I3C_CLK_DATA_TURN_11NS = 3,    ///< 11 ns.
  SL_HAL_I3C_CLK_DATA_TURN_12NS = 4     ///< 12 ns.
};

// Enum for Maximum Read Speed (values 5-7 are reserved).
SL_ENUM(sl_hal_i3c_max_read_speed_t) {
  SL_HAL_I3C_MAX_RD_SPEED_12_5MHZ = 0,    ///< 12.5 MHz.
  SL_HAL_I3C_MAX_RD_SPEED_8MHZ    = 1,    ///< 8 MHz.
  SL_HAL_I3C_MAX_RD_SPEED_6MHZ    = 2,    ///< 6 MHz.
  SL_HAL_I3C_MAX_RD_SPEED_4MHZ    = 3,    ///< 4 MHz.
  SL_HAL_I3C_MAX_RD_SPEED_2MHZ    = 4     ///< 2 MHz.
};

// Enum for IBI Types.
SL_ENUM(sl_hal_i3c_ibi_type_t) {
  SL_HAL_I3C_IBI_TYPE_HJ,        ///< Hot-Join.
  SL_HAL_I3C_IBI_TYPE_MR,        ///< Master Request.
  SL_HAL_I3C_IBI_TYPE_SIR,       ///< Slave Interrupt Request.
  SL_HAL_I3C_IBI_TYPE_UNKNOWN    ///< Unknown type.
};

/// DEVICE_ROLE field in slave characteristics configuration.
SL_ENUM(sl_hal_i3c_device_role_t) {
  SL_HAL_I3C_DEVICE_ROLE_TARGET             = 0,  ///< Slave Only.
  SL_HAL_I3C_DEVICE_ROLE_CONTROLLER_CAPABLE = 1   ///< Secondary master capable.
};

/// Queue / FIFO reset type (maps to RESET_CTRL).
SL_ENUM(sl_hal_i3c_buffer_reset_t) {
  SL_HAL_I3C_RESET_CMD_QUEUE = 0,   ///< Command queue reset.
  SL_HAL_I3C_RESET_RESP_QUEUE,      ///< Response queue reset.
  SL_HAL_I3C_RESET_TX_FIFO,         ///< Transmit FIFO reset.
  SL_HAL_I3C_RESET_RX_FIFO,         ///< Receive FIFO reset.
  SL_HAL_I3C_RESET_IBI_QUEUE,       ///< IBI queue reset (controller mode).
  SL_HAL_I3C_RESET_ALL_QUEUES       ///< All queues and FIFOs.
};

/// Bus reset pattern type (RESET_CTRL.BUS_RESET_TYPE).
SL_ENUM(sl_hal_i3c_bus_reset_type_t) {
  SL_HAL_I3C_BUS_RESET_EXIT_PATTERN         = 0x0,  ///< Exit pattern.
  SL_HAL_I3C_BUS_RESET_TARGET_RESET_PATTERN = 0x1,  ///< Target reset pattern.
  SL_HAL_I3C_BUS_RESET_SCL_LOW_PATTERN      = 0x6   ///< SCL low reset pattern.
};

// Enum for Receive Data Threshold .
SL_ENUM(sl_hal_i3c_recv_data_thld_t) {
  SL_HAL_I3C_RECV_DATA_THLD_16  = 0,     ///< 16 bytes.
  SL_HAL_I3C_RECV_DATA_THLD_32  = 1,     ///< 32 bytes.
  SL_HAL_I3C_RECV_DATA_THLD_64  = 2,     ///< 64 bytes.
  SL_HAL_I3C_RECV_DATA_THLD_128 = 3,     ///< 128 bytes.
  SL_HAL_I3C_RECV_DATA_THLD_252 = 4      ///< 252 bytes.
};

// Enum for Extended TX Buffer Threshold.
SL_ENUM(sl_hal_i3c_ext_tx_buf_thld_t) {
  SL_HAL_I3C_EXT_TX_BUF_THLD_1 = 0,    ///< 1 empty location.
  SL_HAL_I3C_EXT_TX_BUF_THLD_4 = 1,    ///< 4 empty locations.
  SL_HAL_I3C_EXT_TX_BUF_THLD_8 = 2     ///< 8 empty locations.
};

// Validate the threshold level (IC_BUF_LVL_SEL).
#define SL_HAL_I3C_IS_DATA_THLD_VALID(val) ((val) <= SL_HAL_I3C_THLD_64)

// Validate the idle count multiplier.
#define SL_HAL_I3C_IS_IDLE_MULTIPLIER_VALID(val) \
        ((val) <= SL_HAL_I3C_IDLE_MULTIPLIER_8)

// Validate clock-to-data turnaround.
#define SL_HAL_I3C_IS_CLK_DATA_TURN_VALID(val) \
        ((val) <= SL_HAL_I3C_CLK_DATA_TURN_12NS)

// Validate max read speed.
#define SL_HAL_I3C_IS_MAX_RD_SPEED_VALID(val) \
        ((val) <= SL_HAL_I3C_MAX_RD_SPEED_2MHZ)

// Validate device role.
#define SL_HAL_I3C_IS_DEVICE_ROLE_VALID(val) \
        ((val) <= SL_HAL_I3C_DEVICE_ROLE_CONTROLLER_CAPABLE)

#define SL_HAL_I3C_BUFFER_RESET_IS_VALID(val) \
        ((val) <= SL_HAL_I3C_RESET_ALL_QUEUES)

#define SL_HAL_I3C_BUS_RESET_TYPE_IS_VALID(val) \
        (((val) == SL_HAL_I3C_BUS_RESET_EXIT_PATTERN)            \
         || ((val) == SL_HAL_I3C_BUS_RESET_TARGET_RESET_PATTERN) \
         || ((val) == SL_HAL_I3C_BUS_RESET_SCL_LOW_PATTERN))

/*******************************************************************************
 *******************************   STRUCTS   ***********************************
 ******************************************************************************/

// Structure to configure FIFO thresholds.
typedef struct {
  sl_hal_i3c_data_thld_t rx_start_thld;         ///< RX start threshold.
  sl_hal_i3c_data_thld_t tx_start_thld;         ///< TX start threshold.
  sl_hal_i3c_data_thld_t rx_buf_thld;           ///< RX buffer threshold.
  sl_hal_i3c_data_thld_t tx_empty_buf_thld;     ///< TX empty buffer threshold.
} sl_hal_i3c_fifo_thld_t;

// Structure to configure queue thresholds.
typedef struct {
  uint8_t cmd_empty_buf_thld;    ///< Command empty buffer threshold.
  uint8_t resp_buf_thld;         ///< Response buffer threshold.
  uint8_t ibi_data_thld;         ///< IBI data threshold (minimum 1).
  uint8_t ibi_status_thld;       ///< IBI status threshold.
} sl_hal_i3c_queue_thld_t;

// Structure to configure a single Device Address Table (DAT) entry in controller mode.
typedef struct {
  uint8_t dynamic_address;    ///< 7-bit dynamic address.
  uint8_t static_address;     ///< 7-bit static address (0x00-0x7F).
  bool    is_i2c_device;      ///< true for I2C device, false for I3C.
  bool    ibi_with_data;      ///< true to enable IBI with data.
  bool    sir_enable;         ///< true to accept Slave Interrupt Requests.
  bool    mr_enable;          ///< true to accept Master Requests.
} sl_hal_i3c_target_dat_config_t;

// Structure to configure extended command response threshold control.
typedef struct {
  sl_hal_i3c_ext_tx_buf_thld_t ext_tx_buf_thld;     ///< Extended TX buffer threshold.
  sl_hal_i3c_recv_data_thld_t recv_data_thld;       ///< Receive data threshold.
} sl_hal_i3c_ext_cmd_rsp_thld_t;

/// Slave characteristics configuration.
typedef struct {
  bool                      max_data_speed_limit;  ///< Max data speed limit.
  bool                      hdr_capable;           ///< HDR capable.
  sl_hal_i3c_device_role_t device_role;            ///< Device role.
  uint8_t                   dcr;                   ///< Device characteristic register.
} sl_hal_i3c_slave_char_config_t;

/// I3C IP capability information.
typedef struct {
  uint32_t raw;                              ///< Raw HW_CAPABILITY register value
  uint8_t  device_role_config;               ///< DEVICE_ROLE_CONFIG [2:0]
  bool     hdr_ddr_supported;                ///< HDR-DDR supported
  bool     hdr_ts_supported;                 ///< HDR Ternary Symbol (TS) supported
  uint8_t  clock_period;                     ///< CLOCK_PERIOD [10:5]
  uint8_t  hdr_tx_clock_period;              ///< HDR_TX_CLOCK_PERIOD [16:11]
  bool     dma_supported;                    ///< DMA support present
  bool     slave_hot_join_supported;         ///< Slave Hot-Join capability
  bool     slave_ibi_supported;              ///< Slave IBI capability
  bool     target_group_addr_supported;      ///< Target group address capability
  bool     virtual_target_supported;         ///< Virtual target capability
  bool     d2d_tunneling_supported;          ///< Device-to-device tunneling capability
  bool     hdr_bt_supported;                 ///< HDR Bluetooth (BT) supported
} sl_hal_i3c_ip_capability_t;

/// Decoded QUEUE_SIZE_CAPABILITY depths in 32-bit FIFO/queue locations.
typedef struct {
  uint8_t tx_buf_dwords;       ///< TX data FIFO depth.
  uint8_t rx_buf_dwords;       ///< RX data FIFO depth.
  uint8_t cmd_buf_dwords;      ///< Command queue depth.
  uint8_t resp_buf_dwords;     ///< Response queue depth.
  uint8_t ibi_buf_dwords;      ///< IBI queue depth (controller).
} sl_hal_i3c_queue_size_t;

/// Target-mode QUEUE_THLD_CTRL fields (command / response only).
typedef struct {
  uint8_t cmd_empty_buf_thld;  ///< 0 = empty; 1..N empty locations.
  uint8_t resp_buf_thld;       ///< 0 = one entry; N = N+1 entries.
} sl_hal_i3c_target_queue_thld_t;

/*******************************************************************************
 *****************************   PROTOTYPES   **********************************
 ******************************************************************************/
/***************************************************************************//**
 * Sets the I3C peripheral to operate in controller mode.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_controller_mode(I3C_TypeDef *i3c);

/***************************************************************************//**
 * Sets the I3C peripheral to operate in target mode.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_target_mode(I3C_TypeDef *i3c);

/***************************************************************************//**
 * Get the capabilities of the I3C peripheral.
 *
 * @param[in] i3c              A pointer to the I3C peripheral register block.
 *
 * @param[out] capability  A Pointer to store the I3C peripheral capabilities.
 *
 * @return                     On success returns SL_STATUS_OK.
 *                             On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_hw_capability(I3C_TypeDef *i3c,
                                         sl_hal_i3c_ip_capability_t *capability);
/***************************************************************************//**
 * Configures whether I2C slaves are present on the bus.
 *
 * @note This function affects the timing and protocol behavior of the I3C controller.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[in] present  true if I2C slaves are present, false otherwise.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_i2c_slave_present(I3C_TypeDef *i3c,
                                             bool present);

/***************************************************************************//**
 * Abort ongoing I3C controller transfer.
 *
 * @note   This function blocks until the abort operation completes.
 *         The ABORT bit is auto-cleared by hardware when the abort is done.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
sl_status_t sl_hal_i3c_abort_controller_transfer(I3C_TypeDef *i3c);

/***************************************************************************//**
 * Request resume after the I3C core entered halt.
 *
 * @note controller mode clears RESUME when the next command is initiated,
 * target mode clears when ready after error handling.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
sl_status_t sl_hal_i3c_resume(I3C_TypeDef *i3c);

/***************************************************************************//**
 * Sets the dynamic address for the I3C device.
 *
 * @param[in] i3c              A pointer to the I3C peripheral register block.
 *
 * @param[in] dynamic_address  7-bit dynamic address.
 *
 * @return                     On success returns SL_STATUS_OK.
 *                             On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_dynamic_address(I3C_TypeDef *i3c,
                                           uint8_t dynamic_address);

/***************************************************************************//**
 * Gets the current dynamic address of the I3C device.
 *
 * @param[in] i3c               A pointer to the I3C peripheral register block.
 *
 * @param[out] dynamic_address  A Pointer to store the 7-bit dynamic address.
 *
 * @return                      On success returns SL_STATUS_OK.
 *                              On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                 SL_STATUS_NULL_POINTER.
 *                                                 SL_STATUS_FAIL.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_dynamic_address(I3C_TypeDef *i3c,
                                           uint8_t *dynamic_address);

/***************************************************************************//**
 * Sets the static address for the I3C device.
 *
 * @note This address is used for I2C compatibility and static address assignment.
 *
 * @param[in] i3c              A pointer to the I3C peripheral register block.
 *
 * @param[in] static_address   7-bit static address.
 *
 * @return                     On success returns SL_STATUS_OK.
 *                             On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_static_address(I3C_TypeDef *i3c,
                                          uint8_t static_address);

/***************************************************************************//**
 * Gets the current static address of the I3C device.
 *
 * @param[in] i3c              A pointer to the I3C peripheral register block.
 *
 * @param[out] static_address  A Pointer to store the 7-bit static address.
 *
 * @return                     On success returns SL_STATUS_OK.
 *                             On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                SL_STATUS_NULL_POINTER.
 *                                                SL_STATUS_FAIL.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_static_address(I3C_TypeDef *i3c,
                                          uint8_t *static_address);

/***************************************************************************//**
 * Writes data to the I3C transfer data port.
 *
 * @param[in] i3c   A pointer to the I3C peripheral register block.
 *
 * @param[in] data  The 32-bit data to write to the register.
 *
 * @return          On success returns SL_STATUS_OK.
 *                  On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_transfer_data(I3C_TypeDef *i3c,
                                         uint32_t data);

/***************************************************************************//**
 * Reads data from the I3C transfer data port.
 *
 * @param[in] i3c   A pointer to the I3C peripheral register block.
 *
 * @param[out] data A pointer to store the 32-bit data read from register.
 *
 * @return          On success returns SL_STATUS_OK.
 *                  On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                     SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_transfer_data(I3C_TypeDef *i3c,
                                         uint32_t *data);

/***************************************************************************//**
 * Get IBI type from IBI ID in controller mode.
 *
 * @note   IBI_ID format (IBI_QUEUE_PORT bits [15:8]):
 *         - Target address byte along with the RnW bit received from the target.
 *         - Bits [7:1]: 7-bit dynamic address.
 *         - Bit [0]: RnW bit (1 = SIR, 0 = MR).
 *         - Hot-Join: 7-bit reserved address 0x02 with RnW = 0 → IBI_ID byte 0x04.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[in] ibi_id   IBI identifier (8-bit value from IBI_QUEUE_PORT[15:8]).
 *
 * @return             IBI type enumeration (SL_HAL_I3C_IBI_TYPE_HJ, SL_HAL_I3C_IBI_TYPE_SIR,
 *                     SL_HAL_I3C_IBI_TYPE_MR, or SL_HAL_I3C_IBI_TYPE_UNKNOWN).
 ******************************************************************************/
sl_hal_i3c_ibi_type_t sl_hal_i3c_get_ibi_type(I3C_TypeDef *i3c,
                                              uint8_t ibi_id);

/***************************************************************************//**
 * Set queue thresholds for command empty buffer, response buffer,
 * IBI data buffer, and IBI status buffer.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[in] thld     A pointer to queue threshold configuration.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED,
 *                                          SL_STATUS_NULL_POINTER,
 *                                          SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_queue_threshold(I3C_TypeDef *i3c,
                                           const sl_hal_i3c_queue_thld_t *thld);

/***************************************************************************//**
 * Set FIFO thresholds for transmit and receive buffers.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[in] thld     A pointer to FIFO threshold configuration.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                        SL_STATUS_NULL_POINTER.
 *                                        SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_fifo_threshold(I3C_TypeDef *i3c,
                                          const sl_hal_i3c_fifo_thld_t *thld);

/***************************************************************************//**
 * Read QUEUE_SIZE_CAPABILITY and decode each field to a dword depth.
 *
 * @param[in]  i3c   A pointer to the I3C peripheral register block.
 * @param[out] size  Decoded depths in 32-bit locations.
 *
 * @return           SL_STATUS_OK, SL_STATUS_NOT_SUPPORTED, or SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_queue_size_capability(I3C_TypeDef *i3c,
                                                 sl_hal_i3c_queue_size_t *size);

/***************************************************************************//**
 * Program target-mode command and response thresholds only.
 *
 * @details  CMD empty 0 means empty, RESP 0 means one queued response.
 *
 * @param[in] i3c   A pointer to the I3C peripheral register block.
 * @param[in] thld  Command empty and response thresholds.
 *
 * @return          SL_STATUS_OK, SL_STATUS_NOT_SUPPORTED, or SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_target_queue_threshold(I3C_TypeDef *i3c,
                                                  const sl_hal_i3c_target_queue_thld_t *thld);

/***************************************************************************//**
 * Gets the current status of the slave event status register.
 *
 * @note   The returned register value (SLV_EVENT_STATUS) has the following bit layout:
 *         - Bit  [0]   : SIR_EN         - Slave Interrupt Request enable (read-only, set by CCC).
 *         - Bit  [1]   : MR_EN          - Master Request enable (read-only, set by CCC).
 *         - Bit  [3]   : HJ_EN          - Hot-Join enable.
 *         - Bits [5:4] : ACTIVITY_STATE - Activity state.
 *         - Bit  [6]   : MRL_UPDATED    - Max Read Length updated.
 *         - Bit  [7]   : MWL_UPDATED    - Max Write Length updated.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[out] reg_val A pointer to store the SLV_EVENT_STATUS register value.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                        SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_slave_event_status(I3C_TypeDef *i3c,
                                              uint32_t *reg_val);

/***************************************************************************//**
 * Get the current transfer state of the controller.
 *
 * @note This function retrieves the current transfer state of the controller
 * from the PRESENT_STATE register.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[out] reg_val A Pointer to store the transfer state value.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                        SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_controller_transfer_status(I3C_TypeDef *i3c,
                                                      uint32_t *reg_val);

/***************************************************************************//**
 * Get current transfer type of the I3C device.
 *
 * @note   This function retrieves the transfer type status from the CM_TFR_STS field
 *         of the PRESENT_STATE register. Indicates the type of transfer currently
 *         executing.
 *
 * @param[in] i3c              A pointer to the I3C peripheral register block.
 *
 * @param[out] transfer_type   A pointer to store the current transfer type value.
 *
 * @return                     On success returns SL_STATUS_OK.
 *                             On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_transfer_type(I3C_TypeDef *i3c,
                                         uint32_t *transfer_type);

/***************************************************************************//**
 * Configure a target device in the Device Address Table in controller mode.
 *
 * In controller mode, the DAT stores one address entry per attached
 * target. Address-assignment and transfer commands use DEV_INDEX to
 * select the entry.
 *
 * @param[in] i3c               A pointer to the I3C peripheral register block.
 *
 * @param[in] target_index      Index of target device in Device Address Table
 *                              (0 to SL_HAL_I3C_MAX_TARGET_COUNT-1).
 *
 * @param[in] cfg               A pointer to the DAT entry configuration.
 *
 * @return                      On success returns SL_STATUS_OK.
 *                              On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                 SL_STATUS_NULL_POINTER.
 *                                                 SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_configure_target(I3C_TypeDef *i3c,
                                        uint8_t target_index,
                                        const sl_hal_i3c_target_dat_config_t *cfg);

/***************************************************************************//**
 * Get DEV_CHAR_TABLE base register address.
 *
 * @note This function returns the base DEV_CHAR_TABLE register address through output parameter.
 *
 * @param[in] i3c          A pointer to the I3C peripheral register block.
 *
 * @param[out] base_addr   A Pointer to store the base register address.
 *
 * @return                 On success returns SL_STATUS_OK.
 *                         On failure returns SL_STATUS_NOT_SUPPORTED
 *                                            SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_dev_char_table_base(I3C_TypeDef *i3c,
                                               const uint32_t **base_addr);

/***************************************************************************//**
 * Sets the MIPI Manufacturer ID and the Provisional ID Type in target mode.
 *
 * @note   SLV_MIPI_ID_VALUE register layout:
 *         - Bits [15:1]: SLV_MIPI_MFG_ID - MIPI Manufacturer ID (PID[47:33]).
 *         - Bit [0]: SLV_PROV_ID_SEL - Provisional ID Type Selector (PID[32]).
 *           true = Random Value, false = Vendor Fixed Value.
 *
 * @param[in] i3c         A pointer to the I3C peripheral register block.
 *
 * @param[in] mfg_id      MIPI Manufacturer ID (15 bits, 0x0000-0x7FFF).
 *
 * @param[in] prov_id_sel Provisional ID Type Selector.
 *                        false = Vendor Fixed Value, true = Random Value.
 *
 * @return                SL_STATUS_OK on success.
 *                        SL_STATUS_NOT_SUPPORTED if I3C not present.
 *                        SL_STATUS_INVALID_PARAMETER if mfg_id > 0x7FFF.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_slave_mipi_id(I3C_TypeDef *i3c,
                                         uint16_t mfg_id,
                                         bool prov_id_sel);

/***************************************************************************//**
 * Set slave characteristics of I3C device in target mode.
 *
 * @param[in] i3c  A pointer to the I3C peripheral register block.
 * @param[in] cfg  slave characteristics configuration.
 *
 * @return         On success returns SL_STATUS_OK.
 *                 On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                    SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_slave_characteristics(I3C_TypeDef *i3c,
                                                 const sl_hal_i3c_slave_char_config_t *cfg);

/**************************************************************** ***********//**
 * Get slave characteristics register.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[out] value   A Pointer to store the SLV_CHAR_CTRL register value.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                        SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_slave_characteristics(I3C_TypeDef *i3c,
                                                 uint32_t *value);

/***************************************************************************//**
 * Get device role register.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[out] device_role   A Pointer to store the DEVICE_ROLE register value.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                        SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_device_role(I3C_TypeDef *i3c,
                                       sl_hal_i3c_device_role_t *device_role);

/***************************************************************************//**
 * Set clock to data turnaround time for I3C device in target mode.
 *
 * @note   This function specifies the clock to data turnaround time (Tsco parameter) of
 *         DWC_mipi_i3c Slave device.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[in] turn_time Turnaround time value.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                        SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_clock_data_turnaround(I3C_TypeDef *i3c,
                                                 sl_hal_i3c_clock_data_turnaround_t turn_time);

/***************************************************************************//**
 * Set maximum read speed for I3C device in target mode.
 *
 * @note   This function configures the maximum sustained data rate for non-CCC messages
 *         sent by DWC_mipi_i3c slave device to master device.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[in] max_speed Maximum read speed value.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                        SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_max_read_speed(I3C_TypeDef *i3c,
                                          sl_hal_i3c_max_read_speed_t max_speed);

/***************************************************************************//**
 * Configure a Slave Interrupt Request (SIR) in target mode.
 *
 * This function programs SLV_INTR_REQ MDB, SIR_DATA_LENGTH, and SIR_TGT_INDEX
 * before the request is triggered. Program optional payload data separately
 * with @ref sl_hal_i3c_set_target_sir_data().
 * @p payload_length is written to @c SLV_INTR_REQ.SIR_DATA_LENGTH (1–4 total
 * IBI bytes excluding MDB; e.g. 4 for 4 payload bytes). SIR_CTRL is
 * always left as 2'b00 (SLV_SIR_DATA source).
 *
 * Call @ref sl_hal_i3c_trigger_slave_interrupt_request() separately after this
 * function succeeds.
 *
 * @param[in] i3c                   A pointer to the I3C peripheral register block.
 * @param[in] mdb                   Mandatory Data Byte.
 * @param[in] payload_length        Total SIR data bytes excluding MDB (1–4).
 * @param[in] virtual_target_index  Virtual target index (0–3).
 *
 * @return            On success returns SL_STATUS_OK.
 *                    On failure returns SL_STATUS_NOT_SUPPORTED,
 *                                       SL_STATUS_INVALID_PARAMETER,
 *                                       SL_STATUS_BUSY.
 ******************************************************************************/
sl_status_t sl_hal_i3c_configure_sir(I3C_TypeDef *i3c,
                                     uint8_t mdb,
                                     uint8_t payload_length,
                                     uint8_t virtual_target_index);

/***************************************************************************//**
 * Configures SCL open-drain timing for I3C transfers.
 *
 * @note   SCL frequency calculation: SCL_OD_freq = core_clk / (high_count + low_count).
 *
 * @param[in] i3c        A pointer to the I3C peripheral register block.
 *
 * @param[in] high_count I3C Open Drain High Count (8-bit: 0-255).
 *                       Number of core_clk periods SCL remains high.
 *
 * @param[in] low_count  I3C Open Drain Low Count (8-bit: 0-255).
 *                       Number of core_clk periods SCL remains low.
 *
 * @return               On success returns SL_STATUS_OK.
 *                       On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_od_timing(I3C_TypeDef *i3c,
                                     uint8_t high_count,
                                     uint8_t low_count);

/***************************************************************************//**
 * Configures SCL push-pull timing for I3C transfers.
 *
 * @note   SCL frequency calculation: SCL_PP_freq = core_clk / (high_count + low_count).
 *
 * @param[in] i3c        A pointer to the I3C peripheral register block.
 *
 * @param[in] high_count I3C Push Pull High Count (8-bit: 0-255).
 *                       Number of core_clk periods SCL remains high.
 *
 * @param[in] low_count  I3C Push Pull Low Count (8-bit: 0-255).
 *                       Number of core_clk periods SCL remains low.
 *
 * @return               On success returns SL_STATUS_OK.
 *                       On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_pp_timing(I3C_TypeDef *i3c,
                                     uint8_t high_count,
                                     uint8_t low_count);

/***************************************************************************//**
 * Configures SCL timing for I2C fast mode transfers.
 *
 * @note   SCL frequency calculation: SCL_FM_freq = core_clk / (high_count + low_count).
 *
 * @param[in] i3c        A pointer to the I3C peripheral register block.
 *
 * @param[in] high_count I2C Fast Mode High Count (16-bit: 0-65535).
 *                       Number of core_clk periods SCL remains high.
 *
 * @param[in] low_count  I2C Fast Mode Low Count (16-bit: 0-65535).
 *                       Number of core_clk periods SCL remains low.
 *
 * @return               On success returns SL_STATUS_OK.
 *                       On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_i2c_fm_timing(I3C_TypeDef *i3c,
                                         uint16_t high_count,
                                         uint16_t low_count);

/***************************************************************************//**
 * Configures SCL timing for I2C fast mode plus transfers.
 *
 * @note   SCL frequency calculation: SCL_FMP_freq = core_clk / (high_count + low_count).
 *
 * @param[in] i3c        A pointer to the I3C peripheral register block.
 *
 * @param[in] high_count I2C Fast Mode Plus High Count (8-bit: 0-255).
 *                       Number of core_clk periods SCL remains high.
 *
 * @param[in] low_count  I2C Fast Mode Plus Low Count (16-bit: 0-65535).
 *                       Number of core_clk periods SCL remains low.
 *
 * @return               On success returns SL_STATUS_OK.
 *                       On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_i2c_fmp_timing(I3C_TypeDef *i3c,
                                          uint8_t high_count,
                                          uint16_t low_count);

/***************************************************************************//**
 * Sets the idle count multiplier for I3C device in target mode.
 *
 * @param[in] i3c        A pointer to the I3C peripheral register block.
 *
 * @param[in] multiplier Idle count multiplier value.
 *
 * @return               On success returns SL_STATUS_OK.
 *                       On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                          SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_idle_count_multiplier(I3C_TypeDef *i3c,
                                                 sl_hal_i3c_idle_cnt_multiplier_t multiplier);

/**************************************************************************//**
 * Sets the Bus Available Time for the I3C device in target mode.
 *
 * @param[in] i3c                 A pointer to the I3C peripheral register block.
 *
 * @param[in] bus_available_time  Bus Available Time value to be programmed.
 *
 * @return                        On success returns SL_STATUS_OK.
 *                                On failure returns SL_STATUS_NOT_SUPPORTED.
 *****************************************************************************/
sl_status_t sl_hal_i3c_set_bus_available_time(I3C_TypeDef *i3c,
                                              uint16_t bus_available_time);

/**************************************************************************//**
 * Sets the Bus Free Time for the I3C device in controller mode.
 *
 * @param[in] i3c            A pointer to the I3C peripheral register block.
 *
 * @param[in] bus_free_time  Bus Free Time value to be programmed.
 *
 * @return                   On success returns SL_STATUS_OK.
 *                           On failure returns SL_STATUS_NOT_SUPPORTED.
 *****************************************************************************/
sl_status_t sl_hal_i3c_set_bus_free_time(I3C_TypeDef *i3c,
                                         uint16_t bus_free_time);

/***************************************************************************//**
 * Set bus idle time for the I3C target device.
 *
 * @param[in] i3c           A pointer to the I3C peripheral register block.
 *
 * @param[in] bus_idle_time Bus idle time value in cycles.
 *
 * @return                  On success returns SL_STATUS_OK.
 *                          On failure returns SL_STATUS_NOT_SUPPORTED,
 *                                             SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_bus_idle_time(I3C_TypeDef *i3c,
                                         uint32_t bus_idle_time);

/***************************************************************************//**
 * Get bus idle time for the I3C target device.
 *
 * @param[in] i3c            A pointer to the I3C peripheral register block.
 *
 * @param[out] bus_idle_time A Pointer to store the bus idle time value in cycles.
 *
 * @return                   On success returns SL_STATUS_OK.
 *                           On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                              SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_bus_idle_time(I3C_TypeDef *i3c,
                                         uint32_t *bus_idle_time);

/***************************************************************************//**
 * Set release SDA timing for I3C device in target mode.
 *
 * @note   This function is used in target mode to release the SDA line if SCL is not
 *         changing for a prescribed amount of time programmed in this register.
 *         The time is calculated as RELEASE_SDA_TIME * (core clock/pclk period).
 *
 * @param[in] i3c              A pointer to the I3C peripheral register block.
 *
 * @param[in] release_sda_time Release SDA count value.
 *
 * @return                      On success returns SL_STATUS_OK.
 *                              On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                 SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_release_sda_timing(I3C_TypeDef *i3c,
                                              uint32_t release_sda_time);

/***************************************************************************//**
 * Set subscriber ID of the I3C Device.
 *
 * @param[in] i3c           A pointer to the I3C peripheral register block.
 *
 * @param[in] subscriber_id Subscriber ID to set.
 *
 * @return                  On success returns SL_STATUS_OK.
 *                          On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                             SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_subscriber_id(I3C_TypeDef *i3c,
                                         uint8_t subscriber_id);

/***************************************************************************//**
 * Get subscriber ID of the I3C Device.
 *
 * @param[in] i3c            A pointer to the I3C peripheral register block.
 *
 * @param[out] subscriber_id A Pointer to store the subscriber ID.
 *
 * @return                   On success returns SL_STATUS_OK.
 *                           On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                              SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_subscriber_id(I3C_TypeDef *i3c,
                                         uint8_t *subscriber_id);

/***************************************************************************//**
 * Program one Subscriber-ID map table entry (controller D2D).
 *
 * @param[in] i3c               A pointer to the I3C peripheral register block.
 * @param[in] map_index         Slot 0..SL_HAL_I3C_MAX_SUBSCRIBER_MAP_COUNT-1.
 * @param[in] subscriber_id     Subscriber ID to set.
 * @param[in] dynamic_addr_7bit Dynamic address to set.
 *
 * @return                      On success returns SL_STATUS_OK.
 *                              On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                 SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_subscriber_map_entry(I3C_TypeDef *i3c,
                                                uint8_t map_index,
                                                uint8_t subscriber_id,
                                                uint8_t dynamic_addr_7bit);

/***************************************************************************//**
 * Read one Subscriber-ID map table entry.
 *
 * @param[in] i3c               A pointer to the I3C peripheral register block.
 * @param[in] map_index         Slot 0..SL_HAL_I3C_MAX_SUBSCRIBER_MAP_COUNT-1.
 * @param[out] subscriber_id    Pointer to store the subscriber ID.
 * @param[out] dynamic_addr_7bit Pointer to store the dynamic address.
 *
 * @return                      On success returns SL_STATUS_OK.
 *                              On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                 SL_STATUS_NULL_POINTER.
 *                                                 SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_subscriber_map_entry(I3C_TypeDef *i3c,
                                                uint8_t map_index,
                                                uint8_t *subscriber_id,
                                                uint8_t *dynamic_addr_7bit);

/***************************************************************************//**
 * Write transmit data to extended TX data port in target mode.
 *
 * @note   This function is used to write transmit data to the extended TX data port
 *         in target mode. Each extended queue has its own TX data port.
 *
 * @param[in] i3c              A Pointer to the I3C peripheral base address.
 * @param[in] ext_reg_index    Extended register index (0-3).
 * @param[in] data             Data word to write (32-bit).
 *
 * @return              On success returns SL_STATUS_OK.
 *                      On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                         SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_ext_tx_data(I3C_TypeDef *i3c,
                                       uint8_t ext_reg_index,
                                       uint32_t data);

/***************************************************************************//**
 * Program extended command register in target mode.
 *
 * This function is used to push commands which enables the Target to respond
 * with data for a private/vendor-CCC read command from the Controller.
 *
 * @param[in] i3c                A Pointer to the I3C peripheral base address.
 * @param[in] ext_reg_index      Extended register index (0-3).
 * @param[in] command_value      Combined command value (32-bit).
 *
 * @return                   On success returns SL_STATUS_OK.
 *                           On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                              SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_ext_cmd_reg(I3C_TypeDef *i3c,
                                       uint8_t ext_reg_index,
                                       uint32_t command_value);

/***************************************************************************//**
 * Program extended command register (Word 2) in target mode.
 *
 * @note   This register must be written with the appropriate value before writing
 *         to EXT_CMD_REG_x (x = 0-3).
 *
 * @param[in] i3c                 A Pointer to the I3C peripheral base address.
 * @param[in] ext_reg_index       Extended register index (0-3).
 * @param[in] command_value       Combined command value (32-bit).
 *
 * @return                    On success returns SL_STATUS_OK.
 *                           On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                              SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_ext_cmd_reg_2(I3C_TypeDef *i3c,
                                         uint8_t ext_reg_index,
                                         uint32_t command_value);

/***************************************************************************//**
 * Get extended data buffer status level in target mode.
 *
 * @param[in]  i3c            A Pointer to the I3C peripheral base address.
 * @param[in]  ext_reg_index  Extended register index (0-3).
 * @param[out] level          A Pointer to store the buffer level (number of entries).
 *
 * @return                  On success returns SL_STATUS_OK.
 *                          On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                             SL_STATUS_NULL_POINTER.
 *                                             SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_ext_data_buf_status_level(I3C_TypeDef *i3c,
                                                     uint8_t ext_reg_index,
                                                     uint8_t *level);

/***************************************************************************//**
 * This function is used to reset (flush) the extended TX buffer in target mode.
 *
 * @param[in] i3c              A Pointer to the I3C peripheral base address.
 * @param[in] ext_reg_index    Extended register index (0-3).
 *
 * @return                 On success returns SL_STATUS_OK.
 *                         On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                            SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_reset_ext_tx_queue(I3C_TypeDef *i3c,
                                          uint8_t ext_reg_index);

/***************************************************************************//**
 * Get extended command TX buffer threshold status in target mode.
 *
 * @note   This status indicates when the number of empty locations in the
 *         extended TX buffer is greater than or equal to the threshold value
 *         specified in EXT_TX_EMPTY_RX_RSP_THLD_CTRL register.
 *         The status is cleared automatically when empty locations fall
 *         below the threshold.
 *
 * @param[in]  i3c           A Pointer to the I3C peripheral base address.
 * @param[out] status        A Pointer to store threshold status bitmask.
 *
 * @return                   On success returns SL_STATUS_OK.
 *                           On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                              SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_ext_cmd_tx_thld_status(I3C_TypeDef *i3c,
                                                  uint8_t *status);

/***************************************************************************//**
 * This function is used to get the extended command valid status
 *
 * @note  This register is valid only with Extended Transmit Command
 *        Data Structure Format 2.
 *
 * @param[in]  i3c           A Pointer to the I3C peripheral base address.
 * @param[out] valid_status  A Pointer to store the extended command valid status.
 *
 * @return                   On success returns SL_STATUS_OK.
 *                           On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                              SL_STATUS_NULL_POINTER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_ext_cmd_valid_status(I3C_TypeDef *i3c,
                                                uint8_t *valid_status);

/***************************************************************************//**
 * Check whether Format-2 Extended Command register CMD_VLD is still set.
 *
 * @note   Unlike @ref sl_hal_i3c_get_ext_cmd_valid_status, this reads the
 *         command register itself (does not consume EXT_CMD_VLD_STAT).
 *
 * @param[in]  i3c            I3C peripheral base address.
 * @param[in]  ext_reg_index  Extended command index (0-3).
 * @param[out] is_valid       True if CMD_VLD is still set (transfer not done).
 *
 * @return SL_STATUS_OK on success.
 ******************************************************************************/
sl_status_t sl_hal_i3c_ext_cmd_is_valid(I3C_TypeDef *i3c,
                                        uint8_t ext_reg_index,
                                        bool *is_valid);

/***************************************************************************//**
 * Set extended command response threshold control in target mode.
 *
 * @note   This register is valid only with Extended Transmit Command
 *        Data Structure Format 2.
 *
 * @param[in] i3c   A Pointer to the I3C peripheral base address.
 * @param[in] thld  A Pointer to the threshold configuration structure.
 *
 * @return          On success returns SL_STATUS_OK.
 *                  On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                      SL_STATUS_NULL_POINTER.
 *                                     SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_ext_cmd_rsp_threshold(I3C_TypeDef *i3c,
                                                 const sl_hal_i3c_ext_cmd_rsp_thld_t *thld);

/***************************************************************************//**
 * Enable a virtual target in target mode.
 *
 * @param[in] i3c             A Pointer to the I3C peripheral base address.
 * @param[in] virt_tgt_index  Virtual target index (1-4).
 *
 * @return                    On success returns SL_STATUS_OK.
 *                            On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                               SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_enable(I3C_TypeDef *i3c,
                                             uint8_t virt_tgt_index);

/***************************************************************************//**
 * Disable a virtual target in target mode.
 *
 * @param[in] i3c             A Pointer to the I3C peripheral base address.
 * @param[in] virt_tgt_index  Virtual target index (1-4).
 *
 * @return                    On success returns SL_STATUS_OK.
 *                            On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                               SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_disable(I3C_TypeDef *i3c,
                                              uint8_t virt_tgt_index);

/***************************************************************************//**
 * Set static address for a virtual target.
 *
 * @param[in] i3c             A Pointer to the I3C peripheral base address.
 * @param[in] virt_tgt_index  Virtual target index (1-4).
 * @param[in] static_addr     Static address (7-bit, 0x00-0x7F).
 *
 * @return                    On success returns SL_STATUS_OK.
 *                            On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                               SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_set_static_addr(I3C_TypeDef *i3c,
                                                      uint8_t virt_tgt_index,
                                                      uint8_t static_addr);

/***************************************************************************//**
 * Get dynamic address of a virtual target.
 *
 * @param[in]  i3c             A Pointer to the I3C peripheral base address.
 * @param[in]  virt_tgt_index  Virtual target index (1-4).
 * @param[out] dynamic_addr    Dynamic address (7-bit).
 *
 * @return                     On success returns SL_STATUS_OK.
 *                             On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                SL_STATUS_NULL_POINTER.
 *                                                SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_get_dynamic_addr(I3C_TypeDef *i3c,
                                                       uint8_t virt_tgt_index,
                                                       uint8_t *dynamic_addr);

/***************************************************************************//**
 * Set PID value for a virtual target.
 *
 * @param[in] i3c             A Pointer to the I3C peripheral base address.
 * @param[in] virt_tgt_index  Virtual target index (1-4).
 * @param[in] pid_value       PID register value (32-bit).
 *
 * @return                    On success returns SL_STATUS_OK.
 *                            On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                               SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_set_pid(I3C_TypeDef *i3c,
                                              uint8_t virt_tgt_index,
                                              uint32_t pid_value);

/***************************************************************************//**
 * Set MIPI ID value for a virtual target.
 *
 * @param[in] i3c             A Pointer to the I3C peripheral base address.
 * @param[in] virt_tgt_index  Virtual target index (1-4).
 * @param[in] mipi_mfg_id     MIPI Manufacturer ID (15 bits, PID[47:33]).
 * @param[in] prov_id_sel     Provisional ID selector (1 = random, 0 = vendor fixed).
 *
 * @return                    On success returns SL_STATUS_OK.
 *                            On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                               SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_set_mipi_id(I3C_TypeDef *i3c,
                                                  uint8_t virt_tgt_index,
                                                  uint16_t mipi_mfg_id,
                                                  bool prov_id_sel);

/***************************************************************************//**
 * Set DCR (Device Characteristic Register) for a virtual target.
 *
 * @param[in] i3c             A Pointer to the I3C peripheral base address.
 * @param[in] virt_tgt_index  Virtual target index (1-4).
 * @param[in] dcr             DCR value (8 bits).
 *
 * @return                    On success returns SL_STATUS_OK.
 *                            On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                               SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_set_dcr(I3C_TypeDef *i3c,
                                              uint8_t virt_tgt_index,
                                              uint8_t dcr);

/***************************************************************************//**
 * Get control register pointer for a virtual target.
 *
 * @note   This function returns a pointer to the specified virtual target's
 *         control register.
 *
 * @param[in]  i3c             A Pointer to the I3C peripheral base address.
 * @param[in]  virt_tgt_index  Virtual target index (1-4).
 * @param[out] ctrl_reg        A Pointer to store the register address.
 *
 * @return                     On success returns SL_STATUS_OK.
 *                             On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                SL_STATUS_NULL_POINTER.
 *                                                SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_get_ctrl_register(I3C_TypeDef *i3c,
                                                        uint8_t virt_tgt_index,
                                                        volatile uint32_t **ctrl_reg);

/***************************************************************************//**
 * Clear dynamic address and VALID bit for a virtual target.
 *
 * @param[in] i3c             A pointer to the I3C peripheral base address.
 * @param[in] virt_tgt_index  Virtual target index (1-4).
 *
 * @return On success returns SL_STATUS_OK.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_clear_dynamic_addr(I3C_TypeDef *i3c,
                                                         uint8_t virt_tgt_index);

/***************************************************************************//**
 * Get characteristics control register pointer for a virtual target.
 *
 * @note   This function returns a pointer to the specified virtual target's
 *         characteristics control register.
 *
 * @param[in]  i3c             A Pointer to the I3C peripheral base address.
 * @param[in]  virt_tgt_index  Virtual target index (1-4).
 * @param[out] char_ctrl_reg   A Pointer to store the register address.
 *
 * @return                     On success returns SL_STATUS_OK.
 *                             On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                SL_STATUS_NULL_POINTER.
 *                                                SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_virtual_target_get_char_ctrl_register(I3C_TypeDef *i3c,
                                                             uint8_t virt_tgt_index,
                                                             volatile uint32_t **char_ctrl_reg);

/***************************************************************************//**
 * Get group address status register pointer.
 *
 * @note   This function returns a pointer to the specified group address
 *         status register (GRP_ADDR_n_STAT).
 *
 * @param[in]  i3c             A Pointer to the I3C peripheral base address.
 * @param[in]  grp_addr_index  Group address index (0-3).
 * @param[out] stat_reg        A Pointer to store the register address.
 *
 * @return                     On success returns SL_STATUS_OK.
 *                             On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                SL_STATUS_NULL_POINTER.
 *                                                SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_group_address_stat_register(I3C_TypeDef *i3c,
                                                       uint8_t grp_addr_index,
                                                       volatile uint32_t **stat_reg);

/***************************************************************************//**
 * Get group address and target valid mask in target mode.
 *
 * @note   Use this in Target mode to read the group address assigned by the
 *         controller via SETGRPA CCC.
 *
 * @param[in]  i3c               A Pointer to the I3C peripheral base address.
 * @param[in]  grp_addr_index    Group address index (0-3).
 * @param[out] grp_addr          A Pointer to store the 7-bit group address.
 * @param[out] target_valid_mask A Pointer to store target valid bitmask.
 *
 * @return                       On success returns SL_STATUS_OK.
 *                               On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                  SL_STATUS_NULL_POINTER.
 *                                                  SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_get_group_address(I3C_TypeDef *i3c,
                                         uint8_t grp_addr_index,
                                         uint8_t *grp_addr,
                                         uint8_t *target_valid_mask);

/***************************************************************************//**
 * Set group address for a specified group address index (secondary controller mode).
 *
 * @note   Only for use in Secondary Controller mode; register is read-only
 *         in Target only mode and programmable in Secondary Controller mode.
 *
 * @param[in] i3c               A Pointer to the I3C peripheral base address.
 * @param[in] grp_addr_index    Group address index (0-3).
 * @param[in] grp_addr          7-bit group address (0x00-0x7F).
 *
 * @return                      On success returns SL_STATUS_OK.
 *                              On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                 SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_group_address(I3C_TypeDef *i3c,
                                         uint8_t grp_addr_index,
                                         uint8_t grp_addr);

/***************************************************************************//**
 * Enable a target for a group address (secondary controller mode).
 *
 * @note   Only for use in Secondary Controller mode; register is read-only
 *         in Target only mode and programmable in Secondary Controller mode.
 *
 * @param[in] i3c                 A Pointer to the I3C peripheral base address.
 * @param[in] grp_addr_index      Group address index (0-3).
 * @param[in] virt_tgt_index      Target index to enable:
 *                                 - 0 = Target-0 Primary
 *                                 - 1 = Virtual Target-1
 *                                 - 2 = Virtual Target-2
 *                                 - 3 = Virtual Target-3
 *                                 - 4 = Virtual Target-4
 *
 * @return                        On success returns SL_STATUS_OK.
 *                                On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                   SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_group_address_enable_target(I3C_TypeDef *i3c,
                                                   uint8_t grp_addr_index,
                                                   uint8_t virt_tgt_index);

/***************************************************************************//**
 * Disable a target for a group address (secondary controller mode).
 *
 * @note   Only for use in Secondary Controller mode; register is read-only
 *         in Target mode and programmable in Secondary Controller mode.
 *
 * @param[in] i3c                 Pointer to the I3C peripheral base address.
 * @param[in] grp_addr_index      Group address index (0-3).
 * @param[in] virt_tgt_index      Target index to disable:
 *                                 - 0 = Target-0 Primary
 *                                 - 1 = Virtual Target-1
 *                                 - 2 = Virtual Target-2
 *                                 - 3 = Virtual Target-3
 *                                 - 4 = Virtual Target-4
 *
 * @return                        On success returns SL_STATUS_OK.
 *                                On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                   SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_group_address_disable_target(I3C_TypeDef *i3c,
                                                    uint8_t grp_addr_index,
                                                    uint8_t virt_tgt_index);

/***************************************************************************//**
 * Enables the I3C Peripheral.
 *
 * @note   This function blocks until enable completes. Enable completion depends
 *         on SCL availability. In Target mode after POR, the bus interface responds
 *         only after observing Bus Available condition for BUS_AVAILABLE_TIME *
 *         IDLE_CNT_MULTIPLIER PCLK cycles.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_enable(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->DEVICE_CTRL |= I3C_DEVICE_CTRL_ENABLE;

  // Wait until enable is complete.
  while ((i3c->DEVICE_CTRL & I3C_DEVICE_CTRL_ENABLE) != I3C_DEVICE_CTRL_ENABLE) {
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Disables the I3C Peripheral.
 *
 * @note   This function blocks until disable completes. In Controller mode, disable
 *         occurs after completing the transfer command with TOC=1 or IBI reception.
 *         Flush/drain all queues and FIFOs before re-enabling. In Target mode,
 *         disable occurs after completing ongoing transfers. Do not disable Target
 *         when SIR/MR request status is pending.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_disable(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Make sure module is enabled.
  if ((i3c->DEVICE_CTRL & I3C_DEVICE_CTRL_ENABLE) != I3C_DEVICE_CTRL_ENABLE) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  // Disable the I3C controller.
  i3c->DEVICE_CTRL &= ~I3C_DEVICE_CTRL_ENABLE;
  // Wait until disable is complete.
  while ((i3c->DEVICE_CTRL & I3C_DEVICE_CTRL_ENABLE) != 0) {
  }
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable adaptive I2C/I3C mode for I3C device in target mode.
 *
 * This should be set when the slave application does not know which bus
 * (I2C or I3C) the device is connected to.
 *
 * @note
 * The ADAPTIVE_I2C_I3C bit can be SET by software, but it can only be
 * CLEARED automatically by hardware when the controller determines the bus
 * mode is I3C (i.e., when I3C bus activity is detected). Software cannot
 * clear this bit directly.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_enable_adaptive_mode(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->DEVICE_CTRL |= I3C_DEVICE_CTRL_ADAPTIVE_I2C_I3C;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable DMA handshake interface for I3C device.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_enable_dma(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Set the DMA_ENABLE bit in the DEVICE_CTRL register.
  i3c->DEVICE_CTRL |=  I3C_DEVICE_CTRL_DMA_ENABLE;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Disable DMA handshake interface for I3C device.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_disable_dma(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Clear the DMA_ENABLE bit in the DEVICE_CTRL register.
  i3c->DEVICE_CTRL &= ~I3C_DEVICE_CTRL_DMA_ENABLE;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Check if DMA is enabled for I3C device.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[out] enabled A pointer to store DMA enable status.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_is_dma_enabled(I3C_TypeDef *i3c,
                                               bool *enabled)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (enabled == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Check the DMA_ENABLE bit in the DEVICE_CTRL register.
  *enabled = (i3c->DEVICE_CTRL & I3C_DEVICE_CTRL_DMA_ENABLE) != 0;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable or disable Hot-Join capability in controller mode.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[in] enable   true to enable Hot-Join, false to disable.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_controller_configure_hot_join(I3C_TypeDef *i3c,
                                                              bool enable)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  if (enable) {
    i3c->DEVICE_CTRL &= ~I3C_DEVICE_CTRL_HOT_JOIN_CTRL;
  } else {
    i3c->DEVICE_CTRL |= I3C_DEVICE_CTRL_HOT_JOIN_CTRL;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Writes a formatted command value to the I3C command queue.
 *
 * @param[in] i3c            A pointer to the I3C peripheral register block.
 *
 * @param[in] command_value  The command value to write to COMMAND_QUEUE_PORT.
 *
 * @return                   On success returns SL_STATUS_OK.
 *                           On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_set_command_queue(I3C_TypeDef *i3c,
                                                  uint32_t command_value)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->COMMAND_QUEUE_PORT = command_value;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Retrieves the status of the transfer command.
 *
 * @note   This function reads the response queue to retrieve the status of a previously
 *         issued transfer command.
 *
 * @param[in] i3c         A pointer to the I3C peripheral register block.
 *
 * @param[out] response   A Pointer to store command response.
 *
 * @return                On success returns SL_STATUS_OK.
 *                        On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_command_queue_response(I3C_TypeDef *i3c,
                                                           uint32_t *response)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (response == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  // Read the response from the RESPONSE_QUEUE_PORT register.
  *response = i3c->RESPONSE_QUEUE_PORT;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get IBI response from queue port in controller mode.
 *
 * @param[in] i3c       A pointer to the I3C peripheral register block.
 *
 * @param[out] ibi_resp A pointer to store IBI response data.
 *
 * @return              On success returns SL_STATUS_OK.
 *                      On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                         SL_STATUS_NULL_POINTER.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_controller_get_ibi_response(I3C_TypeDef *i3c,
                                                            uint32_t *ibi_resp)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  if (ibi_resp == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  // Read the IBI response from the IBI_QUEUE_PORT.
  *ibi_resp = i3c->IBI_QUEUE_PORT;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Reads the current queue thresholds for command empty buffer, response buffer,
 * IBI data buffer, and IBI status buffer.
 *
 * @note   The returned register value (QUEUE_THLD_CTRL) has the following bit layout:
 *         - Bits [7:0]   : CMD_EMPTY_BUF_THLD - Command empty buffer threshold.
 *         - Bits [15:8]  : RESP_BUF_THLD      - Response buffer threshold.
 *         - Bits [23:16] : IBI_DATA_THLD      - IBI data threshold.
 *         - Bits [31:24] : IBI_STATUS_THLD    - IBI status threshold.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[out] reg_val A pointer to store the QUEUE_THLD_CTRL register value.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                        SL_STATUS_NULL_POINTER.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_queue_threshold(I3C_TypeDef *i3c,
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

  *reg_val = i3c->QUEUE_THLD_CTRL;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get current FIFO threshold settings.
 *
 * @note   The returned register value (DATA_BUFFER_THLD_CTRL) has the following bit layout:
 *         - Bits [2:0]   : TX_EMPTY_BUF_THLD - TX empty buffer threshold.
 *         - Bits [10:8]  : RX_BUF_THLD       - RX buffer threshold.
 *         - Bits [18:16] : TX_START_THLD      - TX start threshold.
 *         - Bits [26:24] : RX_START_THLD      - RX start threshold.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[out] reg_val A pointer to store the DATA_BUFFER_THLD_CTRL register value.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                        SL_STATUS_NULL_POINTER.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_fifo_threshold(I3C_TypeDef *i3c,
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

  *reg_val = i3c->DATA_BUFFER_THLD_CTRL;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configure In-Band Interrupt reject notifications in controller mode.
 *
 * @note   When enabled, the controller generates an IBI response for NACK'd
 *         IBIs, notifying the application of rejected requests.
 *         The notify_flags looks like this:
 *         - Bit [0]: NOTIFY_HJ_REJECTED  - Hot-Join rejected notification.
 *         - Bit [1]: NOTIFY_MR_REJECTED  - Master Request rejected notification.
 *         - Bit [3]: NOTIFY_SIR_REJECTED - SIR rejected notification.
 *
 * @param[in] i3c          A pointer to the I3C peripheral register block.
 *
 * @param[in] notify_flags Bitmask of IBI rejection notifications to enable.
 *
 * @return                 On success returns SL_STATUS_OK.
 *                         On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_ibi_reject_notify(I3C_TypeDef *i3c,
                                                  uint8_t notify_flags)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->IBI_QUEUE_CTRL = notify_flags
                        & (I3C_IBI_QUEUE_CTRL_NOTIFY_HJ_REJECTED
                           | I3C_IBI_QUEUE_CTRL_NOTIFY_MR_REJECTED
                           | I3C_IBI_QUEUE_CTRL_NOTIFY_SIR_REJECTED);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Perform soft reset of I3C device.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_soft_reset(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Set the SOFT_RESET bit in the RESET_CTRL register.
  i3c->RESET_CTRL |= I3C_RESET_CTRL_SOFT_RST;
  // Wait for the reset to complete.
  while ((i3c->RESET_CTRL & I3C_RESET_CTRL_SOFT_RST) != 0U) {
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Reset one or all I3C queues / FIFOs (maps @ref sl_hal_i3c_buffer_reset_t).
 *
 * @param[in] i3c          A pointer to the I3C peripheral register block.
 * @param[in] buffer_type  Queue/FIFO reset scope.
 *
 * @return On success returns SL_STATUS_OK.
 *         On failure returns SL_STATUS_NOT_SUPPORTED or SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_buffer_reset(I3C_TypeDef *i3c,
                                    sl_hal_i3c_buffer_reset_t buffer_type);

/***************************************************************************//**
 * Issue an I3C bus reset pattern (maps @ref sl_hal_i3c_bus_reset_type_t).
 *
 * @param[in] i3c   A pointer to the I3C peripheral register block.
 * @param[in] type  Bus reset pattern type.
 *
 * @return On success returns SL_STATUS_OK.
 *         On failure returns SL_STATUS_NOT_SUPPORTED or SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
sl_status_t sl_hal_i3c_set_bus_reset_type(I3C_TypeDef *i3c,
                                          sl_hal_i3c_bus_reset_type_t type);

/***************************************************************************//**
 * Reset I3C queues and buffers of the I3C device.
 *
 * @note   Low-level RESET_CTRL write. Prefer @ref sl_hal_i3c_buffer_reset /
 *         @ref sl_hal_i3c_bus_reset / @ref sl_hal_i3c_soft_reset from drivers.
 *
 * @note   This function writes a reset mask to the RESET_CTRL register to trigger various reset
 *         operations including bus reset, queue resets, FIFO resets, and core reset.
 *
 * @param[in] i3c         A pointer to the I3C peripheral register block.
 *
 * @param[in] reset_mask  Reset mask to write to the register.
 *
 * @return                On success returns SL_STATUS_OK.
 *                        On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_set_reset_control(I3C_TypeDef *i3c,
                                                  uint32_t reset_mask)
{
  // Self-clearing queue/FIFO/soft reset bits (not BUS_RESET / BUS_RESET_TYPE).
  const uint32_t wait_bits = (I3C_RESET_CTRL_SOFT_RST
                              | I3C_RESET_CTRL_CMD_QUEUE_RST
                              | I3C_RESET_CTRL_RESP_QUEUE_RST
                              | I3C_RESET_CTRL_TX_FIFO_RST
                              | I3C_RESET_CTRL_RX_FIFO_RST
                              | I3C_RESET_CTRL_IBI_QUEUE_RST);
  uint32_t wait_mask;

  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->RESET_CTRL |= reset_mask;

  wait_mask = reset_mask & wait_bits;
  while ((i3c->RESET_CTRL & wait_mask) != 0U) {
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get current reset control register value of the I3C device.
 *
 * @note   The returned register value (RESET_CTRL) has the following bit layout:
 *         - Bit  [0]     : SOFT_RST       - Core soft reset.
 *         - Bit  [1]     : CMD_QUEUE_RST  - Command queue reset.
 *         - Bit  [2]     : RESP_QUEUE_RST - Response queue reset.
 *         - Bit  [3]     : TX_FIFO_RST    - TX FIFO reset.
 *         - Bit  [4]     : RX_FIFO_RST    - RX FIFO reset.
 *         - Bit  [5]     : IBI_QUEUE_RST  - IBI queue reset.
 *         - Bits [30:28] : BUS_RESET_TYPE - Bus reset type.
 *         - Bit  [31]    : BUS_RESET      - Bus reset.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[out] reg_val A pointer to store the RESET_CTRL register value.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                        SL_STATUS_NULL_POINTER.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_reset_control(I3C_TypeDef *i3c,
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

  *reg_val = i3c->RESET_CTRL;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable or disable Hot-Join IBI in target mode.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[in] enable   true to enable Hot-Join IBI, false to disable.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_target_set_hot_join_ibi(I3C_TypeDef *i3c,
                                                        bool enable)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // HJ_EN (bit 3) enables Hot-Join IBI.
  if (enable) {
    i3c->SLV_EVENT_STATUS |= I3C_SLV_EVENT_STATUS_HJ_EN;
  } else {
    i3c->SLV_EVENT_STATUS &= ~I3C_SLV_EVENT_STATUS_HJ_EN;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Clears the specified interrupt status flags.
 *
 * @param[in] i3c              A pointer to the I3C peripheral register block.
 *
 * @param[in] interrupt_mask   Interrupt status mask to clear.
 *
 * @return                     On success returns SL_STATUS_OK.
 *                             On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_clear_interrupt_status(I3C_TypeDef *i3c,
                                                       uint32_t interrupt_mask)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // INTR_STATUS RW bits are W1C (Write 1 to Clear).
  i3c->INTR_STATUS = interrupt_mask;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get the pending I3C interrupts.
 *
 * @param[in] i3c           A pointer to the I3C peripheral register block.
 *
 * @param[out] pending_intr A pointer to store pending interrupt flags.
 *
 * @return                  On success returns SL_STATUS_OK.
 *                          On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                             SL_STATUS_NULL_POINTER.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_pending_interrupts(I3C_TypeDef *i3c,
                                                       uint32_t *pending_intr)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (pending_intr == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *pending_intr = i3c->INTR_STATUS & _I3C_INTR_STATUS_MASK;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enables interrupt status generation for specific signals.
 *
 * @param[in] i3c            A pointer to the I3C peripheral register block.
 *
 * @param[in] interrupt_mask Interrupt status mask to enable.
 *
 * @return                   On success returns SL_STATUS_OK.
 *                           On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_enable_interrupt(I3C_TypeDef *i3c,
                                                 uint32_t interrupt_mask)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->INTR_STATUS_EN |= interrupt_mask;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Disable interrupt status generation.
 *
 * @param[in] i3c            A pointer to the I3C peripheral register block.
 *
 * @param[in] interrupt_mask Interrupt status mask to disable.
 *
 * @return                   On success returns SL_STATUS_OK.
 *                           On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_disable_interrupt(I3C_TypeDef *i3c,
                                                  uint32_t interrupt_mask)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->INTR_STATUS_EN &= ~interrupt_mask;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get the currently enabled I3C interrupt status.
 *
 * @param[in] i3c           A pointer to the I3C peripheral register block.
 *
 * @param[out] enabled_intr A pointer to store enabled interrupt flags.
 *
 * @return                  On success returns SL_STATUS_OK.
 *                          On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                             SL_STATUS_NULL_POINTER.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_enabled_interrupts(I3C_TypeDef *i3c,
                                                       uint32_t *enabled_intr)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (enabled_intr == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *enabled_intr = i3c->INTR_STATUS_EN & _I3C_INTR_STATUS_EN_MASK;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable interrupts for specific signals.
 *
 * @param[in] i3c              A pointer to the I3C peripheral register block.
 *
 * @param[in] interrupt_mask   Interrupt signal mask to enable.
 *
 * @return                     On success returns SL_STATUS_OK.
 *                             On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_enable_interrupt_signal(I3C_TypeDef *i3c,
                                                        uint32_t interrupt_mask)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->INTR_SIGNAL_EN |= interrupt_mask;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Disable interrupts for specific signals.
 *
 * @param[in] i3c               A pointer to the I3C peripheral register block.
 *
 * @param[in] interrupt_mask    Interrupt signal mask to disable.
 *
 * @return                      On success returns SL_STATUS_OK.
 *                              On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_disable_interrupt_signal(I3C_TypeDef *i3c,
                                                         uint32_t interrupt_mask)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->INTR_SIGNAL_EN &= ~interrupt_mask;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get the currently enabled interrupt signals.
 *
 * @param[in] i3c           A pointer to the I3C peripheral register block.
 *
 * @param[out] enabled_intr A Pointer to store enabled interrupt flags.
 *
 * @return                  On success returns SL_STATUS_OK.
 *                          On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                             SL_STATUS_NULL_POINTER.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_enabled_interrupt_signals(I3C_TypeDef *i3c,
                                                              uint32_t *enabled_intr)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (enabled_intr == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *enabled_intr = i3c->INTR_SIGNAL_EN & _I3C_INTR_SIGNAL_EN_MASK;

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Retrieves the current value of the I3C QUEUE_STATUS_LEVEL register.
 *
 * @param[in]  i3c                A pointer to the I3C peripheral register block.
 *
 * @param[out] queue_status_level A Pointer to store the QUEUE_STATUS_LEVEL register value.
 *
 * @return
 *                                On success returns SL_STATUS_OK.
 *                                On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                   SL_STATUS_NULL_POINTER.
 *******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_queue_status_level(I3C_TypeDef *i3c,
                                                       uint32_t *queue_status_level)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (queue_status_level == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *queue_status_level = (i3c->QUEUE_STATUS_LEVEL & _I3C_QUEUE_STATUS_LEVEL_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get response-queue fill level from QUEUE_STATUS_LEVEL.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_resp_buf_level(I3C_TypeDef *i3c,
                                                   uint8_t *resp_blr)
{
  uint32_t qsts;

  if (resp_blr == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (sl_hal_i3c_get_queue_status_level(i3c, &qsts) != SL_STATUS_OK) {
    return SL_STATUS_NOT_SUPPORTED;
  }
  *resp_blr = (uint8_t)((qsts & _I3C_QUEUE_STATUS_LEVEL_RESP_BUF_BLR_MASK)
                        >> _I3C_QUEUE_STATUS_LEVEL_RESP_BUF_BLR_SHIFT);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get IBI status-count and data-buffer level from QUEUE_STATUS_LEVEL.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_ibi_queue_levels(I3C_TypeDef *i3c,
                                                     uint8_t *sts_cnt,
                                                     uint8_t *data_blr)
{
  uint32_t qsts;

  if (sts_cnt == NULL || data_blr == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if (sl_hal_i3c_get_queue_status_level(i3c, &qsts) != SL_STATUS_OK) {
    return SL_STATUS_NOT_SUPPORTED;
  }
  *sts_cnt = (uint8_t)((qsts & _I3C_QUEUE_STATUS_LEVEL_IBI_STS_CNT_MASK)
                       >> _I3C_QUEUE_STATUS_LEVEL_IBI_STS_CNT_SHIFT);
  *data_blr = (uint8_t)((qsts & _I3C_QUEUE_STATUS_LEVEL_IBI_BUF_BLR_MASK)
                        >> _I3C_QUEUE_STATUS_LEVEL_IBI_BUF_BLR_SHIFT);
  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Retrieves the current value of the I3C DATA_BUFFER_STATUS_LEVEL register.
 *
 * @param[in]  i3c                A pointer to the I3C peripheral register block.
 *
 * @param[out] data_buffer_level  A Pointer to store the DATA_BUFFER_STATUS_LEVEL register value.
 *
 * @return                        On success returns SL_STATUS_OK.
 *                                On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                   SL_STATUS_NULL_POINTER.
 *******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_data_buffer_level(I3C_TypeDef *i3c,
                                                      uint32_t *data_buffer_level)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (data_buffer_level == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *data_buffer_level = (i3c->DATA_BUFFER_STATUS_LEVEL & _I3C_DATA_BUFFER_STATUS_LEVEL_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Check if I3C device in controller mode is idle.
 *
 * @param[in] i3c       A pointer to the I3C peripheral register block.
 *
 * @param[out] is_idle  A Pointer to store the idle status (true if idle).
 *
 * @return              On success returns SL_STATUS_OK.
 *                      On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                         SL_STATUS_NULL_POINTER.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_is_controller_idle(I3C_TypeDef *i3c,
                                                   bool *is_idle)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (is_idle == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Check the CONTROLLER_IDLE bit in the PRESENT_STATE register.
  *is_idle = ((i3c->PRESENT_STATE & _I3C_PRESENT_STATE_CONTROLLER_IDLE_MASK) != 0);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Check if the I3C device is current bus master.
 *
 * @param[in] i3c            A pointer to the I3C peripheral register block.
 *
 * @param[out] is_controller A Pointer to store the controller status (true if current master).
 *
 * @return                   On success returns SL_STATUS_OK.
 *                           On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                              SL_STATUS_NULL_POINTER.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_is_current_controller(I3C_TypeDef *i3c,
                                                      bool *is_controller)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (is_controller == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Check the CURRENT_MASTER bit in the PRESENT_STATE register.
  *is_controller = ((i3c->PRESENT_STATE & I3C_PRESENT_STATE_CURRENT_MASTER) != 0);

  return SL_STATUS_OK;
}

/**************************************************************************//**
 * Get CCC Device Status in I3C device in target mode.
 *
 * This register reflects the data which the Slave controller
 * sends in response to GETSTATUS CCC by the Master.
 *
 * @param[in]  i3c               A pointer to the I3C peripheral register block.
 *
 * @param[out] ccc_device_status A pointer to store the CCC_DEVICE_STATUS value.
 *
 * @return                       On success returns SL_STATUS_OK.
 *                               On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                                  SL_STATUS_NULL_POINTER.
 *******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_ccc_device_status(I3C_TypeDef *i3c,
                                                      uint32_t *ccc_device_status)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (ccc_device_status == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *ccc_device_status = (i3c->CCC_DEVICE_STATUS & _I3C_CCC_DEVICE_STATUS_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get device address register pointer.
 *
 * @note   This function returns a pointer to the specified device's address register in the
 *         Device Address Table for direct register manipulation.
 *
 * @param[in] i3c            A pointer to the I3C peripheral register block.
 *
 * @param[in] target_index   Index of target device in Device Address Table.
 *
 * @param[out] register_ptr  A Pointer to store the register address.
 *
 * @return                   On success returns SL_STATUS_OK.
 *                           On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                              SL_STATUS_NULL_POINTER.
 *                                              SL_STATUS_INVALID_PARAMETER.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_device_addr_register(I3C_TypeDef *i3c,
                                                         uint8_t target_index,
                                                         uint32_t **register_ptr)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  // Check for NULL pointer.
  if (register_ptr == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  // Validate target index range.
  if (target_index >= SL_HAL_I3C_MAX_TARGET_COUNT) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Calculate the address of the target device's register.
  *register_ptr = (uint32_t *)((uint8_t *)&i3c->DEV_ADDR_TABLE1_LOC1
                               + target_index * sizeof(uint32_t));

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Sets the Part ID, Instance ID, and DCR in target mode.
 *
 * @note   SLV_PID_VALUE register layout:
 *         - Bits [31:16]: SLV_PART_ID - Part ID of the device.
 *         - Bits [15:12]: SLV_INST_ID - Instance ID of the Slave.
 *         - Bits [11:0]: SLV_PID_DCR - Additional 12-bit ID.
 *
 * @param[in] i3c       A pointer to the I3C peripheral register block.
 *
 * @param[in] reg_value A value to write to the SLV_PID_VALUE register.
 *
 * @return              On success returns SL_STATUS_OK.
 *                      On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_set_slave_pid(I3C_TypeDef *i3c,
                                              uint32_t reg_value)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->SLV_PID_VALUE = reg_value;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Trigger a master request (MR) on the I3C bus.
 *
 * @note   This function triggers a master request (MR) on the I3C bus.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_trigger_master_request(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->SLV_INTR_REQ |= I3C_SLV_INTR_REQ_MR;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Trigger a Slave Interrupt Request (SIR) on the I3C bus.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_trigger_slave_interrupt_request(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->SLV_INTR_REQ |= I3C_SLV_INTR_REQ_SIR;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Set Target Interrupt Request (SIR) data for specific target.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @param[in] data     SIR data to set.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_set_target_sir_data(I3C_TypeDef *i3c,
                                                    uint32_t data)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->SLV_SIR_DATA = data;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get the IBI response status in target mode.
 *
 * @param[in] i3c       A pointer to the I3C peripheral register block.
 *
 * @param[out] ibi_resp A pointer to store IBI response data.
 *
 * @return              On success returns SL_STATUS_OK.
 *                      On failure returns SL_STATUS_NOT_SUPPORTED.
 *                                         SL_STATUS_NULL_POINTER.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_slave_get_ibi_response(I3C_TypeDef *i3c,
                                                       uint32_t *ibi_resp)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  if (ibi_resp == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *ibi_resp = i3c->SLV_IBI_RESP;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Get the value of the STATUS register.
 *
 * @note   This function reads the STATUS register containing I2C transfer status, IBI payload
 *         size, and I2C glitch filter enable status.
 *
 * @param[in] i3c          A pointer to the I3C peripheral register block.
 *
 * @param[out] status_reg  A Pointer to store the STATUS register value.
 *
 * @return                 On success returns SL_STATUS_OK.
 *                         On failure returns SL_STATUS_NOT_SUPPORTED,
 *                                            SL_STATUS_NULL_POINTER.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_get_status_register(I3C_TypeDef *i3c,
                                                    uint32_t *status_reg)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }
  if (status_reg == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *status_reg = i3c->STATUS;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable I2C mode for I3C device in target mode.
 *
 * @note This function enables I2C mode for the I3C device in target mode.
 *       When enabled, the slave can respond to I2C commands.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_enable_i2c_mode(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->CFG |= I3C_CFG_MODEI2C;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Disable I2C mode for I3C device in target mode.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_disable_i2c_mode(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->CFG &= ~I3C_CFG_MODEI2C;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable the I3C peripheral clock.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_enable_peripheral_clock(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Enable CMU clock by setting the CMU_CLK_EN bit in the CFG register.
  i3c->EN |= I3C_EN_CLKEN;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Disable the I3C peripheral clock.
 *
 * @param[in] i3c      A pointer to the I3C peripheral register block.
 *
 * @return             On success returns SL_STATUS_OK.
 *                     On failure returns SL_STATUS_NOT_SUPPORTED.
 ******************************************************************************/
__INLINE sl_status_t sl_hal_i3c_disable_peripheral_clock(I3C_TypeDef *i3c)
{
  // Make sure the module exists on the selected chip.
  if (!SL_HAL_I3C_REF_VALID(i3c)) {
    SL_LOG_DEBUG_ASSERT(false);
    return SL_STATUS_NOT_SUPPORTED;
  }

  i3c->EN &= ~I3C_EN_CLKEN;

  return SL_STATUS_OK;
}

/** @} (end addtogroup i3c) */

#ifdef __cplusplus
}
#endif

#endif /* defined(I3C_PRESENT) */
#endif /* #ifndef SL_HAL_I3C_H */
