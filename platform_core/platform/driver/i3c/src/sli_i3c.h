/***************************************************************************//**
 * @file
 * @brief I3C Driver Private API definition.
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

#ifndef SLI_I3C_H
#define SLI_I3C_H

#include "sl_i3c.h"

#if defined(I3C_PRESENT)

#include "sl_hal_i3c.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 ********************************  DEFINES  ************************************
 ******************************************************************************/

/// Role interrupt enable sets (device INTR_STATUS_EN bits; same positions as STATUS/SIGNAL).
#define SLI_I3C_CONTROLLER_INTR_MASK \
        (I3C_INTR_STATUS_EN_RESP_READY_STS_EN       \
         | I3C_INTR_STATUS_EN_TRANSFER_ABORT_STS_EN \
         | I3C_INTR_STATUS_EN_TRANSFER_ERR_STS_EN   \
         | I3C_INTR_STATUS_EN_BUSOWNER_UPDATED_STS_EN)

/// Controller IBI threshold — enable only when an IBI callback is registered.
#define SLI_I3C_IBI_INTR_MASK  I3C_INTR_STATUS_EN_IBI_THLD_STS_EN

/// TX FIFO space — enable only while a transfer still has bytes to push.
#define SLI_I3C_TX_THLD_INTR_MASK  I3C_INTR_STATUS_EN_TX_THLD_STS_EN

/// RX FIFO data — enable only while a receive still has bytes to pop.
#define SLI_I3C_RX_THLD_INTR_MASK  I3C_INTR_STATUS_EN_RX_THLD_STS_EN

/// Transfer Argument DATA_LENGTH (SDAP=0): 16 bits, 0..65535. 65536 is invalid.
#define SLI_I3C_MAX_XFER_LEN       65535U

/// Bound for IBI_QUEUE_PORT pops (IBI status+data queues are tens of DWORDs).
#define SLI_I3C_IBI_QUEUE_POP_MAX  256U

/// Target default interrupt enable set.
#define SLI_I3C_TARGET_INTR_MASK \
        (I3C_INTR_STATUS_EN_RESP_READY_STS_EN       \
         | I3C_INTR_STATUS_EN_CCC_UPDATED_STS_EN    \
         | I3C_INTR_STATUS_EN_DYN_ADDR_ASSGN_STS_EN \
         | I3C_INTR_STATUS_EN_READ_REQ_RECV_STS_EN  \
         | I3C_INTR_STATUS_EN_TRANSFER_ERR_STS_EN   \
         | I3C_INTR_STATUS_EN_IBI_UPDATED_STS_EN)

// --- Command Attribute Types ---
#define SLI_I3C_CMD_ATTR_TRANSFER_CMD    0U
#define SLI_I3C_CMD_ATTR_TRANSFER_ARG    1U
#define SLI_I3C_CMD_ATTR_SHORT_DATA_ARG  2U
#define SLI_I3C_CMD_ATTR_ADDR_ASSIGN_CMD 3U

// --- DWC MIPI I3C Command Word Bit-Field Definitions ---

// Transfer Command (CMD_ATTR = 0)
#define SLI_I3C_CMD_ATTR_SHIFT         0
#define SLI_I3C_CMD_ATTR_MASK          0x07U
#define SLI_I3C_CMD_TID_SHIFT          3
#define SLI_I3C_CMD_TID_MASK           (0x0FU << SLI_I3C_CMD_TID_SHIFT)
#define SLI_I3C_CMD_CMD_SHIFT          7
#define SLI_I3C_CMD_CMD_MASK           (0xFFU << SLI_I3C_CMD_CMD_SHIFT)
#define SLI_I3C_CMD_CP_SHIFT           15
#define SLI_I3C_CMD_CP_MASK            (0x01U << SLI_I3C_CMD_CP_SHIFT)
#define SLI_I3C_CMD_DEV_INDX_SHIFT     16
#define SLI_I3C_CMD_DEV_INDX_MASK      (0x1FU << SLI_I3C_CMD_DEV_INDX_SHIFT)
#define SLI_I3C_CMD_SPEED_SHIFT        21
#define SLI_I3C_CMD_SPEED_MASK         (0x07U << SLI_I3C_CMD_SPEED_SHIFT)
#define SLI_I3C_CMD_DBP_SHIFT          24
#define SLI_I3C_CMD_DBP_MASK           (0x01U << SLI_I3C_CMD_DBP_SHIFT)
#define SLI_I3C_CMD_ROC_SHIFT          26
#define SLI_I3C_CMD_ROC_MASK           (0x01U << SLI_I3C_CMD_ROC_SHIFT)
#define SLI_I3C_CMD_SDAP_SHIFT         27
#define SLI_I3C_CMD_SDAP_MASK          (0x01U << SLI_I3C_CMD_SDAP_SHIFT)
#define SLI_I3C_CMD_RNW_SHIFT          28
#define SLI_I3C_CMD_RNW_MASK           (0x01U << SLI_I3C_CMD_RNW_SHIFT)
#define SLI_I3C_CMD_TOC_SHIFT          30
#define SLI_I3C_CMD_TOC_MASK           (0x01U << SLI_I3C_CMD_TOC_SHIFT)
#define SLI_I3C_CMD_PEC_SHIFT          31
#define SLI_I3C_CMD_PEC_MASK           (0x01U << SLI_I3C_CMD_PEC_SHIFT)

