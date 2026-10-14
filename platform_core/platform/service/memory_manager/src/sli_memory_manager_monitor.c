/***************************************************************************//**
 * @file
 * @brief Memory Manager Monitor - RAM Usage Query Implementation.
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

#include <string.h>

#include "sli_memory_manager_monitor.h"
#include "sl_memory_manager.h"
#include "sl_memory_manager_region.h"
#include "sl_assert.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

// -----------------------------------------------------------------------------
// DMEM bank defines for EM2 retention measurement.
// These must match the definitions in the retention control HAL.

#if defined(SL_CATALOG_BANK_RETENTION_CONTROL_PRESENT)

#if !defined(DMEM_MEM_BASE)
#define DMEM_MEM_BASE  SRAM_BASE
#endif

#if !defined(DMEM_NUM_BANKS)
#if defined(DMEM_NUM_BANK)
#define DMEM_NUM_BANKS DMEM_NUM_BANK
#elif defined(DMEM0_NUM_BANKS)
#define DMEM_NUM_BANKS DMEM0_NUM_BANKS
#define DMEM_BANK0_SIZE DMEM0_BANK0_SIZE
#elif defined(HOSTDMEM_NUM_BANKS)
#define DMEM_NUM_BANKS HOSTDMEM_NUM_BANKS
#define DMEM_BANK0_SIZE HOSTDMEM_BANK0_SIZE
#endif
#endif

// DMEM mixed bank sizes: some devices have smaller banks at higher indices
// when DMEM_BANK44_SIZE is defined and smaller than DMEM_BANK0_SIZE.
#if defined(DMEM_BANK44_SIZE) && (DMEM_BANK44_SIZE < DMEM_BANK0_SIZE)
#define SLI_MM_MONITOR_DMEM_SMALL_BANK_COUNT \
  (DMEM_NUM_BANKS > 44u ? (uint32_t)(DMEM_NUM_BANKS - 44u) : 0u)
#define SLI_MM_MONITOR_DMEM_SMALL_BANK_SIZE  ((size_t)DMEM_BANK44_SIZE)
#else
#define SLI_MM_MONITOR_DMEM_SMALL_BANK_COUNT 0u
#define SLI_MM_MONITOR_DMEM_SMALL_BANK_SIZE  ((size_t)DMEM_BANK0_SIZE)
#endif

#define SLI_MM_MONITOR_DMEM_SMALL_TOTAL \
  ((size_t)SLI_MM_MONITOR_DMEM_SMALL_BANK_COUNT * SLI_MM_MONITOR_DMEM_SMALL_BANK_SIZE)

// DTCM bank configuration.
#if defined(SL_CATALOG_MEMORY_MANAGER_DTCM_PRESENT)
#define SLI_MM_MONITOR_DTCM_BANK_SIZE   (0x4000u)  // 16 KB per bank.
#define SLI_MM_MONITOR_DTCM_NUM_BANK    (DTCM_MAX_SIZE / SLI_MM_MONITOR_DTCM_BANK_SIZE)
#endif

// ITCM bank and block configuration.
#if defined(SL_CATALOG_MEMORY_MANAGER_ITCM_PRESENT)
#define SLI_MM_MONITOR_ITCM_BANK_SIZE        (0x4000u)   // 16 KB per physical bank.
#define SLI_MM_MONITOR_ITCM_BLOCKS_PER_BANK  (4u)        // 4 physical banks per 64 KB block.
#define SLI_MM_MONITOR_ITCM_BLOCK_SIZE \
  ((size_t)SLI_MM_MONITOR_ITCM_BANK_SIZE * SLI_MM_MONITOR_ITCM_BLOCKS_PER_BANK)
#define SLI_MM_MONITOR_ITCM_LOCAL_BANKS      (8u)        // ITCM-only banks in ITCMRAMRETNCTRL.
#define SLI_MM_MONITOR_ITCM_SHARED_ABS_TOP   (43u)       // Topmost ABS index in DMEM RETNCTRL.
#endif

#endif // SL_CATALOG_BANK_RETENTION_CONTROL_PRESENT

// -----------------------------------------------------------------------------
// Static (file-scope) variables.

/// EM2 retention worst-case snapshot (only updates when retained banks increase).
static sli_memory_monitor_em2_t monitor_em2;

// Power manager subscription objects (only needed when PM is present).
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT) \
  && defined(SL_CATALOG_BANK_RETENTION_CONTROL_PRESENT)

static void on_em_transition(sl_power_manager_em_t from,
                             sl_power_manager_em_t to);

static sl_power_manager_em_transition_event_handle_t em_event_handle;
static const sl_power_manager_em_transition_event_info_t em_event_info = {
  .event_mask = SL_POWER_MANAGER_EVENT_TRANSITION_ENTERING_EM2,
  .on_event   = on_em_transition,
};

#endif

// -----------------------------------------------------------------------------
// Local helper functions.

/***************************************************************************//**
 * Populates a single heap info struct from a heap handle.
 *
 * If @p heap_handle is NULL the output struct is zeroed (heap not present).
 * Only compiled when secondary heaps exist.
 *
 * @param[in]  heap_handle  Heap handle to query, or NULL if not present.
 * @param[out] info         Destination structure to fill.
 ******************************************************************************/
