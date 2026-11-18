/***************************************************************************//**
 * @file
 * @brief Operational Amplifier (OPA) HAL implementation
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

#include "sl_hal_opa.h"
#include "sl_hal_system_generic.h"

#if defined(OPA_PRESENT)

/***************************************************************************//**
 * @addtogroup opa OPA - Operational Amplifier
 * @brief Operational Amplifier peripheral API
 * @{
 ******************************************************************************/

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

/// Largest GPIO pin number encodable in the ABUS routing registers.
#define OPA_ROUTE_PIN_MAX           ((uint32_t)_OPA_ABUSCTRLPOSIN_PINSEL_MASK \
                                     >> (uint32_t)_OPA_ABUSCTRLPOSIN_PINSEL_SHIFT)

/// Convert an OPA HAL port define (@ref SL_HAL_OPA_PORT_A .. @ref SL_HAL_OPA_PORT_D)
/// to the contiguous ABUS port-select encoding offset from port A.
#define OPA_ROUTE_PORT_TO_PORTSEL(port) \
        ((uint32_t)(port) - (uint32_t)SL_HAL_OPA_PORT_A)

/// Control fields programmed only by @ref sl_hal_opa_init() / @ref sl_hal_opa_init_dual().
#define OPA_CTRL1_INIT_MASK         ((uint32_t)(_OPA_CTRL1_ENMUXGAIN_MASK   \
                                                | _OPA_CTRL1_ENMUXN_MASK   \
                                                | _OPA_CTRL1_ENMUXP_MASK   \
                                                | _OPA_CTRL1_ENMUXRES_MASK \
                                                | _OPA_CTRL1_SWP_MASK      \
                                                | _OPA_CTRL1_SELRES_MASK))

/*******************************************************************************
 *****************************   LOCAL TYPES   *********************************
 ******************************************************************************/

/// Mode-derived register settings applied by @ref sl_hal_opa_init and
/// @ref sl_hal_opa_init_dual (see @c opa_mode_recipes[]).
typedef struct {
  bool enable_gain_mux;         ///< Enable gain resistor mux.
  bool enable_res_mux;          ///< Enable R1 resistor mux.
  bool force_vfb;               ///< Enable virtual feedback (TIA).
  bool require_cgm;             ///< Enable constant-gm for this mode.
  bool hysteresis_swap;         ///< Hysteresis comparator swap rules apply.
  bool dual;                    ///< Requires @ref sl_hal_opa_init_dual.
  bool hal_negin;               ///< Enforce mode-specific negative feedback route.
  uint32_t negin_portsel;       ///< Required NEGIN source encoding when hal_negin is set.
} opa_mode_recipe_t;

/// Role of an instance inside a dual-OPA topology.
SL_ENUM(opa_instance_role_t) {
  OPA_ROLE_SINGLE = 0,   ///< Single-instance topology.
  OPA_ROLE_FIRST,        ///< First instance of a dual-instance topology.
  OPA_ROLE_SECOND        ///< Second instance of a dual-instance topology.
};

/// Encoded ABUS routing register values from an initialization structure.
typedef struct {
  uint32_t posin;   ///< Positive-input routing register image.
  uint32_t negin;   ///< Negative-input routing register image.
  uint32_t out;     ///< Alternate-output routing register image.
} opa_route_regs_t;

/*******************************************************************************
 *****************************   LOCAL DATA   **********************************
 ******************************************************************************/

/// Mode-derived settings for every supported OPA operating mode.
static const opa_mode_recipe_t opa_mode_recipes[SL_HAL_OPA_MODE_MAX] = {
  [SL_HAL_OPA_MODE_UNITY_GAIN] = {
    .enable_gain_mux = false,
    .enable_res_mux = false,
    .force_vfb = false,
    .require_cgm = false,
    .hysteresis_swap = false,
    .dual = false,
    .hal_negin = true,
    .negin_portsel = _OPA_ABUSCTRLNEGIN_PORTSEL_FB_X1,
  },
  [SL_HAL_OPA_MODE_TIA] = {
    .enable_gain_mux = true,
    .enable_res_mux = false,
    .force_vfb = true,
    .require_cgm = false,
    .hysteresis_swap = false,
    .dual = false,
    .hal_negin = false,
    .negin_portsel = 0U,
  },
  [SL_HAL_OPA_MODE_PGA_NON_INVERTING] = {
    .enable_gain_mux = true,
    .enable_res_mux = true,
    .force_vfb = false,
    .require_cgm = false,
    .hysteresis_swap = false,
    .dual = false,
    .hal_negin = true,
    .negin_portsel = _OPA_ABUSCTRLNEGIN_PORTSEL_FB_XN,
  },
  [SL_HAL_OPA_MODE_PGA_INVERTING] = {
    .enable_gain_mux = true,
    .enable_res_mux = true,
    .force_vfb = false,
    .require_cgm = false,
    .hysteresis_swap = false,
    .dual = false,
    .hal_negin = true,
    .negin_portsel = _OPA_ABUSCTRLNEGIN_PORTSEL_FB_XN,
  },
  [SL_HAL_OPA_MODE_HYST_COMP_NON_INVERTING] = {
    .enable_gain_mux = true,
    .enable_res_mux = true,
    .force_vfb = false,
    .require_cgm = false,
    .hysteresis_swap = true,
    .dual = false,
    .hal_negin = true,
    .negin_portsel = _OPA_ABUSCTRLNEGIN_PORTSEL_FB_XN,
  },
  [SL_HAL_OPA_MODE_HYST_COMP_INVERTING] = {
    .enable_gain_mux = true,
    .enable_res_mux = true,
    .force_vfb = false,
    .require_cgm = false,
    .hysteresis_swap = true,
    .dual = false,
    .hal_negin = true,
    .negin_portsel = _OPA_ABUSCTRLNEGIN_PORTSEL_FB_XN,
  },
  [SL_HAL_OPA_MODE_CASCADED_PGA_NON_INVERTING] = {
    .enable_gain_mux = true,
    .enable_res_mux = true,
    .force_vfb = false,
    .require_cgm = true,
    .hysteresis_swap = false,
    .dual = true,
    .hal_negin = true,
    .negin_portsel = _OPA_ABUSCTRLNEGIN_PORTSEL_FB_XN,
  },
  [SL_HAL_OPA_MODE_CASCADED_PGA_INVERTING] = {
    .enable_gain_mux = true,
    .enable_res_mux = true,
    .force_vfb = false,
    .require_cgm = true,
    .hysteresis_swap = false,
    .dual = true,
    .hal_negin = true,
    .negin_portsel = _OPA_ABUSCTRLNEGIN_PORTSEL_FB_XN,
  },
  [SL_HAL_OPA_MODE_DIFF_AMP] = {
    .enable_gain_mux = true,
    .enable_res_mux = true,
    .force_vfb = false,
    .require_cgm = true,
    .hysteresis_swap = false,
    .dual = true,
    .hal_negin = true,
    .negin_portsel = _OPA_ABUSCTRLNEGIN_PORTSEL_FB_XN,
  },
};

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

