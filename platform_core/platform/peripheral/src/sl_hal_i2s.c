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

#include "sl_hal_i2s.h"

#if defined(I2S_PRESENT)

#include <stdint.h>

#include "sl_clock_manager.h"
#include "sl_clock_manager_oscillator_config.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/
#define I2S_IMR0_MASK_ALL \
  ((uint32_t)(I2S_IMR0_RXDAM | I2S_IMR0_RXFOM | I2S_IMR0_TXFEM | I2S_IMR0_TXFOM | I2S_IMR0_TXFUM))

/*******************************************************************************
 * TODO: Remove this region once Clock Manager configures PERPLL1 and
 *       SL_CLOCK_BRANCH_I2S0CLK via sli_clock_manager_configure_branch_frequency().
 *       Delete together with i2s_configure_serial_clock_legacy().
 ******************************************************************************/
 // TODO BEGIN //
#define I2S_PERPLL1_MAX_HZ                         50000000U
#define I2S_PERPLL1_LOCK_STATUS_MASK               (PERPLL_STATUS_RDY | PERPLL_STATUS_PLLLOCK | PERPLL_STATUS_ENS)
/* Interim legacy PERPLL poll: busy-loop iteration budget (not milliseconds). Chosen
 * empirically (~seconds at typical core clock). Returns SL_STATUS_TIMEOUT on exhaustion. */
#define I2S_PERPLL1_LOCK_POLL_MAX_ITERATIONS       5000000U

typedef struct {
  uint32_t fout_hz;
  uint8_t fracn_en;
  uint8_t divn;
  uint16_t divf;
  uint8_t dco_div;
  uint8_t div_2pow;
} i2s_perpll_row_t;

#if defined(PERPLL_PRESENT) && defined(PERPLL1) && defined(CMU) \
  && defined(CMU_I2S0CLKCTRL_CLKSEL_PERPLL1) \
  && defined(SL_BUS_CLOCK_PERPLL1) \
  && !defined(EVEREST_FPGA) && !defined(FPGA)
#if (SL_CLOCK_MANAGER_HFXO_FREQ == 40000000UL)
/* Minimal known-good rows for BRD1025C 48 kHz standard (1.536 MHz) and TDM (6.144 MHz). */
static const i2s_perpll_row_t i2s_perpll_table[] = {
  { 1536000u, 1u, 10u, 14785u, 19u, 3u },
  { 2304000u, 1u, 10u, 14785u, 26u, 2u },
#if SL_HAL_I2S_TDM_SUPPORT
  { 6144000u, 1u, 10u, 14785u, 19u, 1u },
#endif
};
#define I2S_PERPLL_LUT_ROW_COUNT  (sizeof(i2s_perpll_table) / sizeof(i2s_perpll_table[0]))
#elif (SL_CLOCK_MANAGER_HFXO_FREQ == 38000000UL) || (SL_CLOCK_MANAGER_HFXO_FREQ == 38400000UL)
#define I2S_PERPLL_LUT_ROW_COUNT  0U
#else
#error "Unsupported SL_CLOCK_MANAGER_HFXO_FREQ for I2S PERPLL1 lookup table"
#endif
#else
#define I2S_PERPLL_LUT_ROW_COUNT  0U
#endif /* PERPLL legacy path available */
// TODO END //

/*******************************************************************************
 *****************************   LOCAL ENUMS   *********************************
 ******************************************************************************/

/// CCR.WSS field encoding (SCLK cycles per word-select period for standard/LJ/RJ).
SL_ENUM(i2s_ws_size_t) {
  I2S_WS_SIZE_16_SCLK = 0,
  I2S_WS_SIZE_24_SCLK = 1,
  I2S_WS_SIZE_32_SCLK = 2,
  I2S_WS_SIZE_MAX
};

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

__STATIC_INLINE void i2s_set_operating_mode(I2S_TypeDef_t *i2s, sl_i2s_operating_mode_t mode);
__STATIC_INLINE void i2s_set_data_format(I2S_TypeDef_t *i2s, sl_i2s_data_format_t format);
__STATIC_INLINE void i2s_set_endianness(I2S_TypeDef_t *i2s, sl_i2s_endianness_t endianness);
__STATIC_INLINE void i2s_set_rx_wlen(I2S_TypeDef_t *i2s,
                                       sl_i2s_channel_id_t channel_id,
                                       sl_i2s_word_length_t wlen);
__STATIC_INLINE void i2s_set_tx_wlen(I2S_TypeDef_t *i2s,
                                     sl_i2s_channel_id_t channel_id,
                                     sl_i2s_word_length_t wlen);
__STATIC_INLINE void i2s_set_rx_fifo_trigger_level(I2S_TypeDef_t *i2s,
                                                   sl_i2s_channel_id_t channel_id,
                                                   sl_i2s_fifo_trigger_level_t level);
__STATIC_INLINE void i2s_set_tx_fifo_trigger_level(I2S_TypeDef_t *i2s,
                                                   sl_i2s_channel_id_t channel_id,
                                                   sl_i2s_fifo_trigger_level_t level);
__STATIC_INLINE void i2s_enable_transmitter(I2S_TypeDef_t *i2s, bool enable);
__STATIC_INLINE void i2s_enable_receiver(I2S_TypeDef_t *i2s, bool enable);
__STATIC_INLINE void i2s_enable_rx_channel(I2S_TypeDef_t *i2s,
                                           sl_i2s_channel_id_t channel_id,
                                           bool enable);
__STATIC_INLINE void i2s_enable_tx_channel(I2S_TypeDef_t *i2s,
                                           sl_i2s_channel_id_t channel_id,
                                           bool enable);
__STATIC_INLINE void i2s_reset_rxdma(I2S_TypeDef_t *i2s);
__STATIC_INLINE void i2s_reset_txdma(I2S_TypeDef_t *i2s);

#if SL_HAL_I2S_TDM_SUPPORT
__STATIC_INLINE void i2s_set_tdm_slots(I2S_TypeDef_t *i2s, uint8_t slots);
__STATIC_INLINE void i2s_enable_tdm_tx_slots(I2S_TypeDef_t *i2s, uint32_t slots);
__STATIC_INLINE void i2s_enable_tdm_rx_slots(I2S_TypeDef_t *i2s, uint32_t slots);
static sl_i2s_tdm_slot_length_t i2s_get_tdm_slot_length(I2S_TypeDef_t *i2s);
static uint32_t i2s_tdm_slot_length_to_bits(sl_i2s_tdm_slot_length_t length);
static sl_i2s_word_length_t i2s_tdm_slot_length_to_max_wlen(sl_i2s_tdm_slot_length_t length);
static void i2s_set_tdm_slot_length(I2S_TypeDef_t *i2s, sl_i2s_tdm_slot_length_t length);
#endif

static void i2s_disable_all_channels(I2S_TypeDef_t *i2s);
static void i2s_quiesce_channels_for_reset(I2S_TypeDef_t *i2s);
static void i2s_reset_quiesce(I2S_TypeDef_t *i2s);
#if defined(I2ST_PRESENT)
static void i2s_reset_target(I2S_TypeDef_t *i2s);
#endif
static void i2s_reset_controller(I2S_TypeDef_t *i2s);

static uint32_t i2s_instance_channel_count(I2S_TypeDef_t *i2s);
static bool i2s_instance_any_tx_channel_enabled(I2S_TypeDef_t *i2s);
static bool i2s_instance_any_rx_channel_enabled(I2S_TypeDef_t *i2s);
static bool i2s_get_standard_wss_bits(I2S_TypeDef_t *i2s, uint32_t *wss_bits);

static sl_status_t i2s_get_serial_clock_branch(I2S_TypeDef_t *i2s, sl_clock_branch_t *branch);
static sl_status_t  i2s_get_sclk_for_sample_rate(I2S_TypeDef_t *i2s,
                                                     uint32_t sample_rate_hz,
                                                     uint32_t *sclk_hz);
static sl_status_t i2s_configure_serial_clock_legacy(I2S_TypeDef_t *i2s, uint32_t sclk_hz);
static sl_status_t i2s_configure_serial_clock(I2S_TypeDef_t *i2s, uint32_t sclk_hz);
static sl_status_t i2s_get_serial_clock_frequency(I2S_TypeDef_t *i2s, uint32_t *clock_frequency_hz);
static sl_status_t i2s_get_post_config_max_wlen(I2S_TypeDef_t *i2s,
                                                const sl_hal_i2s_channel_config_t *config,
                                                sl_i2s_word_length_t *max_wlen);
static sl_status_t i2s_word_length_to_ws_size(sl_i2s_word_length_t word_length, i2s_ws_size_t *ws_size);
static sl_i2s_word_length_t i2s_ws_size_to_max_wlen(i2s_ws_size_t ws_size);
static sl_status_t i2s_compute_ws_size_for_max_wlen(I2S_TypeDef_t *i2s,
                                                      sl_i2s_word_length_t max_wlen,
                                                      i2s_ws_size_t *ws_size,
                                                      bool *ws_update_needed);