#if defined(SL_CATALOG_MEMORY_MANAGER_DTCM_PRESENT)
static void query_heap(const sl_memory_heap_t *heap_handle,
                       sli_memory_monitor_heap_info_t *info)
{
  if (heap_handle == NULL) {
    memset(info, 0, sizeof(*info));
    return;
  }

  info->present        = true;
  info->total          = sl_memory_heap_get_total_size(heap_handle);
  info->used           = sl_memory_heap_get_used_size(heap_handle);
  info->free           = sl_memory_heap_get_free_size(heap_handle);
  info->high_watermark = sl_memory_heap_get_high_watermark(heap_handle);
}
#endif

/***************************************************************************//**
 * Determine whether the stack resides in DMEM.
 *
 * Compares the stack base address against the SRAM (DMEM) region. If the
 * stack falls outside DMEM it is assumed to be in DTCM. This matters on
 * devices where DMEM, DTCM, and ITCM share one physical RAM block and the
 * linker may place the stack in either region.
 *
 * @return  True if the stack is in DMEM, false otherwise.
 ******************************************************************************/
static inline bool stack_is_in_dmem(void)
{
  sl_memory_region_t stack = sl_memory_get_stack_region();
  uintptr_t addr = (uintptr_t)stack.addr;

  return (addr >= (uintptr_t)SRAM_BASE)
         && (addr < ((uintptr_t)SRAM_BASE + (uintptr_t)SRAM_SIZE));
}

/***************************************************************************//**
 * Determine whether the stack is placed after the DMEM heap.
 *
 * SLI_MEMORY_MONITOR_DMEM_BEFORE_HEAP counts all bytes from SRAM_BASE to
 * the heap start.  On Series 2 the linker places the stack before .bss
 * (i.e. inside that range), so it is already included.  On Series 3 the
 * stack is placed after the heap, so it must be added separately.
 *
 * @return  True if the stack is in DMEM and not already accounted for
 *          by SLI_MEMORY_MONITOR_DMEM_BEFORE_HEAP.
 ******************************************************************************/
static inline bool stack_is_after_dmem_heap(void)
{
  if (!stack_is_in_dmem()) {
    return false;
  }

  sl_memory_region_t stack = sl_memory_get_stack_region();
  uintptr_t heap_start = (uintptr_t)SRAM_BASE
                         + SLI_MEMORY_MONITOR_DMEM_BEFORE_HEAP;

  return (uintptr_t)stack.addr >= heap_start;
}

// -----------------------------------------------------------------------------
// Retention snapshot helpers.
// Available whenever bank retention control exists, so both the automatic
// EM2 callback and sli_memory_monitor_refresh_em2() can use them.
#if defined(SL_CATALOG_BANK_RETENTION_CONTROL_PRESENT)

/***************************************************************************//**
 * Count the number of set bits (popcount) in a 32-bit value.
 *
 * Uses Kernighan's algorithm: loop iterations equal the number of set bits.
 * Preferred over __builtin_popcount which emits a libgcc call on Cortex-M.
 *
 * @param[in] x  Value to count set bits in.
 *
 * @return  Number of bits set to 1.
 ******************************************************************************/
static inline uint32_t popcount32(uint32_t x)
{
  uint32_t count = 0;
  while (x != 0) {
    x &= (x - 1u);
    count++;
  }
  return count;
}

