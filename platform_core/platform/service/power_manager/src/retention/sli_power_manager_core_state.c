/***************************************************************************//**
 * @file
 * @brief Power Manager Core State Save/Restore Implementation.
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

#include "sli_power_manager_core_state.h"
#include "em_device.h"
#include "sl_common.h"

#include <string.h>

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

// Maximum number of NVIC ISER registers to save (based on EXT_IRQ_COUNT)
#if defined(EXT_IRQ_COUNT)
#define PM_NVIC_ISER_COUNT  (((EXT_IRQ_COUNT) +31U) / 32U)
#define PM_NVIC_IPR_COUNT   (EXT_IRQ_COUNT)
#else
#define PM_NVIC_ISER_COUNT  (4U)   // Default for up to 128 interrupts
#define PM_NVIC_IPR_COUNT   (128U)
#endif

// Maximum number of MPU regions to save
#define PM_MPU_MAX_REGIONS  (16U)

// Number of SCB System Handler Priority registers (bytes)
#define PM_SCB_SHPR_COUNT   (12U)

/*******************************************************************************
 *****************************   DATA TYPES   **********************************
 ******************************************************************************/

// Core special registers. GPRs r0-r12, LR, CONTROL, MSP, PSP are saved/restored
// by SLI_POWER_MANAGER_SUSPEND/RESUME via sli_power_manager_saved_stack.
typedef struct {
  uint32_t msplim;     // Main Stack Pointer Limit
  uint32_t psplim;     // Process Stack Pointer Limit
  uint32_t primask;    // Priority Mask Register
  uint32_t basepri;    // Base Priority Register
  uint32_t faultmask;  // Fault Mask Register
} core_context_t;

// NVIC context
typedef struct {
  uint32_t iser[PM_NVIC_ISER_COUNT];  // Interrupt Set Enable Registers
  uint8_t  ipr[PM_NVIC_IPR_COUNT];    // Interrupt Priority Registers
} nvic_context_t;

// MPU context
typedef struct {
  uint32_t ctrl;                               // MPU Control Register
  uint32_t rnr;                                // Region Number Register (original value)
  uint32_t rbar[PM_MPU_MAX_REGIONS];    // Region Base Address Registers
  uint32_t rlar[PM_MPU_MAX_REGIONS];    // Region Limit Address Registers
  uint32_t mair0;                              // Memory Attribute Indirection Register 0
  uint32_t mair1;                              // Memory Attribute Indirection Register 1
  uint8_t  num_regions;                        // Number of MPU regions (from TYPE register)
} mpu_context_t;

// SCB context
typedef struct {
  uint32_t vtor;                             // Vector Table Offset Register
  uint32_t aircr;                            // Application Interrupt and Reset Control Register
  uint32_t scr;                              // System Control Register
  uint32_t ccr;                              // Configuration and Control Register
  uint8_t  shpr[PM_SCB_SHPR_COUNT];   // System Handler Priority Registers
  uint32_t shcsr;                            // System Handler Control and State Register
  uint32_t cpacr;                            // Coprocessor Access Control Register
 #if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  uint32_t nsacr;                            // Non-Secure Access Control Register (TrustZone)
 #endif
} scb_context_t;

// SysTick context
typedef struct {
  uint32_t ctrl;   // SysTick Control and Status Register
  uint32_t load;   // SysTick Reload Value Register
  uint32_t val;   // SysTick Current Value Register
} systick_context_t;

#if (__FPU_PRESENT == 1)
// FPU context (Cortex-M33 FPv5: FPCCR, FPCAR, FPDSCR, FPSCR, s0-s31)
typedef struct {
  uint32_t fpccr;   // Floating-Point Context Control Register
  uint32_t fpcar;   // Floating-Point Context Address Register
  uint32_t fpdscr;  // Floating-Point Default Status Control Register
  uint32_t fpscr;   // Floating-Point Status and Control Register (core reg, via VMRS/VMSR)
  uint32_t s[32];   // Single-precision registers s0-s31 (128 bytes, 8-byte aligned)
} fpu_context_t;
#endif