// Transfer Argument (CMD_ATTR = 1, SDAP=0): DATA_LENGTH 0..65535
#define SLI_I3C_ARG_DB_SHIFT           8
#define SLI_I3C_ARG_DB_MASK            (0xFFU << SLI_I3C_ARG_DB_SHIFT)
#define SLI_I3C_ARG_DL_SHIFT           16
#define SLI_I3C_ARG_DL_MASK            (0xFFFFU << SLI_I3C_ARG_DL_SHIFT)

// Fixed TIDs for sl_i3c_controller_transfer_blocking() (no caller command structure).
#define SLI_I3C_WRITE_READ_WR_TID  0U
#define SLI_I3C_WRITE_READ_RD_TID  1U

/*
 * Controller command TID (CMD bits 6:3):
 *   0x0..0x7 — user-defined
 *   0x8..0xF — reserved for the I3C controller
 */
#define SLI_I3C_USER_TID_MAX   7U
#define SLI_I3C_USER_TID_MASK  0x07U

// Short Data Argument (CMD_ATTR = 2, SDAP=1): BYTE_STRB 0, 1, 3, 7
#define SLI_I3C_SHORT_BSTRB_SHIFT      3
#define SLI_I3C_SHORT_BSTRB_MASK       (0x07U << SLI_I3C_SHORT_BSTRB_SHIFT)
#define SLI_I3C_SHORT_DATA0_SHIFT      8
#define SLI_I3C_SHORT_DATA0_MASK       (0xFFU << SLI_I3C_SHORT_DATA0_SHIFT)
#define SLI_I3C_SHORT_DATA1_SHIFT      16
#define SLI_I3C_SHORT_DATA1_MASK       (0xFFU << SLI_I3C_SHORT_DATA1_SHIFT)
#define SLI_I3C_SHORT_DATA2_SHIFT      24
#define SLI_I3C_SHORT_DATA2_MASK       (0xFFU << SLI_I3C_SHORT_DATA2_SHIFT)
#define SLI_I3C_SHORT_DATA_MAX_LEN  3U

// Address Assignment Command (CMD_ATTR = 3)
#define SLI_I3C_AA_DEV_COUNT_SHIFT     21
#define SLI_I3C_AA_DEV_COUNT_MASK      (0x1FU << SLI_I3C_AA_DEV_COUNT_SHIFT)

// Response Queue Entry
#define SLI_I3C_RESP_DL_MASK           0x00FFFFFFU
#define SLI_I3C_RESP_TID_SHIFT         24
#define SLI_I3C_RESP_TID_MASK          (0x0FU << SLI_I3C_RESP_TID_SHIFT)
#define SLI_I3C_RESP_ERR_SHIFT         28
#define SLI_I3C_RESP_ERR_MASK          (0x0FU << SLI_I3C_RESP_ERR_SHIFT)

// Response Error Codes (RESPONSE.ERR_STATUS)
#define SLI_I3C_RESP_ERR_SUCCESS            0x0U
#define SLI_I3C_RESP_ERR_CRC                0x1U
#define SLI_I3C_RESP_ERR_PARITY             0x2U
#define SLI_I3C_RESP_ERR_FRAME              0x3U
#define SLI_I3C_RESP_ERR_ADDR_HEADER        0x4U
#define SLI_I3C_RESP_ERR_NACK               0x5U
#define SLI_I3C_RESP_ERR_OVERFLOW           0x6U
#define SLI_I3C_RESP_ERR_TRANSFER_ABORT     0x8U
#define SLI_I3C_RESP_ERR_I2C_W_NACK         0x9U
#define SLI_I3C_RESP_ERR_EARLY_TERMINATION  0xAU
#define SLI_I3C_RESP_ERR_GETACCCR_MISMATCH  0xBU
#define SLI_I3C_RESP_ERR_PEC                0xCU
#define SLI_I3C_RESP_ERR_HDR_BT_DELAY       0xDU
#define SLI_I3C_RESP_ERR_UNSPECIFIED        0xFU  ///< No RESPONSE_QUEUE entry (e.g. TRANSFER_ERR IRQ)

// Target-side TX command
#define SLI_I3C_TGT_CMD_DL_SHIFT       16
#define SLI_I3C_TGT_CMD_DL_MASK        (0xFFFFU << SLI_I3C_TGT_CMD_DL_SHIFT)
#define SLI_I3C_TGT_CMD_TID_SHIFT      3
#define SLI_I3C_TGT_CMD_TID_MASK       (0x0FU << SLI_I3C_TGT_CMD_TID_SHIFT)

/* Extended Transmit Command Format 2 (IC_VIRT_TGT_EN=1) — Word1 / Word2. */
#define SLI_I3C_EXT_CMD_ATTR_EXTENDED        0x1U
#define SLI_I3C_EXT_CMD_FINITE_DL            (1UL << 6)
#define SLI_I3C_EXT_CMD_EXT_DL_PRESENT       (1UL << 7)
#define SLI_I3C_EXT_CMD_VLD                  (1UL << 16)
#define SLI_I3C_EXT_CMD_CCC                  (1UL << 24)
#define SLI_I3C_EXT_CMD_ADDR_MSK_SHIFT       25
#define SLI_I3C_EXT_CMD_ADDR_OFFSET_SHIFT    28
#define SLI_I3C_EXT_CMD_DATA_LENGTH_SHIFT    16
#define SLI_I3C_EXT_CMD_DEFINING_BYTE_SHIFT  8
#define SLI_I3C_EXT_CMD_CCC_HDR_SHIFT        0
#define SLI_I3C_EXT_TX_BLR_TIMEOUT_MS        10U

