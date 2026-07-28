/***************************************************************************//**
 * @file
 * @brief Inter-IC Sound (I2S) Peripheral API
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
#ifndef SL_HAL_I2S_H
#define SL_HAL_I2S_H

#include "em_device.h"
#if defined(I2S_PRESENT)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sl_assert.h"
#include "sl_device_i2s.h"
#include "sl_status.h"

#ifndef SL_HAL_I2S_TDM_SUPPORT
#define SL_HAL_I2S_TDM_SUPPORT 1
#endif

/*******************************************************************************
 *********************************   DEFINES   *********************************
 ******************************************************************************/
/// Helper macros to identify instance type
#define SL_HAL_I2S_IS_INSTANCE(ref)    (I2S_NUM((I2S_TypeDef *)(ref)) != -1)
#if defined(I2ST_PRESENT)
#define SL_HAL_I2ST_IS_INSTANCE(ref)   (I2ST_NUM((I2ST_TypeDef *)(ref)) != -1)
/// True if @a ref is a valid I2S or I2ST peripheral base pointer for this part
#define SL_HAL_I2S_REF_VALID(ref) \
  (SL_HAL_I2S_IS_INSTANCE(ref) || SL_HAL_I2ST_IS_INSTANCE(ref))
#else
#define SL_HAL_I2ST_IS_INSTANCE(ref)   (0)
/// True if @a ref is a valid I2S peripheral base pointer for this part
#define SL_HAL_I2S_REF_VALID(ref)      SL_HAL_I2S_IS_INSTANCE(ref)
#endif

/// I2S channel register stride in bytes (derived from register structure)
#define SL_HAL_I2S_CHANNEL_STRIDE_BYTES \
  (offsetof(I2S_TypeDef, RER1) - offsetof(I2S_TypeDef, RER0))

_Static_assert((offsetof(I2S_TypeDef, RER1) - offsetof(I2S_TypeDef, RER0)) \
               == (offsetof(I2S_TypeDef, RER2) - offsetof(I2S_TypeDef, RER1)), \
               "I2S_TypeDef channel stride must be uniform (RER0-RER1 vs RER1-RER2)");

/// Helper macro to compute channel register pointer
/// @param i2s          Pointer to I2S peripheral register block
/// @param reg          Base struct member for channel 0 (same name as in @c I2S_TypeDef)
/// @param channel_id   Channel number (0, 1, 2, etc.)
#define SL_HAL_I2S_CHANNEL_REG_PTR(i2s, reg, channel_id) \
  ((volatile uint32_t *)((uintptr_t)&(i2s)->reg + ((channel_id) * SL_HAL_I2S_CHANNEL_STRIDE_BYTES)))

/// Helper macro to validate channel number based on instance type
/// @param i2s          Pointer to I2S peripheral register block
/// @param channel_id   Channel number to validate
#if defined(I2ST_PRESENT)
#define SL_HAL_I2S_VALIDATE_CHANNEL(i2s, channel_id) \
  ((SL_HAL_I2S_IS_INSTANCE(i2s) && ((channel_id) < SL_I2S_MAX_CHANNELS)) \
   || (SL_HAL_I2ST_IS_INSTANCE(i2s) && ((channel_id) < SL_I2ST_MAX_CHANNELS)))
#else
#define SL_HAL_I2S_VALIDATE_CHANNEL(i2s, channel_id) \
  (SL_HAL_I2S_IS_INSTANCE(i2s) && ((channel_id) < SL_I2S_MAX_CHANNELS))
#endif

/**
 * @brief HAL pointer for I2S0 or I2ST0.
 *
 * Device Manager (@ref sl_device_peripheral.h) defines two types — @c I2S_TypeDef
 * and @c I2ST_TypeDef — and separate base getters for I2S0 and I2ST0.
 *
 * This HAL has one API for both blocks. Parameters use @c I2S_TypeDef_t (same as
 * @c I2S_TypeDef) because I2ST uses the same register layout for the fields we
 * program on parts with @c I2ST_PRESENT.
 *
 * Pass the pointer from Device Manager. Use @ref SL_HAL_I2S_IS_INSTANCE and
 * @ref SL_HAL_I2ST_IS_INSTANCE when I2S vs I2ST rules differ (channels, TDM, mode).
 */
typedef I2S_TypeDef I2S_TypeDef_t;

/*******************************************************************************
 ********************************   ENUMS   ************************************
 ******************************************************************************/

/*******************************************************************************
 *******************************   STRUCTS   ***********************************
 ******************************************************************************/

