/***************************************************************************//**
 * @file
 * @brief Operational Amplifier (OPA) Peripheral API
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

#ifndef SL_HAL_OPA_H
#define SL_HAL_OPA_H

#include "em_device.h"

#if defined(OPA_PRESENT)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sl_log_helper.h"
#include "sl_enum.h"
#include "sl_status.h"

/***************************************************************************//**
 * @addtogroup opa OPA - Operational Amplifier
 * @brief Operational Amplifier peripheral API
 * @{
 ******************************************************************************/

/*******************************************************************************
 *********************************   DEFINES   *********************************
 ******************************************************************************/

/// Validation of OPA register block pointer reference for assert statements.
#define SL_HAL_OPA_REF_VALID(ref)       (OPA_NUM((OPA_TypeDef *)(ref)) != -1)

/// GPIO port selection used for OPA ABUS routing.
/// Use @ref SL_HAL_OPA_PORT_A through @ref SL_HAL_OPA_PORT_D for GPIO routes.
typedef uint8_t sl_hal_opa_port_t;

/// ABUS lane selection used by OPA input and output routing.
/// Select one valid lane based on the signal path and pin parity.
typedef uint8_t sl_hal_opa_bus_t;

/// No GPIO port selected.
#define SL_HAL_OPA_PORT_NONE            0U
/// GPIO port A.
#define SL_HAL_OPA_PORT_A               1U
/// GPIO port B.
#define SL_HAL_OPA_PORT_B               2U
/// GPIO port C.
#define SL_HAL_OPA_PORT_C               3U
/// GPIO port D.
#define SL_HAL_OPA_PORT_D               4U

/// No ABUS lane selected.
#define SL_HAL_OPA_BUS_NONE             0U
/// Even ABUS lane 0.
#define SL_HAL_OPA_BUS_EVEN0            1U
/// Even ABUS lane 1.
#define SL_HAL_OPA_BUS_EVEN1            2U
/// Odd ABUS lane 0.
#define SL_HAL_OPA_BUS_ODD0             3U
/// Odd ABUS lane 1.
#define SL_HAL_OPA_BUS_ODD1             4U

/*******************************************************************************
 ********************************   ENUMS   ************************************
 ******************************************************************************/

/// OPA operating mode.
SL_ENUM(sl_hal_opa_mode_t) {
  SL_HAL_OPA_MODE_UNITY_GAIN = 0x0,             ///< Unity-gain buffer.
  SL_HAL_OPA_MODE_TIA,                          ///< Trans-impedance amplifier.
  SL_HAL_OPA_MODE_PGA_NON_INVERTING,            ///< Non-inverting PGA.
  SL_HAL_OPA_MODE_PGA_INVERTING,                ///< Inverting PGA.
  SL_HAL_OPA_MODE_HYST_COMP_NON_INVERTING,      ///< Non-inverting hysteresis comparator.
  SL_HAL_OPA_MODE_HYST_COMP_INVERTING,          ///< Inverting hysteresis comparator.
  SL_HAL_OPA_MODE_CASCADED_PGA_NON_INVERTING,   ///< Cascaded non-inverting PGA.
  SL_HAL_OPA_MODE_CASCADED_PGA_INVERTING,       ///< Cascaded inverting PGA.
  SL_HAL_OPA_MODE_DIFF_AMP,                     ///< Two-op-amp differential amplifier.
  SL_HAL_OPA_MODE_MAX
};

/// Gain selection.
/// Selects the internal R1/R2 resistor combination used to set the OPA gain.
/// Default after reset is @ref SL_HAL_OPA_GAIN_X1P33.
SL_ENUM(sl_hal_opa_gain_t) {
  SL_HAL_OPA_GAIN_X1P33 = _OPA_CTRL1_SELGAIN_X1P33,  ///< x1.33; R1 = 109.2 kOhm, R2 = 36.0 kOhm.
  SL_HAL_OPA_GAIN_X2    = _OPA_CTRL1_SELGAIN_X2,     ///< x2;    R1 = 72.6 kOhm,  R2 = 72.6 kOhm.
  SL_HAL_OPA_GAIN_X2P66 = _OPA_CTRL1_SELGAIN_X2P66,  ///< x2.66; R1 = 54.6 kOhm,  R2 = 90.6 kOhm.
  SL_HAL_OPA_GAIN_X3P2  = _OPA_CTRL1_SELGAIN_X3P2,   ///< x3.2;  R1 = 45.4 kOhm,  R2 = 99.8 kOhm.
  SL_HAL_OPA_GAIN_X4    = _OPA_CTRL1_SELGAIN_X4,     ///< x4;    R1 = 36.3 kOhm,  R2 = 108.9 kOhm.
  SL_HAL_OPA_GAIN_X5P33 = _OPA_CTRL1_SELGAIN_X5P33,  ///< x5.33; R1 = 27.2 kOhm,  R2 = 118.0 kOhm.
  SL_HAL_OPA_GAIN_X8    = _OPA_CTRL1_SELGAIN_X8,     ///< x8;    R1 = 18.2 kOhm,  R2 = 127.0 kOhm.
  SL_HAL_OPA_GAIN_X16   = _OPA_CTRL1_SELGAIN_X16,    ///< x16;   R1 = 9.1 kOhm,   R2 = 136.1 kOhm.
  SL_HAL_OPA_GAIN_X3    = 0x8,                       ///< x3;    R1 = 48.4 kOhm,  R2 = 96.8 kOhm.
  SL_HAL_OPA_GAIN_MAX
};

