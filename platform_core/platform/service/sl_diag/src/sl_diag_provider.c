/***************************************************************************//**
 * @file
 * @brief SL Diagnostic provider registry and provider orchestration
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

#include "em_device.h"
#include "sl_common.h"
#include "sl_core.h"
#include "sli_diag.h"
#include "sli_diag_internal.h"

#if SL_DIAG_ENABLE

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

// Every SLI_DIAG_CONTEXT_* flag.
#define CONTEXT_FLAGS_ALL                       \
  (SLI_DIAG_CONTEXT_TASK | SLI_DIAG_CONTEXT_ISR \
   | SLI_DIAG_CONTEXT_FAULT | SLI_DIAG_CONTEXT_PRE_RESET)

// Trigger mask bits of every capture trigger, SL_DIAG_TRIGGER_NONE excluded.
// Valid for 1 to 32 triggers.
#define TRIGGER_MASK_ALL                                    \
  ((0xFFFFFFFFu >> (32u - (uint32_t)SL_DIAG_TRIGGER_COUNT)) \
   & ~SLI_DIAG_TRIGGER_MASK(SL_DIAG_TRIGGER_NONE))

// IPSR exception numbers that no configurable interrupt can preempt.
#define EXCEPTION_NMI         2u
#define EXCEPTION_HARD_FAULT  3u

// Provider has no outcome yet in the current capture.
#define OUTCOME_NOT_EVALUATED  0xFFu

// The core has a DWT cycle counter. Not on ARMv6-M or ARMv8-M Baseline.
#if defined(DWT) && defined(DWT_CTRL_CYCCNTENA_Msk) && defined(DWT_CTRL_NOCYCCNT_Msk) \
  && defined(CoreDebug) && defined(CoreDebug_DEMCR_TRCENA_Msk) && defined(CONTROL_nPRIV_Msk)
#define CYCLE_COUNTER_PRESENT  1
#else
#define CYCLE_COUNTER_PRESENT  0
#endif

#if CYCLE_COUNTER_PRESENT
#include "sl_cycle_counter.h"
#endif

/*******************************************************************************
 ********************************   TYPES   ************************************
 ******************************************************************************/

// How the time of a callback was charged.
typedef enum {
  CHARGE_MEASURED,      // Elapsed time, measured reliably.
  CHARGE_CONSERVATIVE,  // Counted time above the declared time; not reliable.
  CHARGE_DECLARED       // Declared time; elapsed time unknown.
} latency_charge_t;

// Provider linked into the build and budgeted in the retained slot.
typedef struct {
  uint16_t id;                // Provider ID.
  uint16_t max_output_bytes;  // Budgeted maximum payload in bytes.
} linked_provider_t;

// Start of a callback time measurement.
typedef struct {
  uint32_t start_cycles;  // Cycle counter at the start.
  uint32_t core_hz;       // Core clock at the start, 0 if not measured.
  bool unpreemptible;     // No interrupt could preempt the callback.
} latency_timer_t;

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

// Providers allowed to register.
static const linked_provider_t linked_providers[] = { SLI_DIAG_LINKED_PROVIDER_ENTRIES };

static_assert(SL_DIAG_TRIGGER_COUNT <= 32u, "Triggers do not fit trigger_mask");
static_assert(SLI_DIAG_PROVIDER_REGISTRY_CAPACITY >= 1u, "The provider registry needs at least one entry");
static_assert(SL_ARRAY_SIZE(linked_providers) == SLI_DIAG_LINKED_PROVIDER_COUNT,
              "SLI_DIAG_LINKED_PROVIDER_ENTRIES must list SLI_DIAG_LINKED_PROVIDER_COUNT providers");

// Each linked provider must have a provider ID and a budget that fits its
// 16-bit table entry. The slot is sized from all of them, so each one fits.
#define CHECK_LINKED_PROVIDER(id_, bytes_)                                   \
  static_assert(((id_) > SL_DIAG_TLV_PROVIDER_STATUS) && ((id_) <= 0xFFFFu), \
                "Linked provider ID is not a provider ID");                  \
  static_assert((bytes_) <= 0xFFFFu, "Linked provider budget does not fit max_output_bytes");
