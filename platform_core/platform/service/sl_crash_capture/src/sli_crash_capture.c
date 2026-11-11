/***************************************************************************/ /**
 * @file sli_crash_capture.c
 * @brief Crash Capture: CPU Crash provider and Tier-0 hooks
 * @version 2.0.0
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

#include "sl_common.h"
#include "sli_crash_capture.h"
#include "sli_diag.h"

#if !defined(SL_CATALOG_SL_DIAG_PRESENT) || !SL_DIAG_ENABLE
#error "Crash Capture requires sl_diag with SL_DIAG_PROFILE != SL_DIAG_PROFILE_OFF"
#endif

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if defined(__ARM_ARCH) || defined(__CORTEX_M)
#include "em_device.h"
#include "sl_compiler.h"
#endif
#if defined(__clang__)
#include "cmsis_clang.h"
#elif defined(__GNUC__) && !defined(__ICCARM__) && !defined(__IAR_SYSTEMS_ICC__)
#include "cmsis_gcc.h"
#elif defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
#include "cmsis_iccarm.h"
#endif

#if defined(WDOG_COUNT) && (WDOG_COUNT > 0) && defined(WDOG_IF_WARN)
#include "sl_hal_wdog.h"
#endif

#if defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
#include "sl_log_helper.h"
#endif

#include "sl_assert.h"

/*******************************************************************************
 ***************************   LOCAL DEFINES   *********************************
 ******************************************************************************/

#define SLI_CRASH_CPU_TLV_VERSION       1u /**< Payload version of the CPU crash TLV. */

/** Reason IDs stored in the record. 0x0901-0x0907 are SL Diagnostic; watchdog is 0x0801. */
#define SLI_CRASH_REASON_HARDFAULT      0x0901u /**< HardFault. detail is CFSR. */
#define SLI_CRASH_REASON_MEMFAULT       0x0902u /**< MemManage fault. detail is CFSR. */
#define SLI_CRASH_REASON_BUSFAULT       0x0903u /**< BusFault. detail is CFSR. */
#define SLI_CRASH_REASON_USAGEFAULT     0x0904u /**< UsageFault. detail is CFSR. */
#define SLI_CRASH_REASON_STACKOVERFLOW  0x0905u /**< Stack overflow. detail is CFSR. */
#define SLI_CRASH_REASON_MPUFAULT       0x0906u /**< MPU fault. detail is CFSR. */
#define SLI_CRASH_REASON_ASSERT         0x0907u /**< Assert. detail is the source line. */
#define SLI_CRASH_REASON_WATCHDOG       0x0801u /**< Same as SL_DIAG_REASON_WDOG_WARNING. detail is WDOG flags. */

/** EXC_RETURN and xPSR bits that select the stacked frame layout. Defined
 *  locally because ARMv7-M CMSIS core headers do not provide them. */
#define SLI_CRASH_EXC_RETURN_FTYPE      (1UL << 4)  /**< 0: FP context stacked */
#define SLI_CRASH_EXC_RETURN_DCRS       (1UL << 5)  /**< 0: additional state context stacked */
#define SLI_CRASH_EXC_RETURN_S          (1UL << 6)  /**< 1: frame is on Secure stack */
#define SLI_CRASH_EXC_RETURN_ES         (1UL << 0)  /**< 1: exception targets Secure state */
#define SLI_CRASH_XPSR_FRAME_ALIGN      (1UL << 9)  /**< 1: one padding word stacked */

/** Stacked frame sizes in words. */
#define SLI_CRASH_BASIC_FRAME_WORDS     8u   /**< R0-R3, R12, LR, PC, xPSR */
#define SLI_CRASH_FP_CONTEXT_WORDS      18u  /**< S0-S15, FPSCR, VPR/reserved */
#define SLI_CRASH_ADDITIONAL_STATE_WORDS 10u /**< Integrity signature, reserved, R4-R11 */
#define SLI_CRASH_ADDITIONAL_FP_CONTEXT_WORDS 16u /**< S16-S31 */

