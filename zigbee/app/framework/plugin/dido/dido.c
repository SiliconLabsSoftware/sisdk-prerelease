/***************************************************************************//**
 * @file
 * @brief This file creates a Diagnostic Information Data Object (DIDO) and sends
 * it to a specified destination.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "app/framework/include/af.h"
#include "dido.h"

#include "stack/include/zigbee-event-logger-gen.h"
#include "stack/config/config.h"
#include "stack/config/sl_zigbee_token_defines.h"
#include "stack/include/sl_zigbee_token.h"
#include "compact-logger.h"
#include "app/framework/plugin/counters/counters.h"
#include "app/framework/plugin/fragmentation/fragmentation.h"
#include "zigbee-event-logger-print-gen.h"
#include "app/framework/util/util.h"
#ifdef SL_CATALOG_ZIGBEE_APPLICATION_BOOTLOADER_PRESENT
#include "api/btl_interface.h"
#endif

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif
#ifndef CUSTOMER_APPLICATION_VERSION
#ifdef SL_CATALOG_ZIGBEE_OTA_CLIENT_POLICY_PRESENT
#include "ota-client-policy-config.h"
#define CUSTOMER_APPLICATION_VERSION  SL_ZIGBEE_AF_PLUGIN_OTA_CLIENT_POLICY_FIRMWARE_VERSION
#else // !SL_CATALOG_ZIGBEE_OTA_CLIENT_POLICY_PRESENT
#define CUSTOMER_APPLICATION_VERSION  0x0000
#endif // SL_CATALOG_ZIGBEE_OTA_CLIENT_POLICY_PRESENT
#endif // CUSTOMER_APPLICATION_VERSION

// The general DIDO format is currently defined as follows:
// [DIDO Header] + [Sequence of DIDO Type-Length Values]
// Individual TLV formats are defined later in this file.

// #defines
#define DIDO_TLV_HEADER_LENGTH  4

typedef enum {
  DIDO_TLV_TYPE_CRC = 0,
  DIDO_TLV_TYPE_LOG_DATA = 1,
  DIDO_TLV_TYPE_STACK_TRACE = 2,
  DIDO_TLV_TYPE_EMBER_COUNTERS = 3,
  DIDO_TLV_TYPE_NVM3_STATS = 4,
  DIDO_TLV_TYPE_VERSION = 5,
} EmberAfDidoTlvType;

// Function Prototypes
static sl_status_t insertDidoHeader(uint8_t **buffer, uint16_t *remainingLength);
static sl_status_t insertTlvHeader(uint8_t **buffer,
                                   uint16_t *remainingLength,
                                   EmberAfDidoTlvType tlvType,
                                   uint16_t tlvDataLength);
static sl_status_t insertEventLogTlvData(uint8_t **buffer, uint16_t *remainingLength);
static sl_status_t insertEmberCounterTlvData(uint8_t **buffer, uint16_t *remainingLength);
static sl_status_t insertVersionTlvData(uint8_t **buffer, uint16_t *remainingLength);
//static sl_status_t insertStackTraceTlvData(uint8_t **buffer, uint16_t *remainingLength);    // Not implemented yet.
//static sl_status_t insertNvm3StatTlvData(uint8_t **buffer, uint16_t *remainingLength);      // Not implemented yet.

static sl_status_t insertEventLogHeader(uint8_t **buffer, uint16_t *remainingLength);
static sl_status_t insertEventLogEntryHeader(uint8_t **buffer,
                                             uint16_t *remainingLength,
                                             sl_zigbee_af_plugin_compact_logger_message_info_t *info);

// Temporary, until we can use a fragmentation allocation.
#define DIDO_BUFFER_SIZE 1024
static uint8_t didoBuffer[DIDO_BUFFER_SIZE];

// TLV Handler function definitions
typedef sl_status_t (tlvInsertFunction)(uint8_t **buffer, uint16_t *remainingLength);
typedef struct {
  EmberAfDidoTlvType tlvType;
  tlvInsertFunction *insertTlvData;
} sli_zigbee_tlv_handler_t;

static const sli_zigbee_tlv_handler_t tlvHandlers[] =
{
  { DIDO_TLV_TYPE_LOG_DATA, insertEventLogTlvData },
  //{ DIDO_TLV_TYPE_STACK_TRACE, insertStackTraceTlvData },   // Not implemented yet.
  { DIDO_TLV_TYPE_EMBER_COUNTERS, insertEmberCounterTlvData },
  //{ DIDO_TLV_TYPE_NVM3_STATS, insertNvm3StatTlvData },      // Not implemented yet.
  { DIDO_TLV_TYPE_VERSION, insertVersionTlvData },
};

#define NUM_TLV_HANDLERS (sizeof(tlvHandlers) / sizeof(sli_zigbee_tlv_handler_t))
#define ZCL_HEADER_SIZE 5

static uint16_t filledLen;

sl_status_t sl_zigbee_af_dido_cluster_send_debug_report(uint8_t debugReportId,
                                                        sl_802154_short_addr_t nodeId,
                                                        uint8_t endpoint)
{
  UNUSED_VAR(endpoint);
  sl_status_t status;
  uint8_t *buffer;
  uint16_t remainingLength;
  sl_zigbee_aps_frame_t *papsFrame;
  uint8_t i;
  uint16_t messageTag;
  uint8_t *tlvHeader;
  int32u crc;

  // buffer points to a large buffer where we can store the data for the report.
  buffer = &didoBuffer[ZCL_HEADER_SIZE];
  remainingLength = DIDO_BUFFER_SIZE - ZCL_HEADER_SIZE;

  // Insert the debugReportId at the beginning of the buffer
  // before the DIDO header.
  *buffer++ = debugReportId;
  remainingLength--;

  status = insertDidoHeader(&buffer, &remainingLength);
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Error: Could not create Dido Header");
    return SL_STATUS_FAIL;
  }

  for (i = 0; i < NUM_TLV_HANDLERS; i++) {
    // Get header & data for each TLV
    tlvHeader = buffer;
    buffer = buffer + DIDO_TLV_HEADER_LENGTH;

    filledLen = remainingLength;
    status = tlvHandlers[i].insertTlvData(&buffer, &remainingLength);
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_core_println("Error inserting TLV Data[%d], status=0x%08X", i, status);
      return status;
    }
    filledLen -= remainingLength; // filledLen tells how much data was copied into the buffer.
    sl_zigbee_af_core_println("**  Storing type=%d, filledLen=%d",
                              tlvHandlers[i].tlvType, filledLen);
    status = insertTlvHeader(&tlvHeader,
                             &remainingLength,
                             tlvHandlers[i].tlvType,
                             filledLen);
    if (status != SL_STATUS_OK) {
      sl_zigbee_af_core_println("Error inserting TLV Header[%d], status=0x%08X", i, status);
    }
  }
  // Fill in the CRC TLV.
  filledLen = buffer - didoBuffer;
  tlvHeader = buffer;
  buffer = buffer + DIDO_TLV_HEADER_LENGTH;
  status = insertTlvHeader(&tlvHeader,
                           &remainingLength,
                           DIDO_TLV_TYPE_CRC,
                           sizeof(crc));

  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Error inserting CRC Header, status=0x%08X", status);
    return status;
  }

  // Calculate the CRC over the entire packet except the last 4 (CRC) bytes.
  // Don't include the reportId.
  crc = sl_zigbee_af_get_buffer_crc(&didoBuffer[ZCL_HEADER_SIZE + 1],
                                    filledLen + DIDO_TLV_HEADER_LENGTH - ZCL_HEADER_SIZE - 1,
                                    0);

  sl_util_store_low_high_int32u(buffer, crc);
  buffer += sizeof(crc);
  filledLen = buffer - didoBuffer;

  // Reverse endpoints.
  papsFrame = sl_zigbee_af_get_command_aps_frame();
  i = papsFrame->sourceEndpoint;
  sl_zigbee_af_set_command_endpoints(papsFrame->destinationEndpoint, i);

  // Set the external buffer to point to didoBuffer where we will
  // store the ZCL frame header bytes, before the DIDO report.
  // EMZIGBEE-2645 will replace didoBuffer with a pointer to a
  // fragmentation buffer to free up RAM.
  sl_zigbee_af_set_external_buffer(didoBuffer,
                                   DIDO_BUFFER_SIZE,
                                   &filledLen,
                                   papsFrame);
  // For this fill command, pass in pointer to where the actual REPORT DATA
  // is located, which is after the ZCL header bytes, and after the reportId.
  // Modify the length parameter to reflect the number of remaining bytes
  // after the ZCL header and the debugReportId.
  sl_zigbee_af_fill_command_sl_wwah_cluster_debug_report_query_response(debugReportId,
                                                                        &didoBuffer[ZCL_HEADER_SIZE + 1],
                                                                        (filledLen - ZCL_HEADER_SIZE - 1));

  // sl_zigbee_af_fill_command_sl_wwah_cluster_debug_report_query_response() sets the APS options to defaults.
  // But APS Encryption is required for WWAH cluster, so enable it here.
  papsFrame->options |= SL_ZIGBEE_APS_OPTION_ENCRYPTION;

  // This is a response, must set the ZCL header sequence number to that of the request.
  // Offset of sequence number field depends on whether 2-byte mfg code is present.
  if ((didoBuffer[0] & ZCL_MANUFACTURER_SPECIFIC_MASK) != 0U) {
    didoBuffer[3] = sl_zigbee_af_current_command()->seqNum; // fc, mfg code, seq
  } else {
    didoBuffer[1] = sl_zigbee_af_current_command()->seqNum; // fc, seq
  }

  status = sli_zigbee_af_fragmentation_send_unicast(SL_ZIGBEE_OUTGOING_DIRECT,
                                                    nodeId,
                                                    papsFrame,
                                                    didoBuffer,
                                                    filledLen,
                                                    &messageTag);
  return status;
}

// DIDO Header Definitions
#define MAGIC_NUMBER_LENGTH 8
#define DIDO_HEADER_VERSION 1
#define DIDO_HEADER_LENGTH  (MAGIC_NUMBER_LENGTH + 1)

static const uint8_t DidoHeaderMagicNumber[MAGIC_NUMBER_LENGTH] =
{
  0x3a, 0x46, 0xfa, 0xdb, 0xc8, 0x69, 0xfe, 0x50
};

// DIDO Header:
// [Magic Number(8)] + [Version(1)]
static sl_status_t insertDidoHeader(uint8_t **buffer, uint16_t *remainingLength)
{
  if (*remainingLength >= DIDO_HEADER_LENGTH) {
    memcpy(*buffer, DidoHeaderMagicNumber, MAGIC_NUMBER_LENGTH);
    (*buffer)[MAGIC_NUMBER_LENGTH] = DIDO_HEADER_VERSION;
    *remainingLength -= DIDO_HEADER_LENGTH;
    *buffer += DIDO_HEADER_LENGTH;
    return SL_STATUS_OK;
  }
  return SL_STATUS_FAIL;
}

// TLV Header:
// [TLV Type(2)] + [Data Length(2)]
static sl_status_t insertTlvHeader(uint8_t **buffer,
                                   uint16_t *remainingLength,
                                   EmberAfDidoTlvType tlvType,
                                   uint16_t tlvDataLength)
{
  if (*remainingLength > DIDO_TLV_HEADER_LENGTH) {
    sl_util_store_low_high_int16u(*buffer, tlvType);
    *buffer += 2;
    sl_util_store_low_high_int16u(*buffer, tlvDataLength);
    *buffer += 2;
    *remainingLength -= 4;
    return SL_STATUS_OK;
  }
  return SL_STATUS_FAIL;
}

// TLV Event Log
// [Event Log Header] + [Event Log Entries]
// Each event log entry has a header & data
static sl_status_t insertEventLogTlvData(uint8_t **buffer, uint16_t *remainingLength)
{
  sl_zigbee_af_plugin_compact_logger_message_info_t msgInfo;
  sl_status_t status;
  uint16_t msgSize;

  // Setup Log Data TLV Header
  status = insertEventLogHeader(buffer, remainingLength);
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Insert Event Log Header Error, status=0x%02X", status);
    return status;
  }

  status = sl_zigbee_af_compact_logger_init_iterator();
  if (status == SL_STATUS_OK) {
    do {
      // Get message info and add it to the buffer.
      status = sl_zigbee_af_compact_logger_get_log_message_info_by_iterator(&msgInfo);
      if (status != SL_STATUS_OK) {
        sl_zigbee_af_core_println("Get Log Message Info Error, status=0x%02X", status);
        // Corruption in Log Data TLV
        return status;
      }

      status = insertEventLogEntryHeader(buffer, remainingLength, &msgInfo);
      if (status != SL_STATUS_OK) {
        sl_zigbee_af_core_println("Insert Event Log Entry Header Error, status=0x%02X", status);
        // Corruption in Log Data TLV
        return status;
      }

      // Get the data from the iterator
      if (*remainingLength < msgInfo.dataLength) {
        sl_zigbee_af_core_println("Insert Event Log Entry Error - out of space");
        // Out of space - abort and send what we could fit.
        break;
      }
      msgSize = 0;
      status = sl_zigbee_af_compact_logger_get_log_message_data_by_iterator(0,
                                                                            *remainingLength,
                                                                            &msgSize,
                                                                            *buffer);
      if (status != SL_STATUS_OK) {
        sl_zigbee_af_core_println("Get Log Message Error, status=0x%02X", status);
        return SL_STATUS_FAIL;
      } else if (msgInfo.dataLength < *remainingLength) {
        // Ensure we received data, and the data was not truncated
        // to fit inside the remaining length of the buffer.
        *remainingLength -= msgInfo.dataLength;
        *buffer = *buffer + msgInfo.dataLength;
      } else {
        sl_zigbee_af_core_println("Error: Log Data Error, message length=%d, remainingLength=%d",
                                  msgInfo.dataLength, *remainingLength);
        break;
      }

      // Advance the iterator.  This returns SL_STATUS_IN_PROGRESS
      // until it returns the final entry, at which point it returns
      // SL_STATUS_OK.  Continue to read as long as
      // status == SL_STATUS_IN_PROGRESS.
      status = sl_zigbee_af_compact_logger_iterator_next_entry();
    } while ( status == SL_STATUS_IN_PROGRESS );
  }
  return SL_STATUS_OK;
}

// TLV Event Log Header
// [BootCount (4)] + [NumTlvEntries (2)] + [LoggerVersion (1)]
#define LOG_DATA_TLV_SIZE 7
static sl_status_t insertEventLogHeader(uint8_t **buffer, uint16_t *remainingLength)
{
  uint16_t length;
  uint32_t bootCnt;

  if (*remainingLength < LOG_DATA_TLV_SIZE) {
    return SL_STATUS_FAIL;
  }

  // Setup Event Log TLV Header
  sl_status_t status = slx_zigbee_token_manager_get_data(COMMON_TOKEN_STACK_BOOT_COUNTER, (void *)&bootCnt, sizeof(bootCnt));
  if (status != SL_STATUS_OK) {
    sl_zigbee_af_core_println("Failed to get boot counter, status: 0x%08X", status);
    return status;
  }
  sl_util_store_low_high_int32u(*buffer, bootCnt);
  *buffer += sizeof(bootCnt);
  length = sl_zigbee_af_compact_logger_get_log_count();
  sl_util_store_low_high_int16u(*buffer, length);
  *buffer += sizeof(length);
  **buffer = ZIGBEE_EVENT_LOGGER_VERSION;
  *buffer += 1;
  *remainingLength -= LOG_DATA_TLV_SIZE;
  return SL_STATUS_OK;
}

// Event Log Entry Header:
// [Length (1)] + [Bitmask (1)] + [TimeValue (4)] + [MSPrecision (0/2)] +
// [BootCnt (0/4)] + [MsgId (2)]
#define MAX_LOG_DATA_HEADER_SIZE  (sizeof(sl_zigbee_af_plugin_compact_logger_message_info_t) + 4)    // Worst Case, include boot count
#define EVENT_LOG_ENTRY_INDEX_LENGTH  0
#define EVENT_LOG_ENTRY_INDEX_BITMASK 1
static sl_status_t insertEventLogEntryHeader(uint8_t **buffer,
                                             uint16_t *remainingLength,
                                             sl_zigbee_af_plugin_compact_logger_message_info_t *info)
{
  uint16_t responseLength;
  uint8_t *pdst = *buffer;
  if (*remainingLength < MAX_LOG_DATA_HEADER_SIZE) {
    return SL_STATUS_FAIL;
  }
  pdst[EVENT_LOG_ENTRY_INDEX_BITMASK] = info->bitmask;
  sl_util_store_low_high_int32u(&pdst[2], info->timestampSeconds);
  responseLength = 6;

  if (info->bitmask & SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_BITMASK_MS_PRECISION) {
    sl_util_store_low_high_int16u(&pdst[responseLength], info->millisecondPrecision);
    responseLength += sizeof(info->millisecondPrecision);
  }
  /*if( info->bitmask & SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_BITMASK_BOOT_COUNT)
     {
     // Store boot counter from current log entry.
     // TODO:  This feature is not currently supported.
     sl_util_store_low_high_int32u( &buffer[responseLength], ??? );
     responseLength += sizeof(???);
     }*/
  sl_util_store_low_high_int16u(&pdst[responseLength], info->messageId);
  responseLength += sizeof(info->messageId);
  pdst[EVENT_LOG_ENTRY_INDEX_LENGTH] = responseLength + info->dataLength;

  *remainingLength -= responseLength;
  *buffer += responseLength;

  return SL_STATUS_OK;
}

