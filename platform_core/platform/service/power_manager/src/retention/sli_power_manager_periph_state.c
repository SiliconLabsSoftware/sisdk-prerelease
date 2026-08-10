/***************************************************************************//**
 * @file
 * @brief Power Manager peripheral state save/restore framework implementation.
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

#include "sli_power_manager_periph_state.h"
#include "sli_power_manager_periph_bitmap.h"
#include "sl_power_manager.h"
#include "sli_power_manager_periph_retention_routines.h"
#include "sl_assert.h"
#include "sl_clock_manager.h"
#include "sl_status.h"
#include "sl_device_clock.h"
#include "sl_core.h"
#include "sl_compiler.h"
#include "em_device.h"
#include <string.h>

/*******************************************************************************
 ***************************  LOCAL DEFINES   **********************************
 ******************************************************************************/

#if defined(SL_TRUSTZONE_SECURE) || defined(SL_TZ_SECURE_EXECUTION)
#define SMU_REG    SMU
#define SLI_PM_PPUACCESSGATE(idx)         ((&SMU_REG->PPUACCESSGATE0)[(idx)])
#define SLI_PM_PPUACCESSGATE_SET(idx)     ((&SMU_REG->PPUACCESSGATE0_SET)[(idx)])
#define SLI_PM_PPUACCESSGATE_CLR(idx)     ((&SMU_REG->PPUACCESSGATE0_CLR)[(idx)])
#define SLI_PM_PPUDIRTY(idx)              ((&SMU_REG->PPUDIRTY0)[(idx)])
#define SLI_PM_PPUDIRTY_SET(idx)          ((&SMU_REG->PPUDIRTY0_SET)[(idx)])
#define SLI_PM_PPUDIRTY_CLR(idx)          ((&SMU_REG->PPUDIRTY0_CLR)[(idx)])
#else
#define SMU_REG    SMU_CFGNS
#define SLI_PM_PPUACCESSGATE(idx)         ((&SMU_REG->PPUNSACCESSGATE0)[(idx)])
#define SLI_PM_PPUACCESSGATE_SET(idx)     ((&SMU_REG->PPUNSACCESSGATE0_SET)[(idx)])
#define SLI_PM_PPUACCESSGATE_CLR(idx)     ((&SMU_REG->PPUNSACCESSGATE0_CLR)[(idx)])
#define SLI_PM_PPUDIRTY(idx)              ((&SMU_REG->PPUNSDIRTY0)[(idx)])
#define SLI_PM_PPUDIRTY_SET(idx)          ((&SMU_REG->PPUNSDIRTY0_SET)[(idx)])
#define SLI_PM_PPUDIRTY_CLR(idx)          ((&SMU_REG->PPUNSDIRTY0_CLR)[(idx)])
#endif

// Mapping table: ppu_bit -> sli_power_manager_periph index
static uint8_t sli_ppu_bit_to_periph_index[SLI_POWER_MANAGER_MAX_PPU_BITS];

// CLKEN state backup buffer.
SLI_CLKEN_DECLARE_BACKUP_BUF(sli_clken_state);

/*******************************************************************************
 ***************************  LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

/***************************************************************************//**
 * Finds the ppu_bit for a peripheral by its base address.
 *
 * @param[in]  base  The peripheral base address.
 *
 * @return The ppu_bit if found, SLI_POWER_MANAGER_MAX_PPU_BITS if not found.
 ******************************************************************************/
static uint8_t find_ppu_bit_from_base(uint32_t base)
{
  for (uint8_t i = 0u; i < sli_power_manager_periph_count; i++) {
    if (sli_power_manager_periph[i].base == base) {
      return sli_power_manager_periph[i].ppu_bit;
    }
  }
  return SLI_POWER_MANAGER_MAX_PPU_BITS;
}

/***************************************************************************//**
 * Finds the peripheral from the BFAR address.
 * Only searches peripherals that are currently gated (via HW PPUACCESSGATE)
 * for efficiency, since only gated peripherals can cause bus faults.
 *
 * @param[in]  bfar_address Bus fault address.
 *
 * @return Pointer to the peripheral state management entry. NULL if not found.
 ******************************************************************************/
