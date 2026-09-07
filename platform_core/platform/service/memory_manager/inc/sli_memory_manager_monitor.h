/***************************************************************************//**
 * @file
 * @brief Memory Manager Monitor - RAM Usage Query API.
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

#ifndef SLI_MEMORY_MANAGER_MONITOR_H_
#define SLI_MEMORY_MANAGER_MONITOR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "em_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************//**
 * @addtogroup memory_manager Memory Manager
 * @{
 *****************************************************************************/

/**************************************************************************//**
 * @addtogroup memory_manager_monitor Memory Manager Monitor
 *
 * @details
 * ## Overview
 *
 * The Memory Manager Monitor provides:
 *
 * - **Static RAM macros**: Compile-time macros derived from linker symbols
 *   for .data, .bss, ITCM, DTCM, and DMEM region sizes.
 *
 * - **Dynamic RAM**: Per-heap usage statistics queried live from the
 *   Memory Manager (general-purpose DMEM and DTCM heaps).
 *
 * - **Retained RAM**: Instant query of total RAM that would be retained
 *   should the device enter EM2 right now.
 *
 * - **EM2 Worst-Case Snapshot**: Tracks the maximum (worst-case) retention
 *   state seen across EM2 entries or explicit refresh calls, including
 *   both hardware-level retention and actual RAM in use.
 *
 * ## Usage
 *
 * @code
 * // Static RAM from macros (no function call needed):
 * size_t data = SLI_MEMORY_MONITOR_DATA_SIZE;
 * size_t bss  = SLI_MEMORY_MONITOR_BSS_SIZE;
 *
 * // Dynamic heap usage:
 * sli_memory_monitor_dynamic_t dyn;
 * sli_memory_monitor_get_dynamic(&dyn);
 *
 * // Instant retained RAM query:
 * size_t retained_now = sli_memory_monitor_get_retained_ram();
 *
 * // Worst-case EM2 snapshot:
 * sli_memory_monitor_refresh_em2();
 * sli_memory_monitor_em2_t snap;
 * sli_memory_monitor_get_em2_snapshot(&snap);
 * @endcode
 *
 * @{
 *****************************************************************************/

// ----------------------------------------------------------------------------
// STATIC RAM MACROS
//
// These expand to expressions derived from linker symbols. They can be used
// in any C expression at runtime but are NOT compile-time constants (they
// reference extern symbols resolved at link time).
//
// On some devices where DMEM, DTCM, and ITCM share one physical RAM block,
// the region sizes reflect the configured partition.
// Their sum equals the total physical RAM:
//   SLI_MEMORY_MONITOR_DMEM_SIZE + SLI_MEMORY_MONITOR_DTCM_TOTAL
//                                + SLI_MEMORY_MONITOR_ITCM_TOTAL

#if defined(__GNUC__)

extern char __data_start__[];
extern char __data_end__[];
extern char __bss_start__[];
extern char __bss_end__[];
extern char __HeapBase[];
extern char __dmem_size__[] __attribute__((weak));
extern char __itcm_used__[] __attribute__((weak));
extern char __itcm_size__[] __attribute__((weak));
extern char __dtcm_size__[] __attribute__((weak));
extern char __vma_dtcm_start__[] __attribute__((weak));
extern char __vma_dtcm_end__[] __attribute__((weak));

/// Size of .data section in DMEM (initialized global/static variables).
#define SLI_MEMORY_MONITOR_DATA_SIZE \
  ((size_t)(__data_end__ - __data_start__))

/// Size of .bss section in DMEM (zero-initialized global/static variables).
#define SLI_MEMORY_MONITOR_BSS_SIZE \
  ((size_t)(__bss_end__ - __bss_start__))

/// Static DMEM from RAM base to heap start.
/// On Series 2 this range includes the stack (placed before .bss).
/// On Series 3 the stack is after the heap and must be added separately.
#define SLI_MEMORY_MONITOR_DMEM_BEFORE_HEAP \
  ((size_t)((uintptr_t)__HeapBase - (uintptr_t)SRAM_BASE))

/// Total configured DMEM region size. Falls back to SRAM_SIZE on Series 2.
#define SLI_MEMORY_MONITOR_DMEM_SIZE \
  ((&__dmem_size__ != NULL) ? (size_t)__dmem_size__ : (size_t)SRAM_SIZE)

/// Code + vector table placed in ITCM. Zero if no ITCM.
#define SLI_MEMORY_MONITOR_ITCM_USED \
  ((&__itcm_used__ != NULL) ? (size_t)__itcm_used__ : 0)