// Complete CPU context.
typedef struct {
  core_context_t core;   // Core CPU registers
  nvic_context_t nvic;   // NVIC state
  mpu_context_t  mpu;    // MPU state
  scb_context_t  scb;    // SCB state
  systick_context_t systick;  // SysTick state
#if (__FPU_PRESENT == 1)
  fpu_context_t fpu;     // FPU state
#endif
} cpu_context_t;

#if defined(__CORTEX_M) && (__CORTEX_M >= 3U)
typedef struct {
  uint32_t ctrl;     // DWT Control Register
  uint32_t cyccnt;   // DWT Cycle Count Register
} dwt_context_t;

typedef struct {
  uint32_t demcr;    // Debug Exception and Monitor Control Register
} core_debug_context_t;

typedef struct {
  uint32_t acpr;     // Asynchronous Clock Prescaler Register
  uint32_t sppr;     // Selected Pin Protocol Register
  uint32_t ffcr;     // Formatter and Flush Control Register
} tpiu_context_t;

typedef struct {
  uint32_t tcr;      // Trace Control Register
  uint32_t ter;      // Trace Enable Register
} itm_context_t;

typedef struct {
  dwt_context_t        dwt;
  core_debug_context_t core_debug;
  tpiu_context_t       tpiu;
  itm_context_t        itm;
} debug_trace_context_t;
#endif

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

/// CPU context stored in retained RAM (noinit section to survive reset)
static cpu_context_t cpu_context SL_ATTRIBUTE_SECTION(".noinit");

#if defined(__CORTEX_M) && (__CORTEX_M >= 3U)
static debug_trace_context_t debug_trace_context SL_ATTRIBUTE_SECTION(".noinit");
#endif

/// Stack state for RESUME macro (no function call, no stack use)
sli_power_manager_saved_stack_t sli_power_manager_saved_stack SL_ATTRIBUTE_SECTION(".noinit");