#if (SL_DIAG_STACK_SNAPSHOT_BYTES > 0u)
#if defined(__GNUC__) || defined(__clang__)
extern uint32_t __StackLimit;
extern uint32_t __StackTop;
#elif defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
#pragma section = "CSTACK"
#endif
extern uint32_t __ram_end__;
#endif

/*******************************************************************************
 ***************************   LOCAL VARIABLES   *******************************
 ******************************************************************************/

static sli_diag_cpu_crash_scratch_t s_fault_scratch; /**< Static so a fault on an exhausted stack needs no more stack. */

#if defined(__ARM_ARCH) || defined(__CORTEX_M)
static volatile uint8_t s_in_fault; /**< Set while fault_handler_c runs, to stop a nested fault. */
#endif

/*******************************************************************************
 ***************************   LOCAL HELPERS   *********************************
 ******************************************************************************/

static void crash_map_request(sli_crash_type_t type,
                              const sli_diag_cpu_crash_scratch_t *scratch,
                              sli_diag_capture_request_t *req)
{
  (void)memset(req, 0, sizeof(*req));
  req->depth = SLI_DIAG_CAPTURE_DEPTH_NORMAL;
  req->trigger = SL_DIAG_TRIGGER_HARD_FAULT;
  req->detail = scratch->cfsr;

  switch (type) {
    case SLI_CRASH_TYPE_MEMFAULT:
      req->reason_id = SLI_CRASH_REASON_MEMFAULT;
      break;
    case SLI_CRASH_TYPE_BUSFAULT:
      req->reason_id = SLI_CRASH_REASON_BUSFAULT;
      break;
    case SLI_CRASH_TYPE_USAGEFAULT:
      req->reason_id = SLI_CRASH_REASON_USAGEFAULT;
      break;
    case SLI_CRASH_TYPE_STACKOVERFLOW:
      req->reason_id = SLI_CRASH_REASON_STACKOVERFLOW;
      break;
    case SLI_CRASH_TYPE_MPUFAULT:
      req->reason_id = SLI_CRASH_REASON_MPUFAULT;
      break;
    case SLI_CRASH_TYPE_ASSERT:
      req->trigger = SL_DIAG_TRIGGER_ASSERT;
      req->reason_id = SLI_CRASH_REASON_ASSERT;
      req->detail = scratch->line;
      break;
    case SLI_CRASH_TYPE_WATCHDOG:
      req->trigger = SL_DIAG_TRIGGER_WATCHDOG_WARNING;
      req->reason_id = SLI_CRASH_REASON_WATCHDOG;
      req->detail = scratch->r0;
      req->depth = SLI_DIAG_CAPTURE_DEPTH_MINIMAL;
      break;
    default:
      req->reason_id = SLI_CRASH_REASON_HARDFAULT;
      break;
  }
}

static sl_status_t crash_seal(sli_crash_type_t type,
                              const sli_diag_cpu_crash_scratch_t *scratch) /**< fault_seal writes the tail first. */
{
  sli_diag_capture_request_t req;

  crash_map_request(type, scratch, &req);
  return sli_diag_fault_seal(&req, scratch);
}

static uint32_t hash_string(const char *s)
{
  uint32_t hash = 2166136261u;
  if (s == NULL) {
    return 0u;
  }
  for (const char *p = s; *p != '\0'; p++) {
    hash ^= (uint32_t)(uint8_t)*p;
    hash *= 16777619u;
  }
  return hash;
}

/*******************************************************************************
 ***************************   CPU CRASH PROVIDER   ****************************
 ******************************************************************************/

