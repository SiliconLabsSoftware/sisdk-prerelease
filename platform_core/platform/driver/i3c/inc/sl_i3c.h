/***************************************************************************//**
 * @file
 * @brief I3C - Improved Inter-Integrated Circuit High-Level Driver API
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

#ifndef SL_I3C_H
#define SL_I3C_H

#include <stdbool.h>
#include <stdint.h>
#include "sl_enum.h"
#include "sl_status.h"
#include "sl_hal_i3c.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************//**
 * @addtogroup i3c I3C - Improved Inter-Integrated Circuit
 * @brief I3C controller and target driver
 * @{
 ******************************************************************************/
/*******************************************************************************
 ********************************   ENUMS   ************************************
 ******************************************************************************/

/// I3C role.
SL_ENUM(sl_i3c_role_t) {
  SL_I3C_ROLE_CONTROLLER = 0,              ///< Controller role
  SL_I3C_ROLE_SLAVE = 1                    ///< Target role
};

/// Common Command Codes (CCC).\n
/// Broadcast CCCs (0x00-0x7E) are sent to all devices on the bus.\n
/// Direct CCCs (0x80-0xFF) are sent to specific devices.
SL_ENUM(sl_i3c_common_command_code_t) {
  /* Broadcast Common Command Codes */
  SL_I3C_CCC_BROADCAST_ENEC             = 0x00,   ///< Enable Slave initiated events.
  SL_I3C_CCC_BROADCAST_DISEC            = 0x01,   ///< Disable Slave initiated events.
  SL_I3C_CCC_BROADCAST_ENTAS0           = 0x02,   ///< Enter Activity State 0.
  SL_I3C_CCC_BROADCAST_ENTAS1           = 0x03,   ///< Enter Activity State 1.
  SL_I3C_CCC_BROADCAST_ENTAS2           = 0x04,   ///< Enter Activity State 2.
  SL_I3C_CCC_BROADCAST_ENTAS3           = 0x05,   ///< Enter Activity State 3.
  SL_I3C_CCC_BROADCAST_RSTDAA           = 0x06,   ///< Reset Dynamic Address Assignment.
  SL_I3C_CCC_BROADCAST_ENTDAA           = 0x07,   ///< Enter Dynamic Address Assignment.
  SL_I3C_CCC_BROADCAST_DEFSVLS          = 0x08,   ///< Define List of Slaves.
  SL_I3C_CCC_BROADCAST_SETMWL           = 0x09,   ///< Set Max Write Length.
  SL_I3C_CCC_BROADCAST_SETMRL           = 0x0A,   ///< Set Max Read Length.
  SL_I3C_CCC_BROADCAST_ENTTM            = 0x0B,   ///< Enter Test Mode.
  SL_I3C_CCC_BROADCAST_SETBUSCON        = 0x0C,   ///< Set BUS Context.
  SL_I3C_CCC_BROADCAST_ENDXFER          = 0x12,   ///< Data Transfer Ending Procedure Control.
  SL_I3C_CCC_BROADCAST_ENTHDR0          = 0x20,   ///< Enter HDR Mode 0.
  SL_I3C_CCC_BROADCAST_ENTHDR1          = 0x21,   ///< Enter HDR Mode 1.
  SL_I3C_CCC_BROADCAST_ENTHDR2          = 0x22,   ///< Enter HDR Mode 2.
  SL_I3C_CCC_BROADCAST_ENTHDR3          = 0x23,   ///< Enter HDR Mode 3.
  SL_I3C_CCC_BROADCAST_ENTHDR4          = 0x24,   ///< Enter HDR Mode 4.
  SL_I3C_CCC_BROADCAST_ENTHDR5          = 0x25,   ///< Enter HDR Mode 5.
  SL_I3C_CCC_BROADCAST_ENTHDR6          = 0x26,   ///< Enter HDR Mode 6.
  SL_I3C_CCC_BROADCAST_ENTHDR7          = 0x27,   ///< Enter HDR Mode 7.
  SL_I3C_CCC_BROADCAST_SETXTIME         = 0x28,   ///< Set Exchange Timing Info.
  SL_I3C_CCC_BROADCAST_SETAASA          = 0x29,   ///< Set All Addresses to Static Address.
  SL_I3C_CCC_BROADCAST_RSTACT           = 0x2A,   ///< Slave Reset Action.
  SL_I3C_CCC_BROADCAST_DEFGRPA          = 0x2B,   ///< Define List of Group Address.
  SL_I3C_CCC_BROADCAST_RSTGRPA          = 0x2C,   ///< Reset Group Address.
  SL_I3C_CCC_BROADCAST_MLANE            = 0x2D,   ///< Multi-Lane Data Transfer Control.
  SL_I3C_CCC_BROADCAST                  = 0x7E,   ///< Reserved for broadcast commands.

  /* Direct Common Command Codes */
  SL_I3C_CCC_DIRECT_ENEC               = 0x80,   ///< Enable Slave initiated events.
  SL_I3C_CCC_DIRECT_DISEC              = 0x81,   ///< Disable Slave initiated events.
  SL_I3C_CCC_DIRECT_ENTAS0             = 0x82,   ///< Enter Activity State 0.
  SL_I3C_CCC_DIRECT_ENTAS1             = 0x83,   ///< Enter Activity State 1.
  SL_I3C_CCC_DIRECT_ENTAS2             = 0x84,   ///< Enter Activity State 2.
  SL_I3C_CCC_DIRECT_ENTAS3             = 0x85,   ///< Enter Activity State 3.
  SL_I3C_CCC_DIRECT_RSTDAA             = 0x86,   ///< Reset Dynamic Address Assignment (DEPRECATED v1.0).
  SL_I3C_CCC_DIRECT_SETDASA            = 0x87,   ///< Set Dynamic Address from Static Address.
  SL_I3C_CCC_DIRECT_SETNEWDA           = 0x88,   ///< Set New Dynamic Address.
  SL_I3C_CCC_DIRECT_SETMWL             = 0x89,   ///< Set Max Write Length.
  SL_I3C_CCC_DIRECT_SETMRL             = 0x8A,   ///< Set Max Read Length.
  SL_I3C_CCC_DIRECT_GETMWL             = 0x8B,   ///< Get Max Write Length.
  SL_I3C_CCC_DIRECT_GETMRL             = 0x8C,   ///< Get Max Read Length.
  SL_I3C_CCC_DIRECT_GETPID             = 0x8D,   ///< Get Provisional ID.
  SL_I3C_CCC_DIRECT_GETBCR             = 0x8E,   ///< Get Bus Characteristic Register.
  SL_I3C_CCC_DIRECT_GETDCR             = 0x8F,   ///< Get Device Characteristic Register.
  SL_I3C_CCC_DIRECT_GETSTATUS          = 0x90,   ///< Get Device Status.
  SL_I3C_CCC_DIRECT_GETACCMST          = 0x91,   ///< Get Accept Mastership.
  SL_I3C_CCC_DIRECT_ENDXFER            = 0x92,   ///< Data Transfer Ending Procedure Control.
  SL_I3C_CCC_DIRECT_SETBRGTGT          = 0x93,   ///< Set Bridge Targets.
  SL_I3C_CCC_DIRECT_GETMXDS            = 0x94,   ///< Get Max Data Speed.
  SL_I3C_CCC_DIRECT_GETHDRCAP          = 0x95,   ///< Get HDR Capability.
  SL_I3C_CCC_DIRECT_SETROUTE           = 0x96,   ///< Set Route.
  SL_I3C_CCC_DIRECT_D2DXFER            = 0x97,   ///< D2DXFER (reserved; HLD support deferred).
  SL_I3C_CCC_DIRECT_SETXTIME           = 0x98,   ///< Set Exchange Timing Information.
  SL_I3C_CCC_DIRECT_GETXTIME           = 0x99,   ///< Get Exchange Timing Information.
  SL_I3C_CCC_DIRECT_RSTACT             = 0x9A,   ///< Reset Slave Action.
  SL_I3C_CCC_DIRECT_SETGRPA            = 0x9B,   ///< Set Group Address.
  SL_I3C_CCC_DIRECT_RSTGRPA            = 0x9C,   ///< Reset Group Address.
  SL_I3C_CCC_DIRECT_MLANE              = 0x9D,   ///< Multi-Lane Data Transfer Control.
};

