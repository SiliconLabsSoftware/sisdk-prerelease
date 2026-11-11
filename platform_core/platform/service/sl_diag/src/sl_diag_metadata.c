/***************************************************************************//**
 * @file
 * @brief SL Diagnostic incident metadata TLVs
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

#include <stddef.h>
#include <stdint.h>

#include "sl_diag.h"
#include "sli_diag.h"
#include "sli_diag_internal.h"

#if SL_DIAG_INCIDENT_METADATA_ENABLE

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Writes the Trigger TLV.
 ******************************************************************************/
sl_status_t sli_diag_write_tlv_trigger(sli_diag_writer_t *writer,
                                       const sli_diag_capture_request_t *req)
{
  sl_diag_trigger_tlv_payload_t payload = { 0 };

  if (req == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  // The overwrite policy reads the trigger from this TLV. Do not store a
  // trigger with no priority.
  if ((req->trigger == SL_DIAG_TRIGGER_NONE) || (req->trigger >= SL_DIAG_TRIGGER_COUNT)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // The trigger and reason_id identify the incident for the host and for
  // repeat counting. Read uptime now for this capture.
  payload.core_id = SLI_DIAG_CORE_ID;
  payload.trigger = req->trigger;
  payload.reason_id = req->reason_id;
  payload.detail = req->detail;
  payload.uptime_ms = sl_diag_get_uptime_ms();
  return sli_diag_writer_append_tlv(writer,
                                    SL_DIAG_TLV_TRIGGER,
                                    SLI_DIAG_CORE_TLV_VERSION,
                                    &payload,
                                    (uint16_t)sizeof(payload));
}

/***************************************************************************//**
 * Writes the Reset/Boot TLV.
 ******************************************************************************/
sl_status_t sli_diag_write_tlv_reset_boot(sli_diag_writer_t *writer)
{
  sl_diag_reset_boot_tlv_payload_t payload = { 0 };
  sli_diag_reset_boot_t reset;

  // Store the reset that started this boot. The reset after this capture is
  // recorded on the next boot.
  sli_diag_get_reset_boot(&reset);
  payload.reset_reason = reset.reset_reason;
  payload.reset_reason_ext = reset.reset_reason_ext;

  // Get the boot ID and write the TLV.
  payload.boot_id = sli_diag_get_boot_id();
  return sli_diag_writer_append_tlv(writer,
                                    SL_DIAG_TLV_RESET_BOOT,
                                    SLI_DIAG_CORE_TLV_VERSION,
                                    &payload,
                                    (uint16_t)sizeof(payload));
}

/***************************************************************************//**
 * Writes the Build TLV.
 ******************************************************************************/
sl_status_t sli_diag_write_tlv_build(sli_diag_writer_t *writer)
{
  // Zero the payload first so reserved bytes and unset override fields are
  // zero.
  sl_diag_build_tlv_payload_t payload = { 0 };

  // Get the build info and write the TLV.
  payload.core_id = SLI_DIAG_CORE_ID;
  sl_diag_get_build_info(payload.build_id, &payload.dictionary_id);
  return sli_diag_writer_append_tlv(writer,
                                    SL_DIAG_TLV_BUILD,
                                    SLI_DIAG_CORE_TLV_VERSION,
                                    &payload,
                                    (uint16_t)sizeof(payload));
}

#endif // SL_DIAG_INCIDENT_METADATA_ENABLE