static sli_power_manager_periph_state_mgmt_t *find_peripheral_from_bfar(uint32_t bfar_address)
{
  for (uint8_t w = 0u; w < SLI_PM_SMU_REG_COUNT; w++) {
    uint32_t bits = SLI_PM_PPUACCESSGATE(w);

    // Loop through all the set bits in the word
    while (bits != 0u) {
      uint8_t bit_pos = (uint8_t)__CLZ(__RBIT(bits));
      uint8_t ppu_bit = (w * 32u) + bit_pos;

      uint8_t periph_idx = sli_ppu_bit_to_periph_index[ppu_bit];
      if (periph_idx != SLI_POWER_MANAGER_INVALID_PERIPH_INDEX) {
        EFM_ASSERT(periph_idx < sli_power_manager_periph_count);

        // Check if the bus fault address is within the base address of the peripheral
        uint32_t base = sli_power_manager_periph[periph_idx].base;
        if ((bfar_address >= base) && (bfar_address < (base + 0x1000u))) {
          return &sli_power_manager_periph[periph_idx];
        }
      }

      bits &= ~(1u << bit_pos);
    }
  }
  return NULL;
}

/***************************************************************************//**
 * Initializes the ppu_bit to peripheral index mapping table.
 ******************************************************************************/
static void init_ppu_bit_mapping_table(void)
{
  memset(sli_ppu_bit_to_periph_index,
         SLI_POWER_MANAGER_INVALID_PERIPH_INDEX,
         sizeof(sli_ppu_bit_to_periph_index));

  // Populate mapping for each peripheral in the table
  for (uint8_t i = 0u; i < sli_power_manager_periph_count; i++) {
    uint8_t ppu_bit = sli_power_manager_periph[i].ppu_bit;
    EFM_ASSERT(ppu_bit < SLI_POWER_MANAGER_MAX_PPU_BITS);
    sli_ppu_bit_to_periph_index[ppu_bit] = i;
  }
}

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/
void sli_power_manager_periph_state_init(void)
{
  memcpy(sli_pm_restore_on_wakeup_mask,
         sli_pm_restore_on_wakeup_default_mask,
         sizeof(sli_pm_restore_on_wakeup_mask));

  init_ppu_bit_mapping_table();
}

/***************************************************************************//**
 * Saves the peripherals that are retained (SL_PM_<PERIPHERAL>_RETAINED)
 * using direct bitmap iteration with ppu_bit to peripheral index lookup.
 ******************************************************************************/
void sli_power_manager_save_peripherals(void)
{
  // Save the CMU clock enable state.
  sli_clken_state_save_restore(CMU_BASE, &sli_clken_state, true);

  for (uint8_t w = 0u; w < SLI_PM_SMU_REG_COUNT; w++) {
    uint32_t bits = SLI_PM_PPUDIRTY(w);
    // Loop through all the dirty bits in the word
    while (bits != 0u) {
      uint8_t bit_pos = (uint8_t)__CLZ(__RBIT(bits));
      uint8_t ppu_bit = (w * 32u) + bit_pos;

      uint8_t periph_idx = sli_ppu_bit_to_periph_index[ppu_bit];
      if (periph_idx != SLI_POWER_MANAGER_INVALID_PERIPH_INDEX) {
        EFM_ASSERT(periph_idx < sli_power_manager_periph_count);

        sli_power_manager_periph_state_mgmt_t *entry = &sli_power_manager_periph[periph_idx];

        EFM_ASSERT(entry->routine != NULL);
        EFM_ASSERT(entry->backup_buffer != NULL);

        if (entry->module_bus_clk != SL_BUS_CLOCK_INVALID) {
          sl_clock_manager_enable_bus_clock(entry->module_bus_clk);
        }
        entry->routine(entry->base, entry->backup_buffer, true);

        SLI_PM_PPUDIRTY_CLR(w) = (1u << bit_pos);
      }

      bits &= ~(1u << bit_pos);
    }
  }

  // Restore the CMU clock enable state.
  sli_clken_state_save_restore(CMU_BASE, &sli_clken_state, false);
}

/***************************************************************************//**
 * Restores the peripherals that are retained with the restore on wakeup strategy
 * (SL_PM_<PERIPHERAL>_RETENTION_STRATEGY=SL_PM_RETENTION_STRATEGY_ON_WAKEUP).
 * Processes peripherals in ppu_bit order through the retained mask bitmap.
 ******************************************************************************/