/// Resistive reference selection.
/// Selects the signal connected to the bottom of the internal R1 resistor.
/// The selected source is used as VIN or VREF depending on the OPA mode.
/// This setting is configured during initialization through @c init->res_sel.
SL_ENUM(sl_hal_opa_res_sel_t) {
  SL_HAL_OPA_RES_INTERNAL_GND = _OPA_CTRL1_SELRES_INTERNAL_GND,  ///< Connect R1 to internal ground.
  SL_HAL_OPA_RES_VINRES0      = _OPA_CTRL1_SELRES_VINRES0,       ///< Connect R1 to the shared OPA0/OPA1 resistor node; used in differential mode.
  SL_HAL_OPA_RES_VINRES1      = _OPA_CTRL1_SELRES_VINRES1,       ///< Connect R1 to the other OPA output; used in cascaded inverting mode.
  SL_HAL_OPA_RES_VINRES2      = _OPA_CTRL1_SELRES_VINRES2,       ///< Not supported on this device; rejected by the HAL.
  SL_HAL_OPA_RES_VINRES3      = _OPA_CTRL1_SELRES_VINRES3,       ///< Connect R1 to the dedicated OPAn_NEG pad.
  SL_HAL_OPA_RES_VINRES4      = _OPA_CTRL1_SELRES_VINRES4,       ///< Connect R1 to the dedicated OPAn_POS pad.
  SL_HAL_OPA_RES_MAX
};

/// Active input differential-pair selection.
/// Default after reset is @ref SL_HAL_OPA_INPUT_STAGE_RAIL_TO_RAIL.
SL_ENUM(sl_hal_opa_input_stage_t) {
  SL_HAL_OPA_INPUT_STAGE_NONE = 0x0,     ///< Disable both input pairs.
  SL_HAL_OPA_INPUT_STAGE_NMOS,           ///< Enable NMOS input pair only.
  SL_HAL_OPA_INPUT_STAGE_PMOS,           ///< Enable PMOS input pair only.
  SL_HAL_OPA_INPUT_STAGE_RAIL_TO_RAIL,   ///< Enable both input pairs; reset default.
  SL_HAL_OPA_INPUT_STAGE_MAX
};

/// Input/output swap selection.
/// Selects the internal input/output swap configuration:
/// - @ref SL_HAL_OPA_SWAP_NONE and @ref SL_HAL_OPA_SWAP_BOTH: amplifier modes.
/// - @ref SL_HAL_OPA_SWAP_INPUT and @ref SL_HAL_OPA_SWAP_OUTPUT: hysteresis
///   comparator modes (positive feedback).
SL_ENUM(sl_hal_opa_swap_t) {
  SL_HAL_OPA_SWAP_NONE   = _OPA_CTRL1_SWP_NOT_SWAPPED,        ///< No swap; normal negative-feedback amplifier operation.
  SL_HAL_OPA_SWAP_INPUT  = _OPA_CTRL1_SWP_SWAP_INPUT,         ///< Swap inputs; used for comparator positive feedback.
  SL_HAL_OPA_SWAP_OUTPUT = _OPA_CTRL1_SWP_SWAP_OUTPUT,        ///< Swap output path; used for comparator positive feedback.
  SL_HAL_OPA_SWAP_BOTH   = _OPA_CTRL1_SWP_SWAP_INPUT_OUTPUT,  ///< Swap input and output; used in amplifier modes for offset-polarity reversal.
  SL_HAL_OPA_SWAP_MAX
};

/// Output-load compensation selection.
/// Selects the internal compensation based on the expected capacitive load.
/// The EXTRA options provide additional compensation for better phase margin.
/// Default after reset is @ref SL_HAL_OPA_COMP_50PF.
SL_ENUM(sl_hal_opa_comp_cap_t) {
  SL_HAL_OPA_COMP_5PF        = _OPA_CTRL2_SELCL_LOAD_5PF,         ///< Compensation for approximately 5 pF load.
  SL_HAL_OPA_COMP_5PF_EXTRA  = _OPA_CTRL2_SELCL_LOAD_5PF_EXTRA,   ///< 5 pF load with extra compensation for better phase margin.
  SL_HAL_OPA_COMP_50PF       = _OPA_CTRL2_SELCL_LOAD_50PF,        ///< Compensation for approximately 50 pF load; reset default.
  SL_HAL_OPA_COMP_50PF_EXTRA = _OPA_CTRL2_SELCL_LOAD_50PF_EXTRA,  ///< 50 pF load with extra compensation for better phase margin.
  SL_HAL_OPA_COMP_MAX
};