static sl_status_t i2s_validate_ws_size_for_channel_config(I2S_TypeDef_t *i2s,
                                                           const sl_hal_i2s_channel_config_t *config,
                                                           i2s_ws_size_t *ws_size,
                                                           bool *ws_update_needed);
static void i2s_apply_ws_size(I2S_TypeDef_t *i2s, i2s_ws_size_t ws_size);

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

extern __INLINE bool sl_hal_i2s_is_tx_ready(I2S_TypeDef_t *i2s, sl_i2s_channel_id_t channel_id);
extern __INLINE bool sl_hal_i2s_is_rx_data_ready(I2S_TypeDef_t *i2s, sl_i2s_channel_id_t channel_id);
extern __INLINE void sl_hal_i2s_transmit_word(I2S_TypeDef_t *i2s, uint32_t data);
extern __INLINE uint32_t sl_hal_i2s_receive_word(I2S_TypeDef_t *i2s);
extern __INLINE uint32_t sl_hal_i2s_get_channel_interrupt_status(I2S_TypeDef_t *i2s,
                                                                 sl_i2s_channel_id_t channel_id);
extern __INLINE void sl_hal_i2s_clear_tx_errors(I2S_TypeDef_t *i2s, sl_i2s_channel_id_t channel_id);
extern __INLINE void sl_hal_i2s_clear_rx_overrun(I2S_TypeDef_t *i2s, sl_i2s_channel_id_t channel_id);
extern __INLINE void *sli_hal_i2s_get_tx_data_port_address(I2S_TypeDef_t *i2s);
extern __INLINE void *sli_hal_i2s_get_rx_data_port_address(I2S_TypeDef_t *i2s);
extern __INLINE bool sli_hal_i2s_is_tx_enabled(I2S_TypeDef_t *i2s);
extern __INLINE bool sli_hal_i2s_is_rx_enabled(I2S_TypeDef_t *i2s);
extern __INLINE bool sli_hal_i2s_is_tx_fifo_empty(I2S_TypeDef_t *i2s);
extern __INLINE void sli_hal_i2s_flush_fifos(I2S_TypeDef_t *i2s, sl_i2s_direction_t dir);

/***************************************************************************//**
 * Initialize the I2S peripheral: reset, then apply operating mode, data format
 * (standard I2S / TDM / LJ / RJ), and serial data endianness.
 ******************************************************************************/
sl_status_t sl_hal_i2s_init(I2S_TypeDef_t *i2s, const sl_hal_i2s_init_t *init)
{
  // Make sure the module exists on the selected chip.
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));

  if (init == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if ((init->mode >= SL_I2S_OPERATING_MODE_MAX)
      || (init->format >= SL_I2S_DATA_FORMAT_MAX)
      || (init->endianness >= SL_I2S_ENDIANNESS_MAX)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // I2ST is target-only; controller mode is not supported (see @ref sl_hal_i2s_init note).
#if defined(I2ST_PRESENT)
  if (SL_HAL_I2ST_IS_INSTANCE(i2s)
      && (init->mode != SL_I2S_OPERATING_MODE_TARGET)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
#endif

#if !SL_HAL_I2S_TDM_SUPPORT
  if (init->format == SL_I2S_DATA_FORMAT_TDM) {
    return SL_STATUS_INVALID_PARAMETER;
  }
#endif

  // Reset the I2S peripheral to ensure it is in a known state.
  sl_hal_i2s_reset(i2s);

  // Set operating mode, data format, and endianness.
  i2s_set_operating_mode(i2s, init->mode);
  i2s_set_data_format(i2s, init->format);
  i2s_set_endianness(i2s, init->endianness);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Deinitialize the I2S peripheral: reset to hardware default state.
 ******************************************************************************/
void sl_hal_i2s_deinit(I2S_TypeDef_t *i2s)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));
  sl_hal_i2s_reset(i2s);
}

/***************************************************************************//**
 * Reset the I2S peripheral to a known state: stop serial clock, disable paths,
 * quiesce channels and clear sticky error latches, flush FIFOs, mask interrupts,
 * restore defaults, and disable all per-channel enables.
 ******************************************************************************/
void sl_hal_i2s_reset(I2S_TypeDef_t *i2s)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));

  i2s_reset_quiesce(i2s);
#if defined(I2ST_PRESENT)
  if (SL_HAL_I2ST_IS_INSTANCE(i2s)) {
    i2s_reset_target(i2s);
  } else
#endif
  {
    i2s_reset_controller(i2s);
  }
  i2s_disable_all_channels(i2s);
}

#if SL_HAL_I2S_TDM_SUPPORT
/***************************************************************************//**
 * Configure TDM slot count, slot length, and TX/RX TDM slot enables.
 ******************************************************************************/
