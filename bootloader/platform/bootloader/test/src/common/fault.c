#include "fault.h"
#include "em_device.h"
#include "unity.h"

handler_fn_t test_handler;

void faultInit(void)
{
  // Enable BusFault, UsageFault and MemFault
  SCB->SHCSR |= 0x0
#if defined(SCB_SHCSR_BUSFAULTENA_Msk)
                | SCB_SHCSR_BUSFAULTENA_Msk
#endif
#if defined(SCB_SHCSR_USGFAULTENA_Msk)
                | SCB_SHCSR_USGFAULTENA_Msk
#endif
#if defined(SCB_SHCSR_MEMFAULTENA_Msk)
                | SCB_SHCSR_MEMFAULTENA_Msk
#endif
  ;
  test_handler = NULL;
}

void faultSetHandler(handler_fn_t h)
{
  test_handler = h;
}

void HardFault_Handler(void)
{
  if (test_handler) {
    test_handler();
    return;
  }

  UnityPrintf("HardFault\n");

  while (1) {
    // do nothing
  }
}

#if defined(SCB_SHCSR_USGFAULTENA_Msk)

void UsageFault_Handler(void)
{
  UnityPrintf("UsageFault\n");
  if (SCB->CFSR & SCB_CFSR_DIVBYZERO_Msk) {
    UnityPrintf("Division by zero error\n");
  }
  if (SCB->CFSR & SCB_CFSR_UNALIGNED_Msk) {
    UnityPrintf("Unaligned access error\n");
  }
  if (SCB->CFSR & SCB_CFSR_NOCP_Msk) {
    UnityPrintf("No co-processor error\n");
  }
  if (SCB->CFSR & SCB_CFSR_INVPC_Msk) {
    UnityPrintf("Invalid PC load error\n");
  }
  if (SCB->CFSR & SCB_CFSR_INVSTATE_Msk) {
    UnityPrintf("Invalid state error\n");
  }
  if (SCB->CFSR & SCB_CFSR_UNDEFINSTR_Msk) {
    UnityPrintf("Undefined instruction error\n");
  }

  while (1) {
    // do nothing
  }
}

#endif

#if defined(SCB_SHCSR_BUSFAULTENA_Msk)

void BusFault_Handler(void)
{
  if (test_handler) {
    test_handler();
    return;
  }

  char msg[128];

  UnityPrintf("BusFault\n");
  UnityPrintf(" CFSR=0x%08X\n", SCB->CFSR);
  if (SCB->CFSR & SCB_CFSR_BFARVALID_Msk) {
    UnityPrintf(" BFAR=0x%08X (valid)\n", SCB->BFAR);
  } else {
    UnityPrintf(" BFAR=0x%08X (invalid)\n", SCB->BFAR);
  }
  if (SCB->CFSR & SCB_CFSR_IBUSERR_Msk) {
    UnityPrintf("Instruction bus error\n");
  }
  if (SCB->CFSR & SCB_CFSR_PRECISERR_Msk) {
    UnityPrintf("Precise data bus error\n");
  }
  if (SCB->CFSR & SCB_CFSR_IMPRECISERR_Msk) {
    UnityPrintf("Imprecise data bus error\n");
  }
  if (SCB->CFSR & SCB_CFSR_UNSTKERR_Msk) {
    UnityPrintf("BusFault on unstacking from exception\n");
  }
  if (SCB->CFSR & SCB_CFSR_STKERR_Msk) {
    UnityPrintf("BusFault on stacking for exception entry\n");
  }
#if defined(__FPU_PRESENT)
  if (SCB->CFSR & SCB_CFSR_LSPERR_Msk) {
    UnityPrintf("BusFault during floating point lazy state preservation\n");
  }
#endif

  SCB->SHCSR = SCB->SHCSR & ~SCB_SHCSR_BUSFAULTACT_Msk;

  snprintf(msg, sizeof(msg), "BusFault error");
  TEST_FAIL_MESSAGE(msg);

  while (1) {
    // do nothing
  }
}

#endif // SCB_SHCSR_BUSFAULTENA_Msk
