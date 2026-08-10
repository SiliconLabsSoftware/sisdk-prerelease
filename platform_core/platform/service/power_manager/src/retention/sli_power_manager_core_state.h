/***************************************************************************//**
 * @file
 * @brief Power Manager S2RAM (Suspend-to-RAM) API definition.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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

 #ifndef SLI_POWER_MANAGER_CORE_STATE_H
 #define SLI_POWER_MANAGER_CORE_STATE_H

 #include "em_device.h"
 #include <stdint.h>
 #include <stdbool.h>

 #ifdef __cplusplus
extern "C" {
 #endif

extern void Reset_Handler_C(void);

/*******************************************************************************
 ****************************   DEFINES   **************************************
 ******************************************************************************/

// Magic marker written by SUSPEND to prove a valid context was saved.
#define SLI_POWER_MANAGER_SUSPEND_MARKER  0xA5C3E1D7UL

/*******************************************************************************
 ****************************   MACROS   ***************************************
 ******************************************************************************/

/** @brief Suspend: save core registers to stack, save CONTROL/MSP/PSP, then WFI.
 *  @param is_core_reset  bool variable, set to true if a reset+RESUME occurred,
 *                        false if WFI returned without a core reset. */
#define SLI_POWER_MANAGER_SUSPEND(is_core_reset)         \
  do {                                                   \
    uint32_t _reset_flag;                                \
    sli_power_manager_saved_stack.marker =               \
      SLI_POWER_MANAGER_SUSPEND_MARKER;                  \
    __ASM volatile (                                     \
      "adr.w lr, 1f\n"                                   \
      "orr lr, lr, #1\n"                                 \
      "push {r0-r12, lr}\n"                              \
      "mrs r1, control\n"                                \
      "mrs r2, msp\n"                                    \
      "mrs r3, psp\n"                                    \
      "stm %1, {r1, r2, r3}\n"                           \
      "dsb\n"                                            \
      "isb\n"                                            \
      "wfi\n"                                            \
      /* No EM2 entry (no reset): undo push, flag = 0 */ \
      "add sp, sp, #56\n"                                \
      "mov %0, #0\n"                                     \
      "b 2f\n"                                           \
      /* RESUME branches here after reset: flag = 1 */   \
      "1: mov %0, #1\n"                                  \
      "2:\n"                                             \
      : "=&r" (_reset_flag)                              \
      : "r" (&sli_power_manager_saved_stack)             \
      : "r1", "r2", "r3", "lr", "memory"                 \
      );                                                 \
    sli_power_manager_saved_stack.marker = 0;            \
    (is_core_reset) = (_reset_flag != 0);                \
  } while (0)

/** @brief Resume: Verifies if the wake-up is from EM2 deep sleep AND a valid saved context exists,
   then restores CONTROL, MSP, PSP from saved_stack, pop; branch to Reset_Handler_C if not. */
#define SLI_POWER_MANAGER_RESUME()                                                   \
  __ASM volatile (                                                                   \
    "ldr   r0, [%[emu], %[rstcause_off]]\n"         /* RSTCAUSE */                   \
    "lsr   r0, r0, %[em2_shift]\n"                  /* EM2 bit to LSB */             \
    "and   r0, r0, #1\n"                                                             \
    "cbz   r0, 1f\n"                                                                 \
    "ldr   r0, [%[stack], %[marker_off]]\n"         /* marker at offsetof(marker) */ \
    "cmp   r0, %[marker]\n"                                                          \
    "bne   1f\n"                                                                     \
    "ldm   %[stack], {r1, r2, r3}\n"                /* control, msp, psp */          \
    "msr   msp, r2\n"                                                                \
    "msr   psp, r3\n"                                                                \
    "msr   control, r1\n"                                                            \
    "isb\n"                                                                          \
    "pop   {r0-r12, lr}\n"                                                           \
    "bx    lr\n"                                                                     \
    "1: bx %[reset_handler]\n"                                                       \
    :                                                                                \
    :[emu] "r" (EMU),                                                                \
    [rstcause_off] "r" (offsetof(EMU_TypeDef, RSTCAUSE)),                            \
    [em2_shift] "I" (_EMU_RSTCAUSE_EM2_SHIFT),                                       \
    [stack] "r" (&sli_power_manager_saved_stack),                                    \
    [marker_off] "I" (offsetof(sli_power_manager_saved_stack_t, marker)),            \
    [marker] "r" (SLI_POWER_MANAGER_SUSPEND_MARKER),                                 \
    [reset_handler] "r" (Reset_Handler_C)                                             \
    : "r0", "r1", "r2", "r3", "memory"                                               \
    );

/*******************************************************************************
 *****************************   DATA TYPES   **********************************
 ******************************************************************************/

// Stack state saved by SUSPEND, restored by RESUME (in retained RAM).
// The marker field must be last so that the stm/ldm of {control, msp, psp}
// at offset 0 remain unchanged.
typedef struct {
  uint32_t control;   // offset 0
  uint32_t msp;       // offset 4
  uint32_t psp;       // offset 8
  uint32_t marker;    // offset 12 -- validity marker
} sli_power_manager_saved_stack_t;

/*******************************************************************************
 *****************************   GLOBAL VARIABLES   ****************************
 ******************************************************************************/

extern sli_power_manager_saved_stack_t sli_power_manager_saved_stack;

#if defined(__CORTEX_M) && (__CORTEX_M >= 3U)
/***************************************************************************//**
 * Save CoreDebug, DWT, TPIU, and ITM state used by SWO and the cycle counter.
 ******************************************************************************/
void sli_power_manager_save_debug_trace_context(void);

/***************************************************************************//**
 * Restore CoreDebug, DWT, TPIU, and ITM state used by SWO and the cycle counter.
 ******************************************************************************/
void sli_power_manager_restore_debug_trace_context(void);
#endif

/*******************************************************************************
 *****************************   GLOBAL FUNCTIONS   ****************************
 ******************************************************************************/

/***************************************************************************//**
 * Save peripheral and core special-register context.
 * GPRs r0-r12 and LR are saved by SLI_POWER_MANAGER_SUSPEND.
 ******************************************************************************/
void sli_power_manager_save_cpu_context(void);

/***************************************************************************//**
 * Restore peripheral and core special-register context.
 * GPRs r0-r12 and LR are restored by SLI_POWER_MANAGER_RESUME in startup.
 ******************************************************************************/
void sli_power_manager_restore_cpu_context(void);

 #ifdef __cplusplus
}
 #endif

 #endif /* SLI_POWER_MANAGER_CORE_STATE_H */