sl_status_t sl_hal_i2s_configure_tdm(I2S_TypeDef_t *i2s, const sl_hal_i2s_tdm_config_t *config)
{
  // Make sure the module exists on the selected chip.
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));

  if (config == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if ((i2s->IER & (uint32_t)I2S_IER_IEN) != 0U) {
    return SL_STATUS_INVALID_STATE;
  }
  if ((i2s->CER & (uint32_t)_I2S_CER_CLKEN_MASK) != 0U) {
    return SL_STATUS_INVALID_STATE;
  }

  uint32_t format_reg = (i2s->IER & (uint32_t)_I2S_IER_AUDIO_INTF_TYPE_MASK)
                        >> (uint32_t)_I2S_IER_AUDIO_INTF_TYPE_SHIFT;

  if (format_reg != (uint32_t)SL_I2S_DATA_FORMAT_TDM) {
    return SL_STATUS_INVALID_CONFIGURATION;
  }

  uint32_t max_tdm_slots = SL_I2S_MAX_TDM_SLOTS;
#if defined(I2ST_PRESENT)
  if (SL_HAL_I2ST_IS_INSTANCE(i2s)) {
    max_tdm_slots = SL_I2ST_MAX_TDM_SLOTS;
  }
#endif

  if ((config->num_of_slots == 0)
      || (config->num_of_slots > max_tdm_slots)
      || (config->length >= SL_I2S_TDM_SLOT_LENGTH_MAX)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  uint32_t max_tdm_slot_mask = ((1UL << SL_I2S_MAX_TDM_SLOTS) - 1UL);
  uint32_t valid_slot_mask = (config->num_of_slots >= SL_I2S_MAX_TDM_SLOTS)
                                   ? max_tdm_slot_mask
                                   : ((1UL << config->num_of_slots) - 1UL);

  if ((((uint32_t)config->tx_slots) & ~valid_slot_mask) != 0U
      || (((uint32_t)config->rx_slots) & ~valid_slot_mask) != 0U) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Slot count, slot length, and TDM TX/RX slot enables.
  i2s_set_tdm_slots(i2s, (uint8_t)config->num_of_slots);
  i2s_set_tdm_slot_length(i2s, config->length);
  i2s_enable_tdm_tx_slots(i2s, (uint32_t)config->tx_slots);
  i2s_enable_tdm_rx_slots(i2s, (uint32_t)config->rx_slots);

  return SL_STATUS_OK;
}
#endif // SL_HAL_I2S_TDM_SUPPORT
/***************************************************************************//**
 * Configure one channel: word lengths, FIFO trigger levels, and TX/RX enables per
 * sl_hal_i2s_channel_config_t::direction.
 ******************************************************************************/
sl_status_t sl_hal_i2s_configure_channel(I2S_TypeDef_t *i2s, const sl_hal_i2s_channel_config_t *config)
{
  // Make sure the module exists on the selected chip.
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));

  if (config == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if ((i2s->IER & (uint32_t)I2S_IER_IEN) != 0U) {
    return SL_STATUS_INVALID_STATE;
  }
  if ((i2s->CER & (uint32_t)_I2S_CER_CLKEN_MASK) != 0U) {
    return SL_STATUS_INVALID_STATE;
  }

  if ((config->rx_length >= SL_I2S_WORD_LENGTH_MAX)
      || (config->tx_length >= SL_I2S_WORD_LENGTH_MAX)
      || (config->rx_level >= SL_I2S_FIFO_TRIGGER_LEVEL_MAX)
      || (config->tx_level >= SL_I2S_FIFO_TRIGGER_LEVEL_MAX)
      || (config->direction >= SL_I2S_DIR_MAX)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

#if defined(I2ST_PRESENT)
  if (SL_HAL_I2ST_IS_INSTANCE(i2s)) {
    if (((config->tx_length != SL_I2S_WORD_LENGTH_NONE)
         && (config->tx_length > SL_I2S_WORD_LENGTH_16_BIT))
        || ((config->rx_length != SL_I2S_WORD_LENGTH_NONE)
            && (config->rx_length > SL_I2S_WORD_LENGTH_16_BIT))) {
      return SL_STATUS_INVALID_PARAMETER;
    }
  } else
#endif
  if (SL_HAL_I2S_IS_INSTANCE(i2s)) {
    if (((config->tx_length != SL_I2S_WORD_LENGTH_NONE)
         && (config->tx_length > SL_I2S_WORD_LENGTH_24_BIT))
        || ((config->rx_length != SL_I2S_WORD_LENGTH_NONE)
            && (config->rx_length > SL_I2S_WORD_LENGTH_24_BIT))) {
      return SL_STATUS_INVALID_PARAMETER;
    }
  }

  // NONE is only for a path not selected by direction; enabled paths need a real word length.
  if (((config->direction == SL_I2S_DIR_TX) || (config->direction == SL_I2S_DIR_TX_RX))
      && (config->tx_length == SL_I2S_WORD_LENGTH_NONE)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (((config->direction == SL_I2S_DIR_RX) || (config->direction == SL_I2S_DIR_TX_RX))
      && (config->rx_length == SL_I2S_WORD_LENGTH_NONE)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (!SL_HAL_I2S_VALIDATE_CHANNEL(i2s, config->id)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

#if SL_HAL_I2S_TDM_SUPPORT
  {
    uint32_t format_reg = (i2s->IER & (uint32_t)_I2S_IER_AUDIO_INTF_TYPE_MASK)
                          >> (uint32_t)_I2S_IER_AUDIO_INTF_TYPE_SHIFT;
    if (format_reg == (uint32_t)SL_I2S_DATA_FORMAT_TDM) {
      sl_i2s_word_length_t max_wlen = i2s_tdm_slot_length_to_max_wlen(i2s_get_tdm_slot_length(i2s));

      if ((((config->direction == SL_I2S_DIR_TX) || (config->direction == SL_I2S_DIR_TX_RX))
           && (config->tx_length > max_wlen))
          || (((config->direction == SL_I2S_DIR_RX) || (config->direction == SL_I2S_DIR_TX_RX))
              && (config->rx_length > max_wlen))) {
        return SL_STATUS_INVALID_PARAMETER;
      }
    }
  }
#endif

  i2s_ws_size_t ws_size;
  bool ws_update_needed;
  sl_status_t ws_validate_status;

  ws_validate_status = i2s_validate_ws_size_for_channel_config(i2s,
                                                             config,
                                                             &ws_size,
                                                             &ws_update_needed);
  if (ws_validate_status != SL_STATUS_OK) {
    return ws_validate_status;
  }

  // Quiesce the channel before reprogramming.
  i2s_enable_tx_channel(i2s, config->id, false);
  i2s_enable_rx_channel(i2s, config->id, false);

  // Set the RX and TX word lengths for the channel.
  i2s_set_rx_wlen(i2s, config->id, config->rx_length);
  i2s_set_tx_wlen(i2s, config->id, config->tx_length);

  if (ws_update_needed) {
    i2s_apply_ws_size(i2s, ws_size);
  }

  // Enable TX and/or RX path, FIFO thresholds, and channel enables per direction.
  switch (config->direction) {
    case SL_I2S_DIR_TX:
      i2s_set_tx_fifo_trigger_level(i2s, config->id, config->tx_level);
      i2s_enable_tx_channel(i2s, config->id, true);
      break;

    case SL_I2S_DIR_RX:
      i2s_set_rx_fifo_trigger_level(i2s, config->id, config->rx_level);
      i2s_enable_rx_channel(i2s, config->id, true);
      break;

    case SL_I2S_DIR_TX_RX:
      i2s_set_rx_fifo_trigger_level(i2s, config->id, config->rx_level);
      i2s_set_tx_fifo_trigger_level(i2s, config->id, config->tx_level);
      i2s_enable_tx_channel(i2s, config->id, true);
      i2s_enable_rx_channel(i2s, config->id, true);
      break;

    default:
      EFM_ASSERT(false);
      break;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * In controller mode, derive the required serial bit clock (SCLK) from
 * @p sample_rate_hz and the frame shape already programmed in the I2S registers
 * (standard I2S / LJ / RJ or TDM), program the SoC I2S serial clock branch through an
 * internal helper, and verify the branch frequency. Caller should invoke with
 * @c CER.CLKEN clear; see @ref sl_hal_i2s_set_serial_clock_frequency.
 ******************************************************************************/
sl_status_t sl_hal_i2s_set_serial_clock_frequency(I2S_TypeDef_t *i2s, uint32_t sample_rate_hz)
{
  uint32_t sclk_hz;
  uint32_t actual_clock_hz;
  sl_status_t status;

  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));

  if (sample_rate_hz == 0U) {
    return SL_STATUS_INVALID_PARAMETER;
  }

#if defined(I2ST_PRESENT)
  if (SL_HAL_I2ST_IS_INSTANCE(i2s)) {
    return SL_STATUS_NOT_SUPPORTED;
  }
#endif

  if (((i2s->IER & (uint32_t)_I2S_IER_I2S_OP_MODE_MASK) >> (uint32_t)_I2S_IER_I2S_OP_MODE_SHIFT)
      != (uint32_t)SL_I2S_OPERATING_MODE_CONTROLLER) {
    return SL_STATUS_NOT_SUPPORTED;
  }

  status =  i2s_get_sclk_for_sample_rate(i2s, sample_rate_hz, &sclk_hz);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = i2s_configure_serial_clock(i2s, sclk_hz);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = i2s_get_serial_clock_frequency(i2s, &actual_clock_hz);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (actual_clock_hz != sclk_hz) {
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable the I2S instance (IER.IEN).
 ******************************************************************************/
void sl_hal_i2s_enable(I2S_TypeDef_t *i2s)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));
  i2s->IER |= I2S_IER_IEN;
}

/***************************************************************************//**
 * Disable the I2S instance (clear IER.IEN).
 ******************************************************************************/
void sl_hal_i2s_disable(I2S_TypeDef_t *i2s)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));
  i2s->IER &= ~I2S_IER_IEN;
}

/***************************************************************************//**
 * Enable transmitter and/or receiver block enables when channels use those paths.
 ******************************************************************************/
void sl_hal_i2s_enable_tx_rx(I2S_TypeDef_t *i2s, sl_i2s_direction_t dir)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));
  bool dir_ok = (dir < SL_I2S_DIR_MAX);
  EFM_ASSERT(dir_ok);
  if (!dir_ok) {
    return;
  }

  if ((dir == SL_I2S_DIR_TX) || (dir == SL_I2S_DIR_TX_RX)) {
    i2s_enable_transmitter(i2s, i2s_instance_any_tx_channel_enabled(i2s));
  }
  if ((dir == SL_I2S_DIR_RX) || (dir == SL_I2S_DIR_TX_RX)) {
    i2s_enable_receiver(i2s, i2s_instance_any_rx_channel_enabled(i2s));
  }
}

/***************************************************************************//**
 * Disable transmitter and/or receiver block enables; clear IER.IEN when both
 * blocks are off.
 ******************************************************************************/
void sl_hal_i2s_disable_tx_rx(I2S_TypeDef_t *i2s, sl_i2s_direction_t dir)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));
  bool dir_ok = (dir < SL_I2S_DIR_MAX);
  EFM_ASSERT(dir_ok);
  if (!dir_ok) {
    return;
  }

  if ((dir == SL_I2S_DIR_TX) || (dir == SL_I2S_DIR_TX_RX)) {
    i2s_enable_transmitter(i2s, false);
  }
  if ((dir == SL_I2S_DIR_RX) || (dir == SL_I2S_DIR_TX_RX)) {
    i2s_enable_receiver(i2s, false);
  }

  // ITER/IRER are TX/RX block enable registers. If both blocks are off, 
  // disable the instance via sl_hal_i2s_disable.
  if (((i2s->ITER & (uint32_t)_I2S_ITER_TXEN_MASK) == 0U)
      && ((i2s->IRER & (uint32_t)_I2S_IRER_RXEN_MASK) == 0U)) {
    sl_hal_i2s_disable(i2s);
  }
}

/***************************************************************************//**
 * Enable or disable the serial bit clock (CER.CLKEN) on I2S in controller mode.
 ******************************************************************************/
void sl_hal_i2s_enable_sclk(I2S_TypeDef_t *i2s, bool enable)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));

#if defined(I2ST_PRESENT)
  if (SL_HAL_I2ST_IS_INSTANCE(i2s)) {
    return;
  }
#endif

  if (((i2s->IER & (uint32_t)_I2S_IER_I2S_OP_MODE_MASK) >> (uint32_t)_I2S_IER_I2S_OP_MODE_SHIFT)
      != (uint32_t)SL_I2S_OPERATING_MODE_CONTROLLER) {
    return;
  }

  i2s->CER = (i2s->CER & ~_I2S_CER_CLKEN_MASK)
             | (((uint32_t)enable << _I2S_CER_CLKEN_SHIFT) & _I2S_CER_CLKEN_MASK);
}