void sli_power_manager_restore_peripherals(void)
{
  // Determine which peripherals were reset by reading power domain status.
  uint32_t domains_off = (EMU_CFGNS->PWRSTATUS & _EMU_PWRSTATUS_DIGDOMOFF_MASK)
                         >> _EMU_PWRSTATUS_DIGDOMOFF_SHIFT;

  // Build a combined bitmap of all peripherals belonging to shut-down domains.
  uint32_t reset_periph_bitmap[SLI_PM_SMU_REG_COUNT] = { 0 };
  for (uint32_t d = 0u; d < sli_pm_power_domain_map_count; d++) {
    if ((domains_off & sli_pm_power_domain_map[d].domain_mask) != 0u) {
      for (uint8_t w = 0u; w < SLI_PM_SMU_REG_COUNT; w++) {
        reset_periph_bitmap[w] |= sli_pm_power_domain_map[d].periph_mask[w];
      }
    }
  }

  // Process each word: narrow to retained, restore on-wakeup, gate on-demand.
  for (uint8_t w = 0u; w < SLI_PM_SMU_REG_COUNT; w++) {
    // Narrow to retained peripherals only.
    reset_periph_bitmap[w] &= sli_pm_retained_mask[w];

    uint32_t bits = reset_periph_bitmap[w];
    uint32_t restored_bits = 0u;

    // Loop through all the set bits in the word
    while (bits != 0u) {
      uint8_t bit_pos = (uint8_t)__CLZ(__RBIT(bits));
      uint8_t ppu_bit = (w * 32u) + bit_pos;

      uint8_t periph_idx = sli_ppu_bit_to_periph_index[ppu_bit];
      if (periph_idx != SLI_POWER_MANAGER_INVALID_PERIPH_INDEX) {
        EFM_ASSERT(periph_idx < sli_power_manager_periph_count);

        sli_power_manager_periph_state_mgmt_t *entry = &sli_power_manager_periph[periph_idx];

        if (SLI_PM_BITMAP_IS_BIT_SET(sli_pm_restore_on_wakeup_mask, ppu_bit)) {
          EFM_ASSERT(entry->routine != NULL);
          EFM_ASSERT(entry->backup_buffer != NULL);

          if (entry->module_bus_clk != SL_BUS_CLOCK_INVALID) {
            sl_clock_manager_enable_bus_clock(entry->module_bus_clk);
          }
          entry->routine(entry->base, entry->backup_buffer, false);

          restored_bits |= (1u << bit_pos);
        }
      }

      bits &= ~(1u << bit_pos);
    }

    // Gate on-demand peripherals and clear dirty bits for restored peripherals.
    uint32_t on_demand_gate = reset_periph_bitmap[w] & ~sli_pm_restore_on_wakeup_mask[w];
    if (on_demand_gate != 0u) {
      SLI_PM_PPUACCESSGATE_SET(w) = on_demand_gate;
    }
    if (restored_bits != 0u) {
      SLI_PM_PPUDIRTY_CLR(w) = restored_bits;
    }
  }

  // Restore the CMU clock enable state.
  sli_clken_state_save_restore(CMU_BASE, &sli_clken_state, false);
}

/***************************************************************************//**
 * Gets the peripheral dirty state.
 *
 * @param[in]  peripheral Peripheral to get the dirty state.
 *
 * @return true if the peripheral is dirty, false otherwise.
 *         false if the peripheral is NULL.
 *
 * @note This API is only useful if a driver wants to use the dirty state of a
 *       peripheral to implement its state saving/restoring.
 ******************************************************************************/
bool sl_power_manager_get_peripheral_dirty_state(const sl_peripheral_t peripheral)
{
  if (peripheral == NULL) {
    return false;
  }

  uint8_t ppu_bit = find_ppu_bit_from_base(peripheral->base);
  EFM_ASSERT(ppu_bit < SLI_POWER_MANAGER_MAX_PPU_BITS);

  uint8_t reg = SLI_PM_BITMAP_WORD(ppu_bit);
  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_CRITICAL();
  bool is_dirty = (SLI_PM_PPUDIRTY(reg) & SLI_PM_BITMAP_MASK(ppu_bit)) != 0u;
  CORE_EXIT_CRITICAL();

  return is_dirty;
}

/***************************************************************************//**
 * Restores a peripheral from a bus fault.
 *
 * @param[in]  bfar_address Bus fault address.
 *
 * @return true if the peripheral is restored, false otherwise.
 ******************************************************************************/
