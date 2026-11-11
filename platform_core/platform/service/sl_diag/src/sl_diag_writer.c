/***************************************************************************//**
 * @file
 * @brief SL Diagnostic TLV writer and TLV stream parser
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

#include "sli_diag.h"
#include "sli_diag_internal.h"

#if SL_DIAG_ENABLE

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Initializes a TLV writer on the TLV area of a slot.
 ******************************************************************************/
void sli_diag_writer_init(sli_diag_writer_t *writer,
                          volatile sli_diag_record_slot_t *slot,
                          uint32_t owner_token)
{
  writer->cursor = slot->tlv_data;
  writer->end = slot->tlv_data + SLI_DIAG_SLOT_MAX_BYTES;
  writer->blocks_written = 0u;
  writer->bytes_written = 0u;
  writer->owner_token = owner_token;
}

/***************************************************************************//**
 * Appends one TLV block to the active record.
 ******************************************************************************/
sl_status_t sli_diag_writer_append_tlv(sli_diag_writer_t *writer,
                                       uint16_t type,
                                       uint8_t version,
                                       const void *payload,
                                       uint16_t payload_length)
{
  uint8_t header[SLI_DIAG_TLV_HDR_BYTES];
  uint32_t block_bytes;
  sl_status_t status;

  if (writer == NULL) {
    return SL_STATUS_NULL_POINTER;
  }
  if ((payload == NULL) && (payload_length != 0u)) {
    return SL_STATUS_NULL_POINTER;
  }
  // Type 0 is not a core TLV type or a provider ID. A payload larger than the
  // slot TLV area can never fit. Zero length is valid: an empty Recent-events
  // TLV has no entries.
  if ((type == 0u) || (payload_length > SLI_DIAG_SLOT_MAX_BYTES)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  // The whole block must fit before any byte is written, so a rejected block
  // leaves the stream unchanged.
  block_bytes = SLI_DIAG_TLV_HDR_BYTES + (uint32_t)payload_length;
  if (block_bytes > sli_diag_writer_bytes_remaining(writer)) {
    return SL_STATUS_WOULD_OVERFLOW;
  }

  // Little-endian, byte by byte: TLVs are packed and may be unaligned.
  header[0] = (uint8_t)(type & 0xFFu);
  header[1] = (uint8_t)(type >> 8);
  header[2] = version;
  header[3] = 0u;
  header[4] = (uint8_t)(payload_length & 0xFFu);
  header[5] = (uint8_t)(payload_length >> 8);

  // Copy the header to the writer cursor.
  status = sli_diag_owned_copy(writer->owner_token, writer->cursor, header, SLI_DIAG_TLV_HDR_BYTES);
  if (status == SL_STATUS_OK) {
    // Copy the payload to the writer cursor.
    status = sli_diag_owned_copy(writer->owner_token,
                                 writer->cursor + SLI_DIAG_TLV_HDR_BYTES,
                                 (const uint8_t *)payload,
                                 payload_length);
  }
  // Ownership lost: the partial block is not counted. The record is replaced
  // by the preempting writer.
  if (status != SL_STATUS_OK) {
    return status;
  }

  writer->cursor += block_bytes;
  writer->blocks_written++;
  writer->bytes_written += block_bytes;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Gets the number of bytes left in the writer.
 ******************************************************************************/
uint32_t sli_diag_writer_bytes_remaining(const sli_diag_writer_t *writer)
{
  if ((writer == NULL) || (writer->cursor > writer->end)) {
    return 0u;
  }
  return (uint32_t)(writer->end - writer->cursor);
}

/***************************************************************************//**
 * Decodes the next TLV of a TLV stream.
 ******************************************************************************/
sl_status_t sli_diag_tlv_next(const volatile uint8_t *data,
                              uint32_t total_size,
                              uint32_t *offset,
                              sli_diag_tlv_info_t *tlv)
{
  uint8_t header[SLI_DIAG_TLV_HDR_BYTES];
  uint32_t remaining;

  // The offset must be less than the total size.
  if (*offset == total_size) {
    return SL_STATUS_NOT_FOUND;
  }
  if (*offset > total_size) {
    return SL_STATUS_INVALID_STATE;
  }

  // Bounds are checked by subtraction so a corrupt length cannot overflow.
  remaining = total_size - *offset;
  if (remaining < SLI_DIAG_TLV_HDR_BYTES) {
    return SL_STATUS_INVALID_STATE;
  }

  // Read the header from the data buffer.
  if (sli_diag_retained_read(header, data + *offset, SLI_DIAG_TLV_HDR_BYTES) != SL_STATUS_OK) {
    return SL_STATUS_INVALID_STATE;
  }

  // Extract the type, version, and length from the header.
  tlv->type = (uint16_t)((uint16_t)header[0] | ((uint16_t)header[1] << 8));
  tlv->version = header[2];
  // header[3] is reserved.
  tlv->length = (uint16_t)((uint16_t)header[4] | ((uint16_t)header[5] << 8));

  // Check if the length is valid.
  if ((uint32_t)tlv->length > (remaining - SLI_DIAG_TLV_HDR_BYTES)) {
    return SL_STATUS_INVALID_STATE;
  }

  // Calculate the payload offset.
  tlv->payload_offset = *offset + SLI_DIAG_TLV_HDR_BYTES;

  // Update the offset to the next TLV.
  *offset = tlv->payload_offset + tlv->length;
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Finds the first TLV of a type.
 ******************************************************************************/
sl_status_t sli_diag_tlv_find(const volatile uint8_t *data,
                              uint32_t total_size,
                              uint16_t type,
                              sli_diag_tlv_info_t *tlv)
{
  uint32_t offset = 0u;
  sl_status_t status;

  // Each TLV advances the offset by at least the header size, so the loop
  // ends after at most total_size / SLI_DIAG_TLV_HDR_BYTES iterations.
  do {
    status = sli_diag_tlv_next(data, total_size, &offset, tlv);
  } while ((status == SL_STATUS_OK) && (tlv->type != type));

  return status;
}

#endif // SL_DIAG_ENABLE