/// I3C speed modes.
SL_ENUM(sl_i3c_speed_t) {
  SL_I3C_SPEED_SDR0 = 0,              ///< Single Data Rate mode 0
  SL_I3C_SPEED_SDR1 = 1,              ///< Single Data Rate mode 1
  SL_I3C_SPEED_SDR2 = 2,              ///< Single Data Rate mode 2
  SL_I3C_SPEED_SDR3 = 3,              ///< Single Data Rate mode 3
  SL_I3C_SPEED_SDR4 = 4,              ///< Single Data Rate mode 4
  SL_I3C_SPEED_HDR_TS = 5,            ///< HDR Ternary Symbol
  SL_I3C_SPEED_HDR_DDR = 6,           ///< HDR Double Data Rate
  SL_I3C_SPEED_BROADCAST_I2C_FM = 7   ///< I2C FM (JEDEC broadcast CCC only)
};

/// I2C speed modes for legacy devices.
SL_ENUM(sl_i2c_speed_t) {
  SL_I2C_SPEED_FM = 0,                ///< I2C Fast Mode (400 kHz).
  SL_I2C_SPEED_FMP = 1,               ///< I2C Fast Mode Plus (1 MHz).
};

/// Target-mode events delivered by @ref sl_i3c_target_callback_t.
SL_ENUM(sl_i3c_target_event_t) {
  SL_I3C_TARGET_EVENT_CCC_UPDATED = 0,       ///< CCC updated SLV_EVENT_STATUS / MRL / MWL
  SL_I3C_TARGET_EVENT_DYNAMIC_ADDR_ASSIGNED, ///< Dynamic address assigned
  SL_I3C_TARGET_EVENT_READ_REQUEST,          ///< Controller private read requested
  SL_I3C_TARGET_EVENT_IBI_SUCCESS,           ///< SIR/MR ACK (IBI_STS = Success)
  SL_I3C_TARGET_EVENT_IBI_EARLY_TERMINATE,   ///< SIR payload cut short; driver flushed TX and resumed
  SL_I3C_TARGET_EVENT_IBI_NOT_ATTEMPTED,     ///< SIR/MR not issued or NACKed
  SL_I3C_TARGET_EVENT_DEFSLV,                ///< DEFTGTS / define-list
  SL_I3C_TARGET_EVENT_START,                 ///< START detected
  SL_I3C_TARGET_EVENT_RESET_PATTERN,         ///< Target reset pattern
  SL_I3C_TARGET_EVENT_GROUP_ADDR_ASSIGNED,   ///< Group address assigned
  SL_I3C_TARGET_EVENT_SDA_RELEASED,          ///< SDA released
  SL_I3C_TARGET_EVENT_BUS_OWNER_UPDATED      ///< Bus ownership changed (after MR)
};

/// Transfer completion event codes.
SL_ENUM(sl_i3c_transfer_event_t) {
  SL_I3C_EVENT_TRANSFER_IN_PROGRESS = 0,   ///< Transfer in progress
  SL_I3C_EVENT_TRANSFER_COMPLETED   = 1,   ///< Transfer completed successfully
  SL_I3C_EVENT_TRANSFER_ERROR       = 2,   ///< Transfer error occurred
  SL_I3C_EVENT_TRANSFER_ABORTED     = 3,   ///< Transfer aborted
  SL_I3C_EVENT_RESPONSE_READY       = 4,   ///< Response queue entry ready
  SL_I3C_EVENT_IDLE                 = 5    ///< Instance idle / no active transfer
};

/// Response-queue ERR_STATUS values.
SL_ENUM(sl_i3c_response_error_t) {
  SL_I3C_RESP_ERROR_NONE                 = 0x0, ///< No error
  SL_I3C_RESP_ERROR_CRC                  = 0x1, ///< CRC error
  SL_I3C_RESP_ERROR_PARITY               = 0x2, ///< Parity error
  SL_I3C_RESP_ERROR_FRAME                = 0x3, ///< Frame error
  SL_I3C_RESP_ERROR_BROADCAST_NACK       = 0x4, ///< I3C broadcast address (0x7E) NACK
  SL_I3C_RESP_ERROR_ADDRESS_NACK         = 0x5, ///< Address NACK (e.g. ENTDAA)
  SL_I3C_RESP_ERROR_OVERFLOW_UNDERFLOW   = 0x6, ///< RX overflow / TX underflow
  SL_I3C_RESP_ERROR_TRANSFER_ABORTED     = 0x8, ///< Transfer terminated / aborted
  SL_I3C_RESP_ERROR_I2C_WRITE_NACK       = 0x9, ///< I2C write data NACK / early terminate
  SL_I3C_RESP_ERROR_EARLY_TERMINATION    = 0xA, ///< Controller early termination (target)
  SL_I3C_RESP_ERROR_GETACCCR_MISMATCH    = 0xB, ///< GETACCCR dynamic address mismatch
  SL_I3C_RESP_ERROR_PEC                  = 0xC, ///< PEC validation error
  SL_I3C_RESP_ERROR_HDR_BT_DELAY_EXPIRY  = 0xD  ///< HDR-BT delay byte-count expiry
};

/// Slave address type.
SL_ENUM(sl_i3c_slave_address_type_t) {
  SL_I3C_NO_ADDRESS,                  ///< No Address
  SL_I3C_STATIC_ADDRESS,              ///< Static Address
  SL_I3C_DYNAMIC_ADDRESS              ///< Dynamic Address
};

/// Dynamic address assignment (DAA) modes for @ref sl_i3c_start_address_assignment.
SL_ENUM(sl_i3c_addr_assign_mode_t) {
  SL_I3C_ADDR_ASSIGN_ENTDAA   = SL_I3C_CCC_BROADCAST_ENTDAA,
  SL_I3C_ADDR_ASSIGN_SETAASA  = SL_I3C_CCC_BROADCAST_SETAASA,
  SL_I3C_ADDR_ASSIGN_SETDASA  = SL_I3C_CCC_DIRECT_SETDASA,
  SL_I3C_ADDR_ASSIGN_SETNEWDA = SL_I3C_CCC_DIRECT_SETNEWDA,
};

/// Maximum IBI payload bytes buffered by the driver for one IBI event.
#define SL_I3C_IBI_PAYLOAD_MAX  256U

/// Default timeout for blocking transfer APIs (milliseconds).
#define SL_I3C_BLOCKING_TIMEOUT_DEFAULT_MS  5000U

/*******************************************************************************
 *******************************   TYPEDEFS   **********************************
 ******************************************************************************/

typedef struct sl_i3c_handle_t sl_i3c_handle_t;  ///< Forward declaration of I3C handle type
typedef struct sl_i3c_ibi_info_t sl_i3c_ibi_info_t;  ///< Forward declaration for IBI callback

/*******************************************************************************
 ****************************   CALLBACKS   ************************************
 ******************************************************************************/
/***************************************************************************//**
 * @brief Target-mode callback for interrupt events.
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 * @param[in] event       Decoded target event (@ref sl_i3c_target_event_t).
 ******************************************************************************/
typedef void (*sl_i3c_target_callback_t)(sl_i3c_handle_t *i3c_handle,
                                         sl_i3c_target_event_t event);

/***************************************************************************//**
 * @brief Controller-mode IBI callback.
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 * @param[in] ibi         Parsed IBI event (type, address, payload).
 ******************************************************************************/