SLI_DIAG_LINKED_PROVIDER_LIST(CHECK_LINKED_PROVIDER)

// Runtime registry of registered providers.
static const sli_diag_provider_t * volatile registry[SLI_DIAG_PROVIDER_REGISTRY_CAPACITY];
static volatile uint32_t registry_count = 0u;

/*******************************************************************************
 ***************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/*******************************************************************************
 * Finds a registered provider among the first registered_count registry
 * entries. Returns its index, or registered_count if it is not registered.
 ******************************************************************************/
static uint32_t find_registered_provider(uint16_t id, uint32_t registered_count)
{
  uint32_t i;

  for (i = 0u; i < registered_count; i++) {
    if (registry[i]->id == id) {
      break;
    }
  }
  return i;
}

/*******************************************************************************
 * Finds a provider in the linked provider table. Returns its index, or the
 * table size if the provider is not linked.
 ******************************************************************************/
static uint32_t find_linked_provider(uint16_t id)
{
  uint32_t i;

  for (i = 0u; i < SL_ARRAY_SIZE(linked_providers); i++) {
    if (linked_providers[i].id == id) {
      break;
    }
  }
  return i;
}

#if CYCLE_COUNTER_PRESENT
/*******************************************************************************
 * Converts cycles at core_hz to microseconds, rounded up and saturated.
 * core_hz must not be 0.
 ******************************************************************************/
static uint32_t cycles_to_us(uint32_t cycles, uint32_t core_hz)
{
  uint64_t us = (((uint64_t)cycles * 1000000u) + core_hz - 1u) / core_hz;

  return (us > UINT32_MAX) ? UINT32_MAX : (uint32_t)us;
}

/*******************************************************************************
 * Checks that the caller can access the debug registers. They fault on
 * unprivileged access. Handler mode is privileged.
 ******************************************************************************/
static bool debug_registers_accessible(void)
{
  return (__get_IPSR() != 0u) || ((__get_CONTROL() & CONTROL_nPRIV_Msk) == 0u);
}

/*******************************************************************************
 * Checks that the DWT cycle counter can be read and is enabled. The
 * cycle_counter driver enables it at driver_init. The application, a debugger,
 * or the power manager may disable or pause it later.
 ******************************************************************************/
static bool cycle_counter_running(void)
{
  if (!debug_registers_accessible()) {
    return false;
  }
  return ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) != 0u)
         && ((DWT->CTRL & (DWT_CTRL_CYCCNTENA_Msk | DWT_CTRL_NOCYCCNT_Msk)) == DWT_CTRL_CYCCNTENA_Msk);
}

/*******************************************************************************
 * Checks that no configurable interrupt can preempt the caller: PRIMASK is set,
 * or the caller is the NMI or HardFault handler.
 ******************************************************************************/
static bool preemption_masked(void)
{
  uint32_t exception = __get_IPSR();

  return (__get_PRIMASK() != 0u)
         || (exception == EXCEPTION_NMI)
         || (exception == EXCEPTION_HARD_FAULT);
}
#endif

/*******************************************************************************
 * Starts measuring a callback if the cycle counter runs and the core clock is
 * known.
 ******************************************************************************/
static void latency_timer_start(latency_timer_t *timer)
{
  timer->start_cycles = 0u;
  timer->core_hz = 0u;
  timer->unpreemptible = false;
#if CYCLE_COUNTER_PRESENT
  if (cycle_counter_running() && (SystemCoreClock != 0u)) {
    timer->core_hz = SystemCoreClock;
    timer->unpreemptible = preemption_masked();
    timer->start_cycles = sl_cycle_counter_get_counter();
  }
#endif
}

/*******************************************************************************
 * Returns the time to charge for a callback and sets *charge to how it was
 * obtained.
 *
 * The cycle count gives the elapsed time only if the core clock did not
 * change and the core did not sleep during the callback. That holds when no
 * interrupt could preempt the callback and the clock is the same at both
 * ends. Otherwise a preempting handler may have slowed or stopped the clock,
 * so the larger of the declared and the counted time is charged. Without a
 * running and advancing cycle counter the declared time is charged.
 ******************************************************************************/