// Extended Response Format 2 (target mode with virtual targets enabled).
#define SLI_I3C_EXT_RESP_DL_SHIFT      24
#define SLI_I3C_EXT_RESP_DL_MASK       (0xFFU << SLI_I3C_EXT_RESP_DL_SHIFT)
#define SLI_I3C_EXT_RESP_TID_SHIFT     16
#define SLI_I3C_EXT_RESP_TID_MASK      (0x1FU << SLI_I3C_EXT_RESP_TID_SHIFT)
#define SLI_I3C_EXT_RESP_ERR_SHIFT     8
#define SLI_I3C_EXT_RESP_ERR_MASK      (0x0FU << SLI_I3C_EXT_RESP_ERR_SHIFT)
#define SLI_I3C_EXT_RESP_CCC_SHIFT     7
#define SLI_I3C_EXT_RESP_DEFTGTS_SHIFT 5
#define SLI_I3C_EXT_RESP_FIRST_SHIFT   4
#define SLI_I3C_EXT_RESP_LAST_SHIFT    3
#define SLI_I3C_EXT_RESP_CMD_SIZE_SHIFT 12
#define SLI_I3C_EXT_RESP_CCC_MASK      (1U << SLI_I3C_EXT_RESP_CCC_SHIFT)
#define SLI_I3C_EXT_RESP_DEFTGTS_MASK  (1U << SLI_I3C_EXT_RESP_DEFTGTS_SHIFT)
#define SLI_I3C_EXT_RESP_FIRST_MASK    (1U << SLI_I3C_EXT_RESP_FIRST_SHIFT)
#define SLI_I3C_EXT_RESP_LAST_MASK     (1U << SLI_I3C_EXT_RESP_LAST_SHIFT)
#define SLI_I3C_EXT_RESP_CMD_SIZE_MASK (0x0FU << SLI_I3C_EXT_RESP_CMD_SIZE_SHIFT)

/* Format-2 TARGET_ID bit for Virtual Target N. */
#define SLI_I3C_EXT_RESP_TARGET_ID_BIT(n)  (1U << (n))

// Bits in sl_i3c_handle_t::ibi_enabled_mask (set by sl_i3c_ibi_configure).
#define SLI_I3C_IBI_EN_HJ              (1U << 0)
#define SLI_I3C_IBI_EN_SIR             (1U << 1)
#define SLI_I3C_IBI_EN_MR              (1U << 2)

// IBI_QUEUE_STATUS field helpers (IBI_QUEUE_PORT status dword).
#define SLI_I3C_IBI_STS_DATA_LEN(word)   ((uint8_t)((word) & 0xFFU))
#define SLI_I3C_IBI_STS_ID(word)         ((uint8_t)(((word) >> 8) & 0xFFU))
#define SLI_I3C_IBI_STS_LAST(word)       ((uint8_t)(((word) >> 24) & 0x01U))
#define SLI_I3C_IBI_STS_ERROR(word)      ((uint8_t)(((word) >> 30) & 0x01U))
#define SLI_I3C_IBI_STS_REJECT(word)     ((uint8_t)(((word) >> 31) & 0x01U))

/** Target SLV_IBI_RESP.IBI_STS encodings. */
#define SLI_I3C_SLV_IBI_STS_NONE             0x0U
#define SLI_I3C_SLV_IBI_STS_SUCCESS          0x1U
#define SLI_I3C_SLV_IBI_STS_EARLY_TERMINATE  0x2U
#define SLI_I3C_SLV_IBI_STS_NOT_ATTEMPTED    0x3U

/*******************************************************************************
 *******************************   ENUMS   *************************************
 ******************************************************************************/

/// PRESENT_STATE.CM_TFR_STS — controller transfer type.
typedef enum {
  SLI_I3C_CTRL_TFR_TYPE_IDLE              = 0x00, ///< Idle
  SLI_I3C_CTRL_TFR_TYPE_BCAST_CCC_WR      = 0x01, ///< Broadcast CCC write
  SLI_I3C_CTRL_TFR_TYPE_DIR_CCC_WR        = 0x02, ///< Directed CCC write
  SLI_I3C_CTRL_TFR_TYPE_DIR_CCC_RD        = 0x03, ///< Directed CCC read
  SLI_I3C_CTRL_TFR_TYPE_ENTDAA            = 0x04, ///< ENTDAA address assignment
  SLI_I3C_CTRL_TFR_TYPE_SETDASA           = 0x05, ///< SETDASA address assignment
  SLI_I3C_CTRL_TFR_TYPE_PRIV_I3C_SDR_WR   = 0x06, ///< Private I3C SDR write
  SLI_I3C_CTRL_TFR_TYPE_PRIV_I3C_SDR_RD   = 0x07, ///< Private I3C SDR read
  SLI_I3C_CTRL_TFR_TYPE_PRIV_I2C_SDR_WR   = 0x08, ///< Private I2C SDR write
  SLI_I3C_CTRL_TFR_TYPE_PRIV_I2C_SDR_RD   = 0x09, ///< Private I2C SDR read
  SLI_I3C_CTRL_TFR_TYPE_PRIV_HDR_TS_WR    = 0x0A, ///< Private HDR-TS write
  SLI_I3C_CTRL_TFR_TYPE_PRIV_HDR_TS_RD    = 0x0B, ///< Private HDR-TS read
  SLI_I3C_CTRL_TFR_TYPE_PRIV_HDR_DDR_WR   = 0x0C, ///< Private HDR-DDR write
  SLI_I3C_CTRL_TFR_TYPE_PRIV_HDR_DDR_RD   = 0x0D, ///< Private HDR-DDR read
  SLI_I3C_CTRL_TFR_TYPE_SERVICING_IBI     = 0x0E, ///< Servicing IBI
  SLI_I3C_CTRL_TFR_TYPE_HALT              = 0x0F, ///< Controller halt
} sli_i3c_ctrl_transfer_type_t;