typedef void (*sl_i3c_ibi_callback_t)(sl_i3c_handle_t *i3c_handle,
                                      const sl_i3c_ibi_info_t *ibi);

/*******************************************************************************
 *******************************   STRUCTS   ***********************************
 ******************************************************************************/

/// I3C device descriptor.
typedef struct {
  uint8_t  dynamic_addr;               ///< Dynamic address
  uint8_t  static_addr;                ///< Static address (for I2C devices)
  uint8_t  bcr;                        ///< Bus Characteristics Register
  uint8_t  dcr;                        ///< Device Characteristics Register
  uint16_t max_read_len;               ///< Maximum read length
  uint16_t max_write_len;              ///< Maximum write length
  uint8_t  max_ibi_len;               ///< Maximum IBI length
  uint8_t  max_ibi_payload_len;        ///< Maximum IBI payload length
  uint8_t  pid[6];                     ///< Provisional ID (48-bit)
  uint8_t  device_index;               ///< Device index in address table
  uint8_t  is_i2c      : 1;           ///< True if I2C device
  uint8_t  ibi_enabled : 1;           ///< IBI capability enabled
  uint8_t  _reserved   : 6;           ///< Reserved bits for alignment
} sl_i3c_device_desc_t;

/// CCC payload structure.
typedef struct {
  struct {
    sl_i3c_common_command_code_t id;   ///< CCC command ID
    uint8_t addr;                      ///< Target address.
  } ccc;
  uint8_t *data;                       ///< CCC data buffer
  size_t   len;                        ///< Data length
  uint8_t  tid;                        ///< Transaction ID
  uint8_t  defining_byte;              ///< Defining byte when @p dbp is set in command
  uint8_t  is_read   : 1;              ///< True for read CCC, false for write
  uint8_t  roc       : 1;              ///< Response On Completion required
  uint8_t  toc       : 1;              ///< Termination On Completion (STOP/RESTART)
  uint8_t  pec       : 1;              ///< Parity Error Check enable
  uint8_t  _reserved : 4;              ///< Reserved bits for alignment
} sl_i3c_ccc_payload_t;

/// Parameters for @ref sl_i3c_start_address_assignment.
///
/// Field usage by mode (DWC_MIPI_I3C DAA):
/// - ENTDAA:   @p dat_index, @p dev_count, @p tid — Address Assignment cmd;
///             DAT[DA+parity] pre-programmed via @ref sl_i3c_configure_target.
/// - SETDASA:  @p dat_index, @p dev_count, @p tid — Address Assignment cmd;
///             DAT[SA] and DAT[DA] pre-programmed for each slot in the range.
/// - SETAASA:  @p tid only — Regular Transfer CCC; after success HLD sets
///             DAT[DA] = DAT[SA] for every slot that has a static address.
/// - SETNEWDA: @p dat_index, @p tid, @p new_dynamic_address — Regular Transfer
///             CCC; DAT must hold the current DA; on success HLD writes the new DA.
typedef struct {
  sl_i3c_addr_assign_mode_t mode;                ///< Assignment mode.
  uint8_t                   dat_index;           ///< DAT start index (ENTDAA/SETDASA/SETNEWDA).
  uint8_t                   dev_count;           ///< Device count (ENTDAA/SETDASA; 0 => 1 for SETDASA).
  uint8_t                   tid;                 ///< User transaction ID (0..7).
  uint8_t                   new_dynamic_address; ///< SETNEWDA only: new 7-bit dynamic address.
} sl_i3c_addr_assign_params_t;

/// I3C transfer command parameters.
typedef struct {
  uint8_t  cmd_attr;                   ///< Command Attribute (3 bits)
  uint8_t  tid;                        ///< Transaction ID (4 bits)
  uint8_t  cmd;                        ///< Transfer Command (8 bits)
  uint8_t  dev_indx;                   ///< Device Index (5 bits)
  union {
    sl_i3c_speed_t i3c_speed;          ///< I3C Speed
    sl_i2c_speed_t i2c_speed;          ///< I2C Speed
  } speed;
  uint8_t  cp       : 1;              ///< Command Present flag
  uint8_t  dbp      : 1;              ///< Defining Byte Present
  uint8_t  roc      : 1;              ///< Response On Completion
  uint8_t  sdap     : 1;              ///< Short Data Argument Present
  uint8_t  rnw      : 1;              ///< Read/Write (false=Write, true=Read)
  uint8_t  tgt_rst  : 1;              ///< Target Reset Pattern Generation
  uint8_t  toc      : 1;              ///< Termination On Completion
  uint8_t  pec      : 1;              ///< Parity Error Check Enable
} sl_i3c_transfer_cmd_t;

/// I3C SCL timing (OD and PP).
typedef struct {
  uint8_t od_high_count;               ///< Open-drain high period count
  uint8_t od_low_count;                ///< Open-drain low period count
  uint8_t pp_high_count;               ///< Push-pull high period count
  uint8_t pp_low_count;                ///< Push-pull low period count
} sl_i3c_timing_t;

/// I2C timing configuration.
typedef struct {
  bool     is_fmp_mode;                ///< false = FM (400 kHz), true = FMP (1 MHz)
  uint16_t high_count;                 ///< SCL high count for selected mode
  uint16_t low_count;                  ///< SCL low count for selected mode
} sl_i3c_i2c_timing_t;

/// Controller-mode configuration.
typedef struct {
  sl_i3c_timing_t      i3c_timing;          ///< I3C SCL timing configuration
  sl_i3c_i2c_timing_t  i2c_timing;          ///< I2C SCL timing configuration
  sl_hal_i3c_queue_thld_t  queue_thlds;         ///< Queue threshold configuration
  sl_hal_i3c_fifo_thld_t   fifo_thlds;          ///< FIFO threshold configuration
  uint8_t              dynamic_address;     ///< Controller dynamic address
  bool                 i2c_slave_present;   ///< I2C slave devices present on bus
  uint16_t             bus_free_time;       ///< Bus free time value
} sl_i3c_controller_config_t;

/// Slave-mode configuration.
typedef struct {
  sl_hal_i3c_queue_thld_t             queue_thlds;            ///< Queue threshold configuration
  sl_hal_i3c_fifo_thld_t              fifo_thlds;             ///< FIFO threshold configuration
  sl_hal_i3c_max_read_speed_t       max_read_speed;           ///< Maximum read speed
  sl_hal_i3c_clock_data_turnaround_t clk_data_turnaround;     ///< Clock data turnaround
  sl_hal_i3c_idle_cnt_multiplier_t idle_cnt_multiplier;       ///< Idle count multiplier
  bool                            mode_i2c;               ///< I2C mode enable
  bool                            adaptive_i2c_i3c;       ///< Wait for 7E before Hot-Join if bus type is unknown
  bool                            hot_join_enable;        ///< SLV_EVENT_STATUS.HJ_EN (program while disabled)
  uint32_t                        bus_free_avail_timing;  ///< Bus free available timing
  uint32_t                        bus_idle_timing;        ///< Bus idle timing
  uint32_t                        release_sda_timing;     ///< Release SDA timing (target mode)
  sl_i3c_slave_address_type_t     address_type;           ///< Address type
  uint8_t                         address;                ///< Static or dynamic address value
} sl_i3c_slave_config_t;

/// Device identification configuration for target mode.
typedef struct {
  struct {
    uint16_t mfg_id;                   ///< MIPI manufacturer ID
    bool     is_random;                ///< Use random provisional ID flag
  } mipi_id;                           ///< MIPI ID configuration
  struct {
    uint16_t part_id;                  ///< Part ID value
    uint8_t  inst_id;                  ///< Instance ID value
    uint16_t pid_dcr;                  ///< PID DCR value
  } pid;                               ///< Provisional ID configuration
  bool set_mipi_id;                    ///< Enable MIPI ID configuration
  bool set_pid;                        ///< Enable PID configuration
} sl_i3c_device_id_config_t;