/// Init parameters (mode, format, endianness).
typedef struct {
  sl_i2s_operating_mode_t mode;
  sl_i2s_data_format_t format;
  sl_i2s_endianness_t endianness;
} sl_hal_i2s_init_t;

#if SL_HAL_I2S_TDM_SUPPORT
/// TDM slot count, slot length, and TX/RX slot enables.
typedef struct {
  uint32_t num_of_slots;
  sl_i2s_tdm_slot_length_t length;
  uint16_t tx_slots;
  uint16_t rx_slots;
} sl_hal_i2s_tdm_config_t;
#endif // SL_HAL_I2S_TDM_SUPPORT

/// Per-channel configuration (no GPIO; application or HLD supplies pins).
typedef struct {
  sl_i2s_channel_id_t id;
  sl_i2s_direction_t direction;
  sl_i2s_word_length_t tx_length;
  sl_i2s_word_length_t rx_length;
  sl_i2s_fifo_trigger_level_t tx_level;
  sl_i2s_fifo_trigger_level_t rx_level;
} sl_hal_i2s_channel_config_t;

/*******************************************************************************
 *****************************   PROTOTYPES   **********************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Initialize the I2S peripheral.
 *
 * Calls @ref sl_hal_i2s_reset, then applies operating mode, data format
 * (standard/TDM/LJ/RJ), and serial data endianness. 
 * Bus clock enable/disable remains the application's responsibility (Clock Manager)
 *
 * @param[in] i2s  I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] init Initialization parameters (@ref sl_hal_i2s_init_t).
 *
 * @note On **I2ST**, @c init->mode must be @ref SL_I2S_OPERATING_MODE_TARGET.
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_NULL_POINTER if @p init is NULL.
 *         SL_STATUS_INVALID_PARAMETER if an enum field in @p init is out of range,
 *         or on I2ST if @c init->mode is not @ref SL_I2S_OPERATING_MODE_TARGET.
 ******************************************************************************/
sl_status_t sl_hal_i2s_init(I2S_TypeDef_t *i2s, const sl_hal_i2s_init_t *init);

/***************************************************************************//**
 * @brief Deinitialize the I2S peripheral (manual teardown).
 *
 * Returns the peripheral to hardware default / idle state by calling
 * @ref sl_hal_i2s_reset. Bus clock enable/disable remains the application's responsibility
 * Use this when the application is done with the block and wants an explicit lifecycle
 * pair to @ref sl_hal_i2s_init.
 *
 * Applications should use this API for teardown.
 *
 * After deinit, call @ref sl_hal_i2s_init again before using the peripheral.
 * Pin configuration is owned by the application, not this API.
 *
 * @param[in] i2s I2S (Controller/Target) or I2ST (Target only) peripheral.
 ******************************************************************************/
void sl_hal_i2s_deinit(I2S_TypeDef_t *i2s);

/***************************************************************************//**
 * @brief Reset the I2S peripheral to hardware default / idle state.
 *
 * Stops serial clock and block paths, quiesces channels, clears sticky error
 * latches, flushes FIFOs, masks interrupts, restores default registers, and
 * disables all per-channel enables. Bus clock enable/disable remains the
 * application's responsibility (Clock Manager).
 *
 * Called from @ref sl_hal_i2s_init and @ref sl_hal_i2s_deinit; may also be
 * used directly when a full register reset is needed without re-applying init
 * parameters.
 *
 * @param[in] i2s I2S (Controller/Target) or I2ST (Target only) peripheral.
 ******************************************************************************/
void sl_hal_i2s_reset(I2S_TypeDef_t *i2s);

#if SL_HAL_I2S_TDM_SUPPORT

/***************************************************************************//**
 * @brief Configure TDM slot count, slot length, and TX/RX slot enables.
 *
 * Applies slot count, slot length, and which TDM slots are enabled for transmit and receive.
 *
 * @note \c num_of_slots must be 1–@ref SL_I2S_MAX_TDM_SLOTS on I2S. On I2ST (when
 *       \c I2ST_PRESENT), use @ref SL_I2ST_MAX_TDM_SLOTS as the upper limit.
 *
 * @param[in] i2s    I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] config TDM parameters (@ref sl_hal_i2s_tdm_config_t).
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_NULL_POINTER if @p config is NULL.
 *         SL_STATUS_INVALID_PARAMETER if @c num_of_slots or @c length in @p config is out of range
 *         for the peripheral type (I2S vs I2ST), or if @c tx_slots/@c rx_slots
 *         enable any bit at or above @c num_of_slots.
 *         SL_STATUS_INVALID_CONFIGURATION if @c IER audio format is not TDM
 *         (call @ref sl_hal_i2s_init with @c SL_I2S_DATA_FORMAT_TDM first).
 *         SL_STATUS_INVALID_STATE if @c IER.IEN or @c CER.CLKEN is already set
 *         (no register changes).
 *
 * @note Call after @ref sl_hal_i2s_init with @c SL_I2S_DATA_FORMAT_TDM, before
 *       @ref sl_hal_i2s_enable and @ref sl_hal_i2s_enable_sclk while both
 *       @c IER.IEN and @c CER.CLKEN are clear.
 ******************************************************************************/