// TLV Ember Counter Data
//  [#Counters(1)] + [Counter0Id(1)] + [Counter0Value(2)] + ...
#define COUNTER_ENTRY_SIZE  (1 + sizeof(sl_zigbee_counters[0]) )
#define COUNTER_TLV_INDEX_NUM_COUNTERS  0
static sl_status_t insertEmberCounterTlvData(uint8_t **buffer, uint16_t *remainingLength)
{
  uint16_t responseLength = 1;  // Reserve 1st byte for # counters
  uint8_t i = 0;
  uint8_t *pdst = *buffer;
  for (i = 0; i < SL_ZIGBEE_COUNTER_TYPE_COUNT; i++) {
    if ((responseLength + COUNTER_ENTRY_SIZE) > *remainingLength) {
      // Response is too large.
      sl_zigbee_af_core_println("Error: Out of Space with Ember Counters, i=%d, maxLen=%d",
                                i, *remainingLength);
      // Rather than return an error, break out of the for() loop
      // and report all the counter values that we could fit.
      break;
    }
    pdst[responseLength++] = i;
    sl_util_store_low_high_int16u(&pdst[responseLength], sl_zigbee_counters[i]);
    responseLength += 2;
  }
  pdst[COUNTER_TLV_INDEX_NUM_COUNTERS] = i; // Number of Counters
  *buffer += responseLength;
  *remainingLength -= responseLength;
  return SL_STATUS_OK;
}

