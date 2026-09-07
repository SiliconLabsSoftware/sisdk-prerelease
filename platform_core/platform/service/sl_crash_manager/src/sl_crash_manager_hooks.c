/***************************************************************************/ /**
 * @file sl_crash_manager_hooks.c
 * @brief Implementation of the Silicon Labs Crash Manager hooks
 * @version 1.0.0
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

#include "sl_crash_manager.h"
#include "sli_crash_manager.h"

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
#include "em_wdog.h"
#endif

#if defined(SL_CATALOG_LOG_BACKEND_PROPRIETARY_PRESENT) \
  || defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
#include "sl_log_helper.h"
#endif

#if SL_CRASH_MANAGER_ASSERT_HANDLER_ENABLED
#include "sl_assert.h"
#if !defined(SL_CATALOG_LOG_BACKEND_PROPRIETARY_PRESENT) \
  && !defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
#include "sl_log.h"
#endif
#endif

/*******************************************************************************
 ***************************   EXTERNALS   *************************************
 ******************************************************************************/

extern volatile sl_crash_manager_data_t g_crash_manager_data;

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

#if defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
/**
 * @brief Return whether a debugger is attached (DHCSR.C_DEBUGEN)
 *
 * @return Non-zero if debugger connected
 */
static inline uint8_t is_debugger_connected(void)
{
  return (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0;
}
#endif

#if SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED
/**
 * @brief Clear persisted stack snapshot bytes and length in the crash record
 */
static void clear_stack_snapshot(void)
{
  g_crash_manager_data.stack_snapshot_len = 0U;
  (void)memset((void *)g_crash_manager_data.stack_snapshot,
               0,
               sizeof(g_crash_manager_data.stack_snapshot));
}

/**
 * @brief Copy exception frame and optional stack words into @ref g_crash_manager_data
 *
 * Frame indices: [0..7] R0-R3, R12, LR, PC, xPSR; [8..] extra words per config.
 * Optionally emits register/stack logs when a log backend is present.
 *
 * @param[in] stack_pointer  Pointer to stacked exception frame
 */
static void debug_dump(uint32_t *stack_pointer)
{
  clear_stack_snapshot();

  uint32_t n_stack = 0u;
#if (SL_CRASH_MANAGER_DEBUG_DUMP_STACK_WORDS > 0)
  n_stack = (uint32_t)SL_CRASH_MANAGER_DEBUG_DUMP_STACK_WORDS;
  if (n_stack > SL_CRASH_MANAGER_STACK_SNAPSHOT_MAX_WORDS) {
    n_stack = (uint32_t)SL_CRASH_MANAGER_STACK_SNAPSHOT_MAX_WORDS;
  }
  for (uint32_t i = 0u; i < n_stack; i++) {
    uint32_t w = stack_pointer[8u + i];
    (void)memcpy((void *)&g_crash_manager_data.stack_snapshot[i * sizeof(uint32_t)],
                 &w,
                 sizeof(w));
  }
#endif

#if defined(SL_CATALOG_LOG_BACKEND_PROPRIETARY_PRESENT)
  SL_PRINT_EVENT_CRASH(SL_LOG_EVENT_ID_CRASH_DEBUG_R012,
                       stack_pointer[0], stack_pointer[1], stack_pointer[2]);
  SL_PRINT_EVENT_CRASH(SL_LOG_EVENT_ID_CRASH_DEBUG_R3LR,
                       stack_pointer[3], stack_pointer[4], stack_pointer[5]);
  SL_PRINT_EVENT_CRASH(SL_LOG_EVENT_ID_CRASH_DEBUG_PC,
                       stack_pointer[6], stack_pointer[7], 0u);

  for (uint32_t i = 0u; i < n_stack; i += 2u) {
    uint32_t a = stack_pointer[8u + i];
    uint32_t b = (i + 1u < n_stack) ? stack_pointer[9u + i] : 0u;
    SL_PRINT_EVENT_CRASH(SL_LOG_EVENT_ID_CRASH_DEBUG_STACK,
                         (uint32_t)i, a, b);
  }
#elif defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
  SL_PRINT_STRING_CRASH("Crash regs R0-R2: 0x%08x 0x%08x 0x%08x",
                        stack_pointer[0], stack_pointer[1], stack_pointer[2]);
  SL_PRINT_STRING_CRASH("Crash regs R3,R12,LR: 0x%08x 0x%08x 0x%08x",
                        stack_pointer[3], stack_pointer[4], stack_pointer[5]);
  SL_PRINT_STRING_CRASH("Crash regs PC,xPSR: 0x%08x 0x%08x",
                        stack_pointer[6], stack_pointer[7]);

  for (uint32_t i = 0u; i < n_stack; ) {
    uint32_t a = stack_pointer[8u + i];
    uint32_t b = (i + 1u < n_stack) ? stack_pointer[9u + i] : 0u;
    if (i + 1u < n_stack) {
      SL_PRINT_STRING_CRASH("Stack+%u: 0x%08x 0x%08x", (unsigned int)i, a, b);
      i += 2u;
    } else {
      SL_PRINT_STRING_CRASH("Stack+%u: 0x%08x", (unsigned int)i, a);
      i += 1u;
    }
  }
#endif

#if defined(SL_CATALOG_LOG_BACKEND_PROPRIETARY_PRESENT)
  sl_log_flush();
#endif
  g_crash_manager_data.stack_snapshot_len =
    (uint16_t)(n_stack * (uint32_t)sizeof(uint32_t));
}
#endif

/**
 * @brief Refine crash type using CFSR (stack overflow, MPU fault)
 *
 * @param[in] crash_type  Raw type from the fault vector
 * @return Refined crash type (@ref sl_crash_type_t)
 */
static sl_crash_type_t resolve_crash_type(sl_crash_type_t crash_type)
{
#if defined(SCB_CFSR_MSTKERR_Msk) && defined(SCB_CFSR_STKERR_Msk)
  if (crash_type == SL_CRASH_TYPE_MEMFAULT
      || crash_type == SL_CRASH_TYPE_BUSFAULT
      || crash_type == SL_CRASH_TYPE_HARDFAULT) {
    uint32_t stack_err = (SCB_CFSR_MSTKERR_Msk | SCB_CFSR_MUNSTKERR_Msk
                          | SCB_CFSR_STKERR_Msk | SCB_CFSR_UNSTKERR_Msk);
    if ((SCB->CFSR & stack_err) != 0U) {
      return SL_CRASH_TYPE_STACKOVERFLOW;
    }
  }
#endif
#if defined(SCB_CFSR_STKOF_Msk)
  if (crash_type == SL_CRASH_TYPE_USAGEFAULT
      && (SCB->CFSR & SCB_CFSR_STKOF_Msk) != 0U) {
    return SL_CRASH_TYPE_STACKOVERFLOW;
  }
#endif
#if defined(SCB_CFSR_IACCVIOL_Msk) && defined(SCB_CFSR_DACCVIOL_Msk)
  if (crash_type == SL_CRASH_TYPE_MEMFAULT) {
    uint32_t mpu_err = (SCB_CFSR_IACCVIOL_Msk | SCB_CFSR_DACCVIOL_Msk);
    if ((SCB->CFSR & mpu_err) != 0U) {
      return SL_CRASH_TYPE_MPUFAULT;
    }
  }
#endif
  return crash_type;
}

/*******************************************************************************
 ***************************   FAULT HANDLERS   *********************************
 ******************************************************************************/

#if defined(__ARM_ARCH) || defined(__CORTEX_M)
/** Set while the fault handler function runs; avoids recursive faults if that path faults again. */
static volatile uint8_t s_in_fault;
#endif

/**
 * @brief C-level fault handler: fill crash record, optional debug dump, finalize CRC, dispatch
 *
 * If this path is re-entered (e.g. escalated fault while building the record), the second
 * entry stops in a breakpoint (when supported) and forces a system reset instead of looping.
 *
 * @param[in] stack_pointer  MSP or PSP exception frame
 * @param[in] crash_type     MemManage / HardFault / BusFault / UsageFault
 */
__USED
static void fault_handler_c(uint32_t *stack_pointer, sl_crash_type_t crash_type)
{
#if defined(__ARM_ARCH) || defined(__CORTEX_M)
  if (s_in_fault != 0U) {
    __BKPT(0);
    NVIC_SystemReset();
  }
  s_in_fault = 1U;
#endif

  volatile sl_crash_manager_data_t *record = &g_crash_manager_data;

  const uint32_t fault_r0 = stack_pointer[0];
  const uint32_t fault_r1 = stack_pointer[1];
  const uint32_t fault_r2 = stack_pointer[2];
  const uint32_t fault_r3 = stack_pointer[3];
  const uint32_t fault_r12 = stack_pointer[4];
  const uint32_t fault_lr = stack_pointer[5];
  const uint32_t fault_pc = stack_pointer[6];
  const uint32_t fault_xpsr = stack_pointer[7];
  const uint32_t fault_sp = (uint32_t)stack_pointer;

  crash_type = resolve_crash_type(crash_type);

  sli_crash_record_clear(record);
  sli_crash_fill_common_header(record);
  record->seqno = sli_crash_take_next_seqno();
  record->type = crash_type;

  /* CPU exception frame */
  record->cpu.r0 = fault_r0;
  record->cpu.r1 = fault_r1;
  record->cpu.r2 = fault_r2;
  record->cpu.r3 = fault_r3;
  record->cpu.r12 = fault_r12;
  record->cpu.lr = fault_lr;
  record->cpu.pc = fault_pc;
  record->cpu.xpsr = fault_xpsr;
  record->cpu.sp = fault_sp;
  record->reserved = 0U;

  (void)memset((void *)&record->thread, 0, sizeof(record->thread));
  if (sli_crash_type_is_hardware_fault(crash_type)) {
    sli_crash_capture_fault_regs(&record->fault);
  } else {
    (void)memset((void *)&record->fault, 0, sizeof(record->fault));
  }
  (void)memset((void *)record->user_data, 0, sizeof(record->user_data));

#if defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
  SL_PRINT_STRING_CRASH("Fault type 0x%x occurred!", (unsigned int)crash_type);
  SL_PRINT_STRING_CRASH("PC: 0x%08x SP: 0x%08x LR: 0x%08x",
                        fault_pc,
                        fault_sp,
                        fault_lr);
#endif

#if SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED
  debug_dump(stack_pointer);
#endif

  record->flags |= (uint32_t)SL_CRASH_FLAG_CPU_FRAME_VALID;
  if (sli_crash_type_is_hardware_fault(crash_type)) {
    record->flags |= (uint32_t)SL_CRASH_FLAG_FAULT_REGS_VALID;
  }
#if SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED
  if (record->stack_snapshot_len > 0U) {
    record->flags |= (uint32_t)SL_CRASH_FLAG_STACK_SNAPSHOT_VALID;
  }
#endif

  sli_crash_finalize_record_crc(record);
  sl_crash_manager_handle_crash((sl_crash_manager_data_t *)record);

#if defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
  if (is_debugger_connected()) {
    SL_PRINT_STRING_CRASH("Passing control to debugger via breakpoint.");
    __BKPT(0);
  }
#endif

  while (1) {
  }
}

#if (defined(__GNUC__) && !defined(__ICCARM__) && !defined(__IAR_SYSTEMS_ICC__)) \
  || defined(__clang__)

/**
 * @brief MemManage fault (naked): pass MSP/PSP and @ref SL_CRASH_TYPE_MEMFAULT to fault_handler_c
 */
void __attribute__((naked)) MemManage_Handler(void)
{
  __asm volatile (
    "TST lr, #4 \n"
    "ITE EQ \n"
    "MRSEQ r0, MSP \n"
    "MRSNE r0, PSP \n"
    "MOV r1, %0 \n"
    "B fault_handler_c \n"
    : : "i" (SL_CRASH_TYPE_MEMFAULT)
  );
}

/**
 * @brief HardFault (naked)
 */
void __attribute__((naked)) HardFault_Handler(void)
{
  __asm volatile (
    "TST lr, #4 \n"
    "ITE EQ \n"
    "MRSEQ r0, MSP \n"
    "MRSNE r0, PSP \n"
    "MOV r1, %0 \n"
    "B fault_handler_c \n"
    : : "i" (SL_CRASH_TYPE_HARDFAULT)
  );
}

/**
 * @brief BusFault (naked)
 */
void __attribute__((naked)) BusFault_Handler(void)
{
  __asm volatile (
    "TST lr, #4 \n"
    "ITE EQ \n"
    "MRSEQ r0, MSP \n"
    "MRSNE r0, PSP \n"
    "MOV r1, %0 \n"
    "B fault_handler_c \n"
    : : "i" (SL_CRASH_TYPE_BUSFAULT)
  );
}

/**
 * @brief UsageFault (naked)
 */
void __attribute__((naked)) UsageFault_Handler(void)
{
  __asm volatile (
    "TST lr, #4 \n"
    "ITE EQ \n"
    "MRSEQ r0, MSP \n"
    "MRSNE r0, PSP \n"
    "MOV r1, %0 \n"
    "B fault_handler_c \n"
    : : "i" (SL_CRASH_TYPE_USAGEFAULT)
  );
}

#elif defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)

/**
 * @brief MemManage fault (IAR naked). r1 must match @ref sl_crash_type_t order.
 */
__NO_PROLOGUE void MemManage_Handler(void)
{
  __ASM volatile (
    "TST lr, #4\n"
    "ITE EQ\n"
    "MRSEQ r0, MSP\n"
    "MRSNE r0, PSP\n"
    "MOV r1, #2\n"
    "B fault_handler_c\n"
  );
}

/**
 * @brief HardFault (IAR naked)
 */
__NO_PROLOGUE void HardFault_Handler(void)
{
  __ASM volatile (
    "TST lr, #4\n"
    "ITE EQ\n"
    "MRSEQ r0, MSP\n"
    "MRSNE r0, PSP\n"
    "MOV r1, #1\n"
    "B fault_handler_c\n"
  );
}

/**
 * @brief BusFault (IAR naked)
 */
__NO_PROLOGUE void BusFault_Handler(void)
{
  __ASM volatile (
    "TST lr, #4\n"
    "ITE EQ\n"
    "MRSEQ r0, MSP\n"
    "MRSNE r0, PSP\n"
    "MOV r1, #3\n"
    "B fault_handler_c\n"
  );
}

/**
 * @brief UsageFault (IAR naked)
 */
__NO_PROLOGUE void UsageFault_Handler(void)
{
  __ASM volatile (
    "TST lr, #4\n"
    "ITE EQ\n"
    "MRSEQ r0, MSP\n"
    "MRSNE r0, PSP\n"
    "MOV r1, #4\n"
    "B fault_handler_c\n"
  );
}

#endif /* __GNUC__ || __clang__ || __ICCARM__ */

/*******************************************************************************
 ***************************   WATCHDOG HANDLERS   *******************************
 ******************************************************************************/

#if defined(WDOG_COUNT) && (WDOG_COUNT > 0) && defined(WDOG_IF_WARN)

/**
 * @brief Record watchdog warning: type WATCHDOG, reserved = interrupt flags
 *
 * @param[in] unfed_mask  WDOGn_IntGet() value (e.g. WDOG_IF_WARN)
 */
void sli_crash_manager_on_watchdog_warning(uint32_t unfed_mask)
{
  volatile sl_crash_manager_data_t *record = &g_crash_manager_data;

#if defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
  SL_PRINT_STRING_CRASH("Watchdog warning! Unfed mask: %u (reset imminent)",
                        (unsigned int)unfed_mask);
#endif

  sli_crash_record_clear(record);
  sli_crash_fill_common_header(record);
  record->seqno = sli_crash_take_next_seqno();
  record->type = SL_CRASH_TYPE_WATCHDOG;
  record->reserved = unfed_mask;
  record->watchdog_info = unfed_mask;
  (void)memset((void *)&record->cpu, 0, sizeof(record->cpu));
  (void)memset((void *)&record->thread, 0, sizeof(record->thread));
  (void)memset((void *)&record->fault, 0, sizeof(record->fault));
  (void)memset((void *)record->user_data, 0, sizeof(record->user_data));
#if SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED
  clear_stack_snapshot();
#endif
  sli_crash_finalize_record_crc(record);
  sl_crash_manager_handle_crash((sl_crash_manager_data_t *)record);
}

#if !defined(SL_CATALOG_WATCHDOG_MANAGER_PRESENT)

#if defined(WDOG0)
/**
 * @brief WDOG0 warning IRQ
 */
void WDOG0_IRQHandler(void)
{
  uint32_t flags = WDOGn_IntGet(WDOG0);
  if (flags & WDOG_IF_WARN) {
    sli_crash_manager_on_watchdog_warning(flags);
  }
  WDOGn_IntClear(WDOG0, flags);
}
#endif

#if defined(WDOG1)
/**
 * @brief WDOG1 warning IRQ
 */
void WDOG1_IRQHandler(void)
{
  uint32_t flags = WDOGn_IntGet(WDOG1);
  if (flags & WDOG_IF_WARN) {
    sli_crash_manager_on_watchdog_warning(flags);
  }
  WDOGn_IntClear(WDOG1, flags);
}
#endif

#endif // !SL_CATALOG_WATCHDOG_MANAGER_PRESENT

#endif /* WDOG_COUNT && WDOG_IF_WARN */

/*******************************************************************************
 ***************************   ASSERT HANDLER   ********************************
 ******************************************************************************/

#if SL_CRASH_MANAGER_ASSERT_HANDLER_ENABLED

/**
 * @brief EFM_ASSERT failure path: record assert line/PC/SP, then sl_crash_manager_handle_crash
 *
 * @param[in] file  Source path (__FILE__)
 * @param[in] line  Line number (__LINE__)
 */
void assertEFM(const char *file, int line)
{
  volatile sl_crash_manager_data_t *record = &g_crash_manager_data;

#if defined(__GNUC__) || defined(__clang__)
  uint32_t pc = (uint32_t)__builtin_return_address(0);
  void    *fp = __builtin_frame_address(0);
  uint32_t sp = (fp != ((void *)0)) ? (uint32_t)fp : 0U;
#elif defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
  uint32_t pc = 0U;
  uint32_t sp = __get_MSP();
#else
  uint32_t pc = 0U;
  uint32_t sp = 0U;
#endif

#if defined(SL_CATALOG_LOG_BACKEND_SYSTEMVIEW_PRESENT)
  SEGGER_SYSVIEW_ErrorfTarget("Assert failed file: %s", file);
  SL_PRINT_STRING_CRASH("Assert failed at line %u", (unsigned)line);
#elif defined(SL_CATALOG_LOG_BACKEND_PROPRIETARY_PRESENT)
  SL_PRINT_EVENT_CRASH(SL_LOG_EVENT_ID_CRASH_ASSERT, (uint32_t)(uintptr_t)file, (uint32_t)line);
#endif

  sli_crash_record_clear(record);
  sli_crash_fill_common_header(record);
  record->seqno = sli_crash_take_next_seqno();
  record->type = SL_CRASH_TYPE_ASSERT;
  record->cpu.pc = pc;
  record->cpu.lr = 0U;
  record->cpu.sp = sp;
  record->reserved = (uint32_t)(line & SL_CRASH_ASSERT_LINE_MASK);
  record->assert_line = (uint32_t)(line & SL_CRASH_ASSERT_LINE_MASK);
  (void)memset((void *)&record->thread, 0, sizeof(record->thread));
  (void)memset((void *)&record->fault, 0, sizeof(record->fault));
  (void)memset((void *)record->user_data, 0, sizeof(record->user_data));
#if SL_CRASH_MANAGER_DEBUG_DUMP_ENABLED
  clear_stack_snapshot();
#endif
  record->flags |= (uint32_t)SL_CRASH_FLAG_CPU_FRAME_VALID;
  sli_crash_finalize_record_crc(record);
  sl_crash_manager_handle_crash((sl_crash_manager_data_t *)record);

  sli_log_assert_implementation("Assertion failed");

  while (1) {
  }
}
#endif /* SL_CRASH_MANAGER_ASSERT_HANDLER_ENABLED */
