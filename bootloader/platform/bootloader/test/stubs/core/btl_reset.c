#include "btl_reset.h"
#include "btl_reset_info.h"
#include <stdbool.h>

/// Reset signature when counter is active
 #define BOOTLOADER_RESET_COUNTER_SIGNATURE      0xABC0u
/// Mask for accessing the counter signature
 #define BOOTLOADER_RESET_COUNTER_SIGNATURE_MASK 0xFFF0u

BootloaderResetCause_t sram_base;
BootloaderResetCause_t* resetReasonBase = &sram_base;

bool firstReset = true;

void reset_setResetReason(uint16_t resetReason)
{
  BootloaderResetCause_t *cause = (resetReasonBase);

  cause->reason = resetReason;
  if (!reset_resetCounterEnabled()) {
    // Only update the signature when the counter is not in use.
    cause->signature = BOOTLOADER_RESET_SIGNATURE_VALID;
  }
}

void reset_invalidateResetReason(void)
{
  BootloaderResetCause_t *cause = (resetReasonBase);
  cause->signature = BOOTLOADER_RESET_SIGNATURE_INVALID;
}

void reset_enableResetCounter(void)
{
  BootloaderResetCause_t *cause = (resetReasonBase);

  // Set the upper 12 bits to a shorter signature and use the lower 4 bits
  // for the counter value (starts at 0).
  cause->signature = (BOOTLOADER_RESET_COUNTER_SIGNATURE
                      & BOOTLOADER_RESET_COUNTER_SIGNATURE_MASK);
}

void reset_disableResetCounter(void)
{
  BootloaderResetCause_t *cause = (resetReasonBase);
  cause->signature = BOOTLOADER_RESET_SIGNATURE_VALID;
}

bool reset_resetCounterEnabled(void)
{
  BootloaderResetCause_t* cause = resetReasonBase;

  if (firstReset) {
    cause->reason = 0UL;
    cause->signature = 0UL;
    firstReset = false;
  }

  if ((cause->signature & BOOTLOADER_RESET_COUNTER_SIGNATURE_MASK)
      == BOOTLOADER_RESET_COUNTER_SIGNATURE) {
    return true;
  } else {
    return false;
  }
}

uint8_t reset_getResetCounter(void)
{
  const BootloaderResetCause_t *cause = resetReasonBase;
  // Return 0 if the reset counter is not enabled
  return (uint8_t)(reset_resetCounterEnabled() ? cause->signature & 0xFu : 0u);
}

void reset_incrementResetCounter(void)
{
  BootloaderResetCause_t *cause = resetReasonBase;

  // Only increment when the counter signature is set
  if (reset_resetCounterEnabled()) {
    if ((cause->signature & 0xFu) < 0xFu) {
      cause->signature++;
    } else {
      // Set counter back to 0 in case of overflow.
      cause->signature &= BOOTLOADER_RESET_COUNTER_SIGNATURE_MASK;
    }
  }
}

void reset_resetWithReason(uint16_t resetReason)
{
  reset_setResetReason(resetReason);
}

uint16_t reset_classifyReset(void)
{
  const BootloaderResetCause_t* cause = resetReasonBase;

  if (cause->signature == BOOTLOADER_RESET_SIGNATURE_VALID) {
    return reset_getResetReason();
  } else if ((cause->signature == 0x0000u)
             && (cause->reason == 0x0001u)) {
    // BLE reset cause
    return BOOTLOADER_RESET_REASON_BOOTLOAD;
  } else {
    return BOOTLOADER_RESET_REASON_UNKNOWN;
  }
}

uint16_t reset_getResetReason(void)
{
  const BootloaderResetCause_t* cause = resetReasonBase;
  return cause->reason;
}