/// Positive-input source selection.
SL_ENUM(sl_hal_opa_posin_source_t) {
  SL_HAL_OPA_POSIN_NONE     = _OPA_ABUSCTRLPOSIN_PORTSEL_NONE,     ///< No positive-input source selected.
  SL_HAL_OPA_POSIN_OPA_NEXT = _OPA_ABUSCTRLPOSIN_PORTSEL_OPA_NEXT, ///< Input from the other OPA for cascaded operation.
  SL_HAL_OPA_POSIN_OPA_PAD  = _OPA_ABUSCTRLPOSIN_PORTSEL_OPA_PAD,  ///< Input from the dedicated OPA positive pad.
  SL_HAL_OPA_POSIN_GPIO,                                           ///< Input from a GPIO through ABUS; use port and pin.
  SL_HAL_OPA_POSIN_MAX
};

/// Negative-input source selection.
SL_ENUM(sl_hal_opa_negin_source_t) {
  SL_HAL_OPA_NEGIN_NONE     = _OPA_ABUSCTRLNEGIN_PORTSEL_NONE,     ///< No negative-input source selected.
  SL_HAL_OPA_NEGIN_FB_X1    = _OPA_ABUSCTRLNEGIN_PORTSEL_FB_X1,    ///< Direct unity-gain feedback path.
  SL_HAL_OPA_NEGIN_FB_XN    = _OPA_ABUSCTRLNEGIN_PORTSEL_FB_XN,    ///< Feedback through the programmable resistor network.
  SL_HAL_OPA_NEGIN_OPA_PAD  = _OPA_ABUSCTRLNEGIN_PORTSEL_OPA_PAD,  ///< Input from the dedicated OPA negative pad.
  SL_HAL_OPA_NEGIN_GPIO,                                           ///< Input from a GPIO through ABUS; use port and pin.
  SL_HAL_OPA_NEGIN_MAX
};

/// Alternate-output destination selection.
SL_ENUM(sl_hal_opa_out_source_t) {
  SL_HAL_OPA_OUT_NONE       = _OPA_ABUSCTRLOUT_PORTSEL_NONE,       ///< No alternate-output route selected.
  SL_HAL_OPA_OUT_OPA_NEXT   = _OPA_ABUSCTRLOUT_PORTSEL_OPA_NEXT,   ///< Route output to the other OPA for cascaded operation.
  SL_HAL_OPA_OUT_OPA_TO_CMP = _OPA_ABUSCTRLOUT_PORTSEL_OPA_TO_CMP, ///< Route output internally to the comparator.
  SL_HAL_OPA_OUT_GPIO,                                             ///< Route output to a GPIO through ABUS; use port and pin.
  SL_HAL_OPA_OUT_MAX
};

/// OPA interrupt flags.
/// Multiple flags can be combined using bitwise OR (`|`).
SL_ENUM_GENERIC(sl_hal_opa_interrupt_flag_t, uint32_t) {
  SL_HAL_OPA_IF_APORTALLOCERR      = OPA_IF_APORTALLOCERR,      ///< Requested APORT resource was not allocated to the OPA.
  SL_HAL_OPA_IF_APORTALLOCCONFLICT = OPA_IF_APORTALLOCCONFLICT, ///< Requested APORT resource is already allocated to another peripheral.
};

/// OPA status flags.
/// Values report live hardware conditions and clear when the underlying condition is no longer present.
SL_ENUM_GENERIC(sl_hal_opa_status_flag_t, uint32_t) {
  SL_HAL_OPA_STATUS_CGM                = OPA_STATUS_STATUSCGM,           ///< Constant-gm biasing is currently active.
  SL_HAL_OPA_STATUS_LOCK               = OPA_STATUS_LOCK,                ///< Protected OPA registers are currently locked.
  SL_HAL_OPA_STATUS_APORTALLOCERR      = OPA_STATUS_APORTALLOCERR,       ///< An APORT allocation error is currently present.
  SL_HAL_OPA_STATUS_APORTALLOCCONFLICT = OPA_STATUS_APORTALLOCCONFLICT,  ///< An APORT allocation conflict is currently present.
};

/*******************************************************************************
 *******************************   STRUCTS   ***********************************
 ******************************************************************************/