static uint32_t latency_timer_stop(const latency_timer_t *timer,
                                   uint32_t declared_us,
                                   latency_charge_t *charge)
{
#if CYCLE_COUNTER_PRESENT
  if ((timer->core_hz != 0u) && cycle_counter_running()) {
    uint32_t cycles = sl_cycle_counter_get_counter() - timer->start_cycles;
    uint32_t core_hz = SystemCoreClock;
    uint32_t counted_us;

    // A callback takes at least one cycle; 0 means the counter is stalled.
    if (cycles != 0u) {
      if (timer->unpreemptible && (core_hz == timer->core_hz)) {
        *charge = CHARGE_MEASURED;
        return cycles_to_us(cycles, core_hz);
      }
      if ((core_hz == 0u) || (core_hz > timer->core_hz)) {
        core_hz = timer->core_hz;
      }
      counted_us = cycles_to_us(cycles, core_hz);
      if (counted_us > declared_us) {
        *charge = CHARGE_CONSERVATIVE;
        return counted_us;
      }
    }
  }
#else
  (void)timer;
#endif
  *charge = CHARGE_DECLARED;
  return declared_us;
}

/*******************************************************************************
 * Checks the fields of a provider descriptor.
 ******************************************************************************/
static bool descriptor_is_valid(const sli_diag_provider_t *provider)
{
  // A provider ID must be a provider ID and a capture callback must be provided.
  if ((provider->id <= SL_DIAG_TLV_PROVIDER_STATUS) || (provider->capture == NULL)) {
    return false;
  }
  // A trigger mask must be valid.
  if ((provider->trigger_mask == 0u) || ((provider->trigger_mask & ~TRIGGER_MASK_ALL) != 0u)) {
    return false;
  }
  // A context flags mask must be valid.
  if ((provider->context_flags == 0u) || ((provider->context_flags & ~CONTEXT_FLAGS_ALL) != 0u)) {
    return false;
  }
  // A privacy class must be valid.
  if (provider->privacy_class > SL_DIAG_PRIVACY_SECRET) {
    return false;
  }

  // A callback always takes time, and a zero bound would never be charged.
  if (provider->max_latency_us == 0u) {
    return false;
  }

  // A provider with no output could never write its evidence TLV, and a
  // provider TLV larger than a slot could never be captured.
  if ((provider->max_output_bytes == 0u)
      || ((SLI_DIAG_TLV_HDR_BYTES + (uint32_t)provider->max_output_bytes) > SLI_DIAG_SLOT_MAX_BYTES)) {
    return false;
  }

  if ((provider->dependency_count != 0u) && (provider->dependencies == NULL)) {
    return false;
  }
  // A dependency on itself, on a provider that cannot register in this build,
  // or listed twice can never be met as intended.
  for (uint32_t i = 0u; i < provider->dependency_count; i++) {
    if ((provider->dependencies[i] == provider->id)
        || (find_linked_provider(provider->dependencies[i]) == SL_ARRAY_SIZE(linked_providers))) {
      return false;
    }
    for (uint32_t j = 0u; j < i; j++) {
      if (provider->dependencies[j] == provider->dependencies[i]) {
        return false;
      }
    }
  }
  return true;
}

/*******************************************************************************
 * Checks that every dependency ran earlier in this capture and did not
 * return FAILED.
 ******************************************************************************/
static bool dependencies_met(const sli_diag_provider_t *provider,
                             const uint8_t *outcome,
                             const bool *callback_ran,
                             uint32_t registered_count)
{
  for (uint32_t i = 0u; i < provider->dependency_count; i++) {
    uint32_t index = find_registered_provider(provider->dependencies[i], registered_count);

    // Not registered, not selected or not run yet: not met.
    if ((index == registered_count) || !callback_ran[index] || (outcome[index] == SL_DIAG_PROVIDER_STATUS_FAILED)) {
      return false;
    }
  }
  return true;
}