/*******************************************************************************
 ***************************  LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

/***************************************************************************//**
 * Save core special registers (MSPLIM, PSPLIM, PRIMASK, BASEPRI, FAULTMASK)
 * to context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
static void save_core_context_special_regs(core_context_t *ctx)
{
  ctx->msplim = __get_MSPLIM();
  ctx->psplim = __get_PSPLIM();
  ctx->primask = __get_PRIMASK();
  ctx->basepri = __get_BASEPRI();
  ctx->faultmask = __get_FAULTMASK();
}

/***************************************************************************//**
 * Restore core special registers (MSPLIM, PSPLIM, PRIMASK, BASEPRI, FAULTMASK)
 * from context structure. Must be called before restoring NVIC.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
static void restore_core_context_special_regs(const core_context_t *ctx)
{
  __set_MSPLIM(ctx->msplim);
  __set_PSPLIM(ctx->psplim);
  __set_PRIMASK(ctx->primask);
  __set_BASEPRI(ctx->basepri);
  __set_FAULTMASK(ctx->faultmask);
}

/***************************************************************************//**
 * Save NVIC state to context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void save_nvic_context(nvic_context_t *ctx)
{
  // Save Interrupt Set Enable Registers
  for (uint32_t i = 0; i < PM_NVIC_ISER_COUNT; i++) {
    ctx->iser[i] = NVIC->ISER[i];
  }

  // Save Interrupt Priority Registers
  for (uint32_t i = 0; i < PM_NVIC_IPR_COUNT; i++) {
    ctx->ipr[i] = NVIC->IPR[i];
  }
}

/***************************************************************************//**
 * Restore NVIC state from context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void restore_nvic_context(const nvic_context_t *ctx)
{
  // Restore Interrupt Priority Registers first
  for (uint32_t i = 0; i < PM_NVIC_IPR_COUNT; i++) {
    NVIC->IPR[i] = ctx->ipr[i];
  }

  // Restore Interrupt Set Enable Registers
  for (uint32_t i = 0; i < PM_NVIC_ISER_COUNT; i++) {
    NVIC->ISER[i] = ctx->iser[i];
  }
}

/***************************************************************************//**
 * Save MPU state to context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void save_mpu_context(mpu_context_t *ctx)
{
 #if defined(__MPU_PRESENT) && (__MPU_PRESENT == 1U)
  // Get number of MPU regions from TYPE register
  uint32_t num_regions = (MPU->TYPE & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos;
  if (num_regions > PM_MPU_MAX_REGIONS) {
    num_regions = PM_MPU_MAX_REGIONS;
  }
  ctx->num_regions = (uint8_t)num_regions;

  // Save current region number
  ctx->rnr = MPU->RNR;

  // Save MPU control register
  ctx->ctrl = MPU->CTRL;

  // Save memory attribute registers
  ctx->mair0 = MPU->MAIR0;
  ctx->mair1 = MPU->MAIR1;

  // Save all MPU regions by iterating through RNR
  for (uint32_t i = 0; i < num_regions; i++) {
    MPU->RNR = i;
    ctx->rbar[i] = MPU->RBAR;
    ctx->rlar[i] = MPU->RLAR;
  }

  // Restore original RNR value
  MPU->RNR = ctx->rnr;
 #else
  (void)ctx;
 #endif
}

/***************************************************************************//**
 * Restore MPU state from context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void restore_mpu_context(const mpu_context_t *ctx)
{
 #if defined(__MPU_PRESENT) && (__MPU_PRESENT == 1U)
  // Disable MPU during configuration
  MPU->CTRL = 0;

  // Restore memory attribute registers
  MPU->MAIR0 = ctx->mair0;
  MPU->MAIR1 = ctx->mair1;

  // Restore all MPU regions
  for (uint32_t i = 0; i < ctx->num_regions; i++) {
    MPU->RNR = i;
    MPU->RBAR = ctx->rbar[i];
    MPU->RLAR = ctx->rlar[i];
  }

  // Restore original RNR value
  MPU->RNR = ctx->rnr;

  // Re-enable MPU with original control settings
  MPU->CTRL = ctx->ctrl;

  // Data and Instruction Synchronization Barriers
  __DSB();
  __ISB();
 #else
  (void)ctx;
 #endif
}

/***************************************************************************//**
 * Save SCB state to context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void save_scb_context(scb_context_t *ctx)
{
  // Save Vector Table Offset Register
  ctx->vtor = SCB->VTOR;

  // Save Application Interrupt and Reset Control Register
  // Note: Only save the priority group bits, not the reset/clear bits
  ctx->aircr = SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk;

  // Save System Control Register
  ctx->scr = SCB->SCR;

  // Save Configuration and Control Register
  ctx->ccr = SCB->CCR;

  // Save System Handler Priority Registers
  for (uint32_t i = 0; i < PM_SCB_SHPR_COUNT; i++) {
    ctx->shpr[i] = SCB->SHPR[i];
  }

  // Save System Handler Control and State Register
  ctx->shcsr = SCB->SHCSR;

  // Save Coprocessor Access Control Register
  ctx->cpacr = SCB->CPACR;

 #if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  // Save Non-Secure Access Control Register (TrustZone)
  ctx->nsacr = SCB->NSACR;
 #endif
}

/***************************************************************************//**
 * Restore SCB state from context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void restore_scb_context(const scb_context_t *ctx)
{
  // Restore Vector Table Offset Register
  SCB->VTOR = ctx->vtor;

  // Restore Application Interrupt and Reset Control Register
  // Must write VECTKEY to modify PRIGROUP bits
  SCB->AIRCR = (SCB->AIRCR & ~SCB_AIRCR_PRIGROUP_Msk)
               | ctx->aircr
               | (0x5FAUL << SCB_AIRCR_VECTKEY_Pos);

  // Restore System Control Register
  SCB->SCR = ctx->scr;

  // Restore Configuration and Control Register
  SCB->CCR = ctx->ccr;

  // Restore System Handler Priority Registers
  for (uint32_t i = 0; i < PM_SCB_SHPR_COUNT; i++) {
    SCB->SHPR[i] = ctx->shpr[i];
  }

  // Restore System Handler Control and State Register
  SCB->SHCSR = ctx->shcsr;

  // Restore Coprocessor Access Control Register
  SCB->CPACR = ctx->cpacr;

 #if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  // Restore Non-Secure Access Control Register (TrustZone)
  SCB->NSACR = ctx->nsacr;
 #endif

  // Instruction Synchronization Barrier
  __ISB();
}

/***************************************************************************//**
 * Save SysTick state to context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void save_systick_context(systick_context_t *ctx)
{
  ctx->ctrl = SysTick->CTRL;
  ctx->load = SysTick->LOAD;
  ctx->val = SysTick->VAL;
}

/***************************************************************************//**
 * Restore SysTick state from context structure.
 *
 * Hardware limitation: SysTick->VAL is write-to-clear on Cortex-M; writing
 * any value resets the counter to 0 rather than loading the provided value.
 * To minimize first-tick skew, the saved countdown value is temporarily
 * placed in LOAD so the first tick fires at approximately the correct
 * remaining time. The original period is restored immediately after
 * enabling and takes effect on the next reload.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void restore_systick_context(const systick_context_t *ctx)
{
  // Use saved countdown as the first-tick period (fall back to full period
  // if the counter had already wrapped when we saved).
  uint32_t first_tick = (ctx->val != 0) ? ctx->val : ctx->load;

  SysTick->LOAD = first_tick;   // First tick: remaining count
  SysTick->VAL  = 0;            // Clear counter and COUNTFLAG
  SysTick->CTRL = ctx->ctrl;    // Enable; counter loads first_tick
  SysTick->LOAD = ctx->load;    // Original period for subsequent ticks
}

#if (__FPU_PRESENT == 1)
/***************************************************************************//**
 * Save FPU state to context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void save_fpu_context(fpu_context_t *ctx)
{
  ctx->fpccr = FPU->FPCCR;
  ctx->fpcar = FPU->FPCAR;
  ctx->fpdscr = FPU->FPDSCR;
  ctx->fpscr = __get_FPSCR();

  // Arm forbids changing ASPEN/LSPEN while CONTROL.FPCA is set.
  __set_CONTROL(__get_CONTROL() & ~CONTROL_FPCA_Msk);
  __ISB();

  // Disable lazy/auto state preservation before reading the registers.
  FPU->FPCCR = 0;

  // Save s0-s31 (single-precision registers) via VSTM
  uint32_t *ptr = ctx->s;
  __ASM volatile (
    "vstm %0!, {s0-s31}"
    : "+r" (ptr)
    :
    : "memory"
    );

  // Restore FPCAR before FPCCR so enabling ASPEN cannot race with a stale FPCAR.
  FPU->FPCAR = ctx->fpcar;
  FPU->FPCCR = ctx->fpccr;
}

/***************************************************************************//**
 * Restore FPU state from context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void restore_fpu_context(const fpu_context_t *ctx)
{
  // After EM2 reset FPCCR defaults to ASPEN|LSPEN. Clear FPCA and FPCCR
  // before touching s-regs / FPCAR so exception entry cannot clobber FPCAR,
  // and so ASPEN/LSPEN are not changed while FPCA is set.
  __set_CONTROL(__get_CONTROL() & ~CONTROL_FPCA_Msk);
  __ISB();
  FPU->FPCCR = 0;

  uint32_t *ptr = (uint32_t *)ctx->s;
  __ASM volatile (
    "vldm %0!, {s0-s31}"
    : "+r" (ptr)
    :
    : "memory"
    );

  // Restore FPSCR after s-regs (vldm may clear exception flags)
  __set_FPSCR(ctx->fpscr);

  // Restore address/default-status before restoring FPCCR (ASPEN/LSPEN).
  FPU->FPCAR = ctx->fpcar;
  FPU->FPDSCR = ctx->fpdscr;
  FPU->FPCCR = ctx->fpccr;

  __DSB();
  __ISB();
}
#endif /* __FPU_PRESENT */