/***************************************************************************//**
 * Compute the DMEM RAM currently in use.
 *
 * Sums the static DMEM footprint (linker-placed data) and the currently
 * used portion of the general-purpose DMEM heap. The stack is added only
 * when it is in DMEM but placed after the heap (not already counted in
 * SLI_MEMORY_MONITOR_DMEM_BEFORE_HEAP).
 *
 * @return  DMEM used size in bytes.
 ******************************************************************************/
static size_t compute_dmem_used(void)
{
  size_t dmem_static = SLI_MEMORY_MONITOR_DMEM_BEFORE_HEAP;
  size_t dmem_heap_used = sl_memory_get_used_heap_size();

  if (stack_is_after_dmem_heap()) {
    sl_memory_region_t stack_region = sl_memory_get_stack_region();
    dmem_static += stack_region.size;
  }

  return dmem_static + dmem_heap_used;
}

/***************************************************************************//**
 * Compute the DTCM RAM currently in use.
 *
 * Sums the fixed data placed in DTCM by the linker (from linker symbols)
 * and the currently used portion of the DTCM heap. The stack is included
 * only if it resides in DTCM.
 *
 * @return  DTCM used size in bytes.
 ******************************************************************************/
#if defined(SL_CATALOG_MEMORY_MANAGER_DTCM_PRESENT)
static size_t compute_dtcm_used(void)
{
  size_t dtcm_static = SLI_MEMORY_MONITOR_DTCM_STATIC;

  if (!stack_is_in_dmem()) {
    sl_memory_region_t stack_region = sl_memory_get_stack_region();
    dtcm_static += stack_region.size;
  }

  const sl_memory_heap_t *dtcm_heap = sl_memory_manager_get_dtcm_heap();
  size_t dtcm_heap_used = (dtcm_heap != NULL)
                          ? sl_memory_heap_get_used_size(dtcm_heap) : 0;

  return dtcm_static + dtcm_heap_used;
}
#endif // SL_CATALOG_MEMORY_MANAGER_DTCM_PRESENT

/***************************************************************************//**
 * Snapshot DMEM retention register state.
 *
 * Reads DMEM->RETNCTRL0 (and RETNCTRL1 if present) to determine how many
 * DMEM banks have retention enabled. On some devices where ITCM shares the DMEM
 * retention controller, ITCM-claimed bits are identified and
 * subtracted so only actual DMEM banks are reported.
 *
 * Uses the linker-based DMEM size for total_size rather than DMEM_NUM_BANKS
 * multiplied by bank_size, because the retention controller may include banks
 * assigned to ITCM and banks may have different sizes (16 KB vs 8 KB).
 *
 * Always populates total_size, total_banks, and used_size. Only updates
 * retained_size and retained_banks if the current count exceeds the
 * previously recorded value (worst-case tracking).
 *
 * @param[in,out] dmem  DMEM retention info to update in place.
 ******************************************************************************/