static bool opa_port_is_valid(sl_hal_opa_port_t port);
static bool opa_bus_is_single_lane(sl_hal_opa_bus_t bus,
                                   sl_hal_opa_bus_t first,
                                   sl_hal_opa_bus_t last);
static sl_status_t opa_encode_posin_route(const sl_hal_opa_posin_route_t *route, uint32_t *reg);
static sl_status_t opa_encode_negin_route(const sl_hal_opa_negin_route_t *route, uint32_t *reg);
static sl_status_t opa_encode_out_route(const sl_hal_opa_out_route_t *route, uint32_t *reg);
static sl_status_t opa_validate_init(const sl_hal_opa_init_t *init,
                                     opa_route_regs_t *routes);
static void opa_apply_init(OPA_TypeDef *opa,
                           const sl_hal_opa_init_t *init,
                           const opa_route_regs_t *routes,
                           opa_instance_role_t role);
#if defined(_OPA_TRIM_TRIMOFSPDIF_MASK) && defined(DEVINFO_GP_OPA0TRIM0_OFFSET)
static void sli_hal_opa_refresh_factory_trim(OPA_TypeDef *opa);
#endif

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

extern __INLINE uint32_t sl_hal_opa_get_version(OPA_TypeDef *opa);
extern __INLINE void sl_hal_opa_enable(OPA_TypeDef *opa);
extern __INLINE void sl_hal_opa_disable(OPA_TypeDef *opa);
extern __INLINE bool sl_hal_opa_is_enabled(OPA_TypeDef *opa);
extern __INLINE bool sl_hal_opa_is_cgm_enabled(OPA_TypeDef *opa);
extern __INLINE bool sl_hal_opa_is_main_output_enabled(OPA_TypeDef *opa);
extern __INLINE bool sl_hal_opa_is_high_drive_enabled(OPA_TypeDef *opa);
extern __INLINE sl_hal_opa_comp_cap_t sl_hal_opa_get_comp_cap(OPA_TypeDef *opa);
extern __INLINE bool sl_hal_opa_get_cgm_status(OPA_TypeDef *opa);
extern __INLINE void sl_hal_opa_enable_interrupts(OPA_TypeDef *opa,
                                                  sl_hal_opa_interrupt_flag_t flags);
extern __INLINE void sl_hal_opa_disable_interrupts(OPA_TypeDef *opa,
                                                   sl_hal_opa_interrupt_flag_t flags);
extern __INLINE sl_hal_opa_interrupt_flag_t sl_hal_opa_get_enabled_interrupts(OPA_TypeDef *opa);
extern __INLINE sl_hal_opa_interrupt_flag_t sl_hal_opa_get_pending_interrupts(OPA_TypeDef *opa);
extern __INLINE sl_hal_opa_interrupt_flag_t sl_hal_opa_get_enabled_pending_interrupts(OPA_TypeDef *opa);
extern __INLINE void sl_hal_opa_clear_interrupts(OPA_TypeDef *opa,
                                                 sl_hal_opa_interrupt_flag_t flags);
extern __INLINE void sl_hal_opa_lock(OPA_TypeDef *opa);
extern __INLINE void sl_hal_opa_unlock(OPA_TypeDef *opa);
extern __INLINE bool sl_hal_opa_is_locked(OPA_TypeDef *opa);
extern __INLINE sl_hal_opa_status_flag_t sli_hal_opa_get_status(OPA_TypeDef *opa);

/***************************************************************************//**
 * Configure a single OPA instance for the selected single-instance mode:
 * internal muxes, feedback path, mode-derived control fields, and ABUS routing.
 * The instance is left disabled.
 ******************************************************************************/
