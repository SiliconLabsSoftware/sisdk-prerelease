/***************************************************************************//**
 * @file
 * @brief TrustZone Non-secure main for Gecko Bootloader.
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc.  Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement.  This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include "core/btl_reset_ns.h"
#include "api/btl_reset_info.h"
#include "sl_main_init.h"
#include "sl_event_handler.h"
#include "sl_sleeptimer.h"

#include "em_cmu.h"
#include "em_msc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "command_interpreter.h"

// Veneers
extern BootloaderResetCause_t bootloader_nsc_getResetReason(void);
extern uint32_t bootload_nsc_getBootloaderVersion(void);

static void printBootloaderInfo(void);
void bootloaderInfo(int, char**);
void readmem_ram(int, char**);
void readmem_flash(int, char**);

// CLI
CommandState_t ciState;
char ciBuffer[256];
CommandEntry_t commands[] = {
  COMMAND_ENTRY("bootloader-info", "", bootloaderInfo, "-- Get information about bootloader"),
  COMMAND_ENTRY("read_ram", "w", readmem_ram, "-- Read ram"),
  COMMAND_ENTRY("read_flash", "w", readmem_flash, "-- Read flash"),
  COMMAND_ENTRY(NULL, NULL, NULL, NULL)
};

__STATIC_INLINE uint8_t nibbleToHex(uint8_t nibble)
{
  return (nibble > 9) ? (nibble - 10 + 'A') : (nibble + '0');
}

void bootloaderInfo(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  printBootloaderInfo();
}

void readmem_ram(int argc, char **argv)
{
  (void)argc;
  const char *hexstring = argv[1];
  int address = (int)strtol(hexstring, NULL, 0);
  uint8_t *ram_ptr = (uint8_t *)address;
  printf("%u \n", *ram_ptr);
}

void readmem_flash(int argc, char **argv)
{
  (void)argc;
  const char *hexstring = argv[1];
  int address = (int)strtol(hexstring, NULL, 0);
  uint8_t *flash_ptr = (uint8_t *)address;
  printf("%u \n", *flash_ptr);
}

void print_reset_reason(BootloaderResetCause_t resetCause)
{
  uint32_t reason = resetCause.reason;
  switch (reason) {
    case BOOTLOADER_RESET_REASON_TZ_FAULT:
      printf("Security Fault \n");
      break;
    default:
      printf("Hard reset \n");
  }
}

// Main Bootloader implementation
int main(void)
{
  // Initialize Silicon Labs device, system, service(s) and protocol stack(s).
  // Note that if the kernel is present, processing task(s) will be created by
  // this call.
  sl_main_init();

  BootloaderResetCause_t resetCause = bootloader_nsc_getResetReason();
  printf("%u\n", resetCause.reason);

  // Print a string containing "custom internal TZ Bootloader" to make tools happy
  printf("\nThis is custom internal TZ Bootloader.\n");

#if !defined(SL_TRUSTZONE_NONSECURE) // Accessing the first word of RAM is not allowed by the NS app
  // Reset reset info
  *((uint32_t *)(SRAM_BASE)) = 0;
#endif

  // Set up CLI
  ciInitState(&ciState, ciBuffer, sizeof(ciBuffer), commands);
  printf("bootloader-app> ");

  while (1) {
    char input = getchar();
    if (input != '\0' && input != 0xFF) {
      if (ciProcessInput(&ciState, &input, 1) > 0) {
        printf("bootloader-app> ");
      }
    }
  }

  return 0;
}

static void printBootloaderInfo(void)
{
  char str[] = "\r\nGecko Bootloader vX.YY.ZZ\r\n";

  uint32_t version = bootload_nsc_getBootloaderVersion();
  str[20] = nibbleToHex((version >> 24) & 0x0F);
  str[22] = (((version >> 16) & 0x0F) / 10 + '0');
  str[23] = (((version >> 16) & 0x0F) % 10 + '0');
  str[25] = ((version & 0x0F) / 10 + '0');
  str[26] = ((version & 0x0F) % 10 + '0');

  printf("Bootloader Information:\n");
  printf("  Version: %s \n", str);
}