static void snapshot_dmem_retention(sli_memory_monitor_retention_t *dmem)
{
  size_t dmem_total = SLI_MEMORY_MONITOR_DMEM_SIZE;

  // Inverted logic: bit = 1 means NOT retained.
  uint32_t not_retained_bits_0 = DMEM->RETNCTRL0;

#if defined(_DMEM_RETNCTRL1_MASK) || defined(_HYDRARAM_RETNCTRL1_MASK)
  uint32_t not_retained_bits_1 = DMEM->RETNCTRL1;
#else
  uint32_t not_retained_bits_1 = 0;
#endif

  uint32_t retnctrl_total = DMEM_NUM_BANKS;
  uint32_t not_retained_count = popcount32(not_retained_bits_0)
                                + popcount32(not_retained_bits_1);

  if (not_retained_count > retnctrl_total) {
    not_retained_count = retnctrl_total;
  }

  uint32_t retained_count = retnctrl_total - not_retained_count;

#if defined(SL_CATALOG_MEMORY_MANAGER_ITCM_PRESENT)
  // ITCM shared banks appear as "retained" in DMEM RETNCTRL because the HAL
  // only disables retention for unused ITCM banks. Subtract the ITCM-shared
  // bits that are still set to 0 (retained) in RETNCTRL from the DMEM count.
  {
    uint32_t itcm_blocks = SYSCFG->ITCMNUMSRAMBLK
                           & _SYSCFG_ITCMNUMSRAMBLK_ITCMNUMSRAMBLK_MASK;
    uint32_t itcm_physical = itcm_blocks * SLI_MM_MONITOR_ITCM_BLOCKS_PER_BANK;
    uint32_t itcm_shared = (itcm_physical > SLI_MM_MONITOR_ITCM_LOCAL_BANKS)
                           ? (itcm_physical - SLI_MM_MONITOR_ITCM_LOCAL_BANKS) : 0u;

    if (itcm_shared > 0u) {
      uint32_t abs_start = SLI_MM_MONITOR_ITCM_SHARED_ABS_TOP + 1u - itcm_shared;
      uint32_t itcm_mask0 = 0u;
      uint32_t itcm_mask1 = 0u;

      for (uint32_t i = abs_start; i <= SLI_MM_MONITOR_ITCM_SHARED_ABS_TOP; i++) {
        if (i < 32u) {
          itcm_mask0 |= (1u << i);
        } else {
          itcm_mask1 |= (1u << (i - 32u));
        }
      }

      uint32_t itcm_not_retained = popcount32(not_retained_bits_0 & itcm_mask0)
                                   + popcount32(not_retained_bits_1 & itcm_mask1);
      uint32_t itcm_retained_in_dmem = itcm_shared - itcm_not_retained;

      if (retained_count > itcm_retained_in_dmem) {
        retained_count -= itcm_retained_in_dmem;
      } else {
        retained_count = 0u;
      }
    }
  }
#endif

  // Compute actual DMEM bank count from the linker-based size.
  size_t large_total = (dmem_total > SLI_MM_MONITOR_DMEM_SMALL_TOTAL)
                       ? (dmem_total - SLI_MM_MONITOR_DMEM_SMALL_TOTAL) : dmem_total;
  uint32_t large_banks = (uint32_t)(large_total / (size_t)DMEM_BANK0_SIZE);
  uint32_t small_banks = (dmem_total > SLI_MM_MONITOR_DMEM_SMALL_TOTAL)
                         ? SLI_MM_MONITOR_DMEM_SMALL_BANK_COUNT : 0u;
  uint32_t actual_banks = large_banks + small_banks;

  if (retained_count > actual_banks) {
    retained_count = actual_banks;
  }

  // Compute retained_size accounting for mixed bank sizes.
  size_t retained_size;

  if (retained_count <= large_banks) {
    retained_size = (size_t)retained_count * (size_t)DMEM_BANK0_SIZE;
  } else {
    uint32_t small_retained = retained_count - large_banks;
    retained_size = large_total
                    + (size_t)small_retained * SLI_MM_MONITOR_DMEM_SMALL_BANK_SIZE;
  }

  // Always populate informational fields.
  dmem->total_size  = dmem_total;
  dmem->total_banks = actual_banks;
  dmem->used_size   = compute_dmem_used();

  // Worst-case: only update retained fields if more banks are retained.
  if (retained_count > dmem->retained_banks) {
    dmem->retained_size  = retained_size;
    dmem->retained_banks = retained_count;
  }
}

/***************************************************************************//**
 * Snapshot DTCM retention register state.
 *
 * Reads SYSCFG->DTCMRAMRETNCTRL to determine how many DTCM banks have
 * retention enabled. Always populates total_size, total_banks, and
 * used_size. Only updates retained_size and retained_banks if the current
 * count exceeds the previously recorded value (worst-case tracking).
 *
 * @param[in,out] dtcm  DTCM retention info to update in place.
 ******************************************************************************/
#if defined(SL_CATALOG_MEMORY_MANAGER_DTCM_PRESENT)
static void snapshot_dtcm_retention(sli_memory_monitor_retention_t *dtcm)
{
  uint32_t total_banks = SLI_MM_MONITOR_DTCM_NUM_BANK;

  // Same inverted logic: bit = 0 means retained.
  uint32_t not_retained_bits = SYSCFG->DTCMRAMRETNCTRL;

#if defined(_SYSCFG_DTCMRAMRETNCTRL_MASK)
  not_retained_bits &= _SYSCFG_DTCMRAMRETNCTRL_MASK;
#endif

  uint32_t not_retained_count = popcount32(not_retained_bits);
  if (not_retained_count > total_banks) {
    not_retained_count = total_banks;
  }

  uint32_t retained_count = total_banks - not_retained_count;

  // Always populate informational fields.
  dtcm->total_size = (size_t)total_banks * (size_t)SLI_MM_MONITOR_DTCM_BANK_SIZE;
  dtcm->total_banks = total_banks;
  dtcm->used_size = compute_dtcm_used();

  // Worst-case: only update retained fields if more banks are retained.
  if (retained_count > dtcm->retained_banks) {
    dtcm->retained_size  = (size_t)retained_count * (size_t)SLI_MM_MONITOR_DTCM_BANK_SIZE;
    dtcm->retained_banks = retained_count;
  }
}
#endif // SL_CATALOG_MEMORY_MANAGER_DTCM_PRESENT