/// Routing configuration for the OPA positive input.
/// @c port, @c pin, and @c bus are used only when @c source is
/// @ref SL_HAL_OPA_POSIN_GPIO. The positive input reaches the ABUS through an
/// even lane, so @c bus must select exactly one of @ref SL_HAL_OPA_BUS_EVEN0 or
/// @ref SL_HAL_OPA_BUS_EVEN1. @c port must be one of @ref SL_HAL_OPA_PORT_A
/// through @ref SL_HAL_OPA_PORT_D.
typedef struct {
  sl_hal_opa_posin_source_t source;   ///< Positive-input source.
  sl_hal_opa_port_t port;             ///< GPIO port for an ABUS route (@ref SL_HAL_OPA_PORT_A .. @ref SL_HAL_OPA_PORT_D).
  uint8_t pin;                        ///< GPIO pin number for an ABUS route.
  sl_hal_opa_bus_t bus;               ///< ABUS lane for an ABUS route.
} sl_hal_opa_posin_route_t;

/// Routing configuration for the OPA negative input.
/// @c port, @c pin, and @c bus are used only when @c source is
/// @ref SL_HAL_OPA_NEGIN_GPIO. The negative input reaches the ABUS through an
/// odd lane, so @c bus must select exactly one of @ref SL_HAL_OPA_BUS_ODD0 or
/// @ref SL_HAL_OPA_BUS_ODD1. @c port must be one of @ref SL_HAL_OPA_PORT_A
/// through @ref SL_HAL_OPA_PORT_D.
typedef struct {
  sl_hal_opa_negin_source_t source;   ///< Negative-input source.
  sl_hal_opa_port_t port;             ///< GPIO port for an ABUS route (@ref SL_HAL_OPA_PORT_A .. @ref SL_HAL_OPA_PORT_D).
  uint8_t pin;                        ///< GPIO pin number for an ABUS route.
  sl_hal_opa_bus_t bus;               ///< ABUS lane for an ABUS route.
} sl_hal_opa_negin_route_t;

/// Routing configuration for the OPA alternate output.
/// @c port, @c pin, and @c bus are used only when @c source is
/// @ref SL_HAL_OPA_OUT_GPIO. The output can use any single lane from
/// @ref SL_HAL_OPA_BUS_EVEN0 through @ref SL_HAL_OPA_BUS_ODD1. @c port must be one of @ref SL_HAL_OPA_PORT_A
/// through @ref SL_HAL_OPA_PORT_D.
typedef struct {
  sl_hal_opa_out_source_t source;     ///< Alternate-output destination.
  sl_hal_opa_port_t port;             ///< GPIO port for an ABUS route (@ref SL_HAL_OPA_PORT_A .. @ref SL_HAL_OPA_PORT_D).
  uint8_t pin;                        ///< GPIO pin number for an ABUS route.
  sl_hal_opa_bus_t bus;               ///< ABUS lane for an ABUS route.
} sl_hal_opa_out_route_t;

/// OPA initialization structure.
/// Defines the operating mode, signal routing, resistor source, and swap setting.
typedef struct {
  sl_hal_opa_mode_t mode;               ///< OPA operating mode.
  sl_hal_opa_posin_route_t pos_input;   ///< Positive-input routing.
  sl_hal_opa_negin_route_t neg_input;   ///< Negative-input routing.
  sl_hal_opa_out_route_t output;        ///< Alternate-output routing.
  sl_hal_opa_res_sel_t res_sel;         ///< Source connected to internal R1.
  sl_hal_opa_swap_t swap;               ///< Input/output swap setting.
} sl_hal_opa_init_t;

/*******************************************************************************
 *****************************   PROTOTYPES   **********************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Get the OPA hardware revision.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return OPA hardware revision identifier.
 ******************************************************************************/
__INLINE uint32_t sl_hal_opa_get_version(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  return opa->IPVERSION;
}