static sl_diag_provider_status_t crash_cpu_provider_capture(
  const sli_diag_capture_context_t *ctx,
  sli_diag_writer_t *writer,
  void *provider_state)
{
  const sli_diag_cpu_crash_scratch_t *scratch;
  uint16_t len;

  (void)provider_state;

  if ((ctx == NULL) || (writer == NULL)) {
    return SL_DIAG_PROVIDER_STATUS_FAILED;
  }

  scratch = (const sli_diag_cpu_crash_scratch_t *)ctx->scratch;
  if (scratch == NULL) {
    return SL_DIAG_PROVIDER_STATUS_UNAVAILABLE;
  }

  len = (uint16_t)sizeof(*scratch);
  if (sli_diag_writer_bytes_remaining(writer) < (sizeof(sl_diag_tlv_header_t) + len)) {
    return SL_DIAG_PROVIDER_STATUS_TRUNCATED;
  }

  if (sli_diag_writer_append_tlv(writer,
                                 SL_DIAG_PROVIDER_CPU_CRASH,
                                 SLI_CRASH_CPU_TLV_VERSION,
                                 scratch,
                                 len) != SL_STATUS_OK) {
    return SL_DIAG_PROVIDER_STATUS_FAILED;
  }
  return SL_DIAG_PROVIDER_STATUS_OK;
}

static const sli_diag_provider_t s_cpu_crash_provider = {
  .id = SL_DIAG_PROVIDER_CPU_CRASH,
  .version = SLI_CRASH_CPU_TLV_VERSION,
  .dependency_count = 0u,
  .trigger_mask = SLI_DIAG_TRIGGER_MASK(SL_DIAG_TRIGGER_HARD_FAULT)
                  | SLI_DIAG_TRIGGER_MASK(SL_DIAG_TRIGGER_ASSERT)
                  | SLI_DIAG_TRIGGER_MASK(SL_DIAG_TRIGGER_WATCHDOG_WARNING),
  .context_flags = SLI_DIAG_CONTEXT_TASK | SLI_DIAG_CONTEXT_ISR
                   | SLI_DIAG_CONTEXT_FAULT | SLI_DIAG_CONTEXT_PRE_RESET,
  .privacy_class = SLI_DIAG_PROVIDER_CPU_CRASH_PRIVACY_CLASS,
  .max_latency_us = 200u,
  .max_output_bytes = (uint16_t)SLI_DIAG_CPU_CRASH_MAX_ENCODED_BYTES,
  .dependencies = NULL,
  .capture = crash_cpu_provider_capture,
  .state = NULL,
};

/*******************************************************************************
 ***************************   PUBLIC APIs   ***********************************
 ******************************************************************************/

void sli_crash_capture_init(void)
{
#if defined(SCB_SHCSR_MEMFAULTENA_Msk)
  SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
#endif
#if defined(SCB_SHCSR_BUSFAULTENA_Msk)
  SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk;
#endif

  (void)sli_diag_register_provider(&s_cpu_crash_provider);
}

/*******************************************************************************
 ***************************   TIER-0 HELPERS   ********************************
 ******************************************************************************/