#if defined(__CORTEX_M) && (__CORTEX_M >= 3U)
/***************************************************************************//**
 * Save CoreDebug state to context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void save_core_debug_context(core_debug_context_t *ctx)
{
  ctx->demcr = CoreDebug->DEMCR;
}

/***************************************************************************//**
 * Restore CoreDebug state from context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void restore_core_debug_context(const core_debug_context_t *ctx)
{
  CoreDebug->DEMCR = ctx->demcr;
}

/***************************************************************************//**
 * Save DWT state to context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void save_dwt_context(dwt_context_t *ctx)
{
  ctx->ctrl = DWT->CTRL;
  ctx->cyccnt = DWT->CYCCNT;
}

/***************************************************************************//**
 * Restore DWT state from context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void restore_dwt_context(const dwt_context_t *ctx)
{
  DWT->CYCCNT = ctx->cyccnt;
  DWT->CTRL = ctx->ctrl;
}

/***************************************************************************//**
 * Save TPIU state to context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void save_tpiu_context(tpiu_context_t *ctx)
{
  ctx->acpr = TPIU->ACPR;
  ctx->sppr = TPIU->SPPR;
  ctx->ffcr = TPIU->FFCR;
}

/***************************************************************************//**
 * Restore TPIU state from context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void restore_tpiu_context(const tpiu_context_t *ctx)
{
  TPIU->ACPR = ctx->acpr;
  TPIU->SPPR = ctx->sppr;
  TPIU->FFCR = ctx->ffcr;
}

/***************************************************************************//**
 * Save ITM state to context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void save_itm_context(itm_context_t *ctx)
{
  ctx->tcr = ITM->TCR;
  ctx->ter = ITM->TER;
}

/***************************************************************************//**
 * Restore ITM state from context structure.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
__STATIC_INLINE void restore_itm_context(const itm_context_t *ctx)
{
  ITM->LAR = 0xC5ACCE55UL;
  ITM->TCR = ctx->tcr;
  ITM->TER = ctx->ter;
}
#endif

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

#if defined(__CORTEX_M) && (__CORTEX_M >= 3U)
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
void sli_power_manager_save_debug_trace_context(void)
{
  save_dwt_context(&debug_trace_context.dwt);
  save_core_debug_context(&debug_trace_context.core_debug);
  save_tpiu_context(&debug_trace_context.tpiu);
  save_itm_context(&debug_trace_context.itm);
}

SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
void sli_power_manager_restore_debug_trace_context(void)
{
  restore_core_debug_context(&debug_trace_context.core_debug);
  restore_dwt_context(&debug_trace_context.dwt);
  restore_tpiu_context(&debug_trace_context.tpiu);
  restore_itm_context(&debug_trace_context.itm);
}
#endif

/***************************************************************************//**
 * Save the complete CPU context.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
void sli_power_manager_save_cpu_context(void)
{
  // Save all CPU state components
  save_nvic_context(&cpu_context.nvic);
  save_mpu_context(&cpu_context.mpu);
  save_scb_context(&cpu_context.scb);
  save_systick_context(&cpu_context.systick);
#if (__FPU_PRESENT == 1)
  save_fpu_context(&cpu_context.fpu);
#endif
#if defined(__CORTEX_M) && (__CORTEX_M >= 3U)
  sli_power_manager_save_debug_trace_context();
#endif

  save_core_context_special_regs(&cpu_context.core);
}

/***************************************************************************//**
 * Restore the complete CPU context.
 *
 * Restores in dependency order: core special regs, SCB, MPU, SysTick, debug
 * trace, FPU, NVIC. Must be called after EM2 reset.
 ******************************************************************************/
SL_CODE_CLASSIFY(SL_CODE_COMPONENT_POWER_MANAGER, SL_CODE_CLASS_TIME_CRITICAL)
void sli_power_manager_restore_cpu_context(void)
{
  restore_core_context_special_regs(&cpu_context.core);

  restore_scb_context(&cpu_context.scb);
  restore_mpu_context(&cpu_context.mpu);
  restore_systick_context(&cpu_context.systick);
#if defined(__CORTEX_M) && (__CORTEX_M >= 3U)
  sli_power_manager_restore_debug_trace_context();
#endif
#if (__FPU_PRESENT == 1)
  restore_fpu_context(&cpu_context.fpu);
#endif
  restore_nvic_context(&cpu_context.nvic);
}