sl_status_t sl_hal_i2s_configure_tdm(I2S_TypeDef_t *i2s, const sl_hal_i2s_tdm_config_t *config);

#endif // SL_HAL_I2S_TDM_SUPPORT

/***************************************************************************//**
 * @brief Configure one channel (word length, FIFO levels, and TX/RX usage).
 *
 * Updates the selected channel: turns TX/RX off briefly, sets word lengths for both
 * paths, then applies FIFO thresholds and enables TX and/or RX according to
 * @p config->direction.
 *
 * @param[in] i2s    I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] config Channel configuration (@ref sl_hal_i2s_channel_config_t).
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_NULL_POINTER if @p config is NULL.
 *         SL_STATUS_INVALID_PARAMETER if a field is out of range, @c config->id is invalid
 *         for this instance, @c tx_length/@c rx_length exceed the instance limit (24-bit on I2S,
 *         16-bit on I2ST), or @ref SL_I2S_WORD_LENGTH_NONE is used for a path that
 *         @c config->direction enables, or in TDM mode if an enabled path word length
 *         exceeds the configured TDM slot length.
 *         SL_STATUS_INVALID_CONFIGURATION if the audio format in @c IER is invalid/reserved.
 *         SL_STATUS_INVALID_STATE if @c IER.IEN or @c CER.CLKEN is already set
 *         (no register changes).
 *
 * @note Call before @ref sl_hal_i2s_enable and @ref sl_hal_i2s_enable_sclk.
 *       Configure every channel during the configuration phase while @c IER.IEN
 *       and @c CER.CLKEN are clear. All checks (parameters, quiescent state, and
 *       WSS feasibility for standard/LJ/RJ) run before any channel or @c CCR.WSS
 *       register writes; on success the channel and @c CCR.WSS are updated
 *       atomically. For standard/LJ/RJ formats,
 *       @c CCR.WSS is derived internally from the largest enabled TX/RX word width
 *       (12/16-bit -> 16 SCLK, 20-bit -> 24 SCLK, 24-bit -> 32 SCLK). In target
 *       mode, LJ/RJ use 32 SCLK WSS. For TDM, @c CCR.TDM_SLOT_LENGTH controls slot
 *       width and @c CCR.WSS is frame-sync pulse width.
 ******************************************************************************/
sl_status_t sl_hal_i2s_configure_channel(I2S_TypeDef_t *i2s, const sl_hal_i2s_channel_config_t *config);