/*******************************************************************************
 * Checks that a callback wrote exactly one well-formed TLV with the provider's
 * ID and version. writer is the record writer before the callback,
 * provider_writer the callback's writer after it.
 ******************************************************************************/
static bool evidence_is_valid(const sli_diag_provider_t *provider,
                              const sli_diag_writer_t *writer,
                              const sli_diag_writer_t *provider_writer)
{
  sli_diag_tlv_info_t tlv;
  uint32_t offset = 0u;
  uint32_t size = (uint32_t)(provider_writer->cursor - writer->cursor);

  if (provider_writer->blocks_written != (uint16_t)(writer->blocks_written + 1u)) {
    return false;
  }
  return (sli_diag_tlv_next(writer->cursor, size, &offset, &tlv) == SL_STATUS_OK)
         && (offset == size)
         && (tlv.type == provider->id)
         && (tlv.version == provider->version);
}

/*******************************************************************************
 * Applies the context, dependency, privacy, byte and time gates, then runs the
 * provider callback. Sets callback_ran[index] if the callback ran. Returns the
 * provider outcome.
 ******************************************************************************/
static sl_diag_provider_status_t evaluate_provider(uint32_t index,
                                                   sli_diag_capture_context_t *ctx,
                                                   sli_diag_writer_t *writer,
                                                   const uint8_t *outcome,
                                                   bool *callback_ran,
                                                   uint32_t registered_count,
                                                   sli_diag_provider_run_summary_t *result)
{
  const sli_diag_provider_t *provider = registry[index];
  sli_diag_writer_t provider_writer;
  sl_diag_provider_status_t status;
  latency_timer_t timer;
  latency_charge_t charge;
  uint32_t charged_us;
  uint32_t tlv_limit = SLI_DIAG_TLV_HDR_BYTES + (uint32_t)provider->max_output_bytes;

  // Check that the context flags matches with the provider's context flags.
  if ((provider->context_flags & ctx->context_flags) != ctx->context_flags) {
    return SL_DIAG_PROVIDER_STATUS_SKIPPED;
  }
  // Check that all dependencies ran and did not return FAILED.
  if (!dependencies_met(provider, outcome, callback_ran, registered_count)) {
    return SL_DIAG_PROVIDER_STATUS_SKIPPED;
  }
  // Check that the privacy class is valid.
  if (provider->privacy_class > SL_DIAG_MAX_PRIVACY_CLASS) {
    return SL_DIAG_PROVIDER_STATUS_REDACTED;
  }
  // Check that the budget is not exceeded.
  if (tlv_limit > sli_diag_writer_bytes_remaining(writer)) {
    result->budget_limited = true;
    return SL_DIAG_PROVIDER_STATUS_TRUNCATED;
  }
  // Check that the time remaining is not exceeded.
  if (provider->max_latency_us > ctx->time_remaining_us) {
    result->budget_limited = true;
    return SL_DIAG_PROVIDER_STATUS_SKIPPED;
  }

  // The provider writes through a copy limited to its declared output, so it
  // cannot use the space of later providers.
  provider_writer = *writer;
  provider_writer.end = writer->cursor + tlv_limit;

  // Update the bytes remaining for the current provider.
  ctx->bytes_remaining = sli_diag_writer_bytes_remaining(&provider_writer);

  // Start the timer.
  latency_timer_start(&timer);

  // Run the provider.
  status = provider->capture(ctx, &provider_writer, provider->state);
  charged_us = latency_timer_stop(&timer, provider->max_latency_us, &charge);
  callback_ran[index] = true;

  // OK without the provider's evidence TLV is reported as FAILED.
  if ((status == SL_DIAG_PROVIDER_STATUS_OK)
      && !evidence_is_valid(provider, writer, &provider_writer)) {
    status = SL_DIAG_PROVIDER_STATUS_FAILED;
  }

  // Update the writer cursor.
  writer->cursor = provider_writer.cursor;
  writer->blocks_written = provider_writer.blocks_written;
  writer->bytes_written = provider_writer.bytes_written;

  // A slow callback is not interrupted. Only a reliable measurement shows an
  // overrun; the other charges are at least the declared time.
  // TODO: Measure time on cores without a DWT cycle counter, and in preemptible
  // contexts, in PLATFORM_HYD-9171 once a fault-safe time base is defined.
  if (charge != CHARGE_MEASURED) {
    result->time_unmeasured = true;
  } else if (charged_us > provider->max_latency_us) {
    result->latency_overrun = true;
  }

  // A charge above the time left exhausts the capture budget. The declared
  // charge passed the time gate, so only a measured or conservative charge
  // can do this.
  if (charged_us > ctx->time_remaining_us) {
    result->budget_limited = true;
  }

  // Update the time remaining.
  ctx->time_remaining_us = (charged_us >= ctx->time_remaining_us)
                           ? 0u
                           : (ctx->time_remaining_us - charged_us);

  if ((uint32_t)status > (uint32_t)SL_DIAG_PROVIDER_STATUS_OBSERVATION_FAILED) {
    status = SL_DIAG_PROVIDER_STATUS_FAILED;
  }

  // Update the budget limited flag.
  if (status == SL_DIAG_PROVIDER_STATUS_TRUNCATED) {
    result->budget_limited = true;
  }
  return status;
}