/// Virtual target bring-up configuration (target mode).
typedef struct {
  uint8_t  static_address;             ///< 7-bit static address
  uint8_t  dcr;                        ///< Device Characteristic Register
  uint32_t pid_value;                  ///< VIRT_TGT_x_PID_VALUE (part/inst/PID-DCR)
  uint16_t mipi_mfg_id;                ///< MIPI manufacturer ID (PID[47:33])
  bool     prov_id_sel;                ///< Provisional ID selector (PID[32])
  bool     clear_dynamic_address;      ///< Clear DA + VALID before enable
  bool     enable;                     ///< Enable virtual target after programming
} sl_i3c_virtual_target_config_t;

/// Buffer level information.
typedef struct {
  uint32_t queue_status_level;         ///< Queue status level (Controller mode only)
  uint32_t data_buffer_level;          ///< Data buffer/FIFO level (Both modes)
} sl_i3c_buffer_levels_t;

/// Target address information for target mode.
typedef struct {
  uint8_t dynamic_address;             ///< Dynamic address assigned to target (7-bit)
  uint8_t static_address;              ///< Static address configured for target (7-bit)
} sl_i3c_target_addresses_t;

/// Parsed In-Band Interrupt (IBI) event delivered to the application.
typedef struct sl_i3c_ibi_info_t {
  sl_hal_i3c_ibi_type_t type;          ///< IBI type (see @ref sl_hal_i3c_ibi_type_t)
  uint8_t               address;       ///< 7-bit dynamic address (0 for Hot-Join)
  uint8_t               data[SL_I3C_IBI_PAYLOAD_MAX]; ///< Payload (MDB + optional data)
  uint16_t              len;           ///< Valid bytes in @p data (up to SL_I3C_IBI_PAYLOAD_MAX)
  bool                  error;         ///< True if the IBI status reported an error
  bool                  rejected;      ///< True if this is a reject/notify status entry
} sl_i3c_ibi_info_t;

/// Target-mode SIR configuration.
typedef struct {
  uint8_t mdb;                   ///< Mandatory Data Byte (MDB).
  uint8_t payload_length;        ///< Extra SIR bytes after MDB (4 bytes), written to SIR_DATA_LENGTH.
  uint8_t virtual_target_index;  ///< Virtual target index (0–3).
} sl_i3c_sir_config_t;

/// Target-mode SIR parameters used with @ref sl_i3c_configure_sir().
typedef struct {
  sl_i3c_sir_config_t sir_config;  ///< SIR register configuration.
  uint32_t sir_payload;            ///< SIR payload data (bytes after MDB), little-endian.
} sl_i3c_sir_params_t;

/// Target device characteristics.
typedef struct {
  uint8_t  pid[6];                     ///< Provisional ID (48-bit, MSB first)
  uint8_t  bcr;                        ///< Bus Characteristics Register
  uint8_t  dcr;                        ///< Device Characteristics Register
} sl_i3c_target_characteristics_t;

/// Target device status information.
typedef struct {
  uint16_t pending_intr;               ///< Pending interrupt status
  uint8_t  activity_mode;              ///< Current activity mode
  uint8_t  overflow_underflow;         ///< Overflow/underflow flags
} sl_i3c_target_status_t;

/// Initialization parameters for I3C driver instance.
typedef struct {
  I3C_TypeDef                      *i3c_base_addr;    ///< I3C peripheral register block (e.g. I3C0)
  sl_i3c_role_t                     role;              ///< Operating role: Controller or Slave
  const sl_i3c_controller_config_t *controller_cfg;   ///< Controller configuration (if role == CONTROLLER)
  const sl_i3c_slave_config_t      *slave_cfg;        ///< Slave configuration (if role == SLAVE)
} sl_i3c_init_params_t;

/*******************************************************************************
 ****************************   CONFIGURATION   ********************************
 ******************************************************************************/

/// Default FIFO threshold levels for queue and data buffer configuration.
#define SL_I3C_FIFO_THRESHOLD_DEFAULT   \
        (sl_hal_i3c_fifo_thld_t){             \
          .rx_start_thld     = SL_HAL_I3C_THLD_1,    \
          .tx_start_thld     = SL_HAL_I3C_THLD_1,    \
          .rx_buf_thld       = SL_HAL_I3C_THLD_1,    \
          .tx_empty_buf_thld = SL_HAL_I3C_THLD_1,    \
        }

/// Default queue threshold levels (controller mode).
#define SL_I3C_QUEUE_THRESHOLD_DEFAULT  \
        (sl_hal_i3c_queue_thld_t){          \
          .ibi_status_thld    = 0,            \
          .ibi_data_thld      = 1,            \
          .resp_buf_thld      = 0,            \
          .cmd_empty_buf_thld = 0,            \
        }

/***************************************************************************//**
 * @struct sl_i3c_handle_t
 * @brief Represents an I3C instance handle.
 *        Contains configuration, buffers, state, and callbacks.
 *
 * @warning
 *       This structure is defined in the public header for driver implementation
 *       purposes only. Applications must NOT access, modify, or rely upon any
 *       members of this structure directly.
 *       Use only the provided API functions to interact with I3C handles.
 ******************************************************************************/
typedef struct sl_i3c_handle_t {
  I3C_TypeDef                     *i3c;              ///< I3C peripheral register block
  sl_i3c_role_t                    role;             ///< Controller or target
  bool                             initialized;      ///< Init guard - indicates if the handle is initialized
  uint8_t                          xfer_state;       ///< Internal FSM (sli_i3c_xfer_state_t)
  uint8_t                          last_resp_error;  ///< Last RESPONSE ERR_STATUS
  const uint8_t                   *tx_buf;
  uint8_t                         *rx_buf;
  uint16_t                         tx_len;
  uint16_t                         rx_len;
  uint16_t                         tx_offset;
  uint16_t                         rx_offset;

  /* --- Controller device table (software mirror of DAT) --- */
  sl_i3c_device_desc_t             dev_table[SL_HAL_I3C_MAX_TARGET_COUNT];
  uint8_t                          dev_count;
  uint8_t                          setnewda_dat_index;
  uint8_t                          setnewda_new_addr;  ///< Pending SETNEWDA 7-bit DA
  bool                             setnewda_pending;
  sl_hal_i3c_target_dat_config_t   dat_cfg[SL_HAL_I3C_MAX_TARGET_COUNT];

  /* --- Callbacks / app context --- */
  sl_status_t (*transfer_callback)(sl_i3c_handle_t *i3c_handle,
                                   sl_i3c_transfer_event_t event); ///< One-shot (blocking)
  sl_i3c_target_callback_t         target_callback;
  sl_i3c_ibi_callback_t            ibi_callback;
  void                            *user_data;
  uint8_t                          ibi_enabled_mask;
  uint8_t                          next_tid;           ///< Auto TID for NULL command

  /* --- Blocking sync --- */
  volatile bool                    blocking_done;
  volatile sl_i3c_transfer_event_t blocking_event;

  /* --- Target Format-2 / VT private-read handshake (EXT_CMD only) --- */
  bool                             virt_tgt_enabled;    ///< VT on: RESPONSE_QUEUE is Format-2
  bool                             await_phys_tx_done;  ///< COMMAND_QUEUE physical TX RESP pending
  bool                             await_ext_tx_done;
  uint8_t                          ext_cmd_index;
  uint8_t                          expect_target_id_mask;
  /// @endcond
} sl_i3c_handle_t;

/*******************************************************************************
 *****************************   PROTOTYPES   **********************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Initialize an I3C driver instance.
 *
 * @details Applies controller or target configuration to a caller-owned handle.
 *          An instance may be initialized once until @ref sl_i3c_deinit().
 *
 * @param[in,out] i3c_handle   Pointer to the caller-allocated I3C handle.
 * @param[in]     init_params  Pointer to initialization parameters.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if arguments or configuration are invalid.
 *   - SL_STATUS_ALREADY_INITIALIZED if the peripheral is already initialized.
 ******************************************************************************/