/***************************************************************************//**
 * @brief Configure the controller-mode serial clock frequency.
 *
 * Call this after @ref sl_hal_i2s_configure_channel
#if SL_HAL_I2S_TDM_SUPPORT
 * and, when using TDM, after @ref sl_hal_i2s_configure_tdm
#endif
 * . This API programs the SoC serial clock branch only; it does not change
 * @c CER.CLKEN. Use @ref sl_hal_i2s_enable_sclk to turn bit-clock output on or off.
 * The HAL reads the configured frame shape from the I2S registers and computes
 * the required SCLK:
 * - Standard I2S/LJ/RJ: @c sample_rate_hz * 2 * slot_width_bits.
#if SL_HAL_I2S_TDM_SUPPORT
 * - TDM: @c sample_rate_hz * number_of_slots * slot_width_bits.
#endif
 *
 * @note Call with bit-clock output off. Use
 *       @ref sl_hal_i2s_enable_sclk(@p i2s, false) before this API if SCLK is
 *       already running. Recommended sequence: configure frame →
 *       @c set_serial_clock_frequency → @ref sl_hal_i2s_enable_tx_rx →
 *       @ref sl_hal_i2s_enable_sclk(@p i2s, true).
 *
 * Note: on devices that expose PERPLL1 and I2S0CLK, this API uses an interim
 * legacy path that programs PERPLL1 and CMU I2S0CLK directly with a minimal
 * known-good lookup table (40 MHz HFXO only today). Clock Manager will own
 * runtime branch configuration via @c sli_clock_manager_configure_branch_frequency()
 * after that API merges on main; see @c TODO in @c sl_hal_i2s.c.
 * Keep @c SL_CLOCK_MANAGER_PERPLL1_EN disabled at boot when changing sample rates
 * dynamically to avoid fighting boot-time PERPLL1 init.
 *
 * @param[in] i2s             I2S controller peripheral.
 * @param[in] sample_rate_hz  Audio sample rate in Hz.
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_INVALID_PARAMETER if @p sample_rate_hz is zero, @p i2s is not
 *         I2S0, or the required SCLK is not in the interim PERPLL1 lookup table.
 *         SL_STATUS_NOT_SUPPORTED if @p i2s is I2ST, @p i2s is in target mode,
 *         or TDM is selected while @c SL_HAL_I2S_TDM_SUPPORT is disabled.
 *         SL_STATUS_INVALID_CONFIGURATION if the frame shape in registers cannot be
 *         decoded (reserved @c CCR.WSS or invalid audio-format field).
 *         SL_STATUS_FAIL if the clock branch frequency does not match the
 *         required SCLK after programming.
 *         SL_STATUS_NOT_AVAILABLE if the serial clock branch cannot be queried
 *         or the interim PERPLL1/I2S0CLK path is unavailable (including empty
 *         LUT for 38/38.4 MHz HFXO).
 *         SL_STATUS_TIMEOUT if PERPLL1 does not become ready.
 ******************************************************************************/
sl_status_t sl_hal_i2s_set_serial_clock_frequency(I2S_TypeDef_t *i2s, uint32_t sample_rate_hz);

/***************************************************************************//**
 * @brief Enable the I2S instance block.
 *
 * Sets @c IER.IEN. Call after @ref sl_hal_i2s_configure_channel (and after
 * @ref sl_hal_i2s_set_serial_clock_frequency on controller I2S). Do not call
 * @ref sl_hal_i2s_configure_channel while @c IER.IEN is set.
 *
 * @param[in] i2s I2S (Controller/Target) or I2ST (Target only) peripheral.
 ******************************************************************************/
void sl_hal_i2s_enable(I2S_TypeDef_t *i2s);

/***************************************************************************//**
 * @brief Disable the I2S instance block (clear @c IER.IEN).
 *
 * @param[in] i2s I2S (Controller/Target) or I2ST (Target only) peripheral.
 ******************************************************************************/
void sl_hal_i2s_disable(I2S_TypeDef_t *i2s);

/***************************************************************************//**
 * @brief Enable or disable transmitter (ITER) and receiver (IRER) block enables per @p dir.
 *
 * @param[in] i2s  I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] dir  Which block(s) to affect (@ref SL_I2S_DIR_TX,
 *                 @ref SL_I2S_DIR_RX, or @ref SL_I2S_DIR_TX_RX).
 ******************************************************************************/
void sl_hal_i2s_enable_tx_rx(I2S_TypeDef_t *i2s, sl_i2s_direction_t dir);

/***************************************************************************//**
 * @brief Disable transmitter and/or receiver block enables per @p dir, then clear IER.IEN if both
 *        TX and RX blocks are off.
 *
 * @param[in] i2s  I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] dir  Which block(s) to turn off (@ref SL_I2S_DIR_TX,
 *                 @ref SL_I2S_DIR_RX, or @ref SL_I2S_DIR_TX_RX).
 ******************************************************************************/
void sl_hal_i2s_disable_tx_rx(I2S_TypeDef_t *i2s, sl_i2s_direction_t dir);

/***************************************************************************//**
 * @brief Enable or disable the serial bit clock (SCLK).
 *
 * Programs @c CER.CLKEN on I2S in controller mode only. I2ST and target-mode
 * instances receive SCLK externally; those calls are ignored.
 *
 * @param[in] i2s    I2S controller peripheral.
 * @param[in] enable true to turn the clock on, false to turn it off.
 ******************************************************************************/
void sl_hal_i2s_enable_sclk(I2S_TypeDef_t *i2s, bool enable);

/***************************************************************************//**
 * @brief Allow the listed interrupt sources for a channel (unmask).
 *
 * @param[in] i2s            I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] channel_id     Channel index.
 * @param[in] interrupt_mask Bits to unmask (@c I2S_IMR0_* from the device header).
 ******************************************************************************/