/// PRESENT_STATE.CM_TFR_STS — target transfer type.
typedef enum {
  SLI_I3C_TGT_TFR_TYPE_IDLE               = 0x00, ///< Idle
  SLI_I3C_TGT_TFR_TYPE_HOT_JOIN           = 0x01, ///< Hot-Join transfer
  SLI_I3C_TGT_TFR_TYPE_IBI                = 0x02, ///< IBI transfer
  SLI_I3C_TGT_TFR_TYPE_MASTER_WRITE       = 0x03, ///< Controller write ongoing
  SLI_I3C_TGT_TFR_TYPE_READ_PREFETCH      = 0x04, ///< Read data prefetch
  SLI_I3C_TGT_TFR_TYPE_MASTER_READ        = 0x05, ///< Controller read ongoing
  SLI_I3C_TGT_TFR_TYPE_HALT               = 0x06, ///< Target halt
} sli_i3c_tgt_transfer_type_t;

/// PRESENT_STATE.CM_TFR_ST_STS — controller transfer FSM state.
typedef enum {
  SLI_I3C_CTRL_TFR_ST_IDLE                = 0x00, ///< Idle
  SLI_I3C_CTRL_TFR_ST_START               = 0x01, ///< START generation
  SLI_I3C_CTRL_TFR_ST_RESTART             = 0x02, ///< RESTART generation
  SLI_I3C_CTRL_TFR_ST_STOP                = 0x03, ///< STOP generation
  SLI_I3C_CTRL_TFR_ST_START_HOLD          = 0x04, ///< START hold (target-initiated)
  SLI_I3C_CTRL_TFR_ST_BCAST_WR_HDR        = 0x05, ///< Broadcast write header (7E,W)
  SLI_I3C_CTRL_TFR_ST_BCAST_RD_HDR        = 0x06, ///< Broadcast read header (7E,R)
  SLI_I3C_CTRL_TFR_ST_DAA                 = 0x07, ///< Dynamic address assignment
  SLI_I3C_CTRL_TFR_ST_SLAVE_ADDR          = 0x08, ///< Target address generation
  SLI_I3C_CTRL_TFR_ST_CCC_BYTE            = 0x0B, ///< CCC byte generation
  SLI_I3C_CTRL_TFR_ST_HDR_CMD             = 0x0C, ///< HDR command generation
  SLI_I3C_CTRL_TFR_ST_WRITE_DATA          = 0x0D, ///< Write data transfer
  SLI_I3C_CTRL_TFR_ST_READ_DATA           = 0x0E, ///< Read data transfer
  SLI_I3C_CTRL_TFR_ST_IBI_READ            = 0x0F, ///< IBI (SIR) read data
  SLI_I3C_CTRL_TFR_ST_IBI_AUTO_DISABLE    = 0x10, ///< IBI auto-disable
  SLI_I3C_CTRL_TFR_ST_HDR_DDR_CRC         = 0x11, ///< HDR-DDR CRC
  SLI_I3C_CTRL_TFR_ST_CLOCK_EXT           = 0x12, ///< Clock extension
  SLI_I3C_CTRL_TFR_ST_HALT                = 0x13, ///< Halt
} sli_i3c_ctrl_transfer_state_t;

/*******************************************************************************
 *******************************   STRUCTS   ***********************************
 ******************************************************************************/

/// Internal transfer completion callback (blocking path / IRQ).
typedef sl_status_t (*sli_i3c_transfer_callback_t)(sl_i3c_handle_t *i3c_handle,
                                                   sl_i3c_transfer_event_t event);

/// Internal transfer state tracking (stored as uint8_t in the public handle).
typedef enum {
  SLI_I3C_XFER_IDLE = 0,
  SLI_I3C_XFER_CCC_PENDING,
  SLI_I3C_XFER_WRITE_PENDING,
  SLI_I3C_XFER_READ_PENDING,
  SLI_I3C_XFER_WRITE_READ_PENDING,
  SLI_I3C_XFER_DAA_PENDING,
  SLI_I3C_XFER_TARGET_TX_PENDING,
  SLI_I3C_XFER_TARGET_RX_PENDING,
} sli_i3c_xfer_state_t;

/// Parsed response queue entry (internal only).
typedef struct {
  uint32_t data_length;
  uint8_t  tid;
  uint8_t  err_status;
  uint8_t  cmd_size;  ///< Format-2 CMD_SIZE (valid when first==true)
  bool     first;     ///< Format-2 FIRST (write chunk start)
  bool     last;      ///< Format-2 LAST (write chunk end)
  bool     deftgts;   ///< Format-2 DEFTGTS CCC (no XFER payload)
  bool     ccc;       ///< Format-2 CCC write (vs private write)
} sli_i3c_response_entry_t;

/// Driver context is the public caller-owned handle.
typedef sl_i3c_handle_t sli_i3c_context_t;

/*******************************************************************************
 ****************************   INLINE HELPERS   *******************************
 ******************************************************************************/