sl_status_t sl_hal_opa_init(OPA_TypeDef *opa, const sl_hal_opa_init_t *init)
{
  opa_route_regs_t routes;
  sl_status_t status;

  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  if (init == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  if (init->mode >= SL_HAL_OPA_MODE_MAX) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (opa_mode_recipes[init->mode].dual) {
    return SL_STATUS_NOT_SUPPORTED;
  }

  // Check fields and encode ABUS routing before any register write.
  status = opa_validate_init(init, &routes);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (sl_hal_opa_is_locked(opa)) {
    return SL_STATUS_INVALID_STATE;
  }

  // Disable before programming EN-protected configuration.
  if (sl_hal_opa_is_enabled(opa)) {
    sl_hal_opa_disable(opa);
  }

  // Leave EN cleared; caller enables when the signal path is ready.
  opa_apply_init(opa, init, &routes, OPA_ROLE_SINGLE);

#if defined(_OPA_TRIM_TRIMOFSPDIF_MASK) && defined(DEVINFO_GP_OPA0TRIM0_OFFSET)
  sli_hal_opa_refresh_factory_trim(opa);
#endif

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Configure two OPA instances for a cascaded or differential topology,
 * including the inter-instance connection. Both instances are left disabled.
 ******************************************************************************/
sl_status_t sl_hal_opa_init_dual(OPA_TypeDef *first,
                                 OPA_TypeDef *second,
                                 const sl_hal_opa_init_t *init_first,
                                 const sl_hal_opa_init_t *init_second)
{
#if (OPA_COUNT < 2)
  // The device does not provide a second OPA instance.
  (void)first;
  (void)second;

  if ((init_first == NULL) || (init_second == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  return SL_STATUS_NOT_SUPPORTED;
#else
  opa_route_regs_t routes_first;
  opa_route_regs_t routes_second;
  sl_status_t status;

  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(first));
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(second));

  if ((init_first == NULL) || (init_second == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  if (first == second) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Same dual mode on both instances (cascaded or differential only).
  if ((init_first->mode != init_second->mode)
      || (init_first->mode >= SL_HAL_OPA_MODE_MAX)
      || !opa_mode_recipes[init_first->mode].dual) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = opa_validate_init(init_first, &routes_first);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = opa_validate_init(init_second, &routes_second);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (sl_hal_opa_is_locked(first) || sl_hal_opa_is_locked(second)) {
    return SL_STATUS_INVALID_STATE;
  }

  // Disable before programming EN-protected configuration.
  if (sl_hal_opa_is_enabled(first)) {
    sl_hal_opa_disable(first);
  }
  if (sl_hal_opa_is_enabled(second)) {
    sl_hal_opa_disable(second);
  }

  // Role selects inter-instance ABUS links inside opa_apply_init().
  opa_apply_init(first, init_first, &routes_first, OPA_ROLE_FIRST);
  opa_apply_init(second, init_second, &routes_second, OPA_ROLE_SECOND);

#if defined(_OPA_TRIM_TRIMOFSPDIF_MASK) && defined(DEVINFO_GP_OPA0TRIM0_OFFSET)
  sli_hal_opa_refresh_factory_trim(first);
  sli_hal_opa_refresh_factory_trim(second);
#endif

  return SL_STATUS_OK;
#endif
}

/***************************************************************************//**
 * Return a single OPA instance to its hardware reset state, including the
 * ABUS routing configured during initialization.
 ******************************************************************************/
void sl_hal_opa_deinit(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  sl_hal_opa_reset(opa);
}

/***************************************************************************//**
 * Return both instances of a dual-OPA topology to their hardware reset state,
 * removing the inter-instance configuration.
 ******************************************************************************/
void sl_hal_opa_deinit_dual(OPA_TypeDef *first,
                            OPA_TypeDef *second)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(first));
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(second));

  sl_hal_opa_reset(first);
  sl_hal_opa_reset(second);
}

/***************************************************************************//**
 * Restore the OPA configuration registers to their reset values and clear
 * pending interrupt flags. Register lock state is not modified; unlock the
 * instance first if the protected registers are locked.
 ******************************************************************************/
void sl_hal_opa_reset(OPA_TypeDef *opa)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  // Stop the amplifier before restoring the configuration registers.
  opa->EN = _OPA_EN_RESETVALUE;

  opa->IEN = _OPA_IEN_RESETVALUE;
  opa->IF_CLR = _OPA_IF_MASK;

  opa->CTRL1 = _OPA_CTRL1_RESETVALUE;
  opa->CTRL2 = _OPA_CTRL2_RESETVALUE;
  opa->TRIM = _OPA_TRIM_RESETVALUE;

  opa->ABUSCTRLPOSIN = _OPA_ABUSCTRLPOSIN_RESETVALUE;
  opa->ABUSCTRLNEGIN = _OPA_ABUSCTRLNEGIN_RESETVALUE;
  opa->ABUSCTRLOUT = _OPA_ABUSCTRLOUT_RESETVALUE;
}

/***************************************************************************//**
 * Program the internal gain resistor selection (programmable table or x3 path).
 ******************************************************************************/