/***************************************************************************//**
 * Snapshot ITCM retention state.
 *
 * Reads the runtime ITCM configuration register (SYSCFG->ITCMNUMSRAMBLK)
 * for the configured ITCM size and the retention register
 * (SYSCFG->ITCMRAMRETNCTRL) for the actual hardware retention state.
 *
 * ITCMRAMRETNCTRL has one bit per 16 KB physical bank and covers only the
 * ITCM-local banks (up to 8 banks = 128 KB). Any ITCM banks
 * beyond the local range are shared with DMEM and tracked by the DMEM
 * retention controller; those are handled by snapshot_dmem_retention().
 *
 * @param[in,out] itcm  ITCM retention info to populate.
 ******************************************************************************/
#if defined(SL_CATALOG_MEMORY_MANAGER_ITCM_PRESENT)
static void snapshot_itcm_retention(sli_memory_monitor_retention_t *itcm)
{
  uint32_t itcm_blocks = SYSCFG->ITCMNUMSRAMBLK
                         & _SYSCFG_ITCMNUMSRAMBLK_ITCMNUMSRAMBLK_MASK;
  size_t itcm_total = (size_t)itcm_blocks * SLI_MM_MONITOR_ITCM_BLOCK_SIZE;
  size_t itcm_used  = SLI_MEMORY_MONITOR_ITCM_USED;

  // Configured physical banks, capped to the ITCM-local range covered by
  // ITCMRAMRETNCTRL.  Shared banks beyond this are in DMEM RETNCTRL.
  uint32_t itcm_physical = itcm_blocks * SLI_MM_MONITOR_ITCM_BLOCKS_PER_BANK;
  uint32_t local_count = (itcm_physical < SLI_MM_MONITOR_ITCM_LOCAL_BANKS)
                         ? itcm_physical : SLI_MM_MONITOR_ITCM_LOCAL_BANKS;

  // Read ITCM retention register (inverted: bit = 1 means NOT retained).
  uint32_t not_retained_bits = SYSCFG->ITCMRAMRETNCTRL;

#if defined(_SYSCFG_ITCMRAMRETNCTRL_MASK)
  not_retained_bits &= _SYSCFG_ITCMRAMRETNCTRL_MASK;
#endif

  uint32_t not_retained_count = popcount32(not_retained_bits);

  if (not_retained_count > local_count) {
    not_retained_count = local_count;
  }

  uint32_t retained_banks = local_count - not_retained_count;

  itcm->total_size     = itcm_total;
  itcm->used_size      = itcm_used;
  itcm->total_banks    = local_count;
  itcm->retained_banks = retained_banks;
  itcm->retained_size  = (size_t)retained_banks * (size_t)SLI_MM_MONITOR_ITCM_BANK_SIZE;
}
#endif // SL_CATALOG_MEMORY_MANAGER_ITCM_PRESENT

/***************************************************************************//**
 * Capture full EM2 retention worst-case snapshot.
 *
 * Reads DMEM, DTCM, and ITCM retention state and updates the snapshot.
 * For DMEM and DTCM: total_size, total_banks, and used_size are always
 * refreshed, while retained_size and retained_banks are only updated when
 * the current retained bank count exceeds the previously recorded value
 * (worst-case tracking). ITCM is always fully populated (its retention
 * is static). Sets @p em2->valid to true.
 *
 * @param[in,out] em2  Snapshot structure to update in place.
 ******************************************************************************/
static void capture_em2_snapshot(sli_memory_monitor_em2_t *em2)
{
  em2->valid = true;

  snapshot_dmem_retention(&em2->dmem);

#if defined(SL_CATALOG_MEMORY_MANAGER_DTCM_PRESENT)
  snapshot_dtcm_retention(&em2->dtcm);
#else
  if (!em2->valid || em2->dtcm.total_banks == 0) {
    memset(&em2->dtcm, 0, sizeof(em2->dtcm));
  }
#endif

#if defined(SL_CATALOG_MEMORY_MANAGER_ITCM_PRESENT)
  snapshot_itcm_retention(&em2->itcm);
#else
  memset(&em2->itcm, 0, sizeof(em2->itcm));
#endif
}