static inline uint32_t sli_i3c_form_transfer_command_from_struct(
  const sl_i3c_transfer_cmd_t *cmd,
  bool is_i2c_device)
{
  uint8_t speed = is_i2c_device
                  ? (uint8_t)cmd->speed.i2c_speed
                  : (uint8_t)cmd->speed.i3c_speed;

  uint32_t word = (cmd->cmd_attr & SLI_I3C_CMD_ATTR_MASK);
  word |= ((uint32_t)(cmd->tid & 0x0F))       << SLI_I3C_CMD_TID_SHIFT;
  word |= ((uint32_t)(cmd->cmd))               << SLI_I3C_CMD_CMD_SHIFT;
  word |= ((uint32_t)(cmd->cp ? 1 : 0))        << SLI_I3C_CMD_CP_SHIFT;
  word |= ((uint32_t)(cmd->dev_indx & 0x1F))  << SLI_I3C_CMD_DEV_INDX_SHIFT;
  word |= ((uint32_t)(speed & 0x07U))          << SLI_I3C_CMD_SPEED_SHIFT;
  word |= ((uint32_t)(cmd->dbp ? 1 : 0))       << SLI_I3C_CMD_DBP_SHIFT;
  word |= ((uint32_t)(cmd->roc ? 1 : 0))       << SLI_I3C_CMD_ROC_SHIFT;
  word |= ((uint32_t)(cmd->sdap ? 1 : 0))      << SLI_I3C_CMD_SDAP_SHIFT;
  word |= ((uint32_t)(cmd->rnw ? 1 : 0))       << SLI_I3C_CMD_RNW_SHIFT;
  word |= ((uint32_t)(cmd->toc ? 1 : 0))       << SLI_I3C_CMD_TOC_SHIFT;
  word |= ((uint32_t)(cmd->pec ? 1 : 0))       << SLI_I3C_CMD_PEC_SHIFT;
  return word;
}

static inline uint32_t sli_i3c_form_transfer_command(uint8_t tid,
                                                     uint8_t cmd,
                                                     uint8_t dev_index,
                                                     uint8_t speed,
                                                     bool rnw,
                                                     bool roc,
                                                     bool toc,
                                                     bool cp,
                                                     bool dbp)
{
  uint32_t word = SLI_I3C_CMD_ATTR_TRANSFER_CMD;
  word |= ((uint32_t)(tid & 0x0F))       << SLI_I3C_CMD_TID_SHIFT;
  word |= ((uint32_t)(cmd))              << SLI_I3C_CMD_CMD_SHIFT;
  word |= ((uint32_t)(cp ? 1 : 0))       << SLI_I3C_CMD_CP_SHIFT;
  word |= ((uint32_t)(dev_index & 0x1F)) << SLI_I3C_CMD_DEV_INDX_SHIFT;
  word |= ((uint32_t)(speed & 0x07U))    << SLI_I3C_CMD_SPEED_SHIFT;
  word |= ((uint32_t)(dbp ? 1 : 0))      << SLI_I3C_CMD_DBP_SHIFT;
  word |= ((uint32_t)(roc ? 1 : 0))      << SLI_I3C_CMD_ROC_SHIFT;
  word |= ((uint32_t)(rnw ? 1 : 0))      << SLI_I3C_CMD_RNW_SHIFT;
  word |= ((uint32_t)(toc ? 1 : 0))      << SLI_I3C_CMD_TOC_SHIFT;
  return word;
}

static inline uint32_t sli_i3c_form_transfer_argument(uint16_t data_length,
                                                      uint8_t defining_byte)
{
  uint32_t word = SLI_I3C_CMD_ATTR_TRANSFER_ARG;
  word |= ((uint32_t)defining_byte) << SLI_I3C_ARG_DB_SHIFT;
  word |= ((uint32_t)data_length)   << SLI_I3C_ARG_DL_SHIFT;
  return word;
}

static inline uint32_t sli_i3c_form_short_data_argument(uint8_t byte_strobe,
                                                        uint8_t data0,
                                                        uint8_t data1,
                                                        uint8_t data2)
{
  uint32_t word = SLI_I3C_CMD_ATTR_SHORT_DATA_ARG;
  word |= ((uint32_t)(byte_strobe & 0x07)) << SLI_I3C_SHORT_BSTRB_SHIFT;
  word |= ((uint32_t)data0) << SLI_I3C_SHORT_DATA0_SHIFT;
  word |= ((uint32_t)data1) << SLI_I3C_SHORT_DATA1_SHIFT;
  word |= ((uint32_t)data2) << SLI_I3C_SHORT_DATA2_SHIFT;
  return word;
}

static inline uint32_t sli_i3c_form_address_assignment_command(uint8_t tid,
                                                               uint8_t cmd,
                                                               uint8_t dev_index,
                                                               uint8_t dev_count,
                                                               bool roc,
                                                               bool toc)
{
  uint32_t word = SLI_I3C_CMD_ATTR_ADDR_ASSIGN_CMD;
  word |= ((uint32_t)(tid & 0x0F))       << SLI_I3C_CMD_TID_SHIFT;
  word |= ((uint32_t)(cmd))              << SLI_I3C_CMD_CMD_SHIFT;
  word |= ((uint32_t)(dev_index & 0x1F)) << SLI_I3C_CMD_DEV_INDX_SHIFT;
  word |= ((uint32_t)(dev_count & 0x1F)) << SLI_I3C_AA_DEV_COUNT_SHIFT;
  word |= ((uint32_t)(roc ? 1 : 0))      << SLI_I3C_CMD_ROC_SHIFT;
  word |= ((uint32_t)(toc ? 1 : 0))      << SLI_I3C_CMD_TOC_SHIFT;
  return word;
}