bool sli_power_manager_busfault_restore_periph(uint32_t bfar_address)
{
  sli_power_manager_periph_state_mgmt_t *entry = find_peripheral_from_bfar(bfar_address);
  if (entry == NULL || entry->routine == NULL || entry->backup_buffer == NULL) {
    return false;
  }

  uint8_t ppu_bit = entry->ppu_bit;
  uint8_t reg = SLI_PM_BITMAP_WORD(ppu_bit);
  uint32_t mask = SLI_PM_BITMAP_MASK(ppu_bit);
  bool bus_clock_was_enabled = true;

  // Validate the PPUACCESSGATE bit for this peripheral before restoring.
  if ((SLI_PM_PPUACCESSGATE(reg) & mask) == 0u) {
    return false;
  }

  // Clear PPUACCESSGATE bit to un-gate this peripheral.
  SLI_PM_PPUACCESSGATE_CLR(reg) = mask;

  // Get the bus clock state before the repaint.
  sl_status_t clock_status = sl_clock_manager_is_bus_clock_enabled(entry->module_bus_clk,
                                                                   &bus_clock_was_enabled);
  if (clock_status != SL_STATUS_OK || !bus_clock_was_enabled) {
    return false;
  }

  // Restore the peripheral.
  entry->routine(entry->base, entry->backup_buffer, false);

  // The restore writes set the HW dirty bit automatically. Clear it so the
  // peripheral starts in a clean state after the on-demand repaint.
  SLI_PM_PPUDIRTY_CLR(reg) = mask;
  return true;
}

/***************************************************************************//**
 * C handler for BusFault peripheral restoration.
 * Called from the naked trampoline.
 *
 * @param[in] stack_frame  Pointer to the exception stack frame (DEBUG_EFM only).
 ******************************************************************************/
#if defined(DEBUG_EFM)
__USED static void busfault_restore_handler(uint32_t *stack_frame)
#else
__USED static void busfault_restore_handler(void)
#endif
{
#if defined(DEBUG_EFM)
  (void)stack_frame;
#endif

#if defined(SCB_CFSR_BFARVALID_Msk) && defined(SCB_CFSR_PRECISERR_Msk)
  uint32_t cfsr = SCB->CFSR;
  uint32_t bfar = SCB->BFAR;

  if (((cfsr & SCB_CFSR_BFARVALID_Msk) != 0u)
      && ((cfsr & SCB_CFSR_PRECISERR_Msk) != 0u)
      && sli_power_manager_busfault_restore_periph(bfar)) {
    SCB->CFSR = SCB_CFSR_BFARVALID_Msk | SCB_CFSR_PRECISERR_Msk;
    return;
  }
#endif

  BusFault_Handler();
}

/***************************************************************************//**
 * Bus fault handler for peripheral retention (naked trampoline).
 * This handler is used to restore the peripherals that are retained with the
 * ON_DEMAND strategy.
 * (SL_PM_<PERIPHERAL>_RETENTION_STRATEGY=SL_PM_RETENTION_STRATEGY_RESTORE_ON_DEMAND).
 *
 * Uses a naked function to preserve the exception return (LR) value,
 * ensuring the faulting instruction is re-executed after restoration.
 *
 * In debug builds (DEBUG_EFM), the exception stack frame pointer is loaded
 * into R0 before branching to the C handler. R0 is used because it is the
 * first argument register in the ARM calling convention (AAPCS), allowing
 * the C handler to receive the stack frame for inspection (e.g. stacked PC,
 * registers). This is omitted in release builds to save the extra instructions.
 *
 * If the Bus Fault is not a supported peripheral repaint, the handler will
 * call the default Bus Fault handler.
 ******************************************************************************/
__NO_PROLOGUE
void sli_busfault_handler_peripheral_restore(void)
{
  __ASM volatile (
#if defined(DEBUG_EFM)
    "tst lr, #4          \n"  // Check EXC_RETURN bit 2 to determine active stack
    "ite eq              \n"  // If-Then-Else: EQ when bit 2 is 0 (MSP), NE when 1 (PSP)
    "mrseq r0, msp       \n"  // Bit 2 == 0: exception used MSP, pass stack frame in R0
    "mrsne r0, psp       \n"  // Bit 2 == 1: exception used PSP, pass stack frame in R0
#endif
    "b busfault_restore_handler \n"  // Tail-branch to C handler; preserves LR (EXC_RETURN)
    );
}

/***************************************************************************//**
 * Clears the peripheral dirty state.
 *
 * @param[in]  peripheral Peripheral to clear the dirty state.
 *
 * @note This API is only useful if a driver wants to use the dirty state of a
 *       peripheral to implement its state saving/restoring. It is necessary to
 *       manually clear the dirty state after the states are restored.
 *
 * @return SL_STATUS_OK if the dirty state was cleared successfully.
 *         SL_STATUS_INVALID_PARAMETER if the peripheral is NULL.
 ******************************************************************************/