/***************************************************************************//**
 * @brief Initialize a single OPA instance.
 *
 * @details
 * Configures the selected OPA operating mode, internal signal paths, feedback
 * path, and ABUS routing. If the instance is enabled, it is disabled before
 * the new configuration is applied. The OPA remains disabled after
 * initialization.
 *
 * Gain, input stage, constant-gm biasing, main output, output drive, and
 * compensation retain their reset values and can be configured using the
 * corresponding setter APIs before enabling the OPA. The HAL automatically
 * applies factory trim values.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 * @param[in] init Pointer to the OPA initialization settings.
 *                 @c init->mode selects one of the supported single-instance
 *                 modes:
 *                 - @c SL_HAL_OPA_MODE_UNITY_GAIN
 *                 - @c SL_HAL_OPA_MODE_TIA
 *                 - @c SL_HAL_OPA_MODE_PGA_NON_INVERTING
 *                 - @c SL_HAL_OPA_MODE_PGA_INVERTING
 *                 - @c SL_HAL_OPA_MODE_HYST_COMP_NON_INVERTING
 *                 - @c SL_HAL_OPA_MODE_HYST_COMP_INVERTING
 *
 *                 - @c init->pos_input selects the positive-input route.
 *                 - @c init->neg_input must match the mode feedback tap:
 *                   @c SL_HAL_OPA_NEGIN_FB_X1 for unity gain,
 *                   @c SL_HAL_OPA_NEGIN_FB_XN for PGA and hysteresis modes.
 *                   TIA uses a caller-owned route (typically GPIO).
 *                 - @c init->output selects the alternate-output route.
 *                 - @c init->res_sel selects the source connected through the
 *                   internal R1 resistor.
 *                 - @c init->swap selects the input/output swap configuration.
 *
 * @note Use @ref sl_hal_opa_set_main_output when the dedicated @c OPAn_OUT pin
 *       is required.
 * @note Clock configuration, GPIO/APORT allocation, and power-domain
 *       management are handled outside the OPA HAL.
 * @note Use @ref sl_hal_opa_init_dual for cascaded and differential modes.
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_NULL_POINTER if @p init is NULL.
 *         SL_STATUS_INVALID_PARAMETER if an initialization setting or route
 *         is invalid.
 *         SL_STATUS_NOT_SUPPORTED if a dual-instance mode is requested.
 *         SL_STATUS_INVALID_STATE if the OPA is locked.
 ******************************************************************************/
sl_status_t sl_hal_opa_init(OPA_TypeDef *opa, const sl_hal_opa_init_t *init);

/***************************************************************************//**
 * @brief Initialize two OPA instances for a dual-instance topology.
 *
 * @details
 * Configures both OPA instances and establishes the internal connections
 * required for cascaded or differential operation. Enabled instances are
 * disabled before the new configuration is applied. Both instances are left
 * disabled after initialization.
 *
 * For cascaded modes, @p first is the input stage and @p second is the output
 * stage. For differential mode, both instances have equivalent roles.
 * Factory trim values are applied automatically to both OPA instances from
 * device calibration data.
 *
 * @param[in] first
 *   Pointer to the first OPA peripheral register block.
 * @param[in] second
 *   Pointer to the second OPA peripheral register block. Must be different from @p first.
 * @param[in] init_first  Initialization settings for @p first.
 * @param[in] init_second Initialization settings for @p second.
 *                        Both initialization structures must select the same
 *                        dual-instance mode:
 *                        - @c SL_HAL_OPA_MODE_CASCADED_PGA_NON_INVERTING
 *                        - @c SL_HAL_OPA_MODE_CASCADED_PGA_INVERTING
 *                        - @c SL_HAL_OPA_MODE_DIFF_AMP
 *
 * @note The HAL configures the required inter-OPA connection and mode-derived
 *       settings, including constant-gm where required by the selected mode.
 * @note In differential mode, the same gain must be configured on both
 *       instances.
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_NULL_POINTER if either initialization pointer is NULL.
 *         SL_STATUS_INVALID_PARAMETER if the modes do not match, the same OPA
 *         instance is passed twice, the selected mode is not a dual-instance
 *         mode, or a route is invalid.
 *         SL_STATUS_NOT_SUPPORTED if the device does not provide two OPA
 *         instances.
 *         SL_STATUS_INVALID_STATE if either instance is locked.
 ******************************************************************************/
sl_status_t sl_hal_opa_init_dual(OPA_TypeDef *first,
                                 OPA_TypeDef *second,
                                 const sl_hal_opa_init_t *init_first,
                                 const sl_hal_opa_init_t *init_second);

/***************************************************************************//**
 * @brief Deinitialize a single OPA instance.
 *
 * @details
 * Returns the OPA instance to its hardware reset state and clears the routing
 * configured during initialization.
 *
 * Use this API before changing the operating mode or routing of a
 * single-instance topology.
 *
 * @note For a topology initialized using @ref sl_hal_opa_init_dual, use
 *       @ref sl_hal_opa_deinit_dual.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 ******************************************************************************/
void sl_hal_opa_deinit(OPA_TypeDef *opa);

/***************************************************************************//**
 * @brief Deinitialize two OPA instances configured as a dual topology.
 *
 * @details
 * Returns both OPA instances to their hardware reset state and removes the
 * inter-instance configuration established by @ref sl_hal_opa_init_dual.
 *
 * @param[in] first
 *   Pointer to the first OPA peripheral register block.
 * @param[in] second
 *   Pointer to the second OPA peripheral register block.
 ******************************************************************************/
void sl_hal_opa_deinit_dual(OPA_TypeDef *first,
                            OPA_TypeDef *second);

/***************************************************************************//**
 * @brief Reset an OPA instance to its hardware default state.
 *
 * @details
 * Restores the OPA configuration registers to their reset values and clears
 * pending interrupt flags.
 *
 * @note If the protected registers are locked, unlock them first using
 *       @ref sl_hal_opa_unlock.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 ******************************************************************************/