static inline void sli_i3c_parse_response(uint32_t resp_word,
                                          sli_i3c_response_entry_t *resp)
{
  resp->data_length = resp_word & SLI_I3C_RESP_DL_MASK;
  resp->tid         = (uint8_t)((resp_word & SLI_I3C_RESP_TID_MASK) >> SLI_I3C_RESP_TID_SHIFT);
  resp->err_status  = (uint8_t)((resp_word & SLI_I3C_RESP_ERR_MASK) >> SLI_I3C_RESP_ERR_SHIFT);
  resp->cmd_size    = 0U;
  resp->first       = false;
  resp->last        = true;
  resp->deftgts     = false;
  resp->ccc         = false;
}

/// Parse Extended Response Format 2 used in target mode when virtual targets are enabled.
static inline void sli_i3c_parse_target_ext_response(uint32_t resp_word,
                                                     sli_i3c_response_entry_t *resp)
{
  resp->data_length = (resp_word & SLI_I3C_EXT_RESP_DL_MASK) >> SLI_I3C_EXT_RESP_DL_SHIFT;
  resp->tid         = (uint8_t)((resp_word & SLI_I3C_EXT_RESP_TID_MASK) >> SLI_I3C_EXT_RESP_TID_SHIFT);
  resp->err_status  = (uint8_t)((resp_word & SLI_I3C_EXT_RESP_ERR_MASK) >> SLI_I3C_EXT_RESP_ERR_SHIFT);
  resp->cmd_size    = (uint8_t)((resp_word & SLI_I3C_EXT_RESP_CMD_SIZE_MASK)
                                >> SLI_I3C_EXT_RESP_CMD_SIZE_SHIFT);
  resp->first       = (resp_word & SLI_I3C_EXT_RESP_FIRST_MASK) != 0U;
  resp->last        = (resp_word & SLI_I3C_EXT_RESP_LAST_MASK) != 0U;
  resp->deftgts     = (resp_word & SLI_I3C_EXT_RESP_DEFTGTS_MASK) != 0U;
  resp->ccc         = (resp_word & SLI_I3C_EXT_RESP_CCC_MASK) != 0U;
}

static inline sl_i3c_transfer_event_t sli_i3c_resp_err_to_event(uint8_t err_status)
{
  switch (err_status) {
    case SLI_I3C_RESP_ERR_SUCCESS:
      return SL_I3C_EVENT_TRANSFER_COMPLETED;
    case SLI_I3C_RESP_ERR_TRANSFER_ABORT:
      return SL_I3C_EVENT_TRANSFER_ABORTED;
    default:
      return SL_I3C_EVENT_TRANSFER_ERROR;
  }
}

/***************************************************************************//**
 * @brief Map RESPONSE ERR_STATUS to an @ref sl_status_t (I2C-style mapping).
 ******************************************************************************/
static inline sl_status_t sli_i3c_resp_err_to_status(uint8_t err_status)
{
  switch (err_status) {
    case SLI_I3C_RESP_ERR_SUCCESS:
      return SL_STATUS_OK;
    case SLI_I3C_RESP_ERR_ADDR_HEADER:
    case SLI_I3C_RESP_ERR_NACK:
      return SL_STATUS_NOT_FOUND;
    case SLI_I3C_RESP_ERR_I2C_W_NACK:
    case SLI_I3C_RESP_ERR_TRANSFER_ABORT:
    case SLI_I3C_RESP_ERR_EARLY_TERMINATION:
      return SL_STATUS_ABORT;
    case SLI_I3C_RESP_ERR_OVERFLOW:
      return SL_STATUS_HAS_OVERFLOWED;
    case SLI_I3C_RESP_ERR_CRC:
    case SLI_I3C_RESP_ERR_PARITY:
    case SLI_I3C_RESP_ERR_FRAME:
    case SLI_I3C_RESP_ERR_PEC:
      return SL_STATUS_IO;
    case SLI_I3C_RESP_ERR_GETACCCR_MISMATCH:
    case SLI_I3C_RESP_ERR_HDR_BT_DELAY:
    case SLI_I3C_RESP_ERR_UNSPECIFIED:
    default:
      return SL_STATUS_FAIL;
  }
}

static inline uint32_t sli_i3c_form_target_tx_command(uint16_t data_length,
                                                      uint8_t tid)
{
  uint32_t word = 0;
  word |= ((uint32_t)(tid & 0x0F))      << SLI_I3C_TGT_CMD_TID_SHIFT;
  word |= ((uint32_t)data_length)        << SLI_I3C_TGT_CMD_DL_SHIFT;
  return word;
}

/***************************************************************************//**
 * @brief Build Format-2 EXT_CMD Word1 (program EXT_CMD_REG_x last, with VLD).
 *
 * @param[in] addr_offset  ADDR_OFFSET (0 = T0/physical, 1..4 = virtual targets).
 * @param[in] finite_dl    True for finite length (FINITE_DL=1).
 * @param[in] is_ccc       True for directed CCC read match (CCC=1).
 *
 * @return Packed Word1 value.
 ******************************************************************************/
