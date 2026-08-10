/***************************************************************************//**
 * @file
 * @brief Power Manager peripheral state save/restore framework.
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

#ifndef SLI_POWER_MANAGER_PERIPH_STATE_H
#define SLI_POWER_MANAGER_PERIPH_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include "sl_device_clock.h"
#include "sl_device_peripheral.h"
#include "sl_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *****************************   DEFINES   **************************************
 ******************************************************************************/

#define SLI_POWER_MANAGER_PERIPH_BITMAP_WORDS  (4u)

// Maximum number of ppu_bits
#define SLI_POWER_MANAGER_MAX_PPU_BITS  (SLI_POWER_MANAGER_PERIPH_BITMAP_WORDS * 32u)

// Invalid index marker for mapping table
#define SLI_POWER_MANAGER_INVALID_PERIPH_INDEX  (0xFFu)

#define SLI_PM_BITMAP_WORD(bit)           ((bit) / 32u)
#define SLI_PM_BITMAP_BIT_IN_WORD(bit)    ((bit) % 32u)
#define SLI_PM_BITMAP_MASK(bit)           (1u << SLI_PM_BITMAP_BIT_IN_WORD(bit))

#define SLI_PM_BITMAP_SET(bm, bit) \
  ((bm)[SLI_PM_BITMAP_WORD(bit)] |= SLI_PM_BITMAP_MASK(bit))

#define SLI_PM_BITMAP_CLR(bm, bit) \
  ((bm)[SLI_PM_BITMAP_WORD(bit)] &= ~SLI_PM_BITMAP_MASK(bit))

#define SLI_PM_BITMAP_IS_BIT_SET(bm, bit) \
  (((bm)[SLI_PM_BITMAP_WORD(bit)] & SLI_PM_BITMAP_MASK(bit)) != 0u)

/*******************************************************************************
 *****************************   DATA TYPES   ***********************************
 ******************************************************************************/

/** Function pointer type for peripheral save/restore routine.
 *  @param base          Peripheral base address.
 *  @param backup_buffer Pointer to backup buffer for this peripheral instance.
 *  @param save          True to save (HW -> buffer), false to restore (buffer -> HW).
 */
typedef void (*sli_power_manager_periph_save_restore_t)(uint32_t base,
                                                        void *backup_buffer,
                                                        bool save);

typedef struct sli_power_manager_periph_state_mgmt {
  sli_power_manager_periph_save_restore_t routine;    // Save/restore routine.
  void *backup_buffer;                                // Backup buffer (e.g. in .noinit).
  uint32_t base;                                      // Peripheral base address.
  sl_bus_clock_t module_bus_clk;                      // Bus clock for this peripheral.
  uint8_t ppu_bit;                                    // Packed PPU global index: (register_index << 5) | bit_position.
} sli_power_manager_periph_state_mgmt_t;

/*******************************************************************************
 ***************************   BITMAPS   ****************************************
 ******************************************************************************/

// Compile-time bitmaps from retention config.
extern const uint32_t sli_pm_retained_mask[SLI_POWER_MANAGER_PERIPH_BITMAP_WORDS];
extern const uint32_t sli_pm_restore_on_wakeup_default_mask[SLI_POWER_MANAGER_PERIPH_BITMAP_WORDS];

// Mutable retained peripherals restore-on-wakeup mask modified by sl_power_manager_set_peripheral_retention_strategy().
extern uint32_t sli_pm_restore_on_wakeup_mask[SLI_POWER_MANAGER_PERIPH_BITMAP_WORDS];

// Power domain to peripheral PPU bitmask mapping entry.
typedef struct {
  uint32_t domain_mask;                                        // EMU_PWRSTATUS DIGDOMOFF bitmask for this domain.
  uint32_t periph_mask[SLI_POWER_MANAGER_PERIPH_BITMAP_WORDS]; // PPU bitmask of peripherals in this domain.
} sli_pm_power_domain_entry_t;

// Device-specific power domain map (keyed by EMU DIGDOMOFF bitmask values).
extern const sli_pm_power_domain_entry_t sli_pm_power_domain_map[];
extern const uint32_t sli_pm_power_domain_map_count;

/*******************************************************************************
 ***************************   DEVICE TABLE   ************************************
 ******************************************************************************/

// Device-specific compact table of retained peripheral state management entries.
extern sli_power_manager_periph_state_mgmt_t sli_power_manager_periph[];
extern const uint8_t sli_power_manager_periph_count;

/*******************************************************************************
 **************************   FUNCTIONS   **************************************
 ******************************************************************************/

// Save dirty peripheral states before EM2. Called from save_context() in sleep loop.
void sli_power_manager_save_peripherals(void);

// Restore always-restored peripheral states after EM2 wake. Called from restore_context().
void sli_power_manager_restore_peripherals(void);

// Initialize peripheral state bitmaps. Called from sli_power_manager_init_hardware().
void sli_power_manager_periph_state_init(void);

// Restore a peripheral from a bus fault.
bool sli_power_manager_busfault_restore_periph(uint32_t bfar_address);

// Retention-aware BusFault wrapper.
void sli_busfault_handler_peripheral_restore(void);

// Gets the peripheral dirty state.
bool sl_power_manager_get_peripheral_dirty_state(const sl_peripheral_t peripheral);

// Clears the peripheral dirty state.
sl_status_t sl_power_manager_clear_peripheral_dirty_state(const sl_peripheral_t peripheral);

// Gets the default retention strategy for a peripheral.
uint8_t sli_power_manager_get_peripheral_default_retention_strategy(const sl_peripheral_t peripheral);

#if !defined(SL_CATALOG_POWER_MANAGER_NO_DEEPSLEEP_PRESENT)
// Returns true if any non-EM2-capable peripheral has its clock enabled.
bool sli_power_manager_is_non_em2_peripheral_enabled(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SLI_POWER_MANAGER_PERIPH_STATE_H */
