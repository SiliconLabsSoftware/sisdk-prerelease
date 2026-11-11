/***************************************************************************//**
 * @file
 * @brief SL Diagnostic crash capture example functions
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "em_device.h"
#include "sl_diag.h"
#include "sl_diag_format.h"
#include "sl_log.h"
#include "sl_log_helper.h"
#include "sl_simple_button_instances.h"
#include "diag_crash_capture_app.h"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

#ifndef BUTTON_INSTANCE_0
#define BUTTON_INSTANCE_0   sl_button_btn0
#endif

// Busy-wait iterations that let the UART drain before the fault
#define OUTPUT_DRAIN_SPIN     20000u

// Buffer for the exported retained record (slot header + TLVs)
#define RECORD_BUFFER_SIZE    512u

// Bytes requested per sl_diag_read() call
#define RECORD_READ_CHUNK     64u

// Exported record header that precedes the TLV stream in sl_diag_read()
#define RECORD_HEADER_BYTES   32u

// CPU Crash TLV payload layout decoded from the retained record
#define CPU_CRASH_TLV_VERSION       1u
#define CPU_CRASH_FLAG_FRAME_VALID  (1u << 0)

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

// Set from BTN0 ISR; HardFault is raised from the super-loop after a log flush
static volatile bool hardfault_pending;

// Exported record image from sl_diag_read()
static uint8_t record_buffer[RECORD_BUFFER_SIZE];

typedef struct {
  uint32_t mask;
  const char *name;
} fault_bit_t;

// Fixed part of the CPU Crash TLV payload (version 1), as exported by sl_diag_read()
typedef struct {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t xpsr;
  uint32_t sp;
  uint32_t cfsr;
  uint32_t hfsr;
  uint32_t bfar;
  uint32_t mmfar;
  uint32_t file_hash;
  uint32_t expression_hash;
  uint32_t line;
  uint32_t active_thread_id;
  uint32_t exc_return;
  uint32_t flags;
} cpu_crash_payload_t;

// Configurable Fault Status Register bits (ARMv8-M)
static const fault_bit_t cfsr_bits[] = {
  { 1UL << 0, "IACCVIOL (instruction access violation)" },
  { 1UL << 1, "DACCVIOL (data access violation)" },
  { 1UL << 3, "MUNSTKERR (MemManage fault on unstacking)" },
  { 1UL << 4, "MSTKERR (MemManage fault on stacking)" },
  { 1UL << 5, "MLSPERR (MemManage fault on FP lazy state preservation)" },
  { 1UL << 7, "MMARVALID (MMFAR holds a valid address)" },
  { 1UL << 8, "IBUSERR (instruction bus error)" },
  { 1UL << 9, "PRECISERR (precise data bus error)" },
  { 1UL << 10, "IMPRECISERR (imprecise data bus error)" },
  { 1UL << 11, "UNSTKERR (BusFault on unstacking)" },
  { 1UL << 12, "STKERR (BusFault on stacking)" },
  { 1UL << 13, "LSPERR (BusFault on FP lazy state preservation)" },
  { 1UL << 15, "BFARVALID (BFAR holds a valid address)" },
  { 1UL << 16, "UNDEFINSTR (undefined instruction)" },
  { 1UL << 17, "INVSTATE (invalid EPSR state)" },
  { 1UL << 18, "INVPC (invalid EXC_RETURN)" },
  { 1UL << 19, "NOCP (no coprocessor)" },
  { 1UL << 20, "STKOF (stack overflow)" },
  { 1UL << 24, "UNALIGNED (unaligned access)" },
  { 1UL << 25, "DIVBYZERO (divide by zero)" },
};

// HardFault Status Register bits
static const fault_bit_t hfsr_bits[] = {
  { 1UL << 1, "VECTTBL (vector table read fault)" },
  { 1UL << 30, "FORCED (configurable fault escalated to HardFault)" },
  { 1UL << 31, "DEBUGEVT (debug event)" },
};

/*******************************************************************************
 **************************   LOCAL FUNCTIONS   ********************************
 ******************************************************************************/

static const char *record_state_to_string(sl_diag_record_state_t state)
{
  switch (state) {
    case SL_DIAG_STATE_EMPTY:
      return "EMPTY";
    case SL_DIAG_STATE_WRITING:
      return "WRITING";
    case SL_DIAG_STATE_VALID:
      return "VALID";
    case SL_DIAG_STATE_INCOMPLETE:
      return "INCOMPLETE";
    case SL_DIAG_STATE_CORRUPT:
      return "CORRUPT";
    default:
      return "UNKNOWN";
  }
}