void sl_hal_opa_reset(OPA_TypeDef *opa);

/***************************************************************************//**
 * @brief Enable the OPA.
 *
 * @details
 * Starts OPA operation using the currently programmed configuration.
 *
 * Call this API after initialization and all required configurable settings
 * have been applied.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 ******************************************************************************/
__INLINE void sl_hal_opa_enable(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  opa->EN_SET = OPA_EN_EN;
}

/***************************************************************************//**
 * @brief Disable the OPA.
 *
 * @details
 * Stops OPA operation while preserving the programmed configuration.
 *
 * Disable the OPA before changing configurable operating settings.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 ******************************************************************************/
__INLINE void sl_hal_opa_disable(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  opa->EN_CLR = OPA_EN_EN;
}

/***************************************************************************//**
 * @brief Check whether the OPA is enabled.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return @c true if the OPA is enabled.
 * @return @c false if the OPA is disabled.
 ******************************************************************************/
__INLINE bool sl_hal_opa_is_enabled(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  return ((opa->EN & (uint32_t)_OPA_EN_EN_MASK) != 0U);
}

/***************************************************************************//**
 * @brief Set the OPA programmable gain.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 * @param[in] gain @ref sl_hal_opa_gain_t:
 *                 - @c SL_HAL_OPA_GAIN_X1P33
 *                 - @c SL_HAL_OPA_GAIN_X2
 *                 - @c SL_HAL_OPA_GAIN_X2P66
 *                 - @c SL_HAL_OPA_GAIN_X3P2
 *                 - @c SL_HAL_OPA_GAIN_X4
 *                 - @c SL_HAL_OPA_GAIN_X5P33
 *                 - @c SL_HAL_OPA_GAIN_X8
 *                 - @c SL_HAL_OPA_GAIN_X16
 *                 - @c SL_HAL_OPA_GAIN_X3
 *
 * @note The gain setting has no effect in unity-gain mode.
 * @note Differential mode requires the same gain on both OPA instances.
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_INVALID_PARAMETER if @p gain is invalid.
 *         SL_STATUS_INVALID_STATE if the OPA is locked.
 ******************************************************************************/
sl_status_t sl_hal_opa_set_gain(OPA_TypeDef *opa, sl_hal_opa_gain_t gain);

/***************************************************************************//**
 * @brief Get the currently programmed OPA gain.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return Current @ref sl_hal_opa_gain_t setting.
 ******************************************************************************/
sl_hal_opa_gain_t sl_hal_opa_get_gain(OPA_TypeDef *opa);

/***************************************************************************//**
 * @brief Select the OPA input differential pair(s).
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 * @param[in] stage Input-stage selection:
 *                  - @c SL_HAL_OPA_INPUT_STAGE_NONE
 *                  - @c SL_HAL_OPA_INPUT_STAGE_NMOS
 *                  - @c SL_HAL_OPA_INPUT_STAGE_PMOS
 *                  - @c SL_HAL_OPA_INPUT_STAGE_RAIL_TO_RAIL
 *
 * @note @c SL_HAL_OPA_INPUT_STAGE_NONE disables both input differential pairs
 *       and leaves the amplifier non-functional.
 * @note The HAL reapplies factory trim because the selected input stage changes
 *       which input differential pair calibration is required.
 * @note Constant-gm is for @c SL_HAL_OPA_INPUT_STAGE_RAIL_TO_RAIL; disable it via
 *       @ref sl_hal_opa_set_cgm() when using NMOS-only or PMOS-only stages.
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_INVALID_PARAMETER if @p stage is invalid.
 *         SL_STATUS_INVALID_STATE if the OPA is locked.
 ******************************************************************************/
sl_status_t sl_hal_opa_set_input_stage(OPA_TypeDef *opa,
                                       sl_hal_opa_input_stage_t stage);

/***************************************************************************//**
 * @brief Get the active OPA input differential pair(s).
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return Current @ref sl_hal_opa_input_stage_t setting.
 ******************************************************************************/
sl_hal_opa_input_stage_t sl_hal_opa_get_input_stage(OPA_TypeDef *opa);

/***************************************************************************//**
 * @brief Enable or disable constant-gm biasing.
 *
 * @details
 * Constant-gm operation requires the OPA analog supply to be at least 1.9 V.
 * Use @ref sl_hal_opa_get_cgm_status to determine whether constant-gm biasing
 * is currently active.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 * @param[in] enable @c true to enable constant-gm biasing,
 *                   @c false to disable it.
 *
 * @note The OPA analog supply is managed outside the OPA HAL.
 * @note Dual-instance modes enable constant-gm during @ref sl_hal_opa_init_dual.
 *       Do not disable constant-gm afterward while that topology remains in use.
 * @note Enable constant-gm only with @c SL_HAL_OPA_INPUT_STAGE_RAIL_TO_RAIL;
 *       disable it for NMOS-only or PMOS-only input stages.
 * @note The HAL reapplies factory trim because constant-gm changes the
 *       calibration selection.
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_INVALID_STATE if the OPA is locked.
 ******************************************************************************/