static inline uint32_t sli_i3c_form_ext_cmd_word1(uint8_t addr_offset,
                                                  bool finite_dl,
                                                  bool is_ccc)
{
  uint32_t word = SLI_I3C_EXT_CMD_ATTR_EXTENDED | SLI_I3C_EXT_CMD_VLD;

  word |= ((uint32_t)(addr_offset & 0x0FU)) << SLI_I3C_EXT_CMD_ADDR_OFFSET_SHIFT;
  if (finite_dl) {
    word |= SLI_I3C_EXT_CMD_FINITE_DL;
  }
  if (is_ccc) {
    word |= SLI_I3C_EXT_CMD_CCC;
  }
  return word;
}

/***************************************************************************//**
 * @brief Build Format-2 EXT_CMD Word2 (program EXT_CMD_REG_x_2 before Word1).
 *
 * @param[in] data_length     Finite read length in bytes.
 * @param[in] defining_byte   Defining byte match (0 if unused / private read).
 * @param[in] ccc_hdr_header  CCC code or 0x00 for SDR private read.
 *
 * @return Packed Word2 value.
 ******************************************************************************/
static inline uint32_t sli_i3c_form_ext_cmd_word2(uint16_t data_length,
                                                  uint8_t defining_byte,
                                                  uint8_t ccc_hdr_header)
{
  uint32_t word = ((uint32_t)data_length) << SLI_I3C_EXT_CMD_DATA_LENGTH_SHIFT;

  word |= ((uint32_t)defining_byte) << SLI_I3C_EXT_CMD_DEFINING_BYTE_SHIFT;
  word |= ((uint32_t)ccc_hdr_header) << SLI_I3C_EXT_CMD_CCC_HDR_SHIFT;
  return word;
}

/***************************************************************************//**
 * @brief Encode a 7-bit ID with odd parity in bit 0 (I3C address-byte style).
 ******************************************************************************/
static inline uint8_t sli_i3c_encode_odd_parity_byte(uint8_t id_7bit)
{
  uint8_t v = id_7bit & (uint8_t)_I3C_DEVICE_ADDR_STATIC_ADDR_MASK;
  uint8_t ones = 0U;

  for (uint8_t t = v; t != 0U; t >>= 1) {
    ones = (uint8_t)(ones + (t & 1U));
  }
  return (uint8_t)((v << 1) | ((ones & 1U) ? 0U : 1U));
}

/***************************************************************************//**
 * @brief Decode a 7-bit ID from an odd-parity byte; return false on parity fail.
 ******************************************************************************/
static inline bool sli_i3c_decode_odd_parity_byte(uint8_t byte, uint8_t *id_7bit)
{
  uint8_t id;

  if (id_7bit == NULL) {
    return false;
  }
  id = (uint8_t)(byte >> 1);
  if (sli_i3c_encode_odd_parity_byte(id) != byte) {
    return false;
  }
  *id_7bit = id;
  return true;
}

/***************************************************************************//**
 * @brief Parse one DEV_CHAR_TABLE entry (LOC1..LOC4) using device field macros.
 *
 * @note DCT registers are HW-writable / SW-read-only (@c __IM).
 ******************************************************************************/
static inline void sli_i3c_parse_dct_entry(uint32_t loc1,
                                           uint32_t loc2,
                                           uint32_t loc3,
                                           uint32_t loc4,
                                           uint8_t pid[6],
                                           uint8_t *bcr,
                                           uint8_t *dcr,
                                           uint8_t *dynamic_addr)
{
  uint32_t msb = (loc1 & _I3C_DEV_CHAR_TABLE1_LOC1_MSB_PROVISIONAL_ID_MASK)
                 >> _I3C_DEV_CHAR_TABLE1_LOC1_MSB_PROVISIONAL_ID_SHIFT;
  uint32_t lsb = (loc2 & _I3C_DEV_CHAR_TABLE1_LOC2_LSB_PROVISIONAL_ID_MASK)
                 >> _I3C_DEV_CHAR_TABLE1_LOC2_LSB_PROVISIONAL_ID_SHIFT;
  uint8_t da_byte = (uint8_t)((loc4 & _I3C_DEV_CHAR_TABLE1_LOC4_DEV_DYNAMIC_ADDR_MASK)
                              >> _I3C_DEV_CHAR_TABLE1_LOC4_DEV_DYNAMIC_ADDR_SHIFT);

  if (pid != NULL) {
    pid[0] = (uint8_t)(msb >> 24);
    pid[1] = (uint8_t)(msb >> 16);
    pid[2] = (uint8_t)(msb >> 8);
    pid[3] = (uint8_t)(msb);
    pid[4] = (uint8_t)(lsb >> 8);
    pid[5] = (uint8_t)(lsb);
  }
  if (dcr != NULL) {
    *dcr = (uint8_t)((loc3 & _I3C_DEV_CHAR_TABLE1_LOC3_DCR_MASK)
                     >> _I3C_DEV_CHAR_TABLE1_LOC3_DCR_SHIFT);
  }
  if (bcr != NULL) {
    *bcr = (uint8_t)((loc3 & _I3C_DEV_CHAR_TABLE1_LOC3_BCR_MASK)
                     >> _I3C_DEV_CHAR_TABLE1_LOC3_BCR_SHIFT);
  }
  if (dynamic_addr != NULL) {
    /* LOC4 is an 8-bit address byte; soft state stores the 7-bit ID. */
    *dynamic_addr = da_byte & (uint8_t)_I3C_DEVICE_ADDR_STATIC_ADDR_MASK;
  }
}

/***************************************************************************//**
 * @brief Read PRESENT_STATE.CM_TFR_STS as a controller transfer-type enum.
 ******************************************************************************/