/***************************************************************************//**
 * Unmask interrupt sources for a channel (clear bits in IMRn).
 ******************************************************************************/
void sl_hal_i2s_enable_channel_interrupt(I2S_TypeDef_t *i2s,
                                         sl_i2s_channel_id_t channel_id,
                                         uint32_t interrupt_mask)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));
  bool channel_ok = SL_HAL_I2S_VALIDATE_CHANNEL(i2s, channel_id);
  EFM_ASSERT(channel_ok);
  if (!channel_ok) {
    return;
  }
  volatile uint32_t *imr = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, IMR0, channel_id);
  *imr &= ~interrupt_mask;
}

/***************************************************************************//**
 * Mask interrupt sources for a channel (set bits in IMRn).
 ******************************************************************************/
void sl_hal_i2s_disable_channel_interrupt(I2S_TypeDef_t *i2s,
                                          sl_i2s_channel_id_t channel_id,
                                          uint32_t interrupt_mask)
{
  EFM_ASSERT(SL_HAL_I2S_REF_VALID(i2s));
  bool channel_ok = SL_HAL_I2S_VALIDATE_CHANNEL(i2s, channel_id);
  EFM_ASSERT(channel_ok);
  if (!channel_ok) {
    return;
  }
  volatile uint32_t *imr = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, IMR0, channel_id);
  *imr |= interrupt_mask;
}

/*******************************************************************************
 ************************   INTERNAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * DMACR block DMA enable bits for TX/RX per dir.
 ******************************************************************************/
void sli_hal_i2s_enable_dma(I2S_TypeDef_t *i2s, sl_i2s_direction_t dir, bool enable)
{
  EFM_ASSERT(dir < SL_I2S_DIR_MAX);
  if (!(dir < SL_I2S_DIR_MAX)) {
    return;
  }

  uint32_t mask = 0;

  switch (dir) {
    case SL_I2S_DIR_TX:
      mask = I2S_DMACR_DMAEN_TXBLOCK;
      break;
    case SL_I2S_DIR_RX:
      mask = I2S_DMACR_DMAEN_RXBLOCK;
      break;
    case SL_I2S_DIR_TX_RX:
      mask = I2S_DMACR_DMAEN_TXBLOCK | I2S_DMACR_DMAEN_RXBLOCK;
      break;
    default: {
      EFM_ASSERT(false);
      return;
    }
  }

  if (enable) {
    i2s->DMACR |= mask;
  } else {
    i2s->DMACR &= ~mask;
  }
}

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

/***************************************************************************//**
 * Set IER.I2S_OP_MODE (controller vs target).
 ******************************************************************************/
__STATIC_INLINE void i2s_set_operating_mode(I2S_TypeDef_t *i2s, sl_i2s_operating_mode_t mode)
{
  EFM_ASSERT(mode < SL_I2S_OPERATING_MODE_MAX);
  uint32_t op_mode = (uint32_t)mode;
  i2s->IER = (i2s->IER & ~_I2S_IER_I2S_OP_MODE_MASK)
             | ((op_mode << _I2S_IER_I2S_OP_MODE_SHIFT) & _I2S_IER_I2S_OP_MODE_MASK);
}

/***************************************************************************//**
 * Set IER audio interface type (standard I2S, TDM, LJ, RJ) from sl_i2s_data_format_t.
 ******************************************************************************/
__STATIC_INLINE void i2s_set_data_format(I2S_TypeDef_t *i2s,
                                         sl_i2s_data_format_t format)
{
  EFM_ASSERT(format <= SL_I2S_DATA_FORMAT_RJ);
  uint32_t reg_format = (uint32_t)format;

  i2s->IER = (i2s->IER & ~_I2S_IER_AUDIO_INTF_TYPE_MASK)
             | ((reg_format << _I2S_IER_AUDIO_INTF_TYPE_SHIFT) & _I2S_IER_AUDIO_INTF_TYPE_MASK);
}

/***************************************************************************//**
 * Set IER.SD_ENDIAN (MSB vs LSB first).
 ******************************************************************************/
__STATIC_INLINE void i2s_set_endianness(I2S_TypeDef_t *i2s, sl_i2s_endianness_t endianness)
{
  EFM_ASSERT(endianness < SL_I2S_ENDIANNESS_MAX);
  uint32_t reg_endianness = (uint32_t)endianness;

  i2s->IER = (i2s->IER & ~_I2S_IER_SD_ENDIAN_MASK)
             | ((reg_endianness << _I2S_IER_SD_ENDIAN_SHIFT) & _I2S_IER_SD_ENDIAN_MASK);
}

/***************************************************************************//**
 * Set RCRx.WLEN for the selected channel (receive path).
 ******************************************************************************/
__STATIC_INLINE void i2s_set_rx_wlen(I2S_TypeDef_t *i2s,
                                     sl_i2s_channel_id_t channel_id,
                                     sl_i2s_word_length_t wlen)
{
  volatile uint32_t *rcr_reg = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, RCR0, channel_id);

  EFM_ASSERT(wlen <= SL_I2S_WORD_LENGTH_24_BIT);
  *rcr_reg = (*rcr_reg & ~_I2S_RCR0_WLEN_MASK) | (uint32_t)wlen;
}

/***************************************************************************//**
 * Set TCRx.WLEN for the selected channel (transmit path).
 ******************************************************************************/
__STATIC_INLINE void i2s_set_tx_wlen(I2S_TypeDef_t *i2s,
                                     sl_i2s_channel_id_t channel_id,
                                     sl_i2s_word_length_t wlen)
{
  volatile uint32_t *tcr_reg = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, TCR0, channel_id);

  EFM_ASSERT(wlen <= SL_I2S_WORD_LENGTH_24_BIT);
  *tcr_reg = (*tcr_reg & ~_I2S_TCR0_WLEN_MASK) | (uint32_t)wlen;
}

/***************************************************************************//**
 * Set RFCRx RX FIFO trigger level.
 ******************************************************************************/
__STATIC_INLINE void i2s_set_rx_fifo_trigger_level(I2S_TypeDef_t *i2s,
                                                   sl_i2s_channel_id_t channel_id,
                                                   sl_i2s_fifo_trigger_level_t level)
{
  volatile uint32_t *rfcr = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, RFCR0, channel_id);

  EFM_ASSERT(level < SL_I2S_FIFO_TRIGGER_LEVEL_MAX);
  *rfcr = (*rfcr & ~_I2S_RFCR0_RXCHDT_MASK) | ((uint32_t)level & _I2S_RFCR0_RXCHDT_MASK);
}

/***************************************************************************//**
 * Set TFCRx TX FIFO trigger level.
 ******************************************************************************/
__STATIC_INLINE void i2s_set_tx_fifo_trigger_level(I2S_TypeDef_t *i2s,
                                                    sl_i2s_channel_id_t channel_id,
                                                    sl_i2s_fifo_trigger_level_t level)
{
  volatile uint32_t *tfcr = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, TFCR0, channel_id);

  EFM_ASSERT(level < SL_I2S_FIFO_TRIGGER_LEVEL_MAX);
  *tfcr = (*tfcr & ~_I2S_TFCR0_TXCHET_MASK) | ((uint32_t)level & _I2S_TFCR0_TXCHET_MASK);
}

/***************************************************************************//**
 * Set ITER.TXEN (transmitter block enable).
 ******************************************************************************/
__STATIC_INLINE void i2s_enable_transmitter(I2S_TypeDef_t *i2s, bool enable)
{
  i2s->ITER = (i2s->ITER & ~_I2S_ITER_TXEN_MASK) | (enable << _I2S_ITER_TXEN_SHIFT);
}

/***************************************************************************//**
 * Set IRER.RXEN (receiver block enable).
 ******************************************************************************/
__STATIC_INLINE void i2s_enable_receiver(I2S_TypeDef_t *i2s, bool enable)
{
  i2s->IRER = (i2s->IRER & ~_I2S_IRER_RXEN_MASK) | (enable << _I2S_IRER_RXEN_SHIFT);
}

/***************************************************************************//**
 * Set RERx.RXCHENX for the selected channel.
 ******************************************************************************/
__STATIC_INLINE void i2s_enable_rx_channel(I2S_TypeDef_t *i2s,
                                           sl_i2s_channel_id_t channel_id,
                                           bool enable)
{
  volatile uint32_t *rer_reg = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, RER0, channel_id);
  if (enable) {
    *rer_reg |= I2S_RER0_RXCHENX;
  } else {
    *rer_reg &= ~I2S_RER0_RXCHENX;
  }
}

/***************************************************************************//**
 * Set TERx.TXCHENX for the selected channel.
 ******************************************************************************/