sl_status_t sl_i3c_init(sl_i3c_handle_t *i3c_handle,
                        const sl_i3c_init_params_t *init_params);

/***************************************************************************//**
 * @brief Deinitialize an I3C driver instance.
 *
 * @details Disables the peripheral and clears the instance so
 *          @ref sl_i3c_init() can run again.
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if the handle is invalid.
 ******************************************************************************/
sl_status_t sl_i3c_deinit(sl_i3c_handle_t *i3c_handle);

/***************************************************************************//**
 * @brief Start dynamic address assignment (DAA).
 *
 * @details Supports DWC_MIPI_I3C DAA methods:
 *          - ENTDAA / SETDASA: Address Assignment command (CMD_ATTR=3). Program
 *            DAT with @ref sl_i3c_configure_target() first (ENTDAA: DA+parity;
 *            SETDASA: SA and DA). @p dev_count + @p dat_index must not exceed
 *            DAT depth. RESP.DATA_LENGTH is remaining device count, not RX bytes.
 *          - SETAASA / SETNEWDA: Regular Transfer command (not Address Assignment).
 *            After SETAASA success the driver sets DAT[DA]=DAT[SA]. After SETNEWDA
 *            success the driver sets DAT[DA]=@p new_dynamic_address.
 *          Soft PID/BCR/DCR after ENTDAA come from DCT via
 *          @ref sl_i3c_get_target_characteristics() (DCT is SW read-only).
 *          RSTDAA is not a DAA method — use @ref sl_i3c_do_ccc().
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 * @param[in] params      Mode, DAT index, device count, and TID.
 * @param[in] timeout_ms  Timeout in milliseconds (0 uses default).
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER / SL_STATUS_INVALID_MODE / SL_STATUS_BUSY /
 *     SL_STATUS_TIMEOUT on failure.
 ******************************************************************************/
sl_status_t sl_i3c_start_address_assignment(sl_i3c_handle_t *i3c_handle,
                                            const sl_i3c_addr_assign_params_t *params,
                                            uint32_t timeout_ms);

/***************************************************************************//**
 * @brief Look up a device descriptor by 7-bit address.
 *
 * @param[in]  i3c_handle  I3C instance handle.
 * @param[in]  address     7-bit static or dynamic address to find.
 * @param[out] device      Pointer to receive the matching device descriptor.
 *
 * @return
 *   - SL_STATUS_OK if the device was found.
 *   - SL_STATUS_NOT_FOUND if no matching entry exists in the DAT.
 *   - SL_STATUS_INVALID_PARAMETER if arguments are invalid.
 ******************************************************************************/
sl_status_t sl_i3c_device_find(sl_i3c_handle_t *i3c_handle,
                               uint8_t address,
                               const sl_i3c_device_desc_t **device);

/***************************************************************************//**
 * @brief Issue a Common Command Code (CCC) transfer.
 *
 * @details DAA methods (ENTDAA, SETDASA, SETAASA, SETNEWDA) are not supported
 *          here; use @ref sl_i3c_start_address_assignment() instead.
 *          Broadcast RSTDAA is a normal transfer CCC — issue it with this API.
 *
 * @param[in]  i3c_handle  I3C instance handle.
 * @param[in]  payload     CCC payload.
 * @param[in]  command     Transfer command attributes.
 * @param[out] rx_data     Read response buffer (NULL for write CCC).
 * @param[in]  rx_len      Length of @p rx_data.
 * @param[in]  timeout_ms  Timeout in milliseconds (0 uses default).
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER / SL_STATUS_INVALID_MODE / SL_STATUS_BUSY /
 *     SL_STATUS_TIMEOUT on failure.
 ******************************************************************************/
sl_status_t sl_i3c_do_ccc(sl_i3c_handle_t *i3c_handle,
                          const sl_i3c_ccc_payload_t *payload,
                          const sl_i3c_transfer_cmd_t *command,
                          uint8_t *rx_data,
                          size_t rx_len,
                          uint32_t timeout_ms);

/***************************************************************************//**
 * @brief Controller: assign a group address to a target (SETGRPA).
 *
 * @details Directed SETGRPA with short data `(group_addr << 1)`. SDR only.
 *          After assignment, add a DAT entry for @p group_addr_7bit via
 *          @ref sl_i3c_configure_target and use
 *          @ref sl_i3c_controller_send_blocking for group writes.
 *
 * @param[in] i3c_handle       I3C instance handle (controller).
 * @param[in] device           Target that receives SETGRPA.
 * @param[in] group_addr_7bit  7-bit group address to assign.
 * @param[in] timeout_ms       Timeout (0 uses default).
 *
 * @return SL_STATUS_OK or an error from @ref sl_i3c_do_ccc.
 ******************************************************************************/
sl_status_t sl_i3c_set_group_address(sl_i3c_handle_t *i3c_handle,
                                     const sl_i3c_device_desc_t *device,
                                     uint8_t group_addr_7bit,
                                     uint32_t timeout_ms);

/***************************************************************************//**
 * @brief Controller: reset group address(es) (RSTGRPA).
 *
 * @details If @p device is non-NULL, issues directed RSTGRPA to that target.
 *          If @p device is NULL, issues broadcast RSTGRPA.
 *
 * @param[in] i3c_handle  I3C instance handle (controller).
 * @param[in] device      Target for directed reset, or NULL for broadcast.
 * @param[in] timeout_ms  Timeout (0 uses default).
 *
 * @return SL_STATUS_OK or an error from @ref sl_i3c_do_ccc.
 ******************************************************************************/
sl_status_t sl_i3c_reset_group_address(sl_i3c_handle_t *i3c_handle,
                                       const sl_i3c_device_desc_t *device,
                                       uint32_t timeout_ms);

/***************************************************************************//**
 * @brief Target: read an assigned group-address slot (GRP_ADDR_n_STAT).
 *
 * @param[in]  i3c_handle         I3C instance handle (target).
 * @param[in]  grp_addr_index     Slot index (0..SL_HAL_I3C_MAX_GROUP_ADDRESS_COUNT-1).
 * @param[out] group_addr_7bit    Assigned 7-bit group address.
 * @param[out] target_valid_mask  Optional T0–T4 valid bitmask (may be NULL).
 *
 * @return SL_STATUS_OK on success.
 ******************************************************************************/
sl_status_t sl_i3c_get_group_address(sl_i3c_handle_t *i3c_handle,
                                     uint8_t grp_addr_index,
                                     uint8_t *group_addr_7bit,
                                     uint8_t *target_valid_mask);

/***************************************************************************//**
 * @brief Controller: private write to a device (blocking).
 *
 * @details Controller mode only. Queues a private write and waits until the
 *          response queue reports completion or @p timeout_ms expires.
 *          Pass @p command as NULL to use driver defaults (auto TID, ROC/TOC).
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 * @param[in] device      Target device descriptor.
 * @param[in] buf         Transmit buffer.
 * @param[in] len         Byte count.
 * @param[in] command     Transfer command, or NULL for defaults.
 * @param[in] timeout_ms  Timeout in milliseconds (0 uses default).
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER / SL_STATUS_INVALID_MODE / SL_STATUS_BUSY /
 *     SL_STATUS_NOT_FOUND / SL_STATUS_TIMEOUT on failure.
 ******************************************************************************/
sl_status_t sl_i3c_controller_send_blocking(sl_i3c_handle_t *i3c_handle,
                                            const sl_i3c_device_desc_t *device,
                                            const uint8_t *buf,
                                            size_t len,
                                            const sl_i3c_transfer_cmd_t *command,
                                            uint32_t timeout_ms);