sl_status_t sl_hal_opa_set_cgm(OPA_TypeDef *opa,
                               bool enable);

/***************************************************************************//**
 * @brief Check whether constant-gm biasing is enabled.
 *
 * @details
 * Returns the configured state of the constant-gm enable control.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return @c true if constant-gm biasing is enabled;
 *         @c false otherwise.
 ******************************************************************************/
__INLINE bool sl_hal_opa_is_cgm_enabled(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  return ((opa->CTRL1 & (uint32_t)_OPA_CTRL1_ENCGM_MASK) != 0U);
}

/***************************************************************************//**
 * @brief Enable or disable the dedicated main OPA output.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 * @param[in] enable @c true to enable the dedicated @c OPAn_OUT output,
 *                   @c false to disable it.
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_INVALID_STATE if the OPA is locked.
 ******************************************************************************/
sl_status_t sl_hal_opa_set_main_output(OPA_TypeDef *opa,
                                       bool enable);

/***************************************************************************//**
 * @brief Check whether the dedicated main OPA output is enabled.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return @c true if the dedicated main output is enabled.
 * @return @c false otherwise.
 ******************************************************************************/
__INLINE bool sl_hal_opa_is_main_output_enabled(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  return ((opa->CTRL1 & (uint32_t)_OPA_CTRL1_ENVOMAIN_MASK) != 0U);
}

/***************************************************************************//**
 * @brief Enable or disable additional OPA output drive.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 * @param[in] enable @c true to enable additional output drive,
 *                   @c false to use the normal drive setting.
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_INVALID_STATE if the OPA is locked.
 ******************************************************************************/
sl_status_t sl_hal_opa_set_high_drive(OPA_TypeDef *opa,
                                      bool enable);

/***************************************************************************//**
 * @brief Check whether additional output drive is enabled.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return @c true if additional output drive is enabled.
 * @return @c false otherwise.
 ******************************************************************************/
__INLINE bool sl_hal_opa_is_high_drive_enabled(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  return ((opa->CTRL2 & (uint32_t)_OPA_CTRL2_ENHDRV_MASK) != 0U);
}

/***************************************************************************//**
 * @brief Configure the OPA output-load compensation.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 * @param[in] cap @ref sl_hal_opa_comp_cap_t:
 *                - @c SL_HAL_OPA_COMP_5PF
 *                - @c SL_HAL_OPA_COMP_5PF_EXTRA
 *                - @c SL_HAL_OPA_COMP_50PF (reset default)
 *                - @c SL_HAL_OPA_COMP_50PF_EXTRA
 *
 * @return SL_STATUS_OK on success.
 *         SL_STATUS_INVALID_PARAMETER if @p cap is invalid.
 *         SL_STATUS_INVALID_STATE if the OPA is locked.
 ******************************************************************************/
sl_status_t sl_hal_opa_set_comp_cap(OPA_TypeDef *opa,
                                    sl_hal_opa_comp_cap_t cap);

/***************************************************************************//**
 * @brief Get the currently programmed output-load compensation.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return Current @ref sl_hal_opa_comp_cap_t setting.
 ******************************************************************************/
__INLINE sl_hal_opa_comp_cap_t sl_hal_opa_get_comp_cap(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  return (sl_hal_opa_comp_cap_t)((opa->CTRL2 & (uint32_t)_OPA_CTRL2_SELCL_MASK)
                                 >> (uint32_t)_OPA_CTRL2_SELCL_SHIFT);
}

/***************************************************************************//**
 * @brief Check whether constant-gm biasing is active.
 *
 * @details
 * Returns the current constant-gm hardware status. Constant-gm operation
 * requires the OPA analog supply to be at least 1.9 V.
 *
 * Enabling constant-gm does not by itself indicate that constant-gm
 * biasing is active. Use this function to verify the current hardware state.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return @c true if constant-gm biasing is active;
 *         @c false otherwise.
 ******************************************************************************/
__INLINE bool sl_hal_opa_get_cgm_status(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  return ((opa->STATUS & (uint32_t)_OPA_STATUS_STATUSCGM_MASK) != 0U);
}

/***************************************************************************//**
 * @brief Enable selected OPA interrupt sources.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 * @param[in] flags Interrupt sources to enable (@c SL_HAL_OPA_IF_* from
 *                  @ref sl_hal_opa_interrupt_flag_t).
 ******************************************************************************/