/***************************************************************************//**
 * Registers a diagnostic provider.
 ******************************************************************************/
sl_status_t sli_diag_register_provider(const sli_diag_provider_t *provider)
{
  sl_status_t status = SL_STATUS_OK;
  uint32_t linked_index;
  uint32_t registered_count;
  CORE_DECLARE_IRQ_STATE;

  // A provider descriptor must be provided.
  if (provider == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // A provider descriptor must be valid.
  if (!descriptor_is_valid(provider)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Only linked providers have retained space and a registry entry.
  linked_index = find_linked_provider(provider->id);
  if (linked_index == SL_ARRAY_SIZE(linked_providers)) {
    return SL_STATUS_NOT_SUPPORTED;
  }

  // A provider's budget must not exceed the linked provider's budget.
  if (provider->max_output_bytes > linked_providers[linked_index].max_output_bytes) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Register the provider.
  CORE_ENTER_CRITICAL();
  registered_count = registry_count;
  if (find_registered_provider(provider->id, registered_count) != registered_count) {
    status = SL_STATUS_ALREADY_EXISTS;
  } else if (registered_count >= SLI_DIAG_PROVIDER_REGISTRY_CAPACITY) {
    status = SL_STATUS_FULL;
  } else {
    registry[registered_count] = provider;
    registry_count = registered_count + 1u;
  }
  CORE_EXIT_CRITICAL();

  return status;
}

/***************************************************************************//**
 * Writes a Provider status TLV.
 ******************************************************************************/
sl_status_t sli_diag_write_tlv_provider_status(sli_diag_writer_t *writer,
                                               uint16_t provider_id,
                                               sl_diag_provider_status_t status)
{
  sl_diag_provider_status_tlv_payload_t payload;

  if ((uint32_t)status > (uint32_t)SL_DIAG_PROVIDER_STATUS_OBSERVATION_FAILED) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  payload.provider_id = provider_id;
  payload.status = status;
  payload.reserved = 0u;
  return sli_diag_writer_append_tlv(writer,
                                    SL_DIAG_TLV_PROVIDER_STATUS,
                                    SLI_DIAG_CORE_TLV_VERSION,
                                    &payload,
                                    (uint16_t)sizeof(payload));
}

/*******************************************************************************
 * Checks that a capture profile and caller context are usable.
 ******************************************************************************/
static bool capture_profile_is_valid(const sli_diag_capture_profile_t *profile,
                                     uint32_t context_flags)
{
  // A trigger must be valid.
  if (profile->trigger == SL_DIAG_TRIGGER_NONE) {
    return false;
  }
  if (profile->trigger >= SL_DIAG_TRIGGER_COUNT) {
    return false;
  }
  // A non-empty provider list must point at provider IDs.
  if ((profile->provider_count != 0u) && (profile->providers == NULL)) {
    return false;
  }

  // A context flags mask must be valid.
  if (context_flags == 0u) {
    return false;
  }
  if ((context_flags & ~CONTEXT_FLAGS_ALL) != 0u) {
    return false;
  }
  return true;
}

/***************************************************************************//**
 * Runs the providers of a capture profile into a record.
 ******************************************************************************/
sl_status_t sli_diag_run_providers(sli_diag_writer_t *writer,
                                   const sli_diag_capture_profile_t *profile,
                                   sli_diag_capture_depth_t depth,
                                   uint32_t context_flags,
                                   const void *scratch,
                                   sli_diag_provider_run_summary_t *result)
{
  sli_diag_capture_context_t ctx;
  uint8_t outcome[SLI_DIAG_PROVIDER_REGISTRY_CAPACITY];
  bool callback_ran[SLI_DIAG_PROVIDER_REGISTRY_CAPACITY];
  uint32_t registered_count;

  // Writer, profile and result must be provided.
  if ((writer == NULL) || (profile == NULL) || (result == NULL)) {
    return SL_STATUS_NULL_POINTER;
  }

  // A capture profile must be valid.
  if (!capture_profile_is_valid(profile, context_flags)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // Initialize the result.
  result->evaluated_count = 0u;
  result->not_ok_count = 0u;
  result->budget_limited = false;
  result->status_tlv_dropped = false;
  result->latency_overrun = false;
  result->time_unmeasured = false;

  // Initialize the outcome and callback_ran flags.
  for (uint32_t i = 0u; i < SLI_DIAG_PROVIDER_REGISTRY_CAPACITY; i++) {
    outcome[i] = OUTCOME_NOT_EVALUATED;
    callback_ran[i] = false;
  }

  // The depth is clamped so a caller cannot exceed the trigger's recipe.
  ctx.trigger = profile->trigger;
  ctx.depth = (depth < profile->max_depth) ? depth : profile->max_depth;
  ctx.context_flags = context_flags;
  ctx.bytes_remaining = 0u;
  ctx.time_remaining_us = profile->time_budget_us;
  ctx.scratch = scratch;

  // Providers registered during this capture are not run.
  registered_count = registry_count;

  for (uint32_t i = 0u; i < profile->provider_count; i++) {
    const sli_diag_provider_t *provider;
    sl_diag_provider_status_t status;
    sl_status_t write_status;
    uint32_t index;

    // Check that the writer is current.
    if (!sli_diag_owner_is_current(writer->owner_token)) {
      return SL_STATUS_ABORT;
    }

    // Not linked, not eligible for the trigger, or already evaluated:
    // nothing is emitted.
    index = find_registered_provider(profile->providers[i], registered_count);
    if (index == registered_count) {
      continue;
    }
    provider = registry[index];
    // Check that the provider is eligible for the trigger and not already evaluated.
    if (((provider->trigger_mask & SLI_DIAG_TRIGGER_MASK(ctx.trigger)) == 0u)
        || (outcome[index] != OUTCOME_NOT_EVALUATED)) {
      continue;
    }

    // Run the provider.
    status = evaluate_provider(index, &ctx, writer, outcome, callback_ran, registered_count, result);
    outcome[index] = (uint8_t)status;
    result->evaluated_count++;

    if (status != SL_DIAG_PROVIDER_STATUS_OK) {
      // Privacy redaction is intentional policy, not missing evidence. Still
      // emit the status TLV so hosts can see what was withheld.
      if (status != SL_DIAG_PROVIDER_STATUS_REDACTED) {
        result->not_ok_count++;
      }
      write_status = sli_diag_write_tlv_provider_status(writer, provider->id, status);
      if (write_status == SL_STATUS_ABORT) {
        return SL_STATUS_ABORT;
      }
      if (write_status != SL_STATUS_OK) {
        result->status_tlv_dropped = true;
      }
    }
  }

  return sli_diag_owner_is_current(writer->owner_token) ? SL_STATUS_OK : SL_STATUS_ABORT;
}

#endif // SL_DIAG_ENABLE