/***************************************************************************//**
 * @brief Controller: private read from a device (blocking).
 *
 * @details Controller mode only. Queues a private read and waits until the
 *          response queue reports completion or @p timeout_ms expires.
 *          Pass @p command as NULL to use driver defaults (auto TID, ROC/TOC).
 *
 * @param[in]  i3c_handle  I3C instance handle.
 * @param[in]  device      Target device descriptor.
 * @param[out] buf         Receive buffer.
 * @param[in]  len         Byte count.
 * @param[in]  command     Transfer command, or NULL for defaults.
 * @param[in]  timeout_ms  Timeout in milliseconds (0 uses default).
 *
 * @return
 *   - SL_STATUS_OK on success; all @p len bytes were received.
 *   - SL_STATUS_INVALID_COUNT if the target ended the read before @p len
 *     bytes; the contents of @p buf beyond the received bytes are undefined.
 *   - SL_STATUS_INVALID_PARAMETER / SL_STATUS_INVALID_MODE / SL_STATUS_BUSY /
 *     SL_STATUS_NOT_FOUND / SL_STATUS_TIMEOUT on failure.
 ******************************************************************************/
sl_status_t sl_i3c_controller_receive_blocking(sl_i3c_handle_t *i3c_handle,
                                               const sl_i3c_device_desc_t *device,
                                               uint8_t *buf,
                                               size_t len,
                                               const sl_i3c_transfer_cmd_t *command,
                                               uint32_t timeout_ms);

/***************************************************************************//**
 * @brief Controller: combined write-then-read private transfer (blocking).
 *
 * @details Controller mode only. Writes @p wr_buf with RESTART, then reads
 *          @p rd_len bytes with STOP. Writes of 1-3 bytes use Short Data
 *          (SDAP=1, BYTE_STRB 1/3/7); 0 or 4-65535 use Transfer Argument
 *          (SDAP=0, DATA_LENGTH). Driver owns TIDs.
 *
 * @param[in]  i3c_handle  I3C instance handle.
 * @param[in]  device      Target device descriptor.
 * @param[in]  wr_buf      Write data buffer.
 * @param[in]  wr_len      Number of bytes to write (0..65535).
 * @param[out] rd_buf      Read data buffer.
 * @param[in]  rd_len      Number of bytes to read (0..65535).
 * @param[in]  timeout_ms  Timeout in milliseconds per phase (0 uses default).
 *
 * @return
 *   - SL_STATUS_OK on success; all @p rd_len bytes were received.
 *   - SL_STATUS_INVALID_COUNT if the target ended the read before @p rd_len
 *     bytes; the contents of @p rd_buf beyond the received bytes are undefined.
 *   - SL_STATUS_INVALID_PARAMETER / SL_STATUS_INVALID_MODE / SL_STATUS_BUSY /
 *     SL_STATUS_NOT_FOUND / SL_STATUS_TIMEOUT on failure.
 ******************************************************************************/
sl_status_t sl_i3c_controller_transfer_blocking(sl_i3c_handle_t *i3c_handle,
                                                const sl_i3c_device_desc_t *device,
                                                const uint8_t *wr_buf,
                                                size_t wr_len,
                                                uint8_t *rd_buf,
                                                size_t rd_len,
                                                uint32_t timeout_ms);

/***************************************************************************//**
 * @brief Target: arm physical TX for an upcoming controller private read.
 *
 * @details Target mode only. Flushes the TX FIFO, loads transmit data, and
 *          posts a Transmit Command. Returns after the command is queued —
 *          does not wait for RESP — so the application can immediately call
 *          @ref sl_i3c_target_receive_blocking for a following controller
 *          private write (Read-then-Write peers).
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle (target role).
 * @param[in] buf         Transmit payload.
 * @param[in] len         Byte count (0..65535).
 * @param[in] tid         Transaction ID (0-6 recommended for private transfers).
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER / SL_STATUS_INVALID_MODE / SL_STATUS_BUSY
 *     on failure.
 ******************************************************************************/
sl_status_t sl_i3c_target_send(sl_i3c_handle_t *i3c_handle,
                               const uint8_t *buf,
                               size_t len,
                               uint8_t tid);

/***************************************************************************//**
 * @brief Target: wait for a controller private write (blocking).
 *
 * @details Target mode only. Arms RX and blocks until @p len bytes are
 *          received or @p timeout_ms expires. May be called while physical or
 *          virtual TX is already armed (@ref sl_i3c_target_send /
 *          @ref sl_i3c_virtual_target_send).
 *
 * @param[in]  i3c_handle  I3C instance handle (target role).
 * @param[out] buf         Receive buffer.
 * @param[in]  len         Byte count.
 * @param[in]  timeout_ms  Timeout in milliseconds (0 uses default).
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER / SL_STATUS_INVALID_MODE / SL_STATUS_BUSY /
 *     SL_STATUS_TIMEOUT on failure.
 ******************************************************************************/
sl_status_t sl_i3c_target_receive_blocking(sl_i3c_handle_t *i3c_handle,
                                           uint8_t *buf,
                                           size_t len,
                                           uint32_t timeout_ms);

/***************************************************************************//**
 * @brief Target: arm virtual-target TX for an upcoming controller private read.
 *
 * @details Target mode only. Convenience wrapper around
 *          Format-2 private SDR read (CCC=0).
 *
 * @param[in] i3c_handle            I3C instance handle (target role).
 * @param[in] virtual_target_index  Virtual target index (1..4).
 * @param[in] ext_cmd_index         Extended command / TX queue index (0..3).
 * @param[in] buf                   Transmit payload.
 * @param[in] len                   Byte count (0..65535, finite length).
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER / SL_STATUS_INVALID_MODE /
 *     SL_STATUS_TIMEOUT / SL_STATUS_NOT_SUPPORTED on failure.
 ******************************************************************************/
sl_status_t sl_i3c_virtual_target_send(sl_i3c_handle_t *i3c_handle,
                                       uint8_t virtual_target_index,
                                       uint8_t ext_cmd_index,
                                       const uint8_t *buf,
                                       size_t len);

/***************************************************************************//**
 * @brief Program a controller-mode Device Address Table (DAT) entry.
 *
 * @details Programs a DAT entry and updates the driver device table. Call
 *          before address-assignment APIs when the DAT slot must
 *          be set (I2C devices, ENTDAA, SETDASA, SETNEWDA).
 *
 * @param[in] i3c_handle    I3C instance handle.
 * @param[in] device_index  DAT index (0 to @ref SL_HAL_I3C_MAX_TARGET_COUNT - 1).
 * @param[in] cfg           Full DAT entry configuration.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if arguments are invalid.
 *   - SL_STATUS_INVALID_MODE if not in controller mode.
 ******************************************************************************/
sl_status_t sl_i3c_configure_target(sl_i3c_handle_t *i3c_handle,
                                    uint8_t device_index,
                                    const sl_hal_i3c_target_dat_config_t *cfg);

/***************************************************************************//**
 * @brief Set I3C open-drain and push-pull SCL timing (controller mode).
 *
 * @param[in] i3c_handle     I3C instance handle.
 * @param[in] od_high_count  Open-drain SCL high period count.
 * @param[in] od_low_count   Open-drain SCL low period count.
 * @param[in] pp_high_count  Push-pull SCL high period count.
 * @param[in] pp_low_count   Push-pull SCL low period count.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if the handle is invalid.
 ******************************************************************************/
sl_status_t sl_i3c_set_i3c_timing(sl_i3c_handle_t *i3c_handle,
                                  uint8_t od_high_count,
                                  uint8_t od_low_count,
                                  uint8_t pp_high_count,
                                  uint8_t pp_low_count);