#endif // SL_CATALOG_BANK_RETENTION_CONTROL_PRESENT

// -----------------------------------------------------------------------------
// Power Manager EM transition callback.

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT) \
  && defined(SL_CATALOG_BANK_RETENTION_CONTROL_PRESENT)

/***************************************************************************//**
 * Called by the Power Manager on energy mode transitions.
 *
 * When the target mode is EM2, captures a worst-case retention snapshot
 * at the actual moment of sleep entry.
 *
 * @param[in] from  Energy mode being left (unused).
 * @param[in] to    Energy mode being entered.
 ******************************************************************************/
static void on_em_transition(sl_power_manager_em_t from,
                             sl_power_manager_em_t to)
{
  (void)from;

  if (to == SL_POWER_MANAGER_EM2) {
    capture_em2_snapshot(&monitor_em2);
  }
}

#endif

// -----------------------------------------------------------------------------
// Public API.

/***************************************************************************//**
 * Initializes the Memory Manager Monitor.
 ******************************************************************************/
void sli_memory_monitor_init(void)
{
  // Initialize EM2 snapshot as invalid (no capture yet).
  memset(&monitor_em2, 0, sizeof(monitor_em2));

  // Subscribe to EM2 entry events for automatic worst-case snapshots.
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT) \
  && defined(SL_CATALOG_BANK_RETENTION_CONTROL_PRESENT)
  sl_power_manager_subscribe_em_transition_event(&em_event_handle,
                                                 &em_event_info);
#endif
}

/***************************************************************************//**
 * Retrieves per-heap dynamic RAM usage.
 ******************************************************************************/
void sli_memory_monitor_get_dynamic(sli_memory_monitor_dynamic_t *info)
{
  EFM_ASSERT(info != NULL);

  // General-purpose heap (always present).
  info->general.present        = true;
  info->general.total          = sl_memory_get_total_heap_size();
  info->general.used           = sl_memory_get_used_heap_size();
  info->general.free           = sl_memory_get_free_heap_size();
  info->general.high_watermark = sl_memory_get_heap_high_watermark();

  // DTCM heap.
#if defined(SL_CATALOG_MEMORY_MANAGER_DTCM_PRESENT)
  query_heap(sl_memory_manager_get_dtcm_heap(), &info->dtcm);
#else
  memset(&info->dtcm, 0, sizeof(info->dtcm));
#endif
}

/***************************************************************************//**
 * Returns the total in-use RAM that would need retention in EM2.
 ******************************************************************************/
size_t sli_memory_monitor_get_retained_ram(void)
{
  size_t total = SLI_MEMORY_MONITOR_DMEM_BEFORE_HEAP
                 + sl_memory_get_used_heap_size();

  if (stack_is_after_dmem_heap()) {
    sl_memory_region_t stack_region = sl_memory_get_stack_region();
    total += stack_region.size;
  }

#if defined(SL_CATALOG_MEMORY_MANAGER_DTCM_PRESENT)
  size_t dtcm_static = SLI_MEMORY_MONITOR_DTCM_STATIC;

  if (!stack_is_in_dmem()) {
    sl_memory_region_t stack_region = sl_memory_get_stack_region();
    dtcm_static += stack_region.size;
  }

  const sl_memory_heap_t *dtcm_heap = sl_memory_manager_get_dtcm_heap();
  size_t dtcm_used = (dtcm_heap != NULL)
                     ? sl_memory_heap_get_used_size(dtcm_heap) : 0;
  total += dtcm_static + dtcm_used;
#endif

  total += SLI_MEMORY_MONITOR_ITCM_USED;

  return total;
}

/***************************************************************************//**
 * Retrieves the worst-case EM2 retention snapshot.
 ******************************************************************************/
void sli_memory_monitor_get_em2_snapshot(sli_memory_monitor_em2_t *info)
{
  EFM_ASSERT(info != NULL);
  *info = monitor_em2;
}

/***************************************************************************//**
 * Captures a retention snapshot from the current hardware register state.
 ******************************************************************************/
void sli_memory_monitor_refresh_em2(void)
{
#if defined(SL_CATALOG_BANK_RETENTION_CONTROL_PRESENT)
  capture_em2_snapshot(&monitor_em2);
#endif
}