__INLINE void sl_hal_opa_enable_interrupts(OPA_TypeDef *opa,
                                           sl_hal_opa_interrupt_flag_t flags)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  opa->IEN_SET = flags;
}

/***************************************************************************//**
 * @brief Disable selected OPA interrupt sources.
 *
 * @details
 * Disabling a source does not clear pending interrupt flags.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 * @param[in] flags Interrupt sources to disable (@c SL_HAL_OPA_IF_* from
 *                  @ref sl_hal_opa_interrupt_flag_t).
 ******************************************************************************/
__INLINE void sl_hal_opa_disable_interrupts(OPA_TypeDef *opa,
                                            sl_hal_opa_interrupt_flag_t flags)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  opa->IEN_CLR = flags;
}

/***************************************************************************//**
 * @brief Get the enabled OPA interrupt sources.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return Bit mask of the currently enabled @c SL_HAL_OPA_IF_* interrupt sources.
 ******************************************************************************/
__INLINE sl_hal_opa_interrupt_flag_t sl_hal_opa_get_enabled_interrupts(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  return (sl_hal_opa_interrupt_flag_t)opa->IEN;
}

/***************************************************************************//**
 * @brief Get the pending OPA interrupt flags.
 *
 * @details
 * Returns latched interrupt events. Flags remain set until cleared with
 * @ref sl_hal_opa_clear_interrupts. Reading the flags does not clear them.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return Bit mask of pending @c SL_HAL_OPA_IF_* interrupt flags.
 ******************************************************************************/
__INLINE sl_hal_opa_interrupt_flag_t sl_hal_opa_get_pending_interrupts(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  return (sl_hal_opa_interrupt_flag_t)opa->IF;
}

/***************************************************************************//**
 * @brief Get the enabled pending OPA interrupt flags.
 *
 * @details
 * Returns interrupt flags that are both pending and currently enabled.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return Bit mask of enabled pending @c SL_HAL_OPA_IF_* interrupt flags.
 ******************************************************************************/
__INLINE sl_hal_opa_interrupt_flag_t sl_hal_opa_get_enabled_pending_interrupts(OPA_TypeDef *opa)
{
  sl_hal_opa_interrupt_flag_t ien;

  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  // Read IEN into a temporary to define the order of the volatile accesses.
  ien = (sl_hal_opa_interrupt_flag_t)opa->IEN;

  return (sl_hal_opa_interrupt_flag_t)(opa->IF & ien);
}

/***************************************************************************//**
 * @brief Clear selected OPA interrupt flags.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 * @param[in] flags Interrupt flags to clear (@c SL_HAL_OPA_IF_* from
 *                  @ref sl_hal_opa_interrupt_flag_t).
 ******************************************************************************/
__INLINE void sl_hal_opa_clear_interrupts(OPA_TypeDef *opa,
                                          sl_hal_opa_interrupt_flag_t flags)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  opa->IF_CLR = flags;
}

/***************************************************************************//**
 * @brief Lock the protected OPA registers.
 *
 * @details
 * Prevents modification of protected OPA configuration registers until
 * @ref sl_hal_opa_unlock is called.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 ******************************************************************************/
__INLINE void sl_hal_opa_lock(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  opa->LOCK = ~(uint32_t)_OPA_LOCK_LOCKKEY_UNLOCK & (uint32_t)_OPA_LOCK_LOCKKEY_MASK;
}

/***************************************************************************//**
 * @brief Unlock the protected OPA registers.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 ******************************************************************************/
__INLINE void sl_hal_opa_unlock(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  opa->LOCK = OPA_LOCK_LOCKKEY_UNLOCK;
}

/***************************************************************************//**
 * @brief Check whether the protected OPA registers are locked.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return @c true if the protected registers are locked.
 * @return @c false otherwise.
 ******************************************************************************/
__INLINE bool sl_hal_opa_is_locked(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  return ((opa->STATUS & (uint32_t)_OPA_STATUS_LOCK_MASK) != 0U);
}

/*******************************************************************************
 ************************   INTERNAL APIs   ************************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief Get live OPA status flags.
 *
 * @details
 * Returns the current hardware condition. Unlike latched interrupt flags,
 * status flags clear automatically when the underlying condition goes away.
 *
 * @param[in] opa
 *   Pointer to the OPA peripheral register block.
 *
 * @return Bit mask of live @c SL_HAL_OPA_STATUS_* flags from
 *         @ref sl_hal_opa_status_flag_t.
 ******************************************************************************/
__INLINE sl_hal_opa_status_flag_t sli_hal_opa_get_status(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  return (sl_hal_opa_status_flag_t)opa->STATUS;
}

/** @} (end addtogroup opa) */

#ifdef __cplusplus
}
#endif

#endif /* defined(OPA_PRESENT) */
#endif /* SL_HAL_OPA_H */