/// Total configured ITCM region size. Zero if no ITCM.
#define SLI_MEMORY_MONITOR_ITCM_TOTAL \
  ((&__itcm_size__ != NULL) ? (size_t)__itcm_size__ : 0)

/// Total configured DTCM region size. Zero if no DTCM.
#define SLI_MEMORY_MONITOR_DTCM_TOTAL \
  ((&__dtcm_size__ != NULL) ? (size_t)__dtcm_size__ : 0)

/// Fixed data placed in DTCM by the linker (.ram_dtcm / .dtcm section).
#define SLI_MEMORY_MONITOR_DTCM_STATIC                        \
  ((&__vma_dtcm_start__ != NULL && &__vma_dtcm_end__ != NULL) \
   ? (size_t)(__vma_dtcm_end__ - __vma_dtcm_start__) : 0)

#elif defined(__ICCARM__)

#pragma section = ".data"
#pragma section = ".bss"
#pragma section = "MEMORY_MANAGER_HEAP"
#pragma section = ".dtcm_data"
#pragma section = "application_itcm"

#define SLI_MEMORY_MONITOR_DATA_SIZE \
  (__section_size(".data"))

#define SLI_MEMORY_MONITOR_BSS_SIZE \
  (__section_size(".bss"))

#define SLI_MEMORY_MONITOR_DMEM_BEFORE_HEAP \
  ((size_t)((uintptr_t)__section_begin("MEMORY_MANAGER_HEAP") - (uintptr_t)SRAM_BASE))

#if defined(ITCM_BASE) && defined(DTCM_BASE)
extern uint32_t __dmem_size__;
extern uint32_t __itcm_size__;
extern uint32_t __dtcm_size__;

#define SLI_MEMORY_MONITOR_DMEM_SIZE      ((size_t)&__dmem_size__)
#define SLI_MEMORY_MONITOR_ITCM_USED      (__section_size("application_itcm"))
#define SLI_MEMORY_MONITOR_ITCM_TOTAL     ((size_t)&__itcm_size__)
#define SLI_MEMORY_MONITOR_DTCM_TOTAL     ((size_t)&__dtcm_size__)
#else
#define SLI_MEMORY_MONITOR_DMEM_SIZE      ((size_t)SRAM_SIZE)
#define SLI_MEMORY_MONITOR_ITCM_USED      (0)
#define SLI_MEMORY_MONITOR_ITCM_TOTAL     (0)
#define SLI_MEMORY_MONITOR_DTCM_TOTAL     (0)
#endif
#define SLI_MEMORY_MONITOR_DTCM_STATIC    (__section_size(".dtcm_data"))

#else

#define SLI_MEMORY_MONITOR_DATA_SIZE          (0)
#define SLI_MEMORY_MONITOR_BSS_SIZE           (0)
#define SLI_MEMORY_MONITOR_DMEM_BEFORE_HEAP   (0)
#define SLI_MEMORY_MONITOR_DMEM_SIZE          ((size_t)SRAM_SIZE)
#define SLI_MEMORY_MONITOR_ITCM_USED          (0)
#define SLI_MEMORY_MONITOR_ITCM_TOTAL         (0)
#define SLI_MEMORY_MONITOR_DTCM_TOTAL         (0)
#define SLI_MEMORY_MONITOR_DTCM_STATIC        (0)

#endif

// ----------------------------------------------------------------------------
// DATA TYPES

/// @brief Single heap usage information.
///
/// Represents the state of one memory manager heap (general-purpose
/// or DTCM). The @ref present field indicates whether this heap
/// exists on the current device.
typedef struct {
  bool   present;        ///< True if this heap exists on this device.
  size_t total;          ///< Total heap size in bytes.
  size_t used;           ///< Currently used heap in bytes.
  size_t free;           ///< Currently free heap in bytes.
  size_t high_watermark; ///< Peak heap usage (high watermark) in bytes.
} sli_memory_monitor_heap_info_t;

/// @brief Dynamic RAM usage covering all heaps.
///
/// Queried live from the memory manager's existing statistics APIs.
/// Each heap is reported individually; check the @ref present field
/// to determine if a heap exists on the current device.
typedef struct {
  sli_memory_monitor_heap_info_t general;  ///< General-purpose heap (DMEM). Always present.
  sli_memory_monitor_heap_info_t dtcm;     ///< DTCM heap. Present on devices with tightly-coupled data memory.
} sli_memory_monitor_dynamic_t;