__STATIC_INLINE void i2s_enable_tx_channel(I2S_TypeDef_t *i2s,
                                           sl_i2s_channel_id_t channel_id,
                                           bool enable)
{
  volatile uint32_t *ter_reg = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, TER0, channel_id);
  if (enable) {
    *ter_reg |= I2S_TER0_TXCHENX;
  } else {
    *ter_reg &= ~I2S_TER0_TXCHENX;
  }
}

/***************************************************************************//**
 * Reset RX DMA side (RRXDMA self-clearing pulse).
 ******************************************************************************/
__STATIC_INLINE void i2s_reset_rxdma(I2S_TypeDef_t *i2s)
{
  i2s->RRXDMA = I2S_RRXDMA_RRXDMA;
}

/***************************************************************************//**
 * Reset TX DMA side (RTXDMA self-clearing pulse).
 ******************************************************************************/
__STATIC_INLINE void i2s_reset_txdma(I2S_TypeDef_t *i2s)
{
  i2s->RTXDMA = I2S_RTXDMA_RTXDMA;
}

#if SL_HAL_I2S_TDM_SUPPORT
/***************************************************************************//**
 * Set IER.TDM_SLOTS (slot count encoded as n-1 in hardware).
 ******************************************************************************/
__STATIC_INLINE void i2s_set_tdm_slots(I2S_TypeDef_t *i2s, uint8_t slots)
{
  i2s->IER = (i2s->IER & ~_I2S_IER_TDM_SLOTS_MASK) | (((slots - 1) << _I2S_IER_TDM_SLOTS_SHIFT) & _I2S_IER_TDM_SLOTS_MASK);
}

/***************************************************************************//**
 * Program TER0 TDM TX slot enables (preserving TXCHENX; updates slot-enable bits only).
 ******************************************************************************/
__STATIC_INLINE void i2s_enable_tdm_tx_slots(I2S_TypeDef_t *i2s, uint32_t slots)
{
  // Preserve channel enable bits (TXCHENX) while updating TDM slot enable bits.
  i2s->TER0 = (i2s->TER0 & ~(_I2S_TER0_MASK & ~_I2S_TER0_TXCHENX_MASK))
              | ((slots & 0xFFFFUL) << _I2S_TER0_TXSLOT0_EN_SHIFT);
}

/***************************************************************************//**
 * Program RER0 TDM RX slot enables (preserving RXCHENX; updates slot-enable bits only).
 ******************************************************************************/
__STATIC_INLINE void i2s_enable_tdm_rx_slots(I2S_TypeDef_t *i2s, uint32_t slots)
{
  // Preserve channel enable bits (RXCHENX) while updating TDM slot enable bits.
  i2s->RER0 = (i2s->RER0 & ~(_I2S_RER0_MASK & ~_I2S_RER0_RXCHENX_MASK))
              | ((slots & 0xFFFFUL) << _I2S_RER0_RXSLOT0_EN_SHIFT);
}
#endif // SL_HAL_I2S_TDM_SUPPORT

/***************************************************************************//**
 * Disable all channel TX/RX enables for this instance.
 ******************************************************************************/
static void i2s_disable_all_channels(I2S_TypeDef_t *i2s)
{
  uint32_t channel_count = i2s_instance_channel_count(i2s);

  for (uint32_t ch = 0; ch < channel_count; ch++) {
    sl_i2s_channel_id_t channel_id = (sl_i2s_channel_id_t)ch;
    i2s_enable_tx_channel(i2s, channel_id, false);
    i2s_enable_rx_channel(i2s, channel_id, false);
  }
}

/***************************************************************************//**
 * Quiesce every channel before FIFO flush during @ref sl_hal_i2s_reset: disable
 * TX/RX and clear sticky overrun/underrun latches via TICR/ROR readback.
 ******************************************************************************/
static void i2s_quiesce_channels_for_reset(I2S_TypeDef_t *i2s)
{
  uint32_t channel_count = i2s_instance_channel_count(i2s);

  for (uint32_t ch = 0; ch < channel_count; ch++) {
    sl_i2s_channel_id_t channel_id = (sl_i2s_channel_id_t)ch;
    i2s_enable_tx_channel(i2s, channel_id, false);
    i2s_enable_rx_channel(i2s, channel_id, false);
    sl_hal_i2s_clear_tx_errors(i2s, channel_id);
    sl_hal_i2s_clear_rx_overrun(i2s, channel_id);
  }
}

/***************************************************************************//**
 * Stop instance, serial clock, and block paths; quiesce channels; clear DMA;
 * mask interrupts; flush FIFOs. Shared by all reset paths.
 ******************************************************************************/
static void i2s_reset_quiesce(I2S_TypeDef_t *i2s)
{
  // Stop instance, serial clock, and block enables before touching channels/FIFO.
  i2s->IER &= ~I2S_IER_IEN;
  i2s->CER &= ~_I2S_CER_CLKEN_MASK;
  i2s_enable_transmitter(i2s, false);
  i2s_enable_receiver(i2s, false);

  // Disable each channel and clear sticky TX/RX error latches (TICR / ROR readback).
  i2s_quiesce_channels_for_reset(i2s);

  // Clear DMA enables and pulse DMA resets.
  i2s->DMACR = 0;
  i2s_reset_rxdma(i2s);
  i2s_reset_txdma(i2s);

  // Mask all channel interrupt sources (IMR defaults: interrupts disabled).
  i2s->IMR0 = I2S_IMR0_MASK_ALL;

  if (SL_HAL_I2S_IS_INSTANCE(i2s)) {
    i2s->IMR1 = (_I2S_IMR1_RXDAM_MASK
                  | _I2S_IMR1_RXFOM_MASK
                  | _I2S_IMR1_TXFEM_MASK
                  | _I2S_IMR1_TXFOM_MASK
                  | _I2S_IMR1_TXFUM_MASK);

    i2s->IMR2 = (_I2S_IMR2_RXDAM_MASK
                  | _I2S_IMR2_RXFOM_MASK
                  | _I2S_IMR2_TXFEM_MASK
                  | _I2S_IMR2_TXFOM_MASK
                  | _I2S_IMR2_TXFUM_MASK);
  }

  // Flush FIFOs after channels are quiesced and sticky errors are cleared.
  i2s->RXFFR = I2S_RXFFR_RXFFR;
  i2s->TXFFR = I2S_TXFFR_TXFFR;
}

#if defined(I2ST_PRESENT)
/***************************************************************************//**
 * Restore I2ST hardware-default registers (target-only instance; not IER op mode).
 ******************************************************************************/
static void i2s_reset_target(I2S_TypeDef_t *i2s)
{
  i2s->IER = (I2ST_IER_AUDIO_INTF_TYPE_DEFAULT
              | I2ST_IER_TDM_SLOTS_DEFAULT
              | I2ST_IER_I2S_OP_MODE_DEFAULT
              | I2ST_IER_SD_ENDIAN_DEFAULT);

  i2s->IRER = I2ST_IRER_RXEN_DEFAULT;
  i2s->ITER = I2ST_ITER_TXEN_DEFAULT;
  i2s->CER = I2ST_CER_CLKEN_DEFAULT;

  i2s->CCR = (I2ST_CCR_SCLKG_DEFAULT
              | I2ST_CCR_WSS_DEFAULT
              | I2ST_CCR_TDM_SLOT_LENGTH_DEFAULT);

  i2s->RER0 = _I2ST_RER0_RESETVALUE;
  i2s->TER0 = _I2ST_TER0_RESETVALUE;
  i2s->RCR0 = I2ST_RCR0_WLEN_DEFAULT;
  i2s->TCR0 = I2ST_TCR0_WLEN_DEFAULT;
}
#endif

/***************************************************************************//**
 * Restore I2S hardware-default registers (I2S block; not IER operating mode).
 ******************************************************************************/
static void i2s_reset_controller(I2S_TypeDef_t *i2s)
{
  i2s->IER = (I2S_IER_AUDIO_INTF_TYPE_DEFAULT
              | I2S_IER_TDM_SLOTS_DEFAULT
              | I2S_IER_I2S_OP_MODE_DEFAULT
              | I2S_IER_SD_ENDIAN_DEFAULT);

  i2s->IRER = I2S_IRER_RXEN_DEFAULT;
  i2s->ITER = I2S_ITER_TXEN_DEFAULT;
  i2s->CER = I2S_CER_CLKEN_DEFAULT;

  i2s->CCR = (I2S_CCR_SCLKG_DEFAULT
              | I2S_CCR_WSS_DEFAULT
              | I2S_CCR_TDM_SLOT_LENGTH_DEFAULT);

  i2s->RER0 = _I2S_RER0_RESETVALUE;
  i2s->TER0 = _I2S_TER0_RESETVALUE;
  i2s->RCR0 = I2S_RCR0_WLEN_DEFAULT;
  i2s->TCR0 = I2S_TCR0_WLEN_DEFAULT;

  if (SL_HAL_I2S_IS_INSTANCE(i2s)) {
    i2s->RER1 = _I2S_RER1_RESETVALUE;
    i2s->TER1 = _I2S_TER1_RESETVALUE;
    i2s->RCR1 = I2S_RCR1_WLEN_DEFAULT;
    i2s->TCR1 = I2S_TCR1_WLEN_DEFAULT;

    i2s->RER2 = _I2S_RER2_RESETVALUE;
    i2s->TER2 = _I2S_TER2_RESETVALUE;
    i2s->RCR2 = I2S_RCR2_WLEN_DEFAULT;
    i2s->TCR2 = I2S_TCR2_WLEN_DEFAULT;
  }
}