void sl_hal_i2s_enable_channel_interrupt(I2S_TypeDef_t *i2s, sl_i2s_channel_id_t channel_id, uint32_t interrupt_mask);

/***************************************************************************//**
 * @brief Block the listed interrupt sources for a channel (mask).
 *
 * @param[in] i2s            I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] channel_id     Channel index.
 * @param[in] interrupt_mask Bits to mask (@c I2S_IMR0_* from the device header).
 ******************************************************************************/
void sl_hal_i2s_disable_channel_interrupt(I2S_TypeDef_t *i2s, sl_i2s_channel_id_t channel_id, uint32_t interrupt_mask);

/***************************************************************************//**
 * @brief True if the transmit side can accept another sample for this channel.
 *
 * @param[in] i2s         I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] channel_id  Channel index.
 *
 * @return @c true if the transmit side can accept another sample for that channel.
 * @return @c false if the transmit path is not ready yet.
 ******************************************************************************/
__INLINE bool sl_hal_i2s_is_tx_ready(I2S_TypeDef_t *i2s, sl_i2s_channel_id_t channel_id)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));

  if (!SL_HAL_I2S_VALIDATE_CHANNEL(i2s, channel_id)) {
    EFM_ASSERT(false);
    return false;
  }
  return ((*SL_HAL_I2S_CHANNEL_REG_PTR(i2s, ISR0, channel_id) & (uint32_t)I2S_ISR0_TXFE) != 0U);
}

/***************************************************************************//**
 * @brief True if receive data is ready to read for this channel.
 *
 * @param[in] i2s         I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] channel_id  Channel index.
 *
 * @return @c true if @c RXDA indicates receive data is ready for that channel.
 * @return @c false if no data is ready yet.
 ******************************************************************************/
__INLINE bool sl_hal_i2s_is_rx_data_ready(I2S_TypeDef_t *i2s, sl_i2s_channel_id_t channel_id)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));

  if (!SL_HAL_I2S_VALIDATE_CHANNEL(i2s, channel_id)) {
    EFM_ASSERT(false);
    return false;
  }
  return ((*SL_HAL_I2S_CHANNEL_REG_PTR(i2s, ISR0, channel_id) & (uint32_t)I2S_ISR0_RXDA) != 0U);
}

/***************************************************************************//**
 * @brief Write one transmit sample (one word) to the transmit path.
 *
 * @param[in] i2s   I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] data  Sample to send (width follows channel configuration).
 ******************************************************************************/
__INLINE void sl_hal_i2s_transmit_word(I2S_TypeDef_t *i2s, uint32_t data)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));
  i2s->TXDMA = data;
}

/***************************************************************************//**
 * @brief Read one received sample (one word) from the receive path.
 *
 * @param[in] i2s I2S (Controller/Target) or I2ST (Target only) peripheral.
 *
 * @return Received sample value.
 ******************************************************************************/
__INLINE uint32_t sl_hal_i2s_receive_word(I2S_TypeDef_t *i2s)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));
  return i2s->RXDMA;
}

/***************************************************************************//**
 * @brief Read interrupt status for a channel.
 *
 * @param[in] i2s         I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] channel_id  Channel to query.
 *
 * @return Raw status bits for that channel.
 ******************************************************************************/
__INLINE uint32_t sl_hal_i2s_get_channel_interrupt_status(I2S_TypeDef_t *i2s, sl_i2s_channel_id_t channel_id)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));

  if (!SL_HAL_I2S_VALIDATE_CHANNEL(i2s, channel_id)) {
    EFM_ASSERT(false);
    return 0U;
  }
  return *SL_HAL_I2S_CHANNEL_REG_PTR(i2s, ISR0, channel_id);
}

/***************************************************************************//**
 * @brief Clear latched transmit overrun/underrun status for a channel.
 *
 * Performs the read that clears the sticky TX error state for that channel.
 *
 * @param[in] i2s         I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] channel_id  Channel index.
 ******************************************************************************/
__INLINE void sl_hal_i2s_clear_tx_errors(I2S_TypeDef_t *i2s, sl_i2s_channel_id_t channel_id)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));

  if (!SL_HAL_I2S_VALIDATE_CHANNEL(i2s, channel_id)) {
    EFM_ASSERT(false);
    return;
  }
  (void)*SL_HAL_I2S_CHANNEL_REG_PTR(i2s, TICR0, channel_id);
}