enum {
  STACK_ID_ZIGBEE  = 0,
  STACK_ID_THREAD  = 1,
  STACK_ID_BLE     = 2,
  STACK_ID_CONNECT = 3,
};

// TLV Version Data
// [StackId(1)] + [StackVersion(2)] + [VersionType(1)] +
// [CustomerVersion(2)] + [BootloaderType(1)] + [BootloaderVersion(2)]
#define VERSION_DATA_SIZE   9

static sl_status_t insertVersionTlvData(uint8_t **buffer, uint16_t *remainingLength)
{
  sl_status_t status = SL_STATUS_FAIL;
  uint8_t *pdst = *buffer;
  int16u bootloaderVersion = 0;

  if (*remainingLength >= VERSION_DATA_SIZE) {
#ifdef SL_CATALOG_ZIGBEE_APPLICATION_BOOTLOADER_PRESENT
    BootloaderInformation_t info = { .type = SL_BOOTLOADER, .version = 0U, .capabilities = 0U };
    bootloader_getInfo(&info);
    bootloaderVersion = info.version >> BOOTLOADER_VERSION_MINOR_SHIFT;
#endif
    pdst[0] = STACK_ID_ZIGBEE;
    pdst[1] = LOW_BYTE(SL_ZIGBEE_FULL_VERSION);
    pdst[2] = HIGH_BYTE(SL_ZIGBEE_FULL_VERSION);
    pdst[3] = SL_ZIGBEE_VERSION_TYPE;
    pdst[4] = LOW_BYTE(CUSTOMER_APPLICATION_VERSION);
    pdst[5] = HIGH_BYTE(CUSTOMER_APPLICATION_VERSION);
    pdst[6] = 0;    // Bootloader Type - Legacu or Gecko.  How to detect?
    pdst[7] = LOW_BYTE(bootloaderVersion);
    pdst[8] = HIGH_BYTE(bootloaderVersion);
    *remainingLength -= VERSION_DATA_SIZE;
    *buffer += VERSION_DATA_SIZE;
    status = SL_STATUS_OK;
  }
  return status;
}

#if 0
// TLV STACK TRACE DATA
// TODO - Implement this feature - EMZIGBEE-2821
static sl_status_t insertStackTraceTlvData(uint8_t **buffer, uint16_t *remainingLength)
{
  return SL_STATUS_OK;
}

// TLV NVM3 STATUS DATA
// TODO - Implement this feature - EMZIGBEE-2822
static sl_status_t insertNvm3StatTlvData(uint8_t **buffer, uint16_t *remainingLength)
{
  return SL_STATUS_OK;
}

#endif  // #if 0
