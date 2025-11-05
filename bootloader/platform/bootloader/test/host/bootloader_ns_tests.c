/***************************************************************************//**
 * @file bootloader_ns_test.c
 *******************************************************************************
 * @section License
 * <b>Copyright 2022 Silicon Laboratories, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#include "em_device.h"

#define TEST_RAM_SIZE          (0x500u)
#define TEST_RAM_BASE          (SRAM_BASE + SRAM_SIZE - TEST_RAM_SIZE)
#define TEST_RAM_KNOWN_PATTERN (0xac)

#if defined(BOOTLOADER_SECURE)

#include <string.h>

__attribute__((cmse_nonsecure_entry))
void clean_up_ns_test_traces(void)
{
#if defined(CRYPTOACC_PRESENT) && defined(SMU_PPUSATD1_AHBRADIO)
  SMU->PPUSATD1_SET = SMU_PPUSATD1_AHBRADIO;
  SMU->LOCK = 0;
#if defined(BOOTLOADER_APPLOADER)
  CMU_NS->CLKEN1_CLR = CMU_CLKEN1_SMU;
#else
  CMU->CLKEN1_CLR = CMU_CLKEN1_SMU;
#endif // BOOTLOADER_APPLOADER
#endif // CRYPTOACC_PRESENT && SMU_PPUSATD1_AHBRADIO
}

void prepare_bootloader_ns_test(void)
{
#if defined(CRYPTOACC_PRESENT) && defined(SMU_PPUSATD1_AHBRADIO)
#if defined(BOOTLOADER_APPLOADER)
  CMU_NS->CLKEN1_SET = CMU_CLKEN1_SMU;
#else
  CMU->CLKEN1_SET = CMU_CLKEN1_SMU;
#endif // BOOTLOADER_APPLOADER
  SMU->LOCK = SMU_LOCK_SMULOCKKEY_UNLOCK;
  SMU->PPUSATD1_CLR = SMU_PPUSATD1_AHBRADIO;
#endif // CRYPTOACC_PRESENT && SMU_PPUSATD1_AHBRADIO

  // Fill the last 16 kB (TEST_RAM_SIZE) with some known data
  memset((void *)TEST_RAM_BASE, TEST_RAM_KNOWN_PATTERN, TEST_RAM_SIZE);
}

#else

#include <stddef.h>

extern void clean_up_ns_test_traces(void);

// Do not use UNITY in order to make the test code fit into the NS bootloader
#define TEST_ASSERT_EQUAL(expected, actual) if (expected != actual) { while (1) {} }
#define TEST_ASSERT_NOT_EQUAL(expected, actual) if (expected == actual) { while (1) {} }
#define TEST_ASSERT() while (1) {}

#if defined(CRYPTOACC_PRESENT) && defined(SMU_PPUSATD1_AHBRADIO)
static void verify_FRCRAM_contents(void)
{
  #define VSE_REPLY_SIZE_WORDS                  5
  #define VSE_REPLY_STATUS_TRUSTZONE_ROOT_KEY   (1 << 22)
  #define TZ_SRK_SIZE_WORDS                     8

  typedef struct {
    uint32_t data[TZ_SRK_SIZE_WORDS];
    uint32_t checksum;
  } vse_srk_t;

  // The default VSE mailbox output address
  uint32_t *vse_reply = (uint32_t*)RDMEM_FRCRAM_NS_MEM_BASE;

  // Verify that the TrustZone Root Key flag is present in the reply status
  uint32_t vse_reply_status = vse_reply[2];
  if ((vse_reply_status & VSE_REPLY_STATUS_TRUSTZONE_ROOT_KEY) == 0) {
    TEST_ASSERT();
  }
  // Read the SRK next to the mailbox, i.e. after the VSE reply header, data length and
  // checksum (+1).
  uint32_t vse_data_length = vse_reply[4];
  vse_srk_t *srk = (vse_srk_t*)(vse_reply + VSE_REPLY_SIZE_WORDS + vse_data_length + 1);

  for (size_t i = 0u; i < sizeof(vse_srk_t); i++) {
    TEST_ASSERT_EQUAL(0u, *((uint8_t *)srk + i));
  }
}
#endif // CRYPTOACC_PRESENT && SMU_PPUSATD1_AHBRADIO

static void verify_RAM_contents(void)
{
  uint8_t *ram_ptr = (uint8_t *)TEST_RAM_BASE;
  for (uint32_t i = 0u; i < TEST_RAM_SIZE; i++) {
    TEST_ASSERT_EQUAL(0u, *(ram_ptr + i));
  }
}

void bootloader_ns_test(void)
{
  #if defined(CRYPTOACC_PRESENT) && defined(SMU_PPUSATD1_AHBRADIO)
  verify_FRCRAM_contents();
  #endif
  verify_RAM_contents();

  clean_up_ns_test_traces();
}

#endif // BOOTLOADER_SECURE