/***************************************************************************//**
 * @brief Clear latched receive overrun status for a channel.
 *
 * Performs the read that clears the sticky RX overrun state for that channel.
 *
 * @param[in] i2s         I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] channel_id  Channel index.
 ******************************************************************************/
__INLINE void sl_hal_i2s_clear_rx_overrun(I2S_TypeDef_t *i2s, sl_i2s_channel_id_t channel_id)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));

  if (!SL_HAL_I2S_VALIDATE_CHANNEL(i2s, channel_id)) {
    EFM_ASSERT(false);
    return;
  }
  (void)*SL_HAL_I2S_CHANNEL_REG_PTR(i2s, ROR0, channel_id);
}

/*******************************************************************************
 ************************   INTERNAL APIs   ************************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Turn DMA on or off for transmit, receive, or both.
 *
 * @p dir must not be @ref SL_I2S_DIR_MAX.
 *
 * @param[in] i2s     I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] dir     @ref SL_I2S_DIR_TX, @ref SL_I2S_DIR_RX, or @ref SL_I2S_DIR_TX_RX.
 * @param[in] enable  true to enable, false to disable.
 ******************************************************************************/
void sli_hal_i2s_enable_dma(I2S_TypeDef_t *i2s, sl_i2s_direction_t dir, bool enable);

/***************************************************************************//**
 * @brief Transmit data register address.
 *
 * @param[in] i2s I2S (Controller/Target) or I2ST (Target only) peripheral.
 ******************************************************************************/
__INLINE void *sli_hal_i2s_get_tx_data_port_address(I2S_TypeDef_t *i2s)
{
  return (void *)&i2s->TXDMA;
}

/***************************************************************************//**
 * @brief Receive data register address.
 *
 * @param[in] i2s I2S (Controller/Target) or I2ST (Target only) peripheral.
 ******************************************************************************/
__INLINE void *sli_hal_i2s_get_rx_data_port_address(I2S_TypeDef_t *i2s)
{
  return (void *)&i2s->RXDMA;
}

/***************************************************************************//**
 * @brief True if the transmit block is enabled.
 *
 * @param[in] i2s I2S (Controller/Target) or I2ST (Target only) peripheral.
 ******************************************************************************/
__INLINE bool sli_hal_i2s_is_tx_enabled(I2S_TypeDef_t *i2s)
{
  return ((i2s->ITER & (uint32_t)_I2S_ITER_TXEN_MASK) != 0U);
}

/***************************************************************************//**
 * @brief True if the receive block is enabled.
 *
 * @param[in] i2s I2S (Controller/Target) or I2ST (Target only) peripheral.
 ******************************************************************************/
__INLINE bool sli_hal_i2s_is_rx_enabled(I2S_TypeDef_t *i2s)
{
  return ((i2s->IRER & (uint32_t)_I2S_IRER_RXEN_MASK) != 0U);
}

/***************************************************************************//**
 * @brief True if the instance-wide transmit FIFO is empty.
 *
 * @param[in] i2s I2S (Controller/Target) or I2ST (Target only) peripheral.
 ******************************************************************************/
__INLINE bool sli_hal_i2s_is_tx_fifo_empty(I2S_TypeDef_t *i2s)
{
  return ((i2s->SR & (uint32_t)I2S_SR_TFE) != 0U);
}

/***************************************************************************//**
 * @brief Flush transmit and/or receive FIFO.
 *
 * @param[in] i2s I2S (Controller/Target) or I2ST (Target only) peripheral.
 * @param[in] dir @ref SL_I2S_DIR_TX, @ref SL_I2S_DIR_RX, or @ref SL_I2S_DIR_TX_RX.
 ******************************************************************************/
__INLINE void sli_hal_i2s_flush_fifos(I2S_TypeDef_t *i2s, sl_i2s_direction_t dir)
{
  bool dir_ok = (dir < SL_I2S_DIR_MAX);
  EFM_ASSERT(dir_ok);
  if (!dir_ok) {
    return;
  }

  if ((dir == SL_I2S_DIR_TX) || (dir == SL_I2S_DIR_TX_RX)) {
    i2s->TXFFR = I2S_TXFFR_TXFFR;
  }
  if ((dir == SL_I2S_DIR_RX) || (dir == SL_I2S_DIR_TX_RX)) {
    i2s->RXFFR = I2S_RXFFR_RXFFR;
  }
}

#ifdef __cplusplus
}
#endif

#endif /* defined(I2S_PRESENT) */
#endif /* SL_HAL_I2S_H */
