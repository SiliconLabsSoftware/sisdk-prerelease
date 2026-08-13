/***************************************************************************//**
 * @file
 * @brief CS supported capabilities comparison logging
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

#include "app_config.h"

#if defined(CS_CAPABILITIES_LOG) && (CS_CAPABILITIES_LOG == 1)

#include "app_log.h"
#include "app_cs_capabilities_log.h"
#include "sl_component_catalog.h"

// Route debug output through trace.h on SoC (UART + RTT) or app_log on host.
#if defined(SL_CATALOG_APP_TRACE_PRESENT)
#include "trace.h"
#define cs_cap_log_debug(...)            log_debug(__VA_ARGS__)
#define cs_cap_log_append_debug(...)     log_append_debug(__VA_ARGS__)
#else
#define cs_cap_log_debug(...)            app_log_debug(__VA_ARGS__)
#define cs_cap_log_append_debug(...)     app_log_append_debug(__VA_ARGS__)
#endif

#define CS_CAP_LOG_NL                    APP_LOG_NL
#define CS_CAP_LOG_PREFIX                "[APP] "
#define CS_CAP_LOG_INSTANCE_PREFIX       CS_CAP_LOG_PREFIX "[%u] "

// -----------------------------------------------------------------------------
// Definitions

#define CS_CAP_FIELD_COL_WIDTH           26
#define CS_CAP_VALUE_COL_WIDTH           20

// -----------------------------------------------------------------------------
// Private types

typedef struct {
  uint16_t mask;
  const char *label;
} cs_cap_bit_label_t;

typedef struct {
  uint16_t mask;
  uint8_t  duration_us;
} cs_cap_time_bit_t;

// -----------------------------------------------------------------------------
// Constants

static const cs_cap_time_bit_t cs_cap_ip_times[] = {
  { 0x0001, 10 },
  { 0x0002, 20 },
  { 0x0004, 30 },
  { 0x0008, 40 },
  { 0x0010, 50 },
  { 0x0020, 60 },
  { 0x0040, 80 },
};

static const cs_cap_time_bit_t cs_cap_fcs_times[] = {
  { 0x0001, 10 },
  { 0x0002, 20 },
  { 0x0004, 30 },
  { 0x0008, 40 },
  { 0x0010, 50 },
  { 0x0020, 60 },
  { 0x0040, 80 },
  { 0x0080, 100 },
  { 0x0100, 120 },
};

static const cs_cap_time_bit_t cs_cap_pm_times[] = {
  { 0x0001, 10 },
  { 0x0002, 20 },
};

static const cs_cap_bit_label_t cs_cap_role_labels[] = {
  { 0x01, "Initiator" },
  { 0x02, "Reflector" },
};

static const cs_cap_bit_label_t cs_cap_mode_labels[] = {
  { 0x01, "Mode 3" },
};

static const cs_cap_bit_label_t cs_cap_nadm_labels[] = {
  { 0x01, "Phase-based NADM" },
};

static const cs_cap_bit_label_t cs_cap_cs_sync_phys_labels[] = {
  { 0x02, "LE 2M CS SYNC" },
  { 0x04, "LE 2M 2BT CS SYNC" },
};

static const cs_cap_bit_label_t cs_cap_subfeature_labels[] = {
  { 0x02, "no FAE" },
  { 0x04, "algo #3c" },
  { 0x08, "phase RTT sounding" },
};

static const uint8_t cs_cap_snr_db[] = { 18, 21, 24, 27, 30 };

// -----------------------------------------------------------------------------
// Private functions

static size_t count_bit_label_lines(uint16_t value,
                                    const cs_cap_bit_label_t *labels,
                                    size_t label_count)
{
  size_t count = 0;

  for (size_t i = 0; i < label_count; i++) {
    if ((value & labels[i].mask) != 0) {
      count++;
    }
  }
  return count;
}

static const char *nth_bit_label(uint16_t value,
                                 const cs_cap_bit_label_t *labels,
                                 size_t label_count,
                                 size_t index)
{
  size_t count = 0;

  for (size_t i = 0; i < label_count; i++) {
    if ((value & labels[i].mask) != 0) {
      if (count == index) {
        return labels[i].label;
      }
      count++;
    }
  }
  return NULL;
}

static size_t count_time_lines(uint16_t value,
                               const cs_cap_time_bit_t *times,
                               size_t time_count)
{
  size_t count = 0;

  for (size_t i = 0; i < time_count; i++) {
    if ((value & times[i].mask) != 0) {
      count++;
    }
  }
  return count;
}

static bool nth_time_duration(uint16_t value,
                              const cs_cap_time_bit_t *times,
                              size_t time_count,
                              size_t index,
                              uint8_t *duration_us)
{
  size_t count = 0;

  for (size_t i = 0; i < time_count; i++) {
    if ((value & times[i].mask) != 0) {
      if (count == index) {
        *duration_us = times[i].duration_us;
        return true;
      }
      count++;
    }
  }
  return false;
}

static size_t count_tx_snr_lines(uint8_t tx_snr_capability)
{
  size_t count = 0;

  for (size_t i = 0; i < sizeof(cs_cap_snr_db); i++) {
    if ((tx_snr_capability & (uint8_t)(1U << i)) != 0) {
      count++;
    }
  }
  return count;
}

static bool nth_tx_snr_db(uint8_t tx_snr_capability, size_t index, uint8_t *snr_db)
{
  size_t count = 0;

  for (size_t i = 0; i < sizeof(cs_cap_snr_db); i++) {
    if ((tx_snr_capability & (uint8_t)(1U << i)) != 0) {
      if (count == index) {
        *snr_db = cs_cap_snr_db[i];
        return true;
      }
      count++;
    }
  }
  return false;
}

static void append_local_column_value_suffix(unsigned value,
                                             const char *suffix,
                                             size_t suffix_len)
{
  unsigned digits = (value >= 100u) ? 3u : (value >= 10u) ? 2u : 1u;
  unsigned content_len = digits + (unsigned)suffix_len;
  int pad = (int)(CS_CAP_VALUE_COL_WIDTH - content_len);

  if (pad < 0) {
    pad = 0;
  }
  cs_cap_log_append_debug("%u%s%*s", value, suffix, pad, "");
}

static void append_local_column_str(const char *value)
{
  cs_cap_log_append_debug("%-*s", CS_CAP_VALUE_COL_WIDTH, value);
}

static void append_remote_column_str(const char *value)
{
  cs_cap_log_append_debug(" | %s" CS_CAP_LOG_NL, value);
}

static void append_local_column_uint(unsigned value)
{
  cs_cap_log_append_debug("%-*u", CS_CAP_VALUE_COL_WIDTH, value);
}

static void append_remote_column_uint(unsigned value)
{
  cs_cap_log_append_debug(" | %u" CS_CAP_LOG_NL, value);
}

static void append_local_column_rtt_support(uint8_t value)
{
  if (value == 0) {
    append_local_column_str("not supported");
  } else {
    append_local_column_value_suffix((unsigned)value, " SYNC changes", 13);
  }
}

static void append_remote_column_rtt_support(uint8_t value)
{
  if (value == 0) {
    append_remote_column_str("not supported");
  } else {
    cs_cap_log_append_debug(" | %u SYNC changes" CS_CAP_LOG_NL, (unsigned)value);
  }
}

static void append_local_column_t_sw_times(uint8_t t_sw_times)
{
  switch (t_sw_times) {
    case 0x00:
      append_local_column_str("0 us");
      break;
    case 0x01:
      append_local_column_str("1 us");
      break;
    case 0x02:
      append_local_column_str("2 us");
      break;
    case 0x04:
      append_local_column_str("4 us");
      break;
    case 0x0A:
      append_local_column_str("10 us");
      break;
    default:
      cs_cap_log_append_debug("0x%02X%*s",
                              (unsigned)t_sw_times,
                              (int)(CS_CAP_VALUE_COL_WIDTH - 4u),
                              "");
      break;
  }
}

static void append_remote_column_t_sw_times(uint8_t t_sw_times)
{
  switch (t_sw_times) {
    case 0x00:
      append_remote_column_str("0 us");
      break;
    case 0x01:
      append_remote_column_str("1 us");
      break;
    case 0x02:
      append_remote_column_str("2 us");
      break;
    case 0x04:
      append_remote_column_str("4 us");
      break;
    case 0x0A:
      append_remote_column_str("10 us");
      break;
    default:
      cs_cap_log_append_debug(" | 0x%02X" CS_CAP_LOG_NL, (unsigned)t_sw_times);
      break;
  }
}

static void append_local_column_max_consecutive(uint16_t value)
{
  if (value == 0) {
    append_local_column_str("indefinite");
  } else {
    append_local_column_uint((unsigned)value);
  }
}

static void append_remote_column_max_consecutive(uint16_t value)
{
  if (value == 0) {
    append_remote_column_str("indefinite");
  } else {
    append_remote_column_uint((unsigned)value);
  }
}

static void log_row_field_start(const char *field)
{
  cs_cap_log_debug("%-*s | ", CS_CAP_FIELD_COL_WIDTH, field);
}

static void log_uint8_row(const char *field, uint8_t local, uint8_t remote)
{
  log_row_field_start(field);
  append_local_column_uint((unsigned)local);
  append_remote_column_uint((unsigned)remote);
}

static void log_rtt_support_row(const char *field, uint8_t local, uint8_t remote)
{
  log_row_field_start(field);
  append_local_column_rtt_support(local);
  append_remote_column_rtt_support(remote);
}

static void log_t_sw_times_row(uint8_t local, uint8_t remote)
{
  log_row_field_start("t_sw_times");
  append_local_column_t_sw_times(local);
  append_remote_column_t_sw_times(remote);
}

static void log_max_consecutive_row(uint16_t local, uint16_t remote)
{
  log_row_field_start("max_consecutive_procedures");
  append_local_column_max_consecutive(local);
  append_remote_column_max_consecutive(remote);
}

static void append_local_bit_label_column(uint16_t value,
                                          size_t value_count,
                                          size_t line,
                                          const cs_cap_bit_label_t *labels,
                                          size_t label_count)
{
  if (value_count == 0 && line == 0) {
    append_local_column_str("none");
  } else {
    const char *label = nth_bit_label(value, labels, label_count, line);

    if (label != NULL) {
      append_local_column_str(label);
    } else {
      append_local_column_str("");
    }
  }
}

static void append_remote_bit_label_column(uint16_t value,
                                           size_t value_count,
                                           size_t line,
                                           const cs_cap_bit_label_t *labels,
                                           size_t label_count)
{
  if (value_count == 0 && line == 0) {
    append_remote_column_str("none");
  } else {
    const char *label = nth_bit_label(value, labels, label_count, line);

    if (label != NULL) {
      append_remote_column_str(label);
    } else {
      append_remote_column_str("");
    }
  }
}

static void log_bitmask_row(const char *field,
                            uint16_t local_value,
                            uint16_t remote_value,
                            const cs_cap_bit_label_t *labels,
                            size_t label_count)
{
  size_t local_count = count_bit_label_lines(local_value, labels, label_count);
  size_t remote_count = count_bit_label_lines(remote_value, labels, label_count);
  size_t line_count = local_count;

  if (remote_count > line_count) {
    line_count = remote_count;
  }
  if (line_count == 0) {
    line_count = 1;
  }

  for (size_t line = 0; line < line_count; line++) {
    log_row_field_start((line == 0) ? field : "");
    append_local_bit_label_column(local_value, local_count, line, labels, label_count);
    append_remote_bit_label_column(remote_value, remote_count, line, labels, label_count);
  }
}

static void append_local_time_column(uint16_t value,
                                     size_t value_count,
                                     size_t line,
                                     const cs_cap_time_bit_t *times,
                                     size_t time_count)
{
  uint8_t duration_us;

  if (value_count == 0 && line == 0) {
    append_local_column_str("none");
  } else if (nth_time_duration(value, times, time_count, line, &duration_us)) {
    append_local_column_value_suffix((unsigned)duration_us, " us", 3);
  } else {
    append_local_column_str("");
  }
}

static void append_remote_time_column(uint16_t value,
                                      size_t value_count,
                                      size_t line,
                                      const cs_cap_time_bit_t *times,
                                      size_t time_count)
{
  uint8_t duration_us;

  if (value_count == 0 && line == 0) {
    append_remote_column_str("none");
  } else if (nth_time_duration(value, times, time_count, line, &duration_us)) {
    cs_cap_log_append_debug(" | %u us" CS_CAP_LOG_NL, (unsigned)duration_us);
  } else {
    append_remote_column_str("");
  }
}

static void log_time_bits_row(const char *field,
                              uint16_t local_value,
                              uint16_t remote_value,
                              const cs_cap_time_bit_t *times,
                              size_t time_count)
{
  size_t local_count = count_time_lines(local_value, times, time_count);
  size_t remote_count = count_time_lines(remote_value, times, time_count);
  size_t line_count = local_count;

  if (remote_count > line_count) {
    line_count = remote_count;
  }
  if (line_count == 0) {
    line_count = 1;
  }

  for (size_t line = 0; line < line_count; line++) {
    log_row_field_start((line == 0) ? field : "");
    append_local_time_column(local_value, local_count, line, times, time_count);
    append_remote_time_column(remote_value, remote_count, line, times, time_count);
  }
}

static void append_local_rtt_capability_column(uint8_t value, size_t line)
{
  switch (line) {
    case 0:
      append_local_column_str(((value & 0x01) != 0) ? "AA:10ns" : "AA:150ns");
      break;
    case 1:
      append_local_column_str(((value & 0x02) != 0) ? "Sounding:10ns" : "Sounding:150ns");
      break;
    case 2:
      append_local_column_str(((value & 0x04) != 0) ? "Random:10ns" : "Random:150ns");
      break;
    default:
      append_local_column_str("");
      break;
  }
}

static void append_remote_rtt_capability_column(uint8_t value, size_t line)
{
  switch (line) {
    case 0:
      append_remote_column_str(((value & 0x01) != 0) ? "AA:10ns" : "AA:150ns");
      break;
    case 1:
      append_remote_column_str(((value & 0x02) != 0) ? "Sounding:10ns" : "Sounding:150ns");
      break;
    case 2:
      append_remote_column_str(((value & 0x04) != 0) ? "Random:10ns" : "Random:150ns");
      break;
    default:
      append_remote_column_str("");
      break;
  }
}

static void log_rtt_capability_row(uint8_t local_value, uint8_t remote_value)
{
  for (size_t line = 0; line < 3; line++) {
    log_row_field_start((line == 0) ? "rtt_capability" : "");
    append_local_rtt_capability_column(local_value, line);
    append_remote_rtt_capability_column(remote_value, line);
  }
}

static void append_local_tx_snr_column(uint8_t value, size_t value_count, size_t line)
{
  uint8_t snr_db;

  if (value_count == 0 && line == 0) {
    append_local_column_str("none");
  } else if (nth_tx_snr_db(value, line, &snr_db)) {
    append_local_column_value_suffix((unsigned)snr_db, " dB", 3);
  } else {
    append_local_column_str("");
  }
}

static void append_remote_tx_snr_column(uint8_t value, size_t value_count, size_t line)
{
  uint8_t snr_db;

  if (value_count == 0 && line == 0) {
    append_remote_column_str("none");
  } else if (nth_tx_snr_db(value, line, &snr_db)) {
    cs_cap_log_append_debug(" | %u dB" CS_CAP_LOG_NL, (unsigned)snr_db);
  } else {
    append_remote_column_str("");
  }
}

static void log_tx_snr_row(uint8_t local_value, uint8_t remote_value)
{
  size_t local_count = count_tx_snr_lines(local_value);
  size_t remote_count = count_tx_snr_lines(remote_value);
  size_t line_count = local_count;

  if (remote_count > line_count) {
    line_count = remote_count;
  }
  if (line_count == 0) {
    line_count = 1;
  }

  for (size_t line = 0; line < line_count; line++) {
    log_row_field_start((line == 0) ? "tx_snr_capability" : "");
    append_local_tx_snr_column(local_value, local_count, line);
    append_remote_tx_snr_column(remote_value, remote_count, line);
  }
}

// -----------------------------------------------------------------------------
// Public functions (logging)

void app_cs_log_supported_capabilities(uint8_t conn_handle,
                                       const app_cs_capabilities_t *local,
                                       const sl_bt_evt_cs_read_remote_supported_capabilities_complete_t *remote)
{
  cs_cap_log_debug(CS_CAP_LOG_INSTANCE_PREFIX "CS supported capabilities:" CS_CAP_LOG_NL,
                   conn_handle);
  cs_cap_log_debug("%-*s | %-*s | %s" CS_CAP_LOG_NL,
                   CS_CAP_FIELD_COL_WIDTH,
                   "Field",
                   CS_CAP_VALUE_COL_WIDTH,
                   "Local",
                   "Remote");
  cs_cap_log_debug("%-*s-+-%-*s-+-%s" CS_CAP_LOG_NL,
                   CS_CAP_FIELD_COL_WIDTH,
                   "--------------------------",
                   CS_CAP_VALUE_COL_WIDTH,
                   "--------------------",
                   "--------------------");

  log_uint8_row("num_config", local->num_config, remote->num_config);
  log_max_consecutive_row(local->max_consecutive_procedures, remote->max_consecutive_procedures);
  log_uint8_row("num_antennas", local->num_antennas, remote->num_antennas);
  log_uint8_row("max_antenna_paths", local->max_antenna_paths, remote->max_antenna_paths);

  log_bitmask_row("roles",
                  local->roles,
                  remote->roles,
                  cs_cap_role_labels,
                  sizeof(cs_cap_role_labels) / sizeof(cs_cap_role_labels[0]));
  log_bitmask_row("modes",
                  local->modes,
                  remote->modes,
                  cs_cap_mode_labels,
                  sizeof(cs_cap_mode_labels) / sizeof(cs_cap_mode_labels[0]));
  log_rtt_capability_row(local->rtt_capability, remote->rtt_capability);

  log_rtt_support_row("rtt_aa_only", local->rtt_aa_only, remote->rtt_aa_only);
  log_rtt_support_row("rtt_sounding", local->rtt_sounding, remote->rtt_sounding);
  log_rtt_support_row("rtt_random_payload",
                      local->rtt_random_payload,
                      remote->rtt_random_payload);

  log_bitmask_row("nadm_sounding_capability",
                  local->nadm_sounding_capability,
                  remote->nadm_sounding_capability,
                  cs_cap_nadm_labels,
                  sizeof(cs_cap_nadm_labels) / sizeof(cs_cap_nadm_labels[0]));
  log_bitmask_row("nadm_random_capability",
                  local->nadm_random_capability,
                  remote->nadm_random_capability,
                  cs_cap_nadm_labels,
                  sizeof(cs_cap_nadm_labels) / sizeof(cs_cap_nadm_labels[0]));
  log_bitmask_row("cs_sync_phys",
                  local->cs_sync_phys,
                  remote->cs_sync_phys,
                  cs_cap_cs_sync_phys_labels,
                  sizeof(cs_cap_cs_sync_phys_labels) / sizeof(cs_cap_cs_sync_phys_labels[0]));
  log_bitmask_row("subfeatures",
                  local->subfeatures,
                  remote->subfeatures,
                  cs_cap_subfeature_labels,
                  sizeof(cs_cap_subfeature_labels) / sizeof(cs_cap_subfeature_labels[0]));

  log_time_bits_row("t_ip1_times",
                    local->t_ip1_times,
                    remote->t_ip1_times,
                    cs_cap_ip_times,
                    sizeof(cs_cap_ip_times) / sizeof(cs_cap_ip_times[0]));
  log_time_bits_row("t_ip2_times",
                    local->t_ip2_times,
                    remote->t_ip2_times,
                    cs_cap_ip_times,
                    sizeof(cs_cap_ip_times) / sizeof(cs_cap_ip_times[0]));
  log_time_bits_row("t_fcs_times",
                    local->t_fcs_times,
                    remote->t_fcs_times,
                    cs_cap_fcs_times,
                    sizeof(cs_cap_fcs_times) / sizeof(cs_cap_fcs_times[0]));
  log_time_bits_row("t_pm_times",
                    local->t_pm_times,
                    remote->t_pm_times,
                    cs_cap_pm_times,
                    sizeof(cs_cap_pm_times) / sizeof(cs_cap_pm_times[0]));

  log_t_sw_times_row(local->t_sw_times, remote->t_sw_times);
  log_tx_snr_row(local->tx_snr_capability, remote->tx_snr_capability);
}

#endif // defined(CS_CAPABILITIES_LOG) && (CS_CAPABILITIES_LOG == 1)