static inline sl_status_t sli_i3c_get_ctrl_transfer_type(I3C_TypeDef *i3c,
                                                         sli_i3c_ctrl_transfer_type_t *type)
{
  uint32_t raw = 0U;

  if (type == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  sl_status_t status = sl_hal_i3c_get_transfer_type(i3c, &raw);
  if (status != SL_STATUS_OK) {
    return status;
  }
  *type = (sli_i3c_ctrl_transfer_type_t)raw;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief Read PRESENT_STATE.CM_TFR_STS as a target transfer-type enum.
 ******************************************************************************/
static inline sl_status_t sli_i3c_get_tgt_transfer_type(I3C_TypeDef *i3c,
                                                        sli_i3c_tgt_transfer_type_t *type)
{
  uint32_t raw = 0U;

  if (type == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  sl_status_t status = sl_hal_i3c_get_transfer_type(i3c, &raw);
  if (status != SL_STATUS_OK) {
    return status;
  }
  *type = (sli_i3c_tgt_transfer_type_t)raw;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief Read PRESENT_STATE.CM_TFR_ST_STS as a controller FSM-state enum.
 ******************************************************************************/
static inline sl_status_t sli_i3c_get_ctrl_transfer_state(I3C_TypeDef *i3c,
                                                          sli_i3c_ctrl_transfer_state_t *state)
{
  uint32_t raw = 0U;

  if (state == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  sl_status_t status = sl_hal_i3c_get_controller_transfer_status(i3c, &raw);
  if (status != SL_STATUS_OK) {
    return status;
  }
  *state = (sli_i3c_ctrl_transfer_state_t)raw;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * @brief Check whether the peripheral is halted (waiting for RESUME).
 *
 * @param[in]  i3c             Peripheral register block.
 * @param[in]  is_controller   True if CURRENT_MASTER.
 * @param[out] is_halted       Set true when halted.
 *
 * @return SL_STATUS_OK on success.
 ******************************************************************************/
static inline sl_status_t sli_i3c_is_halted(I3C_TypeDef *i3c,
                                            bool is_controller,
                                            bool *is_halted)
{
  sl_status_t status;

  if (is_halted == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  *is_halted = false;

  if (is_controller) {
    sli_i3c_ctrl_transfer_type_t tfr_type;
    sli_i3c_ctrl_transfer_state_t tfr_st;

    status = sli_i3c_get_ctrl_transfer_type(i3c, &tfr_type);
    if (status != SL_STATUS_OK) {
      return status;
    }
    status = sli_i3c_get_ctrl_transfer_state(i3c, &tfr_st);
    if (status != SL_STATUS_OK) {
      return status;
    }
    *is_halted = ((tfr_st == SLI_I3C_CTRL_TFR_ST_HALT)
                  || (tfr_type == SLI_I3C_CTRL_TFR_TYPE_HALT));
  } else {
    sli_i3c_tgt_transfer_type_t tfr_type;

    status = sli_i3c_get_tgt_transfer_type(i3c, &tfr_type);
    if (status != SL_STATUS_OK) {
      return status;
    }
    *is_halted = (tfr_type == SLI_I3C_TGT_TFR_TYPE_HALT);
  }
  return SL_STATUS_OK;
}

/*******************************************************************************
 *****************************   PROTOTYPES   **********************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Configure hardware and interrupts after context setup.
 *
 * @details Called from @ref sl_i3c_init(). Resets the peripheral, applies role-
 *          specific timing/thresholds, and enables the NVIC line.
 *
 * @param[in] ctx          Driver context for the instance.
 * @param[in] init_params  Initialization parameters from the public API.
 *
 * @return SL_STATUS_OK on success, or an error from the HAL layer.
 ******************************************************************************/
sl_status_t sli_i3c_init_core(sli_i3c_context_t *ctx,
                              const sl_i3c_init_params_t *init_params);

/***************************************************************************//**
 * @brief Handle pending controller-mode interrupts for one instance.
 *
 * @param[in] ctx  Driver context for the instance.
 ******************************************************************************/
void sli_i3c_controller_dispatch_interrupt(sli_i3c_context_t *ctx);

/***************************************************************************//**
 * @brief Handle pending target-mode interrupts for one instance.
 *
 * @param[in] ctx  Driver context for the instance.
 ******************************************************************************/
void sli_i3c_target_dispatch_interrupt(sli_i3c_context_t *ctx);

/***************************************************************************//**
 * @brief Find a device DAT index by 7-bit static or dynamic address.
 *
 * @param[in] ctx      Driver context.
 * @param[in] address  7-bit address to match.
 *
 * @return DAT index (0..SL_HAL_I3C_MAX_TARGET_COUNT-1), or -1 if not found.
 ******************************************************************************/
int8_t sli_i3c_find_dev_index(sli_i3c_context_t *ctx, uint8_t address);

/***************************************************************************//**
 * @brief Write bytes into the TX FIFO while TX_BUF_EMPTY_LOC is non-zero.
 *
 * @param[in] i3c  Peripheral register block.
 * @param[in] buf  Source buffer.
 * @param[in] len  Number of bytes requested.
 *
 * @return Number of bytes written (may be less than @p len when the FIFO is full).
 ******************************************************************************/
uint32_t sli_i3c_write_tx_fifo(I3C_TypeDef *i3c, const uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* defined(I3C_PRESENT) */
#endif /* SLI_I3C_H */