/***************************************************************************//**
 * @brief Set legacy I2C SCL timing for mixed-bus operation (controller mode).
 *
 * @param[in] i3c_handle   I3C instance handle.
 * @param[in] is_fmp_mode  True for Fast Mode Plus, false for Fast Mode.
 * @param[in] high_count   SCL high period count.
 * @param[in] low_count    SCL low period count.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if the handle is invalid.
 ******************************************************************************/
sl_status_t sl_i3c_set_i2c_timing(sl_i3c_handle_t *i3c_handle,
                                  bool is_fmp_mode,
                                  uint16_t high_count,
                                  uint16_t low_count);

/***************************************************************************//**
 * @brief Set queue and FIFO threshold levels.
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 * @param[in] queue_thld  Queue threshold configuration (may be NULL).
 * @param[in] fifo_thld   FIFO threshold configuration (may be NULL).
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if the handle is invalid.
 ******************************************************************************/
sl_status_t sl_i3c_set_thresholds(sl_i3c_handle_t *i3c_handle,
                                  const sl_hal_i3c_queue_thld_t *queue_thld,
                                  const sl_hal_i3c_fifo_thld_t *fifo_thld);

/***************************************************************************//**
 * @brief Set target SLV_CHAR_CTRL / BCR+DCR writable fields (target mode).
 *
 * @details Pass named fields — do not pack BCR bits by hand. Programs
 *          MAX_DATA_SPEED_LIMIT, HDR_CAPABLE, DEVICE_ROLE, and DCR. Call during
 *          bring-up before the active controller enables the bus. Use
 *          @c device_role = @ref SL_HAL_I3C_DEVICE_ROLE_CONTROLLER_CAPABLE for
 *          Master Request (MR).
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 * @param[in] cfg         Writable characteristics (@ref sl_hal_i3c_slave_char_config_t).
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if arguments are invalid.
 ******************************************************************************/
sl_status_t sl_i3c_set_characteristics(sl_i3c_handle_t *i3c_handle,
                                       const sl_hal_i3c_slave_char_config_t *cfg);

/***************************************************************************//**
 * @brief Configure target provisional ID and MIPI manufacturer ID (target mode).
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 * @param[in] config      Device identification configuration.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if arguments are invalid.
 ******************************************************************************/
sl_status_t sl_i3c_set_target_id(sl_i3c_handle_t *i3c_handle,
                                 const sl_i3c_device_id_config_t *config);

/***************************************************************************//**
 * @brief Get the last RESPONSE_QUEUE ERR_STATUS seen by the driver.
 *
 * @details After a blocking transfer returns a non-OK status, this reports the
 *          hardware error code from the hardware.
 *
 * @param[in]  i3c_handle  Pointer to the I3C instance handle.
 * @param[out] error       Pointer to receive the last response error.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if arguments are invalid.
 *   - SL_STATUS_INVALID_HANDLE if the handle is not initialized.
 ******************************************************************************/
sl_status_t sl_i3c_get_last_response_error(sl_i3c_handle_t *i3c_handle,
                                           sl_i3c_response_error_t *error);

/***************************************************************************//**
 * @brief Abort the active transfer.
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if the handle is invalid.
 ******************************************************************************/
sl_status_t sl_i3c_abort_transfer(sl_i3c_handle_t *i3c_handle);

/***************************************************************************//**
 * @brief Request RESUME after an explicit halt.
 *
 * @details Hardware clears the RESUME bit automatically when it leaves halt.
 *          The driver raises RESUME internally in all standard cases:
 *          - blocking transfer timeout / abort (@ref sl_i3c_abort_transfer)
 *          - TRANSFER_ERR / TRANSFER_ABORT IRQ recovery
 *          - SIR early-terminate (handled in the IBI_UPDATED_STS IRQ)
 *          - READ_REQUEST event callback for Format-2 ext-TX release
 *          - the @ref sl_i3c_target_receive_blocking poll loop
 *
 *          Call this API only when the application itself detects a stuck
 *          condition outside of a blocking transfer — for example after
 *          reading @ref sl_i3c_get_target_status and finding
 *          overflow_underflow set.  Do **not** call it unconditionally
 *          around every transfer.
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_HANDLE if the handle is not initialized.
 ******************************************************************************/
sl_status_t sl_i3c_resume(sl_i3c_handle_t *i3c_handle);

/***************************************************************************//**
 * @brief Reset a command queue, response queue, FIFO, or IBI queue.
 *
 * @param[in] i3c_handle   I3C instance handle.
 * @param[in] buffer_type  Buffer or queue to reset.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if arguments are invalid.
 ******************************************************************************/
sl_status_t sl_i3c_buffer_reset(sl_i3c_handle_t *i3c_handle,
                                sl_hal_i3c_buffer_reset_t buffer_type);

/***************************************************************************//**
 * @brief Read command/response queue and FIFO fill levels.
 *
 * @param[in]  i3c_handle  I3C instance handle.
 * @param[out] levels      Pointer to receive buffer level information.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if arguments are invalid.
 ******************************************************************************/
sl_status_t sl_i3c_get_buffer_levels(sl_i3c_handle_t *i3c_handle,
                                     sl_i3c_buffer_levels_t *levels);

/***************************************************************************//**
 * @brief Issue a bus reset pattern from controller mode.
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 * @param[in] type        Bus reset pattern type.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if arguments are invalid.
 ******************************************************************************/
sl_status_t sl_i3c_bus_reset(sl_i3c_handle_t *i3c_handle,
                             sl_hal_i3c_bus_reset_type_t type);

/***************************************************************************//**
 * @brief Read PID, BCR, and DCR for a DAT entry (controller mode).
 *
 * @param[in]  i3c_handle       I3C instance handle.
 * @param[in]  target_index     DAT index to read.
 * @param[out] characteristics  Pointer to receive target characteristics.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if arguments are invalid.
 ******************************************************************************/
sl_status_t sl_i3c_get_target_characteristics(sl_i3c_handle_t *i3c_handle,
                                              uint8_t target_index,
                                              sl_i3c_target_characteristics_t *characteristics);

/***************************************************************************//**
 * @brief Read target device status (target mode).
 *
 * @param[in]  i3c_handle   I3C instance handle.
 * @param[out] status_info  Pointer to receive device status.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if arguments are invalid.
 ******************************************************************************/
sl_status_t sl_i3c_get_target_status(sl_i3c_handle_t *i3c_handle,
                                     sl_i3c_target_status_t *status_info);

/***************************************************************************//**
 * @brief Read static and dynamic addresses configured for target mode.
 *
 * @details Dynamic address is reported as 0 when not yet assigned (VALID clear).
 *          That is not an error; callers poll until a non-zero DA appears after
 *          ENTDAA / SETDASA.
 *
 * @param[in]  i3c_handle  I3C instance handle.
 * @param[out] addresses   Pointer to receive address information.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if arguments are invalid.
 ******************************************************************************/
sl_status_t sl_i3c_get_target_addresses(sl_i3c_handle_t *i3c_handle,
                                        sl_i3c_target_addresses_t *addresses);

/***************************************************************************//**
 * @brief Register a target-mode event callback.
 *
 * @details Invoked from @ref sl_i3c_irq_handler with a decoded
 *          @ref sl_i3c_target_event_t. Call after @ref sl_i3c_init().
 *          Pass NULL to clear.
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 * @param[in] callback    Target event callback.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if the handle is invalid.
 ******************************************************************************/
sl_status_t sl_i3c_set_target_callback(sl_i3c_handle_t *i3c_handle,
                                       sl_i3c_target_callback_t callback);

/***************************************************************************//**
 * @brief Store application context pointer on the handle.
 *
 * @details Retrieve later with @ref sl_i3c_get_user_data() from a callback that
 *          receives the handle.
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 * @param[in] user_data   Application pointer (may be NULL).
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_HANDLE if the handle is not initialized.
 ******************************************************************************/
sl_status_t sl_i3c_set_user_data(sl_i3c_handle_t *i3c_handle,
                                 void *user_data);