sl_status_t sl_power_manager_clear_peripheral_dirty_state(const sl_peripheral_t peripheral)
{
  if (peripheral == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  uint8_t ppu_bit = find_ppu_bit_from_base(peripheral->base);
  EFM_ASSERT(ppu_bit < SLI_POWER_MANAGER_MAX_PPU_BITS);

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_CRITICAL();
  SLI_PM_PPUDIRTY_CLR(SLI_PM_BITMAP_WORD(ppu_bit)) = SLI_PM_BITMAP_MASK(ppu_bit);
  CORE_EXIT_CRITICAL();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Sets the retention strategy for a peripheral.
 *
 * @param[in]  peripheral Peripheral to set the retention strategy for.
 * @param[in]  strategy   SL_PM_RETENTION_STRATEGY_ON_DEMAND or
 *                        SL_PM_RETENTION_STRATEGY_ON_WAKEUP.
 * @return SL_STATUS_OK if the retention strategy was set successfully.
 *         SL_STATUS_INVALID_PARAMETER if the peripheral is NULL, the strategy
 *         is invalid, or SL_PM_<PERIPHERAL>_RETAINED == 0.
 ******************************************************************************/
sl_status_t sl_power_manager_set_peripheral_retention_strategy(const sl_peripheral_t peripheral,
                                                               uint8_t strategy)
{
  if (peripheral == NULL
      || ((strategy != SL_PM_RETENTION_STRATEGY_ON_DEMAND)
          && (strategy != SL_PM_RETENTION_STRATEGY_ON_WAKEUP))) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  uint8_t ppu_bit = find_ppu_bit_from_base(peripheral->base);
  EFM_ASSERT(ppu_bit < SLI_POWER_MANAGER_MAX_PPU_BITS);

  if (!SLI_PM_BITMAP_IS_BIT_SET(sli_pm_retained_mask, ppu_bit)) {
    return SL_STATUS_INVALID_PARAMETER; // Peripheral not retained.
  }

  CORE_DECLARE_IRQ_STATE;
  CORE_ENTER_CRITICAL();
  if (strategy == SL_PM_RETENTION_STRATEGY_ON_WAKEUP) {
    SLI_PM_BITMAP_SET(sli_pm_restore_on_wakeup_mask, ppu_bit);
  } else {
    SLI_PM_BITMAP_CLR(sli_pm_restore_on_wakeup_mask, ppu_bit);
  }
  CORE_EXIT_CRITICAL();

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Gets the retention strategy for a peripheral.
 *
 * @param peripheral  A pointer to peripheral.
 *
 * @return SL_PM_RETENTION_STRATEGY_ON_DEMAND, SL_PM_RETENTION_STRATEGY_ON_WAKEUP
 *         or 0xFFu if the peripheral is NULL.
 ******************************************************************************/
uint8_t sl_power_manager_get_peripheral_retention_strategy(const sl_peripheral_t peripheral)
{
  if (peripheral == NULL) {
    return 0xFFu;
  }

  uint8_t ppu_bit = find_ppu_bit_from_base(peripheral->base);
  EFM_ASSERT(ppu_bit < SLI_POWER_MANAGER_MAX_PPU_BITS);

  if (SLI_PM_BITMAP_IS_BIT_SET(sli_pm_restore_on_wakeup_mask, ppu_bit)) {
    return SL_PM_RETENTION_STRATEGY_ON_WAKEUP;
  }
  return SL_PM_RETENTION_STRATEGY_ON_DEMAND;
}

/***************************************************************************//**
 * Gets the default retention strategy for a peripheral.
 *
 * @param peripheral  A pointer to peripheral.
 *
 * @return SL_PM_RETENTION_STRATEGY_ON_DEMAND, SL_PM_RETENTION_STRATEGY_ON_WAKEUP
 *         or 0xFFu if the peripheral is NULL.
 ******************************************************************************/
uint8_t sli_power_manager_get_peripheral_default_retention_strategy(const sl_peripheral_t peripheral)
{
  if (peripheral == NULL) {
    return 0xFFu;
  }

  uint8_t ppu_bit = find_ppu_bit_from_base(peripheral->base);
  EFM_ASSERT(ppu_bit < SLI_POWER_MANAGER_MAX_PPU_BITS);

  if (SLI_PM_BITMAP_IS_BIT_SET(sli_pm_restore_on_wakeup_default_mask, ppu_bit)) {
    return SL_PM_RETENTION_STRATEGY_ON_WAKEUP;
  }
  return SL_PM_RETENTION_STRATEGY_ON_DEMAND;
}