sl_status_t sl_hal_opa_set_gain(OPA_TypeDef *opa, sl_hal_opa_gain_t gain)
{
  uint32_t ctrl1;

  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  if (gain >= SL_HAL_OPA_GAIN_MAX) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (sl_hal_opa_is_locked(opa)) {
    return SL_STATUS_INVALID_STATE;
  }

  ctrl1 = opa->CTRL1 & ~((uint32_t)_OPA_CTRL1_SELGAIN_MASK
                         | (uint32_t)_OPA_CTRL1_SELGAIN3X_MASK);

  if (gain == SL_HAL_OPA_GAIN_X3) {
    // Dedicated x3 path; keep the programmable gain field cleared.
    ctrl1 |= OPA_CTRL1_SELGAIN3X;
  } else {
    ctrl1 |= ((uint32_t)gain << (uint32_t)_OPA_CTRL1_SELGAIN_SHIFT)
             & (uint32_t)_OPA_CTRL1_SELGAIN_MASK;
  }

  opa->CTRL1 = ctrl1;

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Read the programmed gain, including the dedicated x3 path.
 ******************************************************************************/
sl_hal_opa_gain_t sl_hal_opa_get_gain(OPA_TypeDef *opa)
{
  uint32_t ctrl1;

  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  ctrl1 = opa->CTRL1;

  if ((ctrl1 & (uint32_t)_OPA_CTRL1_SELGAIN3X_MASK) != 0U) {
    return SL_HAL_OPA_GAIN_X3;
  }

  return (sl_hal_opa_gain_t)((ctrl1 & (uint32_t)_OPA_CTRL1_SELGAIN_MASK)
                             >> (uint32_t)_OPA_CTRL1_SELGAIN_SHIFT);
}

/***************************************************************************//**
 * Program the active NMOS/PMOS input differential pair selection.
 ******************************************************************************/
sl_status_t sl_hal_opa_set_input_stage(OPA_TypeDef *opa,
                                       sl_hal_opa_input_stage_t stage)
{
  uint32_t ctrl1;

  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  if (stage >= SL_HAL_OPA_INPUT_STAGE_MAX) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (sl_hal_opa_is_locked(opa)) {
    return SL_STATUS_INVALID_STATE;
  }

  ctrl1 = opa->CTRL1 & ~((uint32_t)_OPA_CTRL1_ENNDIF_MASK
                         | (uint32_t)_OPA_CTRL1_ENPDIF_MASK);

  // sl_hal_opa_input_stage_t: bit 0 enables NMOS pair, bit 1 enables PMOS pair.
  ctrl1 |= (((uint32_t)stage & 0x1UL) << (uint32_t)_OPA_CTRL1_ENNDIF_SHIFT)
           | ((((uint32_t)stage >> 1U) & 0x1UL) << (uint32_t)_OPA_CTRL1_ENPDIF_SHIFT);

  opa->CTRL1 = ctrl1;

#if defined(_OPA_TRIM_TRIMOFSPDIF_MASK) && defined(DEVINFO_GP_OPA0TRIM0_OFFSET)
  sli_hal_opa_refresh_factory_trim(opa);
#endif

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Map ENNDIF/ENPDIF to sl_hal_opa_input_stage_t.
 ******************************************************************************/
sl_hal_opa_input_stage_t sl_hal_opa_get_input_stage(OPA_TypeDef *opa)
{
  uint32_t ctrl1;
  uint32_t stage;

  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  ctrl1 = opa->CTRL1;

  // Map hardware NMOS/PMOS pair enables to sl_hal_opa_input_stage_t.
  stage = ((ctrl1 & (uint32_t)_OPA_CTRL1_ENNDIF_MASK) >> (uint32_t)_OPA_CTRL1_ENNDIF_SHIFT)
          | (((ctrl1 & (uint32_t)_OPA_CTRL1_ENPDIF_MASK) >> (uint32_t)_OPA_CTRL1_ENPDIF_SHIFT) << 1U);

  return (sl_hal_opa_input_stage_t)stage;
}

/***************************************************************************//**
 * Enable or disable constant-gm biasing. The configured enable is separate from
 * the live constant-gm status reported by @ref sl_hal_opa_get_cgm_status().
 ******************************************************************************/
sl_status_t sl_hal_opa_set_cgm(OPA_TypeDef *opa,
                               bool enable)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  if (sl_hal_opa_is_locked(opa)) {
    return SL_STATUS_INVALID_STATE;
  }

  if (enable) {
    opa->CTRL1_SET = OPA_CTRL1_ENCGM;
  } else {
    opa->CTRL1_CLR = OPA_CTRL1_ENCGM;
  }

#if defined(_OPA_TRIM_TRIMOFSPDIF_MASK) && defined(DEVINFO_GP_OPA0TRIM0_OFFSET)
  sli_hal_opa_refresh_factory_trim(opa);
#endif

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable or disable the dedicated main OPA output pin.
 ******************************************************************************/
sl_status_t sl_hal_opa_set_main_output(OPA_TypeDef *opa,
                                       bool enable)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  if (sl_hal_opa_is_locked(opa)) {
    return SL_STATUS_INVALID_STATE;
  }

  if (enable) {
    opa->CTRL1_SET = OPA_CTRL1_ENVOMAIN;
  } else {
    opa->CTRL1_CLR = OPA_CTRL1_ENVOMAIN;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Enable or disable additional output drive strength.
 ******************************************************************************/
sl_status_t sl_hal_opa_set_high_drive(OPA_TypeDef *opa,
                                      bool enable)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  if (sl_hal_opa_is_locked(opa)) {
    return SL_STATUS_INVALID_STATE;
  }

  if (enable) {
    opa->CTRL2_SET = OPA_CTRL2_ENHDRV;
  } else {
    opa->CTRL2_CLR = OPA_CTRL2_ENHDRV;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Program the output-load compensation selection.
 ******************************************************************************/
sl_status_t sl_hal_opa_set_comp_cap(OPA_TypeDef *opa,
                                    sl_hal_opa_comp_cap_t cap)
{
  SL_LOG_DEBUG_ASSERT(SL_HAL_OPA_REF_VALID(opa));

  if (cap >= SL_HAL_OPA_COMP_MAX) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (sl_hal_opa_is_locked(opa)) {
    return SL_STATUS_INVALID_STATE;
  }

  opa->CTRL2 = (opa->CTRL2 & ~(uint32_t)_OPA_CTRL2_SELCL_MASK)
               | (((uint32_t)cap << (uint32_t)_OPA_CTRL2_SELCL_SHIFT)
                  & (uint32_t)_OPA_CTRL2_SELCL_MASK);

  return SL_STATUS_OK;
}

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

/***************************************************************************//**
 * Return true when @p port is one of SL_HAL_OPA_PORT_A .. SL_HAL_OPA_PORT_D.
 ******************************************************************************/
static bool opa_port_is_valid(sl_hal_opa_port_t port)
{
  return ((port >= SL_HAL_OPA_PORT_A) && (port <= SL_HAL_OPA_PORT_D));
}

/***************************************************************************//**
 * Return true when @p bus is in the inclusive range [@p first, @p last].
 ******************************************************************************/
static bool opa_bus_is_single_lane(sl_hal_opa_bus_t bus,
                                   sl_hal_opa_bus_t first,
                                   sl_hal_opa_bus_t last)
{
  return ((bus >= first) && (bus <= last));
}

/***************************************************************************//**
 * Encode the positive-input route into the hardware routing register value.
 *
 * GPIO routes use an even ABUS lane only (@ref SL_HAL_OPA_BUS_EVEN0 or
 * @ref SL_HAL_OPA_BUS_EVEN1).
 ******************************************************************************/
static sl_status_t opa_encode_posin_route(const sl_hal_opa_posin_route_t *route, uint32_t *reg)
{
  uint32_t portsel;
  uint32_t lane;

  if (route->source >= SL_HAL_OPA_POSIN_MAX) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  switch (route->source) {
    case SL_HAL_OPA_POSIN_NONE:
      *reg = OPA_ABUSCTRLPOSIN_PORTSEL_NONE;
      return SL_STATUS_OK;

    case SL_HAL_OPA_POSIN_OPA_NEXT:
      *reg = OPA_ABUSCTRLPOSIN_PORTSEL_OPA_NEXT;
      return SL_STATUS_OK;

    case SL_HAL_OPA_POSIN_OPA_PAD:
      *reg = OPA_ABUSCTRLPOSIN_PORTSEL_OPA_PAD;
      return SL_STATUS_OK;

    case SL_HAL_OPA_POSIN_GPIO:
      break;

    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  if (!opa_port_is_valid(route->port)
      || ((uint32_t)route->pin > OPA_ROUTE_PIN_MAX)
      || !opa_bus_is_single_lane(route->bus,
                                 SL_HAL_OPA_BUS_EVEN0,
                                 SL_HAL_OPA_BUS_EVEN1)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  portsel = (uint32_t)_OPA_ABUSCTRLPOSIN_PORTSEL_PORTA
            + OPA_ROUTE_PORT_TO_PORTSEL(route->port);
  lane = (route->bus == SL_HAL_OPA_BUS_EVEN1)
         ? (uint32_t)_OPA_ABUSCTRLPOSIN_EVEN01SEL_EVEN1
         : (uint32_t)_OPA_ABUSCTRLPOSIN_EVEN01SEL_EVEN0;

  *reg = ((portsel << (uint32_t)_OPA_ABUSCTRLPOSIN_PORTSEL_SHIFT)
          & (uint32_t)_OPA_ABUSCTRLPOSIN_PORTSEL_MASK)
         | (((uint32_t)route->pin << (uint32_t)_OPA_ABUSCTRLPOSIN_PINSEL_SHIFT)
            & (uint32_t)_OPA_ABUSCTRLPOSIN_PINSEL_MASK)
         | ((lane << (uint32_t)_OPA_ABUSCTRLPOSIN_EVEN01SEL_SHIFT)
            & (uint32_t)_OPA_ABUSCTRLPOSIN_EVEN01SEL_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Encode the negative-input route into the hardware routing register value.
 *
 * GPIO routes use an odd ABUS lane only (@ref SL_HAL_OPA_BUS_ODD0 or
 * @ref SL_HAL_OPA_BUS_ODD1).
 ******************************************************************************/
static sl_status_t opa_encode_negin_route(const sl_hal_opa_negin_route_t *route, uint32_t *reg)
{
  uint32_t portsel;
  uint32_t lane;

  if (route->source >= SL_HAL_OPA_NEGIN_MAX) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  switch (route->source) {
    case SL_HAL_OPA_NEGIN_NONE:
      *reg = OPA_ABUSCTRLNEGIN_PORTSEL_NONE;
      return SL_STATUS_OK;

    case SL_HAL_OPA_NEGIN_FB_X1:
      *reg = OPA_ABUSCTRLNEGIN_PORTSEL_FB_X1;
      return SL_STATUS_OK;

    case SL_HAL_OPA_NEGIN_FB_XN:
      *reg = OPA_ABUSCTRLNEGIN_PORTSEL_FB_XN;
      return SL_STATUS_OK;

    case SL_HAL_OPA_NEGIN_OPA_PAD:
      *reg = OPA_ABUSCTRLNEGIN_PORTSEL_OPA_PAD;
      return SL_STATUS_OK;

    case SL_HAL_OPA_NEGIN_GPIO:
      break;

    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  if (!opa_port_is_valid(route->port)
      || ((uint32_t)route->pin > OPA_ROUTE_PIN_MAX)
      || !opa_bus_is_single_lane(route->bus,
                                 SL_HAL_OPA_BUS_ODD0,
                                 SL_HAL_OPA_BUS_ODD1)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  portsel = (uint32_t)_OPA_ABUSCTRLNEGIN_PORTSEL_PORTA
            + OPA_ROUTE_PORT_TO_PORTSEL(route->port);
  lane = (route->bus == SL_HAL_OPA_BUS_ODD1)
         ? (uint32_t)_OPA_ABUSCTRLNEGIN_ODD01SEL_ODD1
         : (uint32_t)_OPA_ABUSCTRLNEGIN_ODD01SEL_ODD0;

  *reg = ((portsel << (uint32_t)_OPA_ABUSCTRLNEGIN_PORTSEL_SHIFT)
          & (uint32_t)_OPA_ABUSCTRLNEGIN_PORTSEL_MASK)
         | (((uint32_t)route->pin << (uint32_t)_OPA_ABUSCTRLNEGIN_PINSEL_SHIFT)
            & (uint32_t)_OPA_ABUSCTRLNEGIN_PINSEL_MASK)
         | ((lane << (uint32_t)_OPA_ABUSCTRLNEGIN_ODD01SEL_SHIFT)
            & (uint32_t)_OPA_ABUSCTRLNEGIN_ODD01SEL_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Encode the alternate-output route into the hardware routing register value.
 *
 * GPIO routes may use any one lane from @ref SL_HAL_OPA_BUS_EVEN0 through
 * @ref SL_HAL_OPA_BUS_ODD1.
 ******************************************************************************/
static sl_status_t opa_encode_out_route(const sl_hal_opa_out_route_t *route, uint32_t *reg)
{
  uint32_t portsel;
  uint32_t lane;

  if (route->source >= SL_HAL_OPA_OUT_MAX) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  switch (route->source) {
    case SL_HAL_OPA_OUT_NONE:
      *reg = OPA_ABUSCTRLOUT_PORTSEL_NONE;
      return SL_STATUS_OK;

    case SL_HAL_OPA_OUT_OPA_NEXT:
      *reg = OPA_ABUSCTRLOUT_PORTSEL_OPA_NEXT;
      return SL_STATUS_OK;

    case SL_HAL_OPA_OUT_OPA_TO_CMP:
      *reg = OPA_ABUSCTRLOUT_PORTSEL_OPA_TO_CMP;
      return SL_STATUS_OK;

    case SL_HAL_OPA_OUT_GPIO:
      break;

    default:
      return SL_STATUS_INVALID_PARAMETER;
  }

  if (!opa_port_is_valid(route->port)
      || ((uint32_t)route->pin > OPA_ROUTE_PIN_MAX)
      || !opa_bus_is_single_lane(route->bus,
                                 SL_HAL_OPA_BUS_EVEN0,
                                 SL_HAL_OPA_BUS_ODD1)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  portsel = (uint32_t)_OPA_ABUSCTRLOUT_PORTSEL_PORTA
            + OPA_ROUTE_PORT_TO_PORTSEL(route->port);

  switch (route->bus) {
    case SL_HAL_OPA_BUS_EVEN0:
      lane = (uint32_t)_OPA_ABUSCTRLOUT_EVEN01ODD01SEL_EVEN0;
      break;

    case SL_HAL_OPA_BUS_EVEN1:
      lane = (uint32_t)_OPA_ABUSCTRLOUT_EVEN01ODD01SEL_EVEN1;
      break;

    case SL_HAL_OPA_BUS_ODD0:
      lane = (uint32_t)_OPA_ABUSCTRLOUT_EVEN01ODD01SEL_ODD0;
      break;

    default:
      lane = (uint32_t)_OPA_ABUSCTRLOUT_EVEN01ODD01SEL_ODD1;
      break;
  }

  *reg = ((portsel << (uint32_t)_OPA_ABUSCTRLOUT_PORTSEL_SHIFT)
          & (uint32_t)_OPA_ABUSCTRLOUT_PORTSEL_MASK)
         | (((uint32_t)route->pin << (uint32_t)_OPA_ABUSCTRLOUT_PINSEL_SHIFT)
            & (uint32_t)_OPA_ABUSCTRLOUT_PINSEL_MASK)
         | ((lane << (uint32_t)_OPA_ABUSCTRLOUT_EVEN01ODD01SEL_SHIFT)
            & (uint32_t)_OPA_ABUSCTRLOUT_EVEN01ODD01SEL_MASK);

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Validate every field of an initialization structure that the caller owns and
 * return the encoded ABUS routing registers.
 *
 * The caller has already checked that @c init->mode is in range.
 ******************************************************************************/
static sl_status_t opa_validate_init(const sl_hal_opa_init_t *init,
                                     opa_route_regs_t *routes)
{
  const opa_mode_recipe_t *recipe = &opa_mode_recipes[init->mode];
  sl_status_t status;

  if (init->swap >= SL_HAL_OPA_SWAP_MAX) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Restrict swap to values valid for the selected mode feedback topology.
  if (recipe->hysteresis_swap) {
    if ((init->swap != SL_HAL_OPA_SWAP_INPUT)
        && (init->swap != SL_HAL_OPA_SWAP_OUTPUT)) {
      return SL_STATUS_INVALID_PARAMETER;
    }
  } else {
    if ((init->swap != SL_HAL_OPA_SWAP_NONE)
        && (init->swap != SL_HAL_OPA_SWAP_BOTH)) {
      return SL_STATUS_INVALID_PARAMETER;
    }
  }

  // SL_HAL_OPA_RES_VINRES2 is not connected on this device.
  if ((init->res_sel >= SL_HAL_OPA_RES_MAX)
      || (init->res_sel == SL_HAL_OPA_RES_VINRES2)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Feedback modes require the mode-matching NEGIN source (unity:
  // SL_HAL_OPA_NEGIN_FB_X1; PGA/hysteresis: SL_HAL_OPA_NEGIN_FB_XN). TIA
  // leaves NEGIN to the caller.
  if (recipe->hal_negin
      && ((uint32_t)init->neg_input.source != recipe->negin_portsel)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = opa_encode_posin_route(&init->pos_input, &routes->posin);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = opa_encode_negin_route(&init->neg_input, &routes->negin);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return opa_encode_out_route(&init->output, &routes->out);
}

/***************************************************************************//**
 * Program the mode-derived control fields and the ABUS routing of one
 * instance. All values have already been validated by the caller.
 *
 * Control registers are updated read-modify-write so configurable operating
 * settings (gain, input stage, constant-gm, outputs, compensation, and related
 * options) keep their current values.
 ******************************************************************************/
static void opa_apply_init(OPA_TypeDef *opa,
                           const sl_hal_opa_init_t *init,
                           const opa_route_regs_t *routes,
                           opa_instance_role_t role)
{
  const opa_mode_recipe_t *recipe = &opa_mode_recipes[init->mode];
  sl_hal_opa_res_sel_t res_sel = init->res_sel;
  uint32_t posin_reg = routes->posin;
  uint32_t negin_reg = routes->negin;
  uint32_t out_reg = routes->out;
  uint32_t ctrl1;
  uint32_t ctrl2;

  // Inter-instance connections of the dual topologies. Only sl_hal_opa_init_dual()
  // reaches these modes, so the role is always OPA_ROLE_FIRST or OPA_ROLE_SECOND.
  switch (init->mode) {
    case SL_HAL_OPA_MODE_CASCADED_PGA_NON_INVERTING:
      if (role == OPA_ROLE_FIRST) {
        // Stage 1 drives stage 2 through the internal OPA_NEXT connection.
        out_reg = OPA_ABUSCTRLOUT_PORTSEL_OPA_NEXT;
      } else {
        // Stage 2 takes its positive input from stage 1.
        posin_reg = OPA_ABUSCTRLPOSIN_PORTSEL_OPA_NEXT;
      }
      break;

    case SL_HAL_OPA_MODE_CASCADED_PGA_INVERTING:
      if (role == OPA_ROLE_FIRST) {
        out_reg = OPA_ABUSCTRLOUT_PORTSEL_OPA_NEXT;
      } else {
        // Stage 2 feeds R1 from the stage 1 output.
        res_sel = SL_HAL_OPA_RES_VINRES1;
      }
      break;

    case SL_HAL_OPA_MODE_DIFF_AMP:
      // Both instances share the OPA_CENTER resistor node.
      res_sel = SL_HAL_OPA_RES_VINRES0;
      break;

    default:
      break;
  }

  ctrl1 = opa->CTRL1 & ~OPA_CTRL1_INIT_MASK;

  // The input muxes are enabled in every supported mode.
  ctrl1 |= OPA_CTRL1_ENMUXP | OPA_CTRL1_ENMUXN;

  if (recipe->enable_gain_mux) {
    ctrl1 |= OPA_CTRL1_ENMUXGAIN;
  }
  if (recipe->enable_res_mux) {
    ctrl1 |= OPA_CTRL1_ENMUXRES;
  }
  if (recipe->require_cgm) {
    // Cascaded and differential topologies require constant-gm on both
    // instances; single-instance modes leave CGM to @ref sl_hal_opa_set_cgm().
    ctrl1 |= OPA_CTRL1_ENCGM;
  }

  ctrl1 |= (((uint32_t)init->swap << (uint32_t)_OPA_CTRL1_SWP_SHIFT)
            & (uint32_t)_OPA_CTRL1_SWP_MASK)
           | (((uint32_t)res_sel << (uint32_t)_OPA_CTRL1_SELRES_SHIFT)
              & (uint32_t)_OPA_CTRL1_SELRES_MASK);

  ctrl2 = opa->CTRL2 & ~(uint32_t)_OPA_CTRL2_FORCEVFB_MASK;
  if (recipe->force_vfb) {
    ctrl2 |= OPA_CTRL2_FORCEVFB;
  }

  opa->CTRL1 = ctrl1;
  opa->CTRL2 = ctrl2;

  opa->ABUSCTRLPOSIN = posin_reg;
  opa->ABUSCTRLNEGIN = negin_reg;
  opa->ABUSCTRLOUT = out_reg;
}

#if defined(_OPA_TRIM_TRIMOFSPDIF_MASK) && defined(DEVINFO_GP_OPA0TRIM0_OFFSET)

/***************************************************************************//**
 * Return the factory trim words for the selected OPA instance.
 ******************************************************************************/
static const sl_hal_system_devinfo_opa_t *sli_hal_opa_devinfo_for_instance(
  const sl_hal_system_devinfo_opa_block_t *block,
  OPA_TypeDef *opa)
{
  int instance = OPA_NUM(opa);

  SL_LOG_DEBUG_ASSERT(instance >= 0);

  return (instance == 0) ? &block->opa0 : &block->opa1;
}

/***************************************************************************//**
 * Apply factory trim that matches the current OPA configuration.
 *
 * Selects offset and bias trim from device information based on the enabled
 * input stage, constant-gm setting, and high-performance operating mode,
 * then programs the trim register. High-performance mode is distinct from
 * @ref sl_hal_opa_set_high_drive (extra output drive).
 *
 * DEVINFO field masks use the OPA0 symbol names; OPA1 trim words use the
 * same bit layout (see @c DEVINFO_GP_OPA1TRIM0_* in the device header).
 ******************************************************************************/
static void sli_hal_opa_refresh_factory_trim(OPA_TypeDef *opa)
{
  sl_hal_system_devinfo_opa_block_t cal_block;
  const sl_hal_system_devinfo_opa_t *devinfo;
  sl_hal_opa_input_stage_t input_stage;
  uint32_t cal_word;
  uint32_t ofs_pdif;
  uint32_t ofs_ndif;
  uint32_t outbias;
  bool cgm_enabled;
  bool high_power;

  if (sl_hal_opa_is_locked(opa)) {
    return;
  }

  sl_hal_system_get_opa_calibration_info(&cal_block);
  devinfo = sli_hal_opa_devinfo_for_instance(&cal_block, opa);
  input_stage = sl_hal_opa_get_input_stage(opa);
  cgm_enabled = sl_hal_opa_is_cgm_enabled(opa);
  high_power = ((opa->CTRL2 & (uint32_t)_OPA_CTRL2_ENHPMODE_MASK) != 0U);

  outbias = (devinfo->trim0 & DEVINFO_GP_OPA0TRIM0_TRIMOUTBIAS_MASK)
            >> DEVINFO_GP_OPA0TRIM0_TRIMOUTBIAS_SHIFT;

  // Constant-gm overrides the input-stage trim selection.
  if (cgm_enabled) {
    cal_word = devinfo->trim1;
    if (high_power) {
      ofs_pdif = (cal_word & DEVINFO_GP_OPA0TRIM1_OFSPDIF_PN_CGM_HP_MASK)
                 >> DEVINFO_GP_OPA0TRIM1_OFSPDIF_PN_CGM_HP_SHIFT;
      ofs_ndif = (cal_word & DEVINFO_GP_OPA0TRIM1_OFSNDIF_PN_CGM_HP_MASK)
                 >> DEVINFO_GP_OPA0TRIM1_OFSNDIF_PN_CGM_HP_SHIFT;
    } else {
      ofs_pdif = (cal_word & DEVINFO_GP_OPA0TRIM1_OFSPDIF_PN_CGM_LP_MASK)
                 >> DEVINFO_GP_OPA0TRIM1_OFSPDIF_PN_CGM_LP_SHIFT;
      ofs_ndif = (cal_word & DEVINFO_GP_OPA0TRIM1_OFSNDIF_PN_CGM_LP_MASK)
                 >> DEVINFO_GP_OPA0TRIM1_OFSNDIF_PN_CGM_LP_SHIFT;
    }
  } else {
    switch (input_stage) {
      case SL_HAL_OPA_INPUT_STAGE_PMOS:
        cal_word = devinfo->trim0;
        if (high_power) {
          ofs_pdif = (cal_word & DEVINFO_GP_OPA0TRIM0_OFSPDIF_P_HP_MASK)
                     >> DEVINFO_GP_OPA0TRIM0_OFSPDIF_P_HP_SHIFT;
        } else {
          ofs_pdif = (cal_word & DEVINFO_GP_OPA0TRIM0_OFSPDIF_P_LP_MASK)
                     >> DEVINFO_GP_OPA0TRIM0_OFSPDIF_P_LP_SHIFT;
        }
        ofs_ndif = 0U;
        break;

      case SL_HAL_OPA_INPUT_STAGE_NMOS:
        cal_word = devinfo->trim0;
        ofs_pdif = 0U;
        if (high_power) {
          ofs_ndif = (cal_word & DEVINFO_GP_OPA0TRIM0_OFSNDIF_N_HP_MASK)
                     >> DEVINFO_GP_OPA0TRIM0_OFSNDIF_N_HP_SHIFT;
        } else {
          ofs_ndif = (cal_word & DEVINFO_GP_OPA0TRIM0_OFSNDIF_N_LP_MASK)
                     >> DEVINFO_GP_OPA0TRIM0_OFSNDIF_N_LP_SHIFT;
        }
        break;

      case SL_HAL_OPA_INPUT_STAGE_RAIL_TO_RAIL:
        if (high_power) {
          cal_word = devinfo->trim0;
          ofs_pdif = (cal_word & DEVINFO_GP_OPA0TRIM0_OFSPDIF_PN_HP_MASK)
                     >> DEVINFO_GP_OPA0TRIM0_OFSPDIF_PN_HP_SHIFT;
          ofs_ndif = (cal_word & DEVINFO_GP_OPA0TRIM0_OFSNDIF_PN_HP_MASK)
                     >> DEVINFO_GP_OPA0TRIM0_OFSNDIF_PN_HP_SHIFT;
        } else {
          cal_word = devinfo->trim1;
          ofs_pdif = (cal_word & DEVINFO_GP_OPA0TRIM1_OFSPDIF_PN_LP_MASK)
                     >> DEVINFO_GP_OPA0TRIM1_OFSPDIF_PN_LP_SHIFT;
          ofs_ndif = (cal_word & DEVINFO_GP_OPA0TRIM1_OFSNDIF_PN_LP_MASK)
                     >> DEVINFO_GP_OPA0TRIM1_OFSNDIF_PN_LP_SHIFT;
        }
        break;

      case SL_HAL_OPA_INPUT_STAGE_NONE:
      default:
        return;
    }
  }

  opa->TRIM = ((ofs_pdif << (uint32_t)_OPA_TRIM_TRIMOFSPDIF_SHIFT)
               & (uint32_t)_OPA_TRIM_TRIMOFSPDIF_MASK)
              | ((ofs_ndif << (uint32_t)_OPA_TRIM_TRIMOFSNDIF_SHIFT)
                 & (uint32_t)_OPA_TRIM_TRIMOFSNDIF_MASK)
              | ((outbias << (uint32_t)_OPA_TRIM_TRIMOUTBIAS_SHIFT)
                 & (uint32_t)_OPA_TRIM_TRIMOUTBIAS_MASK);
}

#endif /* defined(_OPA_TRIM_TRIMOFSPDIF_MASK) && defined(DEVINFO_GP_OPA0TRIM0_OFFSET) */

#endif /* defined(OPA_PRESENT) */