/***************************************************************************//**
 * @brief Read application context pointer from the handle.
 *
 * @param[in]  i3c_handle  Pointer to the I3C instance handle.
 * @param[out] user_data   Receives the pointer previously set (may be NULL).
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_HANDLE if the handle is not initialized.
 *   - SL_STATUS_NULL_POINTER if user_data is NULL.
 ******************************************************************************/
sl_status_t sl_i3c_get_user_data(sl_i3c_handle_t *i3c_handle,
                                 void **user_data);

/***************************************************************************//**
 * @brief Configure In-Band Interrupt (IBI) acceptance and callback (controller).
 *
 * @details
 *   Controller-side policy for incoming IBIs:
 *   - **SIR**: accept/reject in the target DAT entry (@p device required).
 *   - **MR** (Master Request): accept/reject in the target DAT entry. On an
 *     accepted MR, the application should call @ref sl_i3c_accept_mastership()
 *     (GETACCMST) to hand over the bus.
 *   - **HJ**: enable/disable Hot-Join (@p device ignored).
 *   Non-NULL @p callback enables IBI interrupts; NULL disables the selected
 *   type (and the IBI IRQ when no types remain enabled). Call after
 *   @ref sl_i3c_init(); hardware is programmed before software IBI state.
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle (controller role).
 * @param[in] device      Target device (required for SIR/MR; NULL for HJ).
 * @param[in] type        IBI type (@ref sl_hal_i3c_ibi_type_t).
 * @param[in] callback    IBI callback, or NULL to disable.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER / SL_STATUS_INVALID_MODE / SL_STATUS_NOT_FOUND.
 ******************************************************************************/
sl_status_t sl_i3c_ibi_configure(sl_i3c_handle_t *i3c_handle,
                                 const sl_i3c_device_desc_t *device,
                                 sl_hal_i3c_ibi_type_t type,
                                 sl_i3c_ibi_callback_t callback);

/***************************************************************************//**
 * @brief Trigger an IBI from target mode (SIR or MR).
 *
 * @details
 *   Hot-Join is generated by hardware when HJ_EN is set at init; do not
 *   pass @ref SL_HAL_I3C_IBI_TYPE_HJ.
 *   - **SIR**: call @ref sl_i3c_configure_sir() first, then trigger.
 *   - **MR**: requires DEVICE_ROLE = Controller capable via
 *     @ref sl_i3c_set_characteristics before bus enable. Flow:
 *     1. Trigger MR.
 *     2. Wait @ref SL_I3C_TARGET_EVENT_IBI_SUCCESS.
 *     3. Wait @ref SL_I3C_TARGET_EVENT_BUS_OWNER_UPDATED and
 *        @ref sl_i3c_is_current_controller().
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle (target role).
 * @param[in] type        @ref SL_HAL_I3C_IBI_TYPE_SIR or @ref SL_HAL_I3C_IBI_TYPE_MR.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if @p type is Hot-Join or unknown.
 *   - SL_STATUS_INVALID_MODE if not in target mode, or MR with Slave-Only role.
 ******************************************************************************/
sl_status_t sl_i3c_ibi_trigger(sl_i3c_handle_t *i3c_handle,
                               sl_hal_i3c_ibi_type_t type);

/***************************************************************************//**
 * @brief Configure SIR (Slave Interrupt Request) settings for target mode.
 *
 * @details Applies @p config->sir_config and writes @p config->sir_payload.
 *          Call @ref sl_i3c_ibi_trigger() with @ref SL_HAL_I3C_IBI_TYPE_SIR
 *          separately to raise the request. MR has no separate configure API —
 *          use @ref sl_i3c_ibi_trigger() with @ref SL_HAL_I3C_IBI_TYPE_MR.
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle (target role).
 * @param[in] config      SIR configuration.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER if handle/config are invalid.
 *   - SL_STATUS_INVALID_MODE if not in target mode.
 *   - SL_STATUS_BUSY if a previous SIR is still pending.
 ******************************************************************************/
sl_status_t sl_i3c_configure_sir(sl_i3c_handle_t *i3c_handle,
                                 const sl_i3c_sir_params_t *config);

/***************************************************************************//**
 * @brief Report whether this instance is the current bus controller.
 *
 * @details Reflects PRESENT_STATE.CURRENT_MASTER. After a successful MR
 *          handover, the secondary master becomes current controller.
 *
 * @param[in]  i3c_handle      Pointer to the I3C instance handle.
 * @param[out] is_controller   True if this device owns the bus.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_HANDLE / SL_STATUS_NULL_POINTER / HAL errors.
 ******************************************************************************/
sl_status_t sl_i3c_is_current_controller(sl_i3c_handle_t *i3c_handle,
                                         bool *is_controller);

/***************************************************************************//**
 * @brief Accept a Master Request by issuing directed GETACCMST (controller).
 *
 * @details Call from the IBI callback (or after) when @ref sl_i3c_ibi_info_t
 *          reports @ref SL_HAL_I3C_IBI_TYPE_MR and the request was not
 *          rejected. On success this controller relinquishes the bus; HW
 *          raises BUSOWNER_UPDATED and CURRENT_MASTER clears.
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle (controller role).
 * @param[in] device      Secondary master that issued the MR.
 * @param[in] timeout_ms  Blocking timeout for the CCC.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_MODE if not controller.
 *   - Transfer / CCC errors otherwise.
 ******************************************************************************/
sl_status_t sl_i3c_accept_mastership(sl_i3c_handle_t *i3c_handle,
                                     const sl_i3c_device_desc_t *device,
                                     uint32_t timeout_ms);

/***************************************************************************//**
 * @brief Configure (and optionally enable) a virtual target.
 *
 * @details Programs PID / MIPI ID / DCR / static address. When
 *          @p config->clear_dynamic_address is true, clears DYNAMIC_ADDR and
 *          DYNAMIC_ADDR_VALID so the active controller can assign a fresh DA.
 *          When @p config->enable is true, enables the virtual target.
 *
 * @param[in] i3c_handle            I3C instance handle (target role).
 * @param[in] virtual_target_index  Virtual target index (1..4).
 * @param[in] config                Virtual target configuration.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER / SL_STATUS_INVALID_MODE /
 *     SL_STATUS_NOT_SUPPORTED on failure.
 ******************************************************************************/
sl_status_t sl_i3c_configure_virtual_target(sl_i3c_handle_t *i3c_handle,
                                            uint8_t virtual_target_index,
                                            const sl_i3c_virtual_target_config_t *config);

/***************************************************************************//**
 * @brief Read the dynamic address assigned to a virtual target.
 *
 * @param[in]  i3c_handle            I3C instance handle (target role).
 * @param[in]  virtual_target_index  Virtual target index (1..4).
 * @param[out] dynamic_addr          7-bit dynamic address.
 *
 * @return
 *   - SL_STATUS_OK on success.
 *   - SL_STATUS_INVALID_PARAMETER / SL_STATUS_INVALID_MODE /
 *     SL_STATUS_NOT_SUPPORTED on failure.
 ******************************************************************************/
sl_status_t sl_i3c_get_virtual_target_dynamic_addr(sl_i3c_handle_t *i3c_handle,
                                                   uint8_t virtual_target_index,
                                                   uint8_t *dynamic_addr);

/***************************************************************************//**
 * @brief I3C interrupt service routine entry point.
 *
 * @details Dispatches controller or target interrupts and invokes registered
 *          callbacks. Connect to the peripheral IRQ vector or call from the
 *          board ISR.
 *
 * @param[in] i3c_handle  Pointer to the I3C instance handle.
 ******************************************************************************/
void sl_i3c_irq_handler(sl_i3c_handle_t *i3c_handle);

/** @} (end addtogroup i3c) */

#ifdef __cplusplus
}
#endif

#endif /* SL_I3C_H */