/***************************************************************************//**
 * Number of channels for this instance.
 *
 * Public entry points that take @c i2s use @ref SL_HAL_I2S_REF_VALID; if neither
 * I2S nor I2ST is recognized, returns 0 so helpers do not assume I2ST and touch
 * an arbitrary address.
 ******************************************************************************/
static uint32_t i2s_instance_channel_count(I2S_TypeDef_t *i2s)
{
  if (SL_HAL_I2S_IS_INSTANCE(i2s)) {
    return (uint32_t)SL_I2S_CHANNEL_ID_MAX;
#if defined(I2ST_PRESENT)
  } else if (SL_HAL_I2ST_IS_INSTANCE(i2s)) {
    return (uint32_t)SL_I2ST_MAX_CHANNELS;
#endif
  }
  return 0U;
}

/***************************************************************************//**
 * Return true if any channel has TX enabled (TERx.TXCHENX), accounting for I2S vs I2ST.
 ******************************************************************************/
static bool i2s_instance_any_tx_channel_enabled(I2S_TypeDef_t *i2s)
{
  uint32_t channel_count = i2s_instance_channel_count(i2s);

  for (uint32_t ch = 0; ch < channel_count; ch++) {
    volatile uint32_t *ter = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, TER0, (sl_i2s_channel_id_t)ch);
    if ((*ter & (uint32_t)_I2S_TER0_TXCHENX_MASK) != 0U) {
      return true;
    }
  }
  return false;
}

/***************************************************************************//**
 * Return true if any channel has RX enabled (RERx.RXCHENX).
 ******************************************************************************/
static bool i2s_instance_any_rx_channel_enabled(I2S_TypeDef_t *i2s)
{
  uint32_t channel_count = i2s_instance_channel_count(i2s);

  for (uint32_t ch = 0; ch < channel_count; ch++) {
    volatile uint32_t *rer = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, RER0, (sl_i2s_channel_id_t)ch);
    if ((*rer & (uint32_t)_I2S_RER0_RXCHENX_MASK) != 0U) {
      return true;
    }
  }
  return false;
}

/***************************************************************************//**
 * Return the configured standard/LJ/RJ word-select size from CCR.WSS.
 ******************************************************************************/
static bool i2s_get_standard_wss_bits(I2S_TypeDef_t *i2s, uint32_t *wss_bits)
{
  uint32_t wss_reg = (i2s->CCR & (uint32_t)_I2S_CCR_WSS_MASK) >> (uint32_t)_I2S_CCR_WSS_SHIFT;

  switch ((i2s_ws_size_t)wss_reg) {
    case I2S_WS_SIZE_16_SCLK:
      *wss_bits = 16U;
      break;

    case I2S_WS_SIZE_24_SCLK:
      *wss_bits = 24U;
      break;

    case I2S_WS_SIZE_32_SCLK:
      *wss_bits = 32U;
      break;

    default:
      return false;
  }

  return true;
}

/***************************************************************************//**
 * Return the Clock Manager serial clock branch for the selected instance.
 ******************************************************************************/