static void print_fault_bits(const char *reg_name,
                             uint32_t value,
                             const fault_bit_t *bits,
                             uint32_t bit_count)
{
  for (uint32_t i = 0; i < bit_count; i++) {
    if ((value & bits[i].mask) != 0u) {
      SL_PRINT_STRING_INFO("    %s: %s\r\n", reg_name, bits[i].name);
    }
  }
}

static void output_drain(void)
{
  volatile uint32_t spin = OUTPUT_DRAIN_SPIN;

  while (spin > 0u) {
    spin--;
  }
}

/***************************************************************************//**
 * Read the exported retained record (header + TLV bytes) into record_buffer.
 *
 * @param[in] total_size  TLV payload size from sl_diag_get_info().
 * @param[out] exported_len  Bytes read into record_buffer on success.
 *
 * @return SL_STATUS_OK on success.
 ******************************************************************************/
static sl_status_t read_exported_record(uint32_t total_size, uint32_t *exported_len)
{
  uint32_t offset = 0u;
  uint32_t returned_len;
  uint32_t request_len;
  uint32_t expected_len;
  sl_status_t status;

  // sl_diag_read() exports the 32-byte record header followed by total_size TLV bytes
  expected_len = RECORD_HEADER_BYTES + total_size;
  if ((exported_len == NULL) || (expected_len > sizeof(record_buffer))) {
    SL_PRINT_STRING_ERROR("Retained record (%lu bytes: %u header + %lu TLV) does not fit in the %u byte buffer\r\n",
                          (unsigned long)expected_len,
                          (unsigned int)RECORD_HEADER_BYTES,
                          (unsigned long)total_size,
                          (unsigned int)sizeof(record_buffer));
    return SL_STATUS_WOULD_OVERFLOW;
  }

  while (offset < sizeof(record_buffer)) {
    request_len = sizeof(record_buffer) - offset;
    if (request_len > RECORD_READ_CHUNK) {
      request_len = RECORD_READ_CHUNK;
    }

    status = sl_diag_read(offset, &record_buffer[offset], request_len, &returned_len);
    if (status != SL_STATUS_OK) {
      SL_PRINT_STRING_ERROR("sl_diag_read failed at offset %lu (status 0x%04lX)\r\n",
                            (unsigned long)offset,
                            (unsigned long)status);
      return status;
    }
    if (returned_len == 0u) {
      break;
    }
    offset += returned_len;
  }

  if (offset < expected_len) {
    SL_PRINT_STRING_ERROR("sl_diag_read returned %lu bytes, expected at least %lu\r\n",
                          (unsigned long)offset,
                          (unsigned long)expected_len);
    return SL_STATUS_FAIL;
  }

  *exported_len = offset;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Locate the CPU Crash TLV in the exported record and copy its fixed payload.
 ******************************************************************************/
static bool find_cpu_crash_payload(uint32_t exported_len, cpu_crash_payload_t *payload)
{
  uint32_t offset;

  if ((payload == NULL) || (exported_len < RECORD_HEADER_BYTES)) {
    return false;
  }

  offset = RECORD_HEADER_BYTES;
  while ((exported_len - offset) >= sizeof(sl_diag_tlv_header_t)) {
    sl_diag_tlv_header_t tlv_header;
    uint32_t payload_len;

    (void)memcpy(&tlv_header, &record_buffer[offset], sizeof(tlv_header));
    payload_len = tlv_header.length;
    if (payload_len > (exported_len - offset - sizeof(tlv_header))) {
      return false;
    }
    if (tlv_header.type == SL_DIAG_PROVIDER_CPU_CRASH) {
      if ((tlv_header.version != CPU_CRASH_TLV_VERSION)
          || (payload_len < sizeof(*payload))) {
        return false;
      }
      (void)memcpy(payload, &record_buffer[offset + sizeof(tlv_header)], sizeof(*payload));
      return true;
    }
    offset += sizeof(tlv_header) + payload_len;
  }

  return false;
}

/***************************************************************************//**
 * Print decoded CPU Crash evidence from the SL_DIAG_PROVIDER_CPU_CRASH TLV.
 ******************************************************************************/
static void print_cpu_crash_evidence(const cpu_crash_payload_t *payload)
{
  SL_PRINT_STRING_INFO("CPU crash evidence\r\n");
  SL_PRINT_STRING_INFO("  PC   : 0x%08lX  <- faulting instruction\r\n", (unsigned long)payload->pc);
  SL_PRINT_STRING_INFO("  LR   : 0x%08lX\r\n", (unsigned long)payload->lr);
  SL_PRINT_STRING_INFO("  SP   : 0x%08lX\r\n", (unsigned long)payload->sp);
  SL_PRINT_STRING_INFO("  xPSR : 0x%08lX\r\n", (unsigned long)payload->xpsr);
  SL_PRINT_STRING_INFO("  R0   : 0x%08lX  R1 : 0x%08lX\r\n",
                       (unsigned long)payload->r0,
                       (unsigned long)payload->r1);
  SL_PRINT_STRING_INFO("  R2   : 0x%08lX  R3 : 0x%08lX\r\n",
                       (unsigned long)payload->r2,
                       (unsigned long)payload->r3);
  SL_PRINT_STRING_INFO("  R12  : 0x%08lX\r\n", (unsigned long)payload->r12);
  SL_PRINT_STRING_INFO("  CFSR : 0x%08lX\r\n", (unsigned long)payload->cfsr);
  print_fault_bits("CFSR", payload->cfsr, cfsr_bits, sizeof(cfsr_bits) / sizeof(cfsr_bits[0]));
  SL_PRINT_STRING_INFO("  HFSR : 0x%08lX\r\n", (unsigned long)payload->hfsr);
  print_fault_bits("HFSR", payload->hfsr, hfsr_bits, sizeof(hfsr_bits) / sizeof(hfsr_bits[0]));
  SL_PRINT_STRING_INFO("  BFAR : 0x%08lX  MMFAR : 0x%08lX\r\n",
                       (unsigned long)payload->bfar,
                       (unsigned long)payload->mmfar);
  if ((payload->flags & CPU_CRASH_FLAG_FRAME_VALID) == 0u) {
    SL_PRINT_STRING_WARN("  stacked exception frame was not marked valid\r\n");
  }
}

/***************************************************************************//**
 * Print the exported retained record as one continuous hex stream ("00 00 ...").
 * The formatted logger does not append a newline, so each byte continues
 * the same line. Calls stay under the 200-byte line limit.
 ******************************************************************************/
static bool print_retained_raw_dump(uint32_t exported_len, uint32_t total_size)
{
  if (exported_len < (RECORD_HEADER_BYTES + total_size)) {
    return false;
  }

  SL_PRINT_STRING_INFO("=== STREAM ===\r\n");
  for (uint32_t i = 0; i < exported_len; i++) {
    SL_PRINT_STRING_INFO("%02X ", (unsigned int)record_buffer[i]);
  }
  SL_PRINT_STRING_INFO("\r\n");
  return true;
}

/***************************************************************************//**
 * Print the record summary (sl_diag), the decoded CPU Crash TLV, and a raw
 * hex dump of the retained exported record.
 *
 * @return SL_STATUS_OK when the export was printed at INFO.
 *         Any other status means the record was not delivered and must be kept.
 ******************************************************************************/
static sl_status_t print_record(void)
{
  sl_diag_info_t info;
  cpu_crash_payload_t crash;
  uint32_t exported_len = 0u;
  sl_status_t status;

  // The runtime level can be raised above the compile-time ceiling. INFO
  // output is then suppressed, which is not a delivered export.
  if ((sl_log_level_t)SL_LOG_CONFIG_LEVEL_INFO < sl_log_get_loglevel()) {
    return SL_STATUS_NOT_AVAILABLE;
  }

  status = sl_diag_get_info(&info);
  if (status != SL_STATUS_OK) {
    SL_PRINT_STRING_ERROR("No retained crash record (status 0x%04lX)\r\n", (unsigned long)status);
    return status;
  }

  SL_PRINT_STRING_INFO("SL Diagnostic record\r\n");
  SL_PRINT_STRING_INFO("SL Diagnostic profile: %u\r\n", (unsigned int)SL_DIAG_PROFILE);
  SL_PRINT_STRING_INFO("  state          : %s\r\n", record_state_to_string(info.state));
  SL_PRINT_STRING_INFO("  core_id        : %u\r\n", (unsigned int)info.core_id);
  SL_PRINT_STRING_INFO("  format_version : %u\r\n", (unsigned int)info.format_version);
  SL_PRINT_STRING_INFO("  generation     : %lu\r\n", (unsigned long)info.generation);
  SL_PRINT_STRING_INFO("  block_count    : %u\r\n", (unsigned int)info.block_count);
  SL_PRINT_STRING_INFO("  total_size     : %lu bytes\r\n", (unsigned long)info.total_size);
#if SL_DIAG_INCIDENT_METADATA_ENABLE
  SL_PRINT_STRING_INFO("  trigger        : %u\r\n", (unsigned int)info.trigger);
  SL_PRINT_STRING_INFO("  reason_id      : 0x%04lX\r\n", (unsigned long)info.reason_id);
  SL_PRINT_STRING_INFO("  detail         : 0x%08lX\r\n", (unsigned long)info.detail);
  SL_PRINT_STRING_INFO("  uptime_ms      : %lu\r\n", (unsigned long)info.uptime_ms);
  SL_PRINT_STRING_INFO("  boot_id        : %lu\r\n", (unsigned long)info.boot_id);
  SL_PRINT_STRING_INFO("  reset_reason   : 0x%08lX\r\n", (unsigned long)info.reset_reason);
#endif
#if SL_DIAG_REPEAT_COUNT_ENABLE
  SL_PRINT_STRING_INFO("  repeat_count   : %u\r\n", (unsigned int)info.repeat_count);
#endif

  status = read_exported_record(info.total_size, &exported_len);
  if (status != SL_STATUS_OK) {
    return status;
  }

  if (find_cpu_crash_payload(exported_len, &crash)) {
    print_cpu_crash_evidence(&crash);
  } else {
    SL_PRINT_STRING_WARN("No CPU Crash TLV found in the retained record\r\n");
  }

  if (!print_retained_raw_dump(exported_len, info.total_size)) {
    return SL_STATUS_FAIL;
  }

  // Formatted output is written directly to the console. sl_log_flush() only
  // drains the compact Host ring buffer, and on this backend it returns
  // SL_STATUS_OK without checking that write.
  return SL_STATUS_OK;
}

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Raise a HardFault by writing to an unmapped address.
 *
 * Crash Capture (provider) captures the CPU state, seals it into the SL
 * Diagnostic retained record and resets the device.
 ******************************************************************************/
static void raise_hardfault(void)
{
  SL_PRINT_STRING_WARN("Raising a HardFault (store to unmapped address). The device resets...\r\n");
  (void)sl_log_flush();
  output_drain();
  output_drain();

  // Crash Capture enables the MemManage, BusFault and UsageFault handlers.
  // Disabling them escalates the BusFault below to a HardFault (same approach
  // as the Crash Capture HardFault unit test).
  SCB->SHCSR &= ~(SCB_SHCSR_MEMFAULTENA_Msk
                  | SCB_SHCSR_BUSFAULTENA_Msk
                  | SCB_SHCSR_USGFAULTENA_Msk);
  __DSB();
  __ISB();

  *(volatile uint32_t *)0xFFFFFFFCu = 0u;

  for (;;) {
  }
}

/***************************************************************************//**
 * Initialize the SL Diagnostic crash capture example.
 ******************************************************************************/
void diag_crash_capture_app_init(void)
{
  sl_status_t status;

  SL_PRINT_STRING_INFO("\r\nSL Diagnostic crash capture example\r\n");

  // sl_diag and Crash Capture are initialized by the service_init event,
  // before app_init(). A record sealed before the last reset is readable here.
  if (sl_diag_has_record()) {
    SL_PRINT_STRING_INFO("A crash record from a previous boot was found:\r\n");
    status = print_record();
    if (status != SL_STATUS_OK) {
      // Keep the only retained copy when the export did not complete.
      SL_PRINT_STRING_ERROR("Export failed (status 0x%04lX); retained record was not cleared.\r\n",
                            (unsigned long)status);
    } else {
      // Clear after a completed export so the next BTN0 HardFault can
      // WRITE_NEW on every profile (Crash Basic keeps the first VALID
      // record; Standard may REPEAT).
      status = sl_diag_clear(SL_DIAG_CLEAR_RECORD);
      if ((status == SL_STATUS_OK) && (sl_diag_has_record() == false)) {
        SL_PRINT_STRING_INFO("Record cleared; next BTN0 will seal a new generation.\r\n");
      } else {
        SL_PRINT_STRING_ERROR("sl_diag_clear failed (status 0x%04lX); has_record=%u.\r\n",
                              (unsigned long)status,
                              (unsigned int)sl_diag_has_record());
      }
    }
  } else {
    SL_PRINT_STRING_INFO("No crash record.\r\n");
  }

  SL_PRINT_STRING_INFO("Press BTN0 to raise a HardFault (Crash Capture seals + resets).\r\n");
  SL_PRINT_STRING_INFO("After reset.\r\n");
}

/***************************************************************************//**
 * SL Diagnostic crash capture example ticking function.
 ******************************************************************************/
void diag_crash_capture_app_process_action(void)
{
  if (hardfault_pending) {
    hardfault_pending = false;
    raise_hardfault();
  }
}

/***************************************************************************//**
 * Callback on button change — queue a HardFault (same BTN0 pattern as the
 * debug logger / simple_button samples). Do not fault from IRQ context.
 ******************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  if (sl_button_get_state(handle) != SL_SIMPLE_BUTTON_PRESSED) {
    return;
  }
  if (handle != &BUTTON_INSTANCE_0) {
    return;
  }

  hardfault_pending = true;
}