#if defined(__ARM_ARCH) || defined(__CORTEX_M)
static inline bool is_debugger_connected(void) /**< True when DHCSR.C_DEBUGEN is set. */
{
  return (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0u;
}
#endif

static sli_crash_type_t resolve_crash_type(sli_crash_type_t crash_type) /**< Stack overflow or MPU fault from CFSR. */
{
#if defined(SCB_CFSR_MSTKERR_Msk) && defined(SCB_CFSR_STKERR_Msk)
  if ((crash_type == SLI_CRASH_TYPE_MEMFAULT)
      || (crash_type == SLI_CRASH_TYPE_BUSFAULT)
      || (crash_type == SLI_CRASH_TYPE_HARDFAULT)) {
    uint32_t stack_err = (SCB_CFSR_MSTKERR_Msk | SCB_CFSR_MUNSTKERR_Msk
                          | SCB_CFSR_STKERR_Msk | SCB_CFSR_UNSTKERR_Msk);
    if ((SCB->CFSR & stack_err) != 0U) {
      return SLI_CRASH_TYPE_STACKOVERFLOW;
    }
  }
#endif
#if defined(SCB_CFSR_STKOF_Msk)
  if ((crash_type == SLI_CRASH_TYPE_USAGEFAULT)
      && ((SCB->CFSR & SCB_CFSR_STKOF_Msk) != 0U)) {
    return SLI_CRASH_TYPE_STACKOVERFLOW;
  }
#endif
#if defined(SCB_CFSR_IACCVIOL_Msk) && defined(SCB_CFSR_DACCVIOL_Msk)
  if (crash_type == SLI_CRASH_TYPE_MEMFAULT) {
    uint32_t mpu_err = (SCB_CFSR_IACCVIOL_Msk | SCB_CFSR_DACCVIOL_Msk);
    if ((SCB->CFSR & mpu_err) != 0U) {
      return SLI_CRASH_TYPE_MPUFAULT;
    }
  }
#endif
  return crash_type;
}

/**
 * @brief Return whether the stack frame has the integer callee-saved context.
 *
 * DCRS = 0 always indicates that the context is present. For a Secure to
 * Non-secure exception, the architecture applies its default stacking rules
 * (DCRS = 1) but still pushes the context to protect the Secure registers.
 */
static bool crash_has_additional_state_context(uint32_t exc_return)
{
  return ((exc_return & SLI_CRASH_EXC_RETURN_DCRS) == 0u)
         || (((exc_return & SLI_CRASH_EXC_RETURN_S) != 0u)
             && ((exc_return & SLI_CRASH_EXC_RETURN_ES) == 0u));
}

/**
 * @brief Return whether the stack frame has the FP callee-saved context.
 *
 * FPCCR.TS is meaningful only for an extended FP frame. Device headers define
 * it only for cores that support the TrustZone FP state extension.
 */
static bool crash_has_additional_fp_context(uint32_t exc_return)
{
#if defined(FPU) && defined(FPU_FPCCR_TS_Msk)
  return ((exc_return & SLI_CRASH_EXC_RETURN_FTYPE) == 0u)
         && ((FPU->FPCCR & FPU_FPCCR_TS_Msk) != 0u);
#else
  (void)exc_return;
  return false;
#endif
}

#if (SL_DIAG_STACK_SNAPSHOT_BYTES > 0u)
/**
 * @brief Exclusive end of a snapshot that stays inside one RAM region.
 *
 * A pointer below the main stack stops at the main stack, so the copy does
 * not enter it. A pointer on the main stack stops at the top of that stack.
 * @return The region end, or @p start when @p start is outside the region.
 */
static uintptr_t crash_snapshot_region_end(uintptr_t start,
                                           uintptr_t region_low,
                                           uintptr_t region_high,
                                           uintptr_t stack_low,
                                           uintptr_t stack_high)
{
  if ((region_low >= region_high)
      || (start < region_low)
      || (start >= region_high)) {
    return start;
  }

  if ((stack_low < stack_high)
      && (stack_low >= region_low)
      && (stack_high <= region_high)) {
    if (start < stack_low) {
      return stack_low;
    }
    if (start < stack_high) {
      return stack_high;
    }
  }
  return region_high;
}

/**
 * @brief Return the safe end address for a stack snapshot.
 *
 * The main stack is one window, including when it is in DTCM outside the
 * primary RAM range. RAM below that stack, such as an RTOS task stack, is
 * copied only up to the main stack. Other banks and the other security alias
 * are not probed from the fault handler.
 */
static const uint32_t *crash_snapshot_end(const uint32_t *start)
{
  uintptr_t start_address = (uintptr_t)start;
  uintptr_t stack_low;
  uintptr_t stack_high;
  uintptr_t end;
  uintptr_t ram_end = (uintptr_t)&__ram_end__;

#if defined(__GNUC__) || defined(__clang__)
  stack_low = (uintptr_t)&__StackLimit;
  stack_high = (uintptr_t)&__StackTop;
#elif defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
  stack_low = (uintptr_t)__section_begin("CSTACK");
  stack_high = (uintptr_t)__section_end("CSTACK");
#else
  return start;
#endif

  if ((start_address & (sizeof(uint32_t) - 1u)) != 0u) {
    return start;
  }

  end = crash_snapshot_region_end(start_address,
                                  SRAM_BASE,
                                  ram_end,
                                  stack_low,
                                  stack_high);
  if (end != start_address) {
    return (const uint32_t *)end;
  }

#if defined(DTCM_BASE) && defined(DTCM_MAX_SIZE)
  end = crash_snapshot_region_end(start_address,
                                  DTCM_BASE,
                                  DTCM_BASE + DTCM_MAX_SIZE,
                                  stack_low,
                                  stack_high);
  if (end != start_address) {
    return (const uint32_t *)end;
  }
#endif

  return start;
}

static uint16_t crash_copy_stack_snapshot(uint8_t *destination,
                                          const uint32_t *start,
                                          uint32_t destination_size) /**< Words inside the snapshot end only. */
{
  const uint32_t *end = crash_snapshot_end(start);
  uint32_t word_count;
  uint32_t byte_count;

  if (end <= start) {
    return 0u;
  }
  word_count = (uint32_t)(end - start);
  if (word_count > (destination_size / sizeof(*start))) {
    word_count = destination_size / sizeof(*start);
  }
  byte_count = word_count * sizeof(*start);
  for (uint32_t i = 0u; i < byte_count; i++) {
    destination[i] = ((const uint8_t *)start)[i];
  }
  return (uint16_t)byte_count;
}
#endif

/**
 * @brief True when exception entry failed to stack the frame.
 *
 * UNSTKERR and MUNSTKERR mean a previous exception return failed. The frame
 * for this entry is still the one just stacked, so those bits do not apply.
 */
static bool crash_entry_stacking_failed(uint32_t cfsr)
{
  uint32_t stacking_error = 0u;

#if defined(SCB_CFSR_MSTKERR_Msk)
  stacking_error |= SCB_CFSR_MSTKERR_Msk;
#endif
#if defined(SCB_CFSR_STKERR_Msk)
  stacking_error |= SCB_CFSR_STKERR_Msk;
#endif
  return (cfsr & stacking_error) != 0u;
}

/**
 * @brief True when the handler can read the stacked frame.
 *
 * EXC_RETURN.S is the security state that owns the frame. EXC_RETURN.ES is
 * the security state the handler runs in. MRS of MSP or PSP reads the
 * handler's stack, so a mismatch would copy the wrong memory.
 * On a core without TrustZone both bits are set in a normal EXC_RETURN.
 */
static bool crash_stacked_state_matches_handler(uint32_t exc_return)
{
  bool stacked_secure = (exc_return & SLI_CRASH_EXC_RETURN_S) != 0u;
  bool handler_secure = (exc_return & SLI_CRASH_EXC_RETURN_ES) != 0u;

  return stacked_secure == handler_secure;
}

static bool crash_span_in_region(uintptr_t start,
                                 uintptr_t end,
                                 uintptr_t region_low,
                                 uintptr_t region_high)
{
  return (region_low < region_high)
         && (start >= region_low)
         && (end <= region_high);
}

/**
 * @brief True when @p word_count words at @p start can be read.
 *
 * The architecture doubleword-aligns an exception frame. A misaligned or
 * out-of-RAM pointer is not read.
 */
static bool crash_frame_span_is_readable(const uint32_t *start, uint32_t word_count)
{
  uintptr_t span_start;
  uintptr_t span_end;

  if ((start == NULL) || (word_count == 0u)) {
    return false;
  }
  span_start = (uintptr_t)start;
  if ((span_start & 7u) != 0u) {
    return false;
  }
  span_end = span_start + ((uintptr_t)word_count * sizeof(uint32_t));
  if (span_end < span_start) {
    return false;
  }

#if defined(SRAM_BASE) && defined(SRAM_SIZE)
  if (crash_span_in_region(span_start,
                           span_end,
                           SRAM_BASE,
                           SRAM_BASE + SRAM_SIZE)) {
    return true;
  }
#endif
#if defined(DTCM_BASE) && defined(DTCM_MAX_SIZE)
  if (crash_span_in_region(span_start,
                           span_end,
                           DTCM_BASE,
                           DTCM_BASE + DTCM_MAX_SIZE)) {
    return true;
  }
#endif
  return false;
}

/**
 * @brief Copy the exception frame, fault registers and optional stack words
 *        into the scratch.
 *
 * Fault registers are stored before the frame is read. FRAME_VALID is set
 * only when the stacked frame belongs to this handler's security state and
 * the basic frame is aligned and inside RAM. Additional integer state, when
 * present, is stacked before R0. The floating-point context, when FTYPE is
 * 0, follows xPSR. The recorded SP and stack snapshot start past all stacked
 * state.
 *
 * @param[out] scratch        Scratch to fill
 * @param[in]  stack_pointer  MSP or PSP at exception entry
 * @param[in]  exc_return     EXC_RETURN value of the fault exception
 */
static void crash_capture_scratch(sli_diag_cpu_crash_scratch_t *scratch,
                                  const uint32_t *stack_pointer,
                                  uint32_t exc_return)
{
  const uint32_t *frame = stack_pointer;
  const uint32_t *pre_exception_sp;
  uint32_t frame_words = SLI_CRASH_BASIC_FRAME_WORDS;

  (void)memset(scratch, 0, sizeof(*scratch));
  scratch->exc_return = exc_return;
#if defined(SCB)
  scratch->cfsr = SCB->CFSR;
  scratch->hfsr = SCB->HFSR;
  scratch->bfar = SCB->BFAR;
  scratch->mmfar = SCB->MMFAR;
#endif

  if (crash_entry_stacking_failed(scratch->cfsr)
      || !crash_stacked_state_matches_handler(exc_return)) {
    return;
  }

  if (crash_has_additional_state_context(exc_return)) {
    frame_words += SLI_CRASH_ADDITIONAL_STATE_WORDS;
  }
  if (!crash_frame_span_is_readable(stack_pointer, frame_words)) {
    return;
  }
  if (crash_has_additional_state_context(exc_return)) {
    frame += SLI_CRASH_ADDITIONAL_STATE_WORDS;
  }

  scratch->r0 = frame[0];
  scratch->r1 = frame[1];
  scratch->r2 = frame[2];
  scratch->r3 = frame[3];
  scratch->r12 = frame[4];
  scratch->lr = frame[5];
  scratch->pc = frame[6];
  scratch->xpsr = frame[7];
  scratch->flags = SLI_DIAG_CPU_CRASH_FLAG_FRAME_VALID;

  pre_exception_sp = frame + SLI_CRASH_BASIC_FRAME_WORDS;
  if ((exc_return & SLI_CRASH_EXC_RETURN_FTYPE) == 0u) {
    pre_exception_sp += SLI_CRASH_FP_CONTEXT_WORDS;
    if (crash_has_additional_fp_context(exc_return)) {
      pre_exception_sp += SLI_CRASH_ADDITIONAL_FP_CONTEXT_WORDS;
    }
  }
  if ((scratch->xpsr & SLI_CRASH_XPSR_FRAME_ALIGN) != 0u) {
    pre_exception_sp++;
  }
  scratch->sp = (uint32_t)pre_exception_sp;
#if (SL_DIAG_STACK_SNAPSHOT_BYTES > 0u)
  scratch->stack_snapshot_size = crash_copy_stack_snapshot(scratch->stack_snapshot,
                                                            pre_exception_sp,
                                                            sizeof(scratch->stack_snapshot));
#endif
}

static void crash_reset(void) /**< fault_seal requires the caller to reset after it returns. */
{
#if defined(__ARM_ARCH) || defined(__CORTEX_M)
  NVIC_SystemReset();
#endif
  while (1) {
  }
}

/*******************************************************************************
 ***************************   FAULT HANDLERS   ********************************
 ******************************************************************************/

/**
 * @brief C-level fault handler: Tier-0 scratch, seal via SL_DIAG, then reset.
 *
 * If this path is re-entered (e.g. escalated fault while building the record), the second
 * entry stops in a breakpoint when a debugger is attached, then forces a system reset.
 *
 * @param[in] stack_pointer  MSP or PSP at exception entry
 * @param[in] crash_type     MemManage / HardFault / BusFault / UsageFault
 * @param[in] exc_return     LR on exception entry
 */
__USED
static void fault_handler_c(uint32_t *stack_pointer,
                            sli_crash_type_t crash_type,
                            uint32_t exc_return)
{
#if defined(__ARM_ARCH) || defined(__CORTEX_M)
  if (s_in_fault != 0U) {
    if (is_debugger_connected()) {
      __BKPT(0);
    }
    NVIC_SystemReset();
  }
  s_in_fault = 1U;
#endif

  crash_type = resolve_crash_type(crash_type);
  crash_capture_scratch(&s_fault_scratch, stack_pointer, exc_return);
  (void)crash_seal(crash_type, &s_fault_scratch);

#if defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
  SL_PRINT_STRING_CRASH("Fault type 0x%x occurred!", (unsigned int)crash_type);
  SL_PRINT_STRING_CRASH("PC: 0x%08x SP: 0x%08x LR: 0x%08x",
                        s_fault_scratch.pc,
                        s_fault_scratch.sp,
                        s_fault_scratch.lr);
#endif

#if defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
  if (is_debugger_connected()) {
    SL_PRINT_STRING_CRASH("Passing control to debugger via breakpoint.");
    __BKPT(0);
  }
#endif

  crash_reset();
}

#if (defined(__GNUC__) && !defined(__ICCARM__) && !defined(__IAR_SYSTEMS_ICC__)) \
  || defined(__clang__)

void __attribute__((naked)) MemManage_Handler(void) /**< MSP/PSP and MEMFAULT to fault_handler_c. */
{
  __asm volatile (
    "TST lr, #4 \n"
    "ITE EQ \n"
    "MRSEQ r0, MSP \n"
    "MRSNE r0, PSP \n"
    "MOV r1, %0 \n"
    "MOV r2, lr \n"
    "B fault_handler_c \n"
    : : "i" (SLI_CRASH_TYPE_MEMFAULT)
  );
}

void __attribute__((naked)) HardFault_Handler(void) /**< MSP/PSP and HARDFAULT to fault_handler_c. */
{
  __asm volatile (
    "TST lr, #4 \n"
    "ITE EQ \n"
    "MRSEQ r0, MSP \n"
    "MRSNE r0, PSP \n"
    "MOV r1, %0 \n"
    "MOV r2, lr \n"
    "B fault_handler_c \n"
    : : "i" (SLI_CRASH_TYPE_HARDFAULT)
  );
}

void __attribute__((naked)) BusFault_Handler(void) /**< MSP/PSP and BUSFAULT to fault_handler_c. */
{
  __asm volatile (
    "TST lr, #4 \n"
    "ITE EQ \n"
    "MRSEQ r0, MSP \n"
    "MRSNE r0, PSP \n"
    "MOV r1, %0 \n"
    "MOV r2, lr \n"
    "B fault_handler_c \n"
    : : "i" (SLI_CRASH_TYPE_BUSFAULT)
  );
}

void __attribute__((naked)) UsageFault_Handler(void) /**< MSP/PSP and USAGEFAULT to fault_handler_c. */
{
  __asm volatile (
    "TST lr, #4 \n"
    "ITE EQ \n"
    "MRSEQ r0, MSP \n"
    "MRSNE r0, PSP \n"
    "MOV r1, %0 \n"
    "MOV r2, lr \n"
    "B fault_handler_c \n"
    : : "i" (SLI_CRASH_TYPE_USAGEFAULT)
  );
}

#elif defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)

__NO_PROLOGUE void MemManage_Handler(void) /**< r1 is SLI_CRASH_TYPE_MEMFAULT. */
{
  __ASM volatile (
    "TST lr, #4\n"
    "ITE EQ\n"
    "MRSEQ r0, MSP\n"
    "MRSNE r0, PSP\n"
    "MOV r1, #2\n"
    "MOV r2, lr\n"
    "B fault_handler_c\n"
  );
}

__NO_PROLOGUE void HardFault_Handler(void) /**< r1 is SLI_CRASH_TYPE_HARDFAULT. */
{
  __ASM volatile (
    "TST lr, #4\n"
    "ITE EQ\n"
    "MRSEQ r0, MSP\n"
    "MRSNE r0, PSP\n"
    "MOV r1, #1\n"
    "MOV r2, lr\n"
    "B fault_handler_c\n"
  );
}

__NO_PROLOGUE void BusFault_Handler(void) /**< r1 is SLI_CRASH_TYPE_BUSFAULT. */
{
  __ASM volatile (
    "TST lr, #4\n"
    "ITE EQ\n"
    "MRSEQ r0, MSP\n"
    "MRSNE r0, PSP\n"
    "MOV r1, #3\n"
    "MOV r2, lr\n"
    "B fault_handler_c\n"
  );
}

__NO_PROLOGUE void UsageFault_Handler(void) /**< r1 is SLI_CRASH_TYPE_USAGEFAULT. */
{
  __ASM volatile (
    "TST lr, #4\n"
    "ITE EQ\n"
    "MRSEQ r0, MSP\n"
    "MRSNE r0, PSP\n"
    "MOV r1, #4\n"
    "MOV r2, lr\n"
    "B fault_handler_c\n"
  );
}

#endif /* __GNUC__ || __clang__ || __ICCARM__ */

/*******************************************************************************
 ***************************   WATCHDOG HANDLERS   *****************************
 ******************************************************************************/

void sli_crash_capture_on_watchdog_warning(uint32_t unfed_mask) /**< detail is the WDOG interrupt flags. */
{
  sli_diag_cpu_crash_scratch_t scratch;

#if defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
  SL_PRINT_STRING_CRASH("Watchdog warning! Unfed mask: %u (reset imminent)",
                        (unsigned int)unfed_mask);
#endif

  (void)memset(&scratch, 0, sizeof(scratch));
  scratch.r0 = unfed_mask;
  (void)crash_seal(SLI_CRASH_TYPE_WATCHDOG, &scratch);
}

#if defined(WDOG_COUNT) && (WDOG_COUNT > 0) && defined(WDOG_IF_WARN)
#if !defined(SL_CATALOG_WATCHDOG_MANAGER_PRESENT)

#if defined(WDOG0)
void WDOG0_IRQHandler(void) /**< WDOG0 warning IRQ. */
{
  uint32_t flags = sl_hal_wdog_get_pending_interrupts(WDOG0);
  if (flags & WDOG_IF_WARN) {
    sli_crash_capture_on_watchdog_warning(flags);
  }
  sl_hal_wdog_clear_interrupts(WDOG0, flags);
}
#endif

#if defined(WDOG1)
void WDOG1_IRQHandler(void) /**< WDOG1 warning IRQ. */
{
  uint32_t flags = sl_hal_wdog_get_pending_interrupts(WDOG1);
  if (flags & WDOG_IF_WARN) {
    sli_crash_capture_on_watchdog_warning(flags);
  }
  sl_hal_wdog_clear_interrupts(WDOG1, flags);
}
#endif

#endif // !SL_CATALOG_WATCHDOG_MANAGER_PRESENT
#endif /* WDOG_COUNT && WDOG_IF_WARN */

/*******************************************************************************
 ***************************   ASSERT HANDLER   ********************************
 ******************************************************************************/

SL_WEAK void assertEFM(const char *file, int line) /**< Record file, line, PC, and SP, then seal and reset. An application assertEFM replaces this one. */
{
  sli_diag_cpu_crash_scratch_t scratch;

#if defined(__GNUC__) || defined(__clang__)
  uint32_t pc = (uint32_t)__builtin_return_address(0);
  uint32_t sp = (uint32_t)__builtin_frame_address(0);
#elif defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
  uint32_t pc = 0U;
  uint32_t sp = __get_MSP();
#else
  uint32_t pc = 0U;
  uint32_t sp = 0U;
#endif

  (void)memset(&scratch, 0, sizeof(scratch));
  scratch.pc = pc;
  scratch.sp = sp;
  scratch.line = (uint32_t)line;
  scratch.file_hash = hash_string(file);

  (void)crash_seal(SLI_CRASH_TYPE_ASSERT, &scratch);

#if defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
  SEGGER_SYSVIEW_ErrorfTarget("Assert failed file: %s", file);
  SL_PRINT_STRING_CRASH("Assert failed at line %u", (unsigned)line);
#endif

#if defined(__ARM_ARCH) || defined(__CORTEX_M)
  if (is_debugger_connected()) {
    __BKPT(0);
  }
#endif
  crash_reset();
}