/// @brief Retention info for a single memory region (DMEM or DTCM).
typedef struct {
  size_t   retained_size;   ///< RAM retained by hardware in EM2 (bank-level granularity).
  size_t   used_size;       ///< RAM in use at snapshot time (static + used heap).
  size_t   total_size;      ///< Total region size in bytes.
  uint32_t retained_banks;  ///< Number of banks with retention enabled.
  uint32_t total_banks;     ///< Total number of banks in this region.
} sli_memory_monitor_retention_t;

/// @brief EM2 retention worst-case snapshot.
///
/// Tracks the worst case (maximum retained banks) observed across all EM2
/// entries and explicit refresh calls. Only updates when the current
/// retained bank count exceeds the stored value.
///
/// Use @ref sli_memory_monitor_get_retained_ram() for an instant query of
/// the current retention state without worst-case tracking.
typedef struct {
  bool valid;                          ///< True once at least one snapshot has been captured.
  sli_memory_monitor_retention_t dmem;  ///< DMEM retention info (worst case).
  sli_memory_monitor_retention_t dtcm;  ///< DTCM retention info (worst case). Zeros if no DTCM.
  sli_memory_monitor_retention_t itcm;  ///< ITCM retention info. Zeros if no ITCM.
} sli_memory_monitor_em2_t;

// ----------------------------------------------------------------------------
// PROTOTYPES

/***************************************************************************//**
 * Initializes the Memory Manager Monitor.
 *
 * Subscribes to the Power Manager's EM2 entry event for automatic
 * worst-case retention snapshots (if Power Manager is present).
 *
 * @note Called automatically during service_init. Do not call directly.
 ******************************************************************************/
void sli_memory_monitor_init(void);

/***************************************************************************//**
 * Retrieves per-heap dynamic RAM usage.
 *
 * Queries the memory manager's existing statistics APIs to populate the
 * result. The returned data reflects the heap state at the time of the call.
 *
 * @param[out] info  Pointer to a dynamic RAM structure to fill.
 *                   Must not be NULL.
 ******************************************************************************/
void sli_memory_monitor_get_dynamic(sli_memory_monitor_dynamic_t *info);

/***************************************************************************//**
 * Returns the total RAM in use that would need retention in EM2.
 *
 * Computes the sum of static allocations (linker-placed data) and heap
 * usage across DMEM, DTCM, and ITCM (if present). This is a pure software
 * query based on linker symbols and heap statistics -- it does not read
 * hardware retention registers.
 *
 * The returned value represents the minimum RAM that must survive EM2 for
 * the firmware to resume correctly. On devices with bank retention control,
 * the actual hardware retention may be larger due to bank granularity
 * (see @ref sli_memory_monitor_get_em2_snapshot for bank-level detail).
 *
 * @note On devices where the memory manager does not yet implement bank
 *       retention control, all SRAM is typically retained in EM2. This
 *       function still returns only the in-use portion, not the full
 *       SRAM size.
 *
 * @return  Total in-use RAM in bytes (static + heap used, DMEM + DTCM + ITCM).
 ******************************************************************************/
size_t sli_memory_monitor_get_retained_ram(void);

/***************************************************************************//**
 * Retrieves the worst-case EM2 retention snapshot.
 *
 * Returns the snapshot with the highest retained bank count observed so
 * far, captured either automatically at EM2 entry or by an explicit call
 * to @ref sli_memory_monitor_refresh_em2(). If no snapshot has been taken
 * yet, @ref valid is false.
 *
 * @param[out] info  Pointer to an EM2 snapshot structure to fill.
 *                   Must not be NULL.
 ******************************************************************************/
void sli_memory_monitor_get_em2_snapshot(sli_memory_monitor_em2_t *info);

/***************************************************************************//**
 * Captures a retention snapshot from the current hardware register state.
 *
 * Reads the DMEM and DTCM retention control registers. Updates the stored
 * worst-case snapshot only if the current retained bank count exceeds what
 * was previously recorded.
 *
 * On devices without bank retention control this is a no-op.
 ******************************************************************************/
void sli_memory_monitor_refresh_em2(void);

/** @} end addtogroup memory_manager_monitor */
/** @} end addtogroup memory_manager */

#ifdef __cplusplus
}
#endif

#endif /* SLI_MEMORY_MANAGER_MONITOR_H_ */