static sl_status_t i2s_get_serial_clock_branch(I2S_TypeDef_t *i2s, sl_clock_branch_t *branch)
{
  if (branch == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!SL_HAL_I2S_IS_INSTANCE(i2s) || (I2S_NUM((I2S_TypeDef *)i2s) != 0)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

#if defined(SL_CATALOG_DEVICE_CLOCK_INTERNAL_PRESENT)
  *branch = SL_CLOCK_BRANCH_I2S0CLK;
  return SL_STATUS_OK;
#else
  return SL_STATUS_NOT_AVAILABLE;
#endif
}

/***************************************************************************//**
 * Compute required SCLK from sample rate and the already-configured frame shape.
 ******************************************************************************/
static sl_status_t  i2s_get_sclk_for_sample_rate(I2S_TypeDef_t *i2s,
                                                     uint32_t sample_rate_hz,
                                                     uint32_t *sclk_hz)
{
  uint32_t frame_bits;
  uint32_t format_reg;
  sl_i2s_data_format_t format_enum;

  if ((i2s == NULL) || (sclk_hz == NULL) || (sample_rate_hz == 0U)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  format_reg = (i2s->IER & (uint32_t)_I2S_IER_AUDIO_INTF_TYPE_MASK)
              >> (uint32_t)_I2S_IER_AUDIO_INTF_TYPE_SHIFT;

  if (format_reg > (uint32_t)SL_I2S_DATA_FORMAT_RJ) {
    return SL_STATUS_INVALID_CONFIGURATION;
  }

  format_enum = (sl_i2s_data_format_t)format_reg;

  switch (format_enum) {
    case SL_I2S_DATA_FORMAT_STANDARD:
    case SL_I2S_DATA_FORMAT_LJ:
    case SL_I2S_DATA_FORMAT_RJ:
      if (!i2s_get_standard_wss_bits(i2s, &frame_bits)) {
        return SL_STATUS_INVALID_CONFIGURATION;
      }
      frame_bits *= 2U;
      break;

#if SL_HAL_I2S_TDM_SUPPORT
    case SL_I2S_DATA_FORMAT_TDM: {
      uint32_t slot_length_bits;
      uint32_t num_slots = ((i2s->IER & (uint32_t)_I2S_IER_TDM_SLOTS_MASK)
                            >> (uint32_t)_I2S_IER_TDM_SLOTS_SHIFT) + 1U;

      slot_length_bits = i2s_tdm_slot_length_to_bits(i2s_get_tdm_slot_length(i2s));
      if (slot_length_bits == 0U) {
        return SL_STATUS_INVALID_CONFIGURATION;
      }
      frame_bits = num_slots * slot_length_bits;
      break;
    }
#else
    case SL_I2S_DATA_FORMAT_TDM:
      return SL_STATUS_NOT_SUPPORTED;
#endif

    default:
      return SL_STATUS_INVALID_CONFIGURATION;
  }

  *sclk_hz = sample_rate_hz * frame_bits;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configure I2S0CLK from PERPLL1 using a minimal interim lookup table.
 *
 * TODO : Remove legacy PERPLL1/CMU register access once
 * sli_clock_manager_configure_branch_frequency() is merged on main.
 ******************************************************************************/
static sl_status_t i2s_configure_serial_clock_legacy(I2S_TypeDef_t *i2s, uint32_t sclk_hz)
{
#if defined(PERPLL_PRESENT) && defined(PERPLL1) && defined(CMU) \
  && defined(CMU_I2S0CLKCTRL_CLKSEL_PERPLL1) \
  && defined(SL_BUS_CLOCK_PERPLL1) \
  && !defined(EVEREST_FPGA) && !defined(FPGA)
  static const uint8_t prescaler_dividers[] = { 8U, 4U, 2U, 1U };
  const i2s_perpll_row_t *selected_row = NULL;
  uint8_t selected_prescaler = 0U;
  uint32_t actual_clock_hz;
  sl_status_t status;
  PERPLL_TypeDef *perpll;
  uint32_t ctrl;
  uint32_t dco_cfg;
  uint32_t timeout;

  sl_clock_branch_t clk_branch;

  if (!SL_HAL_I2S_IS_INSTANCE(i2s) || (I2S_NUM((I2S_TypeDef *)i2s) != 0)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = i2s_get_serial_clock_branch(i2s, &clk_branch);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (I2S_PERPLL_LUT_ROW_COUNT == 0U) {
    return SL_STATUS_NOT_AVAILABLE;
  }

  for (uint32_t prescaler_index = 0U; prescaler_index < sizeof(prescaler_dividers); prescaler_index++) {
    uint64_t perpll_hz64 = (uint64_t)sclk_hz * (uint64_t)prescaler_dividers[prescaler_index];

    if (perpll_hz64 > I2S_PERPLL1_MAX_HZ) {
      continue;
    }

    for (uint32_t row = 0U; row < I2S_PERPLL_LUT_ROW_COUNT; row++) {
      if (i2s_perpll_table[row].fout_hz == (uint32_t)perpll_hz64) {
        selected_row = &i2s_perpll_table[row];
        selected_prescaler = prescaler_dividers[prescaler_index];
        break;
      }
    }

    if (selected_row != NULL) {
      break;
    }
  }

  if (selected_row == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_PERPLL1);
  if (status != SL_STATUS_OK) {
    return status;
  }

  /* TODO : Replace direct CMU I2S0CLK programming with Clock Manager. */
  CMU->I2S0CLKCTRL = (CMU->I2S0CLKCTRL & ~((uint32_t)_CMU_I2S0CLKCTRL_CLKEN_MASK
                                           | (uint32_t)_CMU_I2S0CLKCTRL_EN_MASK
                                           | (uint32_t)_CMU_I2S0CLKCTRL_CLKSEL_MASK));

  /* TODO : Replace direct PERPLL1 register access with Clock Manager. */
  perpll = PERPLL1;
  perpll->LOCKKEY = PERPLL_LOCKKEY_LOCKKEY_UNLOCK;
  perpll->CTRL_SET = PERPLL_CTRL_DISONDEMAND;
  perpll->CTRL_CLR = PERPLL_CTRL_FORCEEN;
  timeout = I2S_PERPLL1_LOCK_POLL_MAX_ITERATIONS;
  while (((perpll->STATUS & PERPLL_STATUS_ENS) != 0U) && (timeout > 0U)) {
    timeout--;
  }
  if (timeout == 0U) {
    return SL_STATUS_TIMEOUT;
  }

  ctrl = ((uint32_t)selected_row->fracn_en << _PERPLL_CTRL_ENFRACN_SHIFT)
         | ((uint32_t)selected_row->divn << _PERPLL_CTRL_PERPLLDIVN_SHIFT)
         | ((uint32_t)selected_row->divf << _PERPLL_CTRL_PERPLLDIVF_SHIFT);
  dco_cfg = ((uint32_t)selected_row->dco_div << _PERPLL_DCOCFG_PERPLLDCOOUTDIV_SHIFT)
            | ((uint32_t)selected_row->div_2pow << _PERPLL_DCOCFG_PERPLLOUTDIV2POW_SHIFT);

  perpll->CTRL = (perpll->CTRL & ~(_PERPLL_CTRL_ENFRACN_MASK
                                   | _PERPLL_CTRL_PERPLLDIVN_MASK
                                   | _PERPLL_CTRL_PERPLLDIVF_MASK))
                 | ctrl;
  perpll->DCOCFG = (perpll->DCOCFG & ~(_PERPLL_DCOCFG_PERPLLDCOOUTDIV_MASK
                                       | _PERPLL_DCOCFG_PERPLLOUTDIV2POW_MASK))
                   | dco_cfg;

  perpll->CTRL_SET = PERPLL_CTRL_ENPLLCLKOUT0 | PERPLL_CTRL_ENPLLCLKOUT1;
  perpll->CTRL_SET = PERPLL_CTRL_FORCEEN;
  timeout = I2S_PERPLL1_LOCK_POLL_MAX_ITERATIONS;
  while (((perpll->STATUS & I2S_PERPLL1_LOCK_STATUS_MASK) != I2S_PERPLL1_LOCK_STATUS_MASK) && (timeout > 0U)) {
    timeout--;
  }
  if (timeout == 0U) {
    return SL_STATUS_TIMEOUT;
  }

  SystemPERPLLClockSet(1U, selected_row->fout_hz);
  perpll->CTRL_CLR = PERPLL_CTRL_DISONDEMAND;
  perpll->CTRL_CLR = PERPLL_CTRL_FORCEEN;

  CMU->I2S0CLKCTRL = (CMU->I2S0CLKCTRL & ~((uint32_t)_CMU_I2S0CLKCTRL_CLKSEL_MASK
                                           | (uint32_t)_CMU_I2S0CLKCTRL_PRESC_MASK))
                     | CMU_I2S0CLKCTRL_CLKSEL_PERPLL1
                     | (((uint32_t)(selected_prescaler - 1U) << _CMU_I2S0CLKCTRL_PRESC_SHIFT)
                        & (uint32_t)_CMU_I2S0CLKCTRL_PRESC_MASK)
                     | CMU_I2S0CLKCTRL_CLKEN
                     | CMU_I2S0CLKCTRL_EN;

  status = sl_clock_manager_get_clock_branch_frequency(clk_branch, &actual_clock_hz);
  if (status != SL_STATUS_OK) {
    return status;
  }
  if (actual_clock_hz != sclk_hz) {
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
#else
  (void)i2s;
  (void)sclk_hz;
  return SL_STATUS_NOT_AVAILABLE;
#endif
}

/***************************************************************************//**
 * Configure I2S0CLK for controller mode (interim legacy path until CM merge).
 ******************************************************************************/
static sl_status_t i2s_configure_serial_clock(I2S_TypeDef_t *i2s, uint32_t sclk_hz)
{
  sl_status_t status;
  /* TODO : Remove legacy PERPLL1/CMU register access once
   * sli_clock_manager_configure_branch_frequency() is merged on main.
   * Replace the block below with:
   *
   *   sl_clock_branch_t clk_branch;
   *   status = i2s_get_serial_clock_branch(i2s, &clk_branch);
   *   if (status != SL_STATUS_OK) {
   *     return status;
   *   }
   *   status = sli_clock_manager_configure_branch_frequency(clk_branch, sclk_hz);
   *   if (status != SL_STATUS_OK) {
   *     return status;
   *   }
   *   return SL_STATUS_OK;
   */
  status = i2s_configure_serial_clock_legacy(i2s, sclk_hz);
  return status;
}

/***************************************************************************//**
 * Return the current serial clock branch frequency for controller-mode I2S.
 ******************************************************************************/
static sl_status_t i2s_get_serial_clock_frequency(I2S_TypeDef_t *i2s, uint32_t *clock_frequency_hz)
{
  if (clock_frequency_hz == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (!SL_HAL_I2S_IS_INSTANCE(i2s)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

#if defined(SL_CATALOG_DEVICE_CLOCK_INTERNAL_PRESENT)
  {
    sl_clock_branch_t clk_branch;
    sl_status_t status = i2s_get_serial_clock_branch(i2s, &clk_branch);

    if (status != SL_STATUS_OK) {
      return status;
    }

    return sl_clock_manager_get_clock_branch_frequency(clk_branch, clock_frequency_hz);
  }
#else
  (void)i2s;
#endif

  return SL_STATUS_NOT_AVAILABLE;
}

/***************************************************************************//**
 * Map word length to CCR.WSS encoding.
 ******************************************************************************/
static sl_status_t i2s_word_length_to_ws_size(sl_i2s_word_length_t word_length, i2s_ws_size_t *ws_size)
{
  if (ws_size == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  switch (word_length) {
    case SL_I2S_WORD_LENGTH_12_BIT:
    case SL_I2S_WORD_LENGTH_16_BIT:
      *ws_size = I2S_WS_SIZE_16_SCLK;
      break;

    case SL_I2S_WORD_LENGTH_20_BIT:
      *ws_size = I2S_WS_SIZE_24_SCLK;
      break;

    case SL_I2S_WORD_LENGTH_24_BIT:
      *ws_size = I2S_WS_SIZE_32_SCLK;
      break;

    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Return the maximum word length that fits a CCR.WSS encoding.
 ******************************************************************************/
static sl_i2s_word_length_t i2s_ws_size_to_max_wlen(i2s_ws_size_t ws_size)
{
  switch (ws_size) {
    case I2S_WS_SIZE_16_SCLK:
      return SL_I2S_WORD_LENGTH_16_BIT;

    case I2S_WS_SIZE_24_SCLK:
    case I2S_WS_SIZE_32_SCLK:
      return SL_I2S_WORD_LENGTH_24_BIT;

    default:
      EFM_ASSERT(false);
      return SL_I2S_WORD_LENGTH_NONE;
  }
}

/***************************************************************************//**
 * Update @p max_wlen from one channel's enabled TX/RX paths and word lengths.
 ******************************************************************************/
static sl_status_t i2s_accumulate_channel_max_wlen(bool tx_enabled,
                                                   sl_i2s_word_length_t tx_wlen,
                                                   bool rx_enabled,
                                                   sl_i2s_word_length_t rx_wlen,
                                                   sl_i2s_word_length_t *max_wlen)
{
  if (tx_enabled) {
    if ((tx_wlen < SL_I2S_WORD_LENGTH_12_BIT) || (tx_wlen > SL_I2S_WORD_LENGTH_24_BIT)) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    if (tx_wlen > *max_wlen) {
      *max_wlen = tx_wlen;
    }
  }

  if (rx_enabled) {
    if ((rx_wlen < SL_I2S_WORD_LENGTH_12_BIT) || (rx_wlen > SL_I2S_WORD_LENGTH_24_BIT)) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    if (rx_wlen > *max_wlen) {
      *max_wlen = rx_wlen;
    }
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Return the largest enabled TX/RX word length after applying @p config.
 ******************************************************************************/
static sl_status_t i2s_get_post_config_max_wlen(I2S_TypeDef_t *i2s,
                                                const sl_hal_i2s_channel_config_t *config,
                                                sl_i2s_word_length_t *max_wlen)
{
  sl_status_t status;
  bool new_tx_enabled;
  bool new_rx_enabled;

  if (max_wlen == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  *max_wlen = SL_I2S_WORD_LENGTH_NONE;
  new_tx_enabled = (config->direction == SL_I2S_DIR_TX)
                   || (config->direction == SL_I2S_DIR_TX_RX);
  new_rx_enabled = (config->direction == SL_I2S_DIR_RX)
                   || (config->direction == SL_I2S_DIR_TX_RX);

  for (uint32_t ch = 0U; ch < i2s_instance_channel_count(i2s); ch++) {
    sl_i2s_channel_id_t channel_id = (sl_i2s_channel_id_t)ch;

    if (channel_id == config->id) {
      status = i2s_accumulate_channel_max_wlen(new_tx_enabled,
                                               config->tx_length,
                                               new_rx_enabled,
                                               config->rx_length,
                                               max_wlen);
    } else {
      volatile uint32_t *ter = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, TER0, channel_id);
      volatile uint32_t *rer = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, RER0, channel_id);
      volatile uint32_t *tcr = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, TCR0, channel_id);
      volatile uint32_t *rcr = SL_HAL_I2S_CHANNEL_REG_PTR(i2s, RCR0, channel_id);
      bool tx_enabled = ((*ter & (uint32_t)_I2S_TER0_TXCHENX_MASK) != 0U);
      bool rx_enabled = ((*rer & (uint32_t)_I2S_RER0_RXCHENX_MASK) != 0U);
      sl_i2s_word_length_t tx_wlen = (sl_i2s_word_length_t)(*tcr & (uint32_t)_I2S_TCR0_WLEN_MASK);
      sl_i2s_word_length_t rx_wlen = (sl_i2s_word_length_t)(*rcr & (uint32_t)_I2S_RCR0_WLEN_MASK);

      status = i2s_accumulate_channel_max_wlen(tx_enabled,
                                               tx_wlen,
                                               rx_enabled,
                                               rx_wlen,
                                               max_wlen);
    }

    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Map the largest enabled word length to a CCR.WSS encoding (standard/LJ/RJ).
 ******************************************************************************/
static sl_status_t i2s_compute_ws_size_for_max_wlen(I2S_TypeDef_t *i2s,
                                                      sl_i2s_word_length_t max_wlen,
                                                      i2s_ws_size_t *ws_size,
                                                      bool *ws_update_needed)
{
  uint32_t format_reg;
  uint32_t op_mode_reg;
  sl_i2s_data_format_t format;
  sl_status_t status;

  format_reg = (i2s->IER & (uint32_t)_I2S_IER_AUDIO_INTF_TYPE_MASK)
               >> (uint32_t)_I2S_IER_AUDIO_INTF_TYPE_SHIFT;

  if ((format_reg == (uint32_t)SL_I2S_DATA_FORMAT_TDM)
      && ((ws_size == NULL) || (ws_update_needed == NULL))) {
      return SL_STATUS_NULL_POINTER;
  }

  if (format_reg == (uint32_t)SL_I2S_DATA_FORMAT_TDM) {
    *ws_update_needed = false;
    return SL_STATUS_OK;
  }

  *ws_update_needed = false;

  if (format_reg > (uint32_t)SL_I2S_DATA_FORMAT_RJ) {
    return SL_STATUS_INVALID_CONFIGURATION;
  }
  format = (sl_i2s_data_format_t)format_reg;

  if (max_wlen == SL_I2S_WORD_LENGTH_NONE) {
    return SL_STATUS_OK;
  }

  op_mode_reg = (i2s->IER & (uint32_t)_I2S_IER_I2S_OP_MODE_MASK)
                >> (uint32_t)_I2S_IER_I2S_OP_MODE_SHIFT;

  if ((op_mode_reg == (uint32_t)SL_I2S_OPERATING_MODE_TARGET)
      && ((format == SL_I2S_DATA_FORMAT_LJ) || (format == SL_I2S_DATA_FORMAT_RJ))) {
    *ws_size = I2S_WS_SIZE_32_SCLK;
  } else {
    status = i2s_word_length_to_ws_size(max_wlen, ws_size);
    if (status != SL_STATUS_OK) {
      return status;
    }
  }

  if (max_wlen > i2s_ws_size_to_max_wlen(*ws_size)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  *ws_update_needed = true;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Dry-run WSS resolution for the post-config channel state before register writes.
 ******************************************************************************/
static sl_status_t i2s_validate_ws_size_for_channel_config(I2S_TypeDef_t *i2s,
                                                           const sl_hal_i2s_channel_config_t *config,
                                                           i2s_ws_size_t *ws_size,
                                                           bool *ws_update_needed)
{
  sl_i2s_word_length_t max_wlen;
  sl_status_t status;

  if ((ws_size == NULL) || (ws_update_needed == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  status = i2s_get_post_config_max_wlen(i2s, config, &max_wlen);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return i2s_compute_ws_size_for_max_wlen(i2s, max_wlen, ws_size, ws_update_needed);
}

/***************************************************************************//**
 * Program @c CCR.WSS from a value already validated by the caller.
 ******************************************************************************/
static void i2s_apply_ws_size(I2S_TypeDef_t *i2s, i2s_ws_size_t ws_size)
{
  i2s->CCR = (i2s->CCR & ~(uint32_t)_I2S_CCR_WSS_MASK)
             | (((uint32_t)ws_size << (uint32_t)_I2S_CCR_WSS_SHIFT) & (uint32_t)_I2S_CCR_WSS_MASK);
}

#if SL_HAL_I2S_TDM_SUPPORT
/***************************************************************************//**
 * Return CCR.TDM_SLOT_LENGTH enum value.
 ******************************************************************************/
static sl_i2s_tdm_slot_length_t i2s_get_tdm_slot_length(I2S_TypeDef_t *i2s)
{
  uint32_t slot_length;

  slot_length = (i2s->CCR & (uint32_t)_I2S_CCR_TDM_SLOT_LENGTH_MASK)
                >> (uint32_t)_I2S_CCR_TDM_SLOT_LENGTH_SHIFT;

  return (sl_i2s_tdm_slot_length_t)slot_length;
}

/***************************************************************************//**
 * Return TDM slot length in bits for SCLK frame calculation.
 ******************************************************************************/
static uint32_t i2s_tdm_slot_length_to_bits(sl_i2s_tdm_slot_length_t length)
{
  switch (length) {
    case SL_I2S_TDM_SLOT_LENGTH_12:
      return 12U;

    case SL_I2S_TDM_SLOT_LENGTH_16:
      return 16U;

    case SL_I2S_TDM_SLOT_LENGTH_20:
      return 20U;

    case SL_I2S_TDM_SLOT_LENGTH_24:
      return 24U;

    case SL_I2S_TDM_SLOT_LENGTH_32:
      return 32U;

    default:
      return 0U;
  }
}

/***************************************************************************//**
 * Return the maximum word length that fits a TDM slot length.
 ******************************************************************************/
static sl_i2s_word_length_t i2s_tdm_slot_length_to_max_wlen(sl_i2s_tdm_slot_length_t length)
{
  switch (length) {
    case SL_I2S_TDM_SLOT_LENGTH_12:
      return SL_I2S_WORD_LENGTH_12_BIT;

    case SL_I2S_TDM_SLOT_LENGTH_16:
      return SL_I2S_WORD_LENGTH_16_BIT;

    case SL_I2S_TDM_SLOT_LENGTH_20:
      return SL_I2S_WORD_LENGTH_20_BIT;

    case SL_I2S_TDM_SLOT_LENGTH_24:
    case SL_I2S_TDM_SLOT_LENGTH_32:
      return SL_I2S_WORD_LENGTH_24_BIT;

    default:
      return SL_I2S_WORD_LENGTH_NONE;
  }
}

/***************************************************************************//**
 * Set CCR.TDM_SLOT_LENGTH.
 ******************************************************************************/
static void i2s_set_tdm_slot_length(I2S_TypeDef_t *i2s, sl_i2s_tdm_slot_length_t length)
{
  uint32_t reg_length = (uint32_t)length;

  i2s->CCR = (i2s->CCR & ~_I2S_CCR_TDM_SLOT_LENGTH_MASK)
             | ((reg_length << _I2S_CCR_TDM_SLOT_LENGTH_SHIFT) & _I2S_CCR_TDM_SLOT_LENGTH_MASK);
}
#endif // SL_HAL_I2S_TDM_SUPPORT
#endif /* defined(I2S_PRESENT) */
