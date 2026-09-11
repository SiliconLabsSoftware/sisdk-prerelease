/*******************************************************************************
 * @file
 * @brief Zigbee watchdog LWM crash handler implementation
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: LicenseRef-MSLA
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement
 * By installing, copying or otherwise using this software, you agree to the
 * terms of the MSLA.
 *
 ******************************************************************************/

#include "sl_zb_wdog_lwm_handler.h"
#include "sl_component_catalog.h"
#include "sl_zb_wdog_lwm_handler_config.h"

// CORTEXM3 is the legacy HAL name for all device_cortexm parts (M33, M55, etc.).
#if (SL_ZIGBEE_WATCHDOG_LWM_HANDLER_ENABLE != 0) && defined(CORTEXM3)

#include PLATFORM_HEADER

#include <stdint.h>
#include <string.h>

#include "em_device.h"
#include "cortexm3/diagnostic.h"

#include "sl_code_classification.h"
#include "sl_watchdog_manager.h"

#if defined(SL_CATALOG_IOSTREAM_UART_COMMON_PRESENT)
#include "sl_iostream.h"
#define SL_ZB_WDOG_LWM_HANDLER_LOG(fmt, ...)                                                   \
  sl_iostream_printf(SL_IOSTREAM_STDOUT, fmt "\n", ##__VA_ARGS__)
#else
#define SL_ZB_WDOG_LWM_HANDLER_LOG(fmt, ...)
#endif

#if defined(SL_CATALOG_CMSIS_OS_COMMON_PRESENT)
#define SL_ZB_WDOG_LWM_HANDLER_FREERTOS 1
#else
#define SL_ZB_WDOG_LWM_HANDLER_FREERTOS 0
#endif

extern HalCrashInfoType halCrashInfo;

// WDOG_IF_WARN comes from the device *_wdog.h header when the part exposes a
// WDOG warning interrupt. Parts can still have a hardware WDOG that resets on
// timeout without WARN; this LWM handler cannot capture on those parts and the
// init path below is a no-op.
#if defined(WDOG_IF_WARN)

#define SL_ZB_WDOG_LWM_HANDLER_XPSR_THUMB_BIT 0x01000000U

// Max bytes to scan below PSP when searching for a preempted task exception frame.
#define SL_ZB_WDOG_LWM_HANDLER_PSP_SCAN_BYTES 0x2000U

// ARMv7-M / ARMv8-M EXC_RETURN pattern (Cortex-M3/M4/M33/M55): 0xFFFFFF00–0xFFFFFFEF.
#define SL_ZB_WDOG_LWM_HANDLER_EXC_RETURN_MASK 0xFFFFFF00U
#define SL_ZB_WDOG_LWM_HANDLER_EXC_RETURN_LOW_MASK 0x0000000FU
#define SL_ZB_WDOG_LWM_HANDLER_EXC_RETURN_THREAD 0xFFFFFFFDU
#define SL_ZB_WDOG_LWM_HANDLER_EXC_RETURN_HANDLER 0xFFFFFFF9U

// Standard hardware exception frame: R0–R3, R12, LR, PC, xPSR (8 words).
#define SL_ZB_WDOG_LWM_HANDLER_EXCEPTION_FRAME_WORDS 8U
// Stacked PC word index (same as halInternalCrashHandler sp += 6).
#define SL_ZB_WDOG_LWM_HANDLER_EXCEPTION_FRAME_PC_INDEX 6U

// Return true if pc is a Thumb address in the code segment.
static bool is_text_thumb_pc(uint32_t pc)
{
  if ((pc & 1U) == 0U) {
    return false;
  }

  const uintptr_t addr = (uintptr_t)pc;

  return (addr >= (uintptr_t)_TEXT_SEGMENT_BEGIN) && (addr < (uintptr_t)_TEXT_SEGMENT_END);
}

// Return true if lr looks like a stacked ARM EXC_RETURN value.
static bool is_plausible_exc_return(uint32_t lr)
{
  return ((lr & SL_ZB_WDOG_LWM_HANDLER_EXC_RETURN_MASK) ==
          SL_ZB_WDOG_LWM_HANDLER_EXC_RETURN_MASK) &&
         ((lr & SL_ZB_WDOG_LWM_HANDLER_EXC_RETURN_LOW_MASK) != 0U);
}

// Return true if frame has room for the stacked PC word at index 6.
static bool is_frame_index_bounds_ok(const uint32_t *frame)
{
  const uintptr_t frameBegin = (uintptr_t)frame;
  const uintptr_t frameEnd =
      frameBegin + (SL_ZB_WDOG_LWM_HANDLER_EXCEPTION_FRAME_WORDS * sizeof(uint32_t));
  const uintptr_t pcWordAddr =
      frameBegin + (SL_ZB_WDOG_LWM_HANDLER_EXCEPTION_FRAME_PC_INDEX * sizeof(uint32_t));

  if (frameEnd <= frameBegin) {
    return false;
  }

  if ((pcWordAddr >= frameEnd) || (pcWordAddr <= frameBegin)) {
    return false;
  }

  return true;
}

// Strict match for MSP stack walk (reject stale main-stack false positives).
static bool is_valid_msp_exception_frame(const uint32_t *frame)
{
  if (!is_frame_index_bounds_ok(frame)) {
    return false;
  }

  // Stacked hardware exception frame: [5]=EXC_RETURN, [6]=PC, [7]=xPSR.
  if (!is_plausible_exc_return(frame[5])) {
    return false;
  }

  if (!is_text_thumb_pc(frame[SL_ZB_WDOG_LWM_HANDLER_EXCEPTION_FRAME_PC_INDEX])) {
    return false;
  }

  if (frame[SL_ZB_WDOG_LWM_HANDLER_EXCEPTION_FRAME_WORDS - 1U] == 0U) {
    return false;
  }

  if ((frame[SL_ZB_WDOG_LWM_HANDLER_EXCEPTION_FRAME_WORDS - 1U] &
       SL_ZB_WDOG_LWM_HANDLER_XPSR_THUMB_BIT) == 0U) {
    return false;
  }

  return true;
}

// PSP at IRQ preemption points at frame base; match halInternalCrashHandler PC read (sp + 6).
static bool is_valid_psp_exception_frame(const uint32_t *frame)
{
  if (!is_frame_index_bounds_ok(frame)) {
    return false;
  }

  return is_text_thumb_pc(frame[SL_ZB_WDOG_LWM_HANDLER_EXCEPTION_FRAME_PC_INDEX]);
}

// Walk down from spLimit and return the first strict exception frame match.
static const uint32_t *find_exception_frame_below(uintptr_t spLimit, uintptr_t stackLow)
{
  uintptr_t addr = spLimit;

  const uintptr_t frameSize = SL_ZB_WDOG_LWM_HANDLER_EXCEPTION_FRAME_WORDS * sizeof(uint32_t);

  while ((addr >= stackLow) && ((addr - stackLow) >= frameSize)) {
    const uint32_t *candidate = (const uint32_t *)(addr - frameSize);

    if (is_valid_msp_exception_frame(candidate)) {
      return candidate;
    }

    addr -= sizeof(uint32_t);
  }

  return NULL;
}

// Search PSP for a preempted task exception frame, then below PSP if needed.
static const uint32_t *find_psp_exception_frame(uintptr_t pspTop)
{
  uintptr_t pspLow = 0U;

  if (pspTop != 0U) {
    const uint32_t *frameAtPsp = (const uint32_t *)pspTop;

    // After IRQ preemption, PSP points at the frame base (R0), not above it.
    if (is_valid_psp_exception_frame(frameAtPsp)) {
      return frameAtPsp;
    }
  }

  if (pspTop > SL_ZB_WDOG_LWM_HANDLER_PSP_SCAN_BYTES) {
    pspLow = pspTop - SL_ZB_WDOG_LWM_HANDLER_PSP_SCAN_BYTES;
  }

  return find_exception_frame_below(pspTop, pspLow);
}

// Return the first non-zero thumb PC stored in the returns array.
static uint32_t pick_pc_from_probable_returns(const HalCrashInfoType *c)
{
  for (uint8_t i = 0; i < NUM_RETURNS; i++) {
    const uint32_t pc = c->returns[i];

    if ((pc != 0U) && is_text_thumb_pc(pc)) {
      return pc;
    }
  }

  return 0U;
}

// Prefer a preempted RTOS frame on PSP; otherwise walk MSP for a stacked frame.
static const uint32_t *find_preempted_exception_frame(uint32_t msp, uint32_t psp)
{
  const uint32_t *frame = NULL;

  if (SL_ZB_WDOG_LWM_HANDLER_FREERTOS != 0) {
    // Prefer preempted RTOS task frame on PSP before walking MSP.
    frame = find_psp_exception_frame(psp);
  }

  if (frame == NULL) {
    frame = find_exception_frame_below(msp, (uintptr_t)_CSTACK_SEGMENT_BEGIN);
  }

  return frame;
}

// Returns the bytes used in the main stack area.
static uint32_t main_stack_bytes_used(uintptr_t stackBegin)
{
  const uintptr_t stackLow = (uintptr_t)_CSTACK_SEGMENT_BEGIN;
  const uintptr_t stackHigh = (uintptr_t)_CSTACK_SEGMENT_END;
  size_t offset = 0U;
  size_t stackByteSize = 0U;

  if (stackHigh <= stackLow) {
    return 0U;
  }

  stackByteSize = stackHigh - stackLow;

  if (stackBegin <= stackLow) {
    offset = 0U;
  } else if (stackBegin >= stackHigh) {
    offset = stackByteSize;
  } else {
    offset = stackBegin - stackLow;
  }

  while (sizeof(uint32_t) <= (stackByteSize - offset)) {
    uint32_t word = 0U;
    const uintptr_t sourceAddress = stackLow + offset;

    memcpy(&word, (const void *)sourceAddress, sizeof(word));

    if (word != STACK_FILL_VALUE) {
      break;
    }

    offset += sizeof(uint32_t);
  }

  return (uint32_t)(stackByteSize - offset);
}

// Search the stack downward for probable return addresses (thumb code pointers).
static void capture_probable_returns(HalCrashInfoType *c, const uint32_t *sEnd,
                                     const uint32_t *stackTop)
{
  uint8_t i = 0U;
  uint8_t j = 0U;
  uint32_t data = 0U;
  uintptr_t addr = (uintptr_t)stackTop;
  const uintptr_t endAddr = (uintptr_t)sEnd;
  const uintptr_t stackLowBound = (uintptr_t)_CSTACK_SEGMENT_BEGIN;
  const uintptr_t wordSize = sizeof(uint32_t);

  for (i = 0U; i < NUM_RETURNS; i++) {
    c->returns[i] = 0U;
  }

  i = 0U;

  while ((addr >= endAddr) && ((addr - endAddr) >= wordSize) && (addr >= stackLowBound) &&
         ((addr - stackLowBound) >= wordSize)) {
    addr -= wordSize;
    data = *(const uint32_t *)addr;

    if (!is_text_thumb_pc(data)) {
      continue;
    }

    for (j = 0U; j < NUM_RETURNS; j++) {
      if (c->returns[j] == data) {
        break;
      }
    }

    if (j == NUM_RETURNS) {
      i = (i != 0U) ? (i - 1U) : (NUM_RETURNS - 1U);
      c->returns[i] = data;
    }
  }

  while (i != 0U) {
    data = c->returns[0];

    for (j = 0U; j < (NUM_RETURNS - 1U); j++) {
      c->returns[j] = c->returns[j + 1U];
    }

    c->returns[NUM_RETURNS - 1U] = data;
    i--;
  }
}

// Return the default EXC_RETURN LR for thread or handler stack context.
static uint32_t default_exc_return_lr(void)
{
  return (SL_ZB_WDOG_LWM_HANDLER_FREERTOS != 0) ? SL_ZB_WDOG_LWM_HANDLER_EXC_RETURN_THREAD
                                                    : SL_ZB_WDOG_LWM_HANDLER_EXC_RETURN_HANDLER;
}

// Return true when halCrashInfo LR indicates the process stack was in use.
static bool crash_uses_process_stack(const HalCrashInfoType *c)
{
  return (SL_ZB_WDOG_LWM_HANDLER_FREERTOS != 0) && ((c->LR & 4U) != 0U);
}

// Capture SCB and NVIC registers into halCrashInfo (same fields as halInternalCrashHandler).
static void capture_crash_scb_registers(HalCrashInfoType *c)
{
  c->icsr.word = SCB->ICSR;
  c->shcsr.word = SCB->SHCSR;

  uint8_t num_bitmasks = (sizeof(c->intActive.word) / sizeof(c->intActive.word[0]));
  for (uint8_t mask_num = 0; mask_num < num_bitmasks; mask_num++) {
    c->intActive.word[mask_num] = NVIC->IABR[mask_num];
  }

  c->cfsr.word = SCB->CFSR;
  c->hfsr.word = SCB->HFSR;
  c->dfsr.word = SCB->DFSR;
  c->faultAddress = SCB->MMFAR;
  c->afsr.word = SCB->AFSR;
}

// Copy a located hardware exception frame into halCrashInfo.
static void load_crash_info_from_frame(HalCrashInfoType *c, const uint32_t *frame)
{
  c->R0 = frame[0];
  c->R1 = frame[1];
  c->R2 = frame[2];
  c->R3 = frame[3];
  c->R4 = c->R5 = c->R6 = c->R7 = 0U;
  c->R8 = c->R9 = c->R10 = c->R11 = 0U;
  c->R12 = frame[4];
  if (is_plausible_exc_return(frame[5])) {
    c->LR = frame[5];
  } else {
    c->LR = default_exc_return_lr();
  }
  c->PC = frame[SL_ZB_WDOG_LWM_HANDLER_EXCEPTION_FRAME_PC_INDEX];
  c->xPSR.word = frame[SL_ZB_WDOG_LWM_HANDLER_EXCEPTION_FRAME_WORDS - 1U];
  c->processSP = (uint32_t)(uintptr_t)frame;
}

// Initialize halCrashInfo when no stacked exception frame was found.
static void load_crash_info_without_frame(HalCrashInfoType *c, uint32_t psp)
{
  c->LR = default_exc_return_lr();
  c->processSP = psp;
  c->PC = 0U;
  c->xPSR.word = 0U;
}

// Set stackBottom/stackTop for the probable-returns scan (process or main stack).
static void setup_stack_scan_window(HalCrashInfoType *c, const uint32_t *sp,
                                    const uint32_t **stackBottom, const uint32_t **stackTop)
{
  if (crash_uses_process_stack(c)) {
    *stackBottom = sp;
    *stackTop = sp + SL_ZB_WDOG_LWM_HANDLER_EXCEPTION_FRAME_WORDS;
    c->processSPUsed = *stackTop - sp;
  } else {
    *stackBottom = (const uint32_t *)(uintptr_t)c->mainStackBottom;
    *stackTop = _CSTACK_SEGMENT_END;
    c->processSPUsed = 0U;
  }
}

// Widen the stack scan upper bound when searching a starved RTOS task stack.
static const uint32_t *expanded_scan_top(const uint32_t *frame, uint32_t psp,
                                         const HalCrashInfoType *c, const uint32_t *stackTop)
{
  uintptr_t scanTopAddr = (uintptr_t)stackTop;

  if ((frame == NULL) && crash_uses_process_stack(c) && (psp != 0U)) {
    const uintptr_t expandedTop = (uintptr_t)psp + SL_ZB_WDOG_LWM_HANDLER_PSP_SCAN_BYTES;

    if (expandedTop > scanTopAddr) {
      scanTopAddr = expandedTop;
    }
  }

  return (const uint32_t *)scanTopAddr;
}

// Clamp the stack scan lower bound when SP is outside the expected window.
static const uint32_t *adjust_stack_scan_end(const uint32_t *frame, HalCrashInfoType *c,
                                             const uint32_t *sp, const uint32_t *stackBottom,
                                             const uint32_t *stackTop, const uint32_t *sEnd)
{
  const uintptr_t spAddress = (uintptr_t)sp;
  const uintptr_t bottomAddress = (uintptr_t)stackBottom;
  const uintptr_t topAddress = (uintptr_t)stackTop;
  const uintptr_t frameSize = SL_ZB_WDOG_LWM_HANDLER_EXCEPTION_FRAME_WORDS * sizeof(uint32_t);

  const bool outsideStack = (topAddress < bottomAddress) || (spAddress < bottomAddress) ||
                            (spAddress > topAddress) || ((topAddress - spAddress) < frameSize);

  if (outsideStack) {
    if ((frame == NULL) && (c->PC == 0U)) {
      c->xPSR.word = 0U;
    }

    return stackBottom;
  }

  return sEnd;
}

// Walk the stack for probable returns and set PC when no exception frame exists.
static void capture_stack_returns_and_pc(HalCrashInfoType *c, const uint32_t *frame, uint32_t psp,
                                         const uint32_t *sp, const uint32_t *sEnd,
                                         const uint32_t *stackBottom, const uint32_t *stackTop)
{
  const uint32_t *scanTop = expanded_scan_top(frame, psp, c, stackTop);
  const uint32_t *scanEnd = adjust_stack_scan_end(frame, c, sp, stackBottom, stackTop, sEnd);

  c->mainSPUsed = main_stack_bytes_used(c->mainStackBottom);
  capture_probable_returns(c, scanEnd, scanTop);

  if (frame == NULL) {
    const uint32_t probablePc = pick_pc_from_probable_returns(c);

    if (probablePc != 0U) {
      c->PC = probablePc;
    }
  }
}

// Capture crash context at LWM; hardware WDOG reset prints PC via diagnostic.
static inline void starve_capture(void)
{
  HalCrashInfoType *c = &halCrashInfo;
  const uint32_t *frame = NULL;
  uint32_t msp = __get_MSP();
  uint32_t psp = __get_PSP();
  const uint32_t *sp = NULL;
  const uint32_t *sEnd = NULL;
  const uint32_t *stackBottom = NULL;
  const uint32_t *stackTop = NULL;

  frame = find_preempted_exception_frame(msp, psp);
  capture_crash_scb_registers(c);

  c->mainStackBottom = (uint32_t)_CSTACK_SEGMENT_BEGIN;
  c->mainSP = msp;

  if (frame != NULL) {
    load_crash_info_from_frame(c, frame);
  } else {
    load_crash_info_without_frame(c, psp);
  }

  sp = (const uint32_t *)(uintptr_t)(((c->LR & 4U) != 0U) ? c->processSP : c->mainSP);
  sEnd = sp;
  setup_stack_scan_window(c, sp, &stackBottom, &stackTop);
  capture_stack_returns_and_pc(c, frame, psp, sp, sEnd, stackBottom, stackTop);

  halWatchdogLwmCaptureMark(c->PC);
}

// Watchdog Manager starve callback entry point.
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_LEGACY_HAL, SL_CODE_CLASS_TIME_CRITICAL)
static void starve_callback(const sl_watchdog_manager_starve_context_t *context)
{
  (void)context;
  starve_capture();
}

// Register the platform Watchdog Manager starve callback for LWM crash capture.
void sl_zb_wdog_lwm_handler_init(void)
{
  sl_status_t status = sl_watchdog_manager_set_starve_callback(starve_callback);

  if (status != SL_STATUS_OK) {
    SL_ZB_WDOG_LWM_HANDLER_LOG(
        "Watchdog LWM handler disabled: platform starve callback unsupported (0x%lx)",
        (unsigned long)status);
    return;
  }

  SL_ZB_WDOG_LWM_HANDLER_LOG("Watchdog LWM handler enabled (platform WM starve callback)");
}

#else // WDOG_IF_WARN

void sl_zb_wdog_lwm_handler_init(void)
{
  SL_ZB_WDOG_LWM_HANDLER_LOG("Watchdog LWM handler unsupported: no WDOG_IF_WARN on this part");
}

#endif // WDOG_IF_WARN

#else // SL_ZIGBEE_WATCHDOG_LWM_HANDLER_ENABLE && CORTEXM3

void sl_zb_wdog_lwm_handler_init(void)
{
}

#endif // SL_ZIGBEE_WATCHDOG_LWM_HANDLER_ENABLE && CORTEXM3
