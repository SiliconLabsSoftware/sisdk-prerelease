/***************************************************************************//**
 * @file
 * @brief Compact logger Plugin
 *
 * This plugin defines an interface to add enumerated log messages with
 * a timestamp.  This plugin does not define the exact enumerations or format
 * but in general the format is the following:
 *
 * - UTC Time seconds:  4 bytes
 * - Log Message Bitmask: 1 bytes
 * - Log Message ID: 2 bytes
 * - Log Message Data Length: 2 bytes
 * - Log Message Data: Variable
 *
 * The messages definitions are meant to be enumerated by some other
 * predefined entity.  This allows the embedded code running on chip to
 * store only the enumerated value (log message ID) and the variable data.
 *
 * For example, log Message ID 3 could be defined as:
 *   Rejoining to network using channel bitmask 0x%08X
 *
 * The compact logger only needs to store ID 3, and the 32-bit value that
 * would represent the bitmask parameter.  Different message IDs may
 * have different numbers or types of parameters but this plugin
 * is ignorant of that.  A seperate tool or plugin would be necessary to
 * print the data, and define how to serialize the enumerated message.
 *
 * This implementation stores all log data in a ring buffer in RAM.
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
#include "compact-logger.h"
#include "ring-buffer.h"
#include "app/framework/util/time-util.h"
#include "simple-clock.h"

#ifdef SL_ZIGBEE_SCRIPTED_TEST
 #include "app/framework/plugin/compact-logger/config/compact-logger-config.h"
#else // SL_ZIGBEE_SCRIPTED_TEST
 #include "compact-logger-config.h"
  #if (SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_MILLISECOND_PRECISION == 1)
   #define MILLISECOND_PRECISION
  #endif // SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_MILLISECOND_PRECISION == 1
#endif // SL_ZIGBEE_SCRIPTED_TEST

//------------------------------------------------------------------------------
// Globals

// NOTE: The Log message length will NOT be stored because the ring
// buffer has a length field it uses.  The log message length
// will be the size of the ring buffer entry MINUS the various overhead
// fields defined below.

// Internal Format:
//   UTC Time Seconds
//   Log Message Bitmask
//   Milliseconds precision (2-bytes) : OPTIONAL
//     Enabled only if SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_BITMASK_MS_PRECISION
//   Log Message ID
//   Log Message Data (variable. Could be 0)

#define UTC_TIME_SIZE_BYTES 4
#define LOG_MESSAGE_ID_SIZE_BYTES 2
#define LOG_MESSAGE_BITMASK_SIZE_BYTES 1

#define MILLISECOND_PRECISION_BYTES 2

#define UTC_TIME_OFFSET 0
#define LOG_MESSAGE_BITMASK_OFFSET (UTC_TIME_SIZE_BYTES)
#define LOG_MESSAGE_ID_OFFSET (LOG_MESSAGE_BITMASK_OFFSET + LOG_MESSAGE_ID_SIZE_BYTES)

#define SERIALIZED_LOG_MESSAGE_LENGTH \
  (UTC_TIME_SIZE_BYTES                \
   + LOG_MESSAGE_BITMASK_SIZE_BYTES   \
   + LOG_MESSAGE_ID_SIZE_BYTES)

#define UTC_TIME_SYNCED_BITMASK BIT(0)

#if defined(SL_ZIGBEE_SCRIPTED_TEST)
  #define MAX_MESSAGE_COUNT 10
// This will define a storage space that can store MAX_MESSAGE_COUNT assuming all messages
// have no variable data.
  #undef SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_STORAGE_SIZE
  #define SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_STORAGE_SIZE                                         \
  ((SERIALIZED_LOG_MESSAGE_LENGTH + SL_ZIGBEE_AF_PLUGIN_RING_BUFFER_NARROW_LENGTH_ENTRY_OVERHEAD) \
   * MAX_MESSAGE_COUNT)
#endif

static sl_zigbee_ring_buffer_t ring = {
  .maxSize = SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_STORAGE_SIZE

             // Don't care about other variables.  Will be initialized by ring buffer init
};

static uint8_t ringBufferStorage[SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_STORAGE_SIZE];

#define MAX_PRINT_MESSAGE_COUNT 100
#define MAX_MESSAGE_DATA_PRINT_LENGTH 50
#define PRINT_BUFFER_LENGTH 10

// The severity and facility are used to filter which mesages are written
// to the logger, and which are not.  See
// sl_zigbee_af_compact_logger_check_facility_and_severity().
static uint8_t compactLoggerSeverity;
static uint16_t compactLoggerFacility;

// Design note:
// Storing the bitmask is somewhat of a waste because the only thing stored is
// the time sync bit.  Storing that with each log message is inefficient because
// we update all existing logs with the real time stamp once we obtain
// time sync from a time server.  The only case where this would be beneficial
// is if we couldn't atomically update all messages.  Not really sure why
// that would happen in a non-RTOS environment.
// A better way would be to store the time synchronization in a global.

//------------------------------------------------------------------------------
// Forward Declarations

static void convertSerializedLogDataToStruct(uint8_t* serializedData,
                                             sl_zigbee_af_plugin_compact_logger_message_info_t* info);
static void serializeLogDataFromStruct(sl_zigbee_af_plugin_compact_logger_message_info_t* info,
                                       uint8_t* serializedLogData);
static sl_status_t updateLogMessageInfoByIterator(sl_zigbee_af_plugin_compact_logger_message_info_t* info);

//------------------------------------------------------------------------------
// Public API

void sl_zigbee_af_compact_logger_init(void)
{
  // TODO: the ring buffer plugin should initialize itself.
  sl_zigbee_af_ring_buffer_init_struct(&ring,
                                       false, // wide length field? (false = 1 byte length)
                                       NULL, // delete function callback
                                       ringBufferStorage);

  compactLoggerSeverity = SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_SEVERITY_NOTICE;
  compactLoggerFacility = SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_FACILITY_ALL;
}

sl_status_t sl_zigbee_af_compact_logger_add(uint16_t logMessageId,
                                            uint16_t logDataLength,
                                            uint8_t* logData)
{
  sl_status_t status;
  uint8_t serializedLogData[SERIALIZED_LOG_MESSAGE_LENGTH];
  sl_zigbee_af_plugin_compact_logger_message_info_t info;

  memset(&info, 0, sizeof(sl_zigbee_af_plugin_compact_logger_message_info_t));

  info.timestampSeconds = sl_zigbee_af_get_current_time_seconds_with_ms_precision(&(info.millisecondPrecision));
  info.messageId = logMessageId;
  if (sl_zigbee_af_simple_clock_get_time_sync_status()
      != SL_ZIGBEE_AF_SIMPLE_CLOCK_NEVER_UTC_SYNC) {
    info.bitmask |= UTC_TIME_SYNCED_BITMASK;
  }

  serializeLogDataFromStruct(&info,
                             serializedLogData);

  status = sl_zigbee_af_ring_buffer_add_entry(&ring,
                                              serializedLogData,
                                              SERIALIZED_LOG_MESSAGE_LENGTH);
  if (status || logDataLength == 0) {
    return status;
  }

  status = sl_zigbee_af_ring_buffer_append_last_entry(&ring,
                                                      logData,
                                                      logDataLength);

  return status;
}

uint16_t sl_zigbee_af_compact_logger_get_log_count(void)
{
  return ring.entryCount;
}

void sl_zigbee_af_compact_logger_print_all_messages(void)
{
  sl_status_t status = sl_zigbee_af_compact_logger_init_iterator();
  bool keepGoing = true;
  sl_zigbee_af_plugin_compact_logger_message_info_t info;
  uint16_t logMessageNumber = 0;

  sl_zigbee_af_core_println("# Timestamp           Sync ID     Length");
  // example          3 yyyy-mm-dd hh:dd:ss 0x01 0x0001 0x0000
  sl_zigbee_af_core_println("----------------------------------------");
  if (status) {
    // No entries in log
    return;
  }

  do {
    uint16_t i;
    status = sl_zigbee_af_compact_logger_get_log_message_info_by_iterator(&info);
    if (status) {
      sl_zigbee_af_core_println("Error retrieving message info.");
      return;
    }
    sl_zigbee_af_core_print("%d: ", logMessageNumber);
    if (info.bitmask & UTC_TIME_SYNCED_BITMASK) {
      sl_zigbee_af_print_time_iso_format(info.timestampSeconds);
    } else {
      sl_zigbee_af_core_print("0x%08X         ", info.timestampSeconds);
    }
    sl_zigbee_af_core_print(" %s  0x%04X 0x%04X ",
                            (info.bitmask & UTC_TIME_SYNCED_BITMASK
                             ? "yes"
                             : "no "),
                            info.messageId,
                            info.dataLength);
    uint16_t indexIntoEntry = 0;
    for (i = 0; i < info.dataLength && i < MAX_MESSAGE_DATA_PRINT_LENGTH; i += PRINT_BUFFER_LENGTH) {
      uint8_t data[PRINT_BUFFER_LENGTH];
      uint16_t returnDataSize;
      sl_status_t status = sl_zigbee_af_compact_logger_get_log_message_data_by_iterator(indexIntoEntry,
                                                                                        PRINT_BUFFER_LENGTH,
                                                                                        &returnDataSize,
                                                                                        data);
      if (status != SL_STATUS_OK) {
        sl_zigbee_af_core_println("Error: Could not retrieve log data.");
        return;
      }
      sl_zigbee_af_core_print_buffer(data, returnDataSize, false); // false = without space
      indexIntoEntry += returnDataSize;
    }
    sl_zigbee_af_core_println("");

    status = sl_zigbee_af_compact_logger_iterator_next_entry();

    // Always increment so the print at the end is accurate for the log message count.
    logMessageNumber++;

    // The code arbitrarily limits the number of messages printed to prevent the user
    // from accidentally printing a giant log output that causes a watchdog timeout.
    if (logMessageNumber >= MAX_PRINT_MESSAGE_COUNT) {
      sl_zigbee_af_core_println("Reached max message print count (%d)", MAX_PRINT_MESSAGE_COUNT);
      keepGoing = false;
    }

    if (status == SL_STATUS_OK) {
      keepGoing = false;
    } else if (status != SL_STATUS_IN_PROGRESS) {
      sl_zigbee_af_core_println("Error getting next log message: 0x%02X", status);
      keepGoing = false;
      return;
    }
  } while (keepGoing);

  sl_zigbee_af_core_println("\n%d log messages", logMessageNumber);
}

sl_status_t sl_zigbee_af_compact_logger_init_iterator(void)
{
  return sl_zigbee_af_ring_buffer_init_iterator(&ring);
}

sl_status_t sl_zigbee_af_compact_logger_get_log_message_info_by_iterator(sl_zigbee_af_plugin_compact_logger_message_info_t* info)
{
  uint8_t serializedLogData[SERIALIZED_LOG_MESSAGE_LENGTH];
  uint16_t entrySize;
  uint16_t returnDataSize;
  sl_status_t status;

  status = sl_zigbee_af_ring_buffer_get_entry_by_iterator(&ring,
                                                          0,
                                                          &entrySize,
                                                          SERIALIZED_LOG_MESSAGE_LENGTH,
                                                          &returnDataSize,
                                                          serializedLogData);

  if (status) {
    return status;
  }

  convertSerializedLogDataToStruct(serializedLogData, info);
  info->dataLength = entrySize - returnDataSize;

  return SL_STATUS_OK;
}

sl_status_t sl_zigbee_af_compact_logger_get_log_message_data_by_iterator(uint16_t indexIntoEntry,
                                                                         uint16_t maxMessageSize,
                                                                         uint16_t* returnDataSize,
                                                                         uint8_t* returnData)
{
  sl_status_t status;
  uint16_t entrySize;
  uint16_t headerSize = SERIALIZED_LOG_MESSAGE_LENGTH;

  status = sl_zigbee_af_ring_buffer_get_entry_by_iterator(&ring,
                                                          headerSize + indexIntoEntry,
                                                          &entrySize,
                                                          maxMessageSize,
                                                          returnDataSize,
                                                          returnData);
  return status;
}

// This should be called when the embedded code synchronizes
// with UTC time, meaning subsequent calls to sl_zigbee_af_get_current_time()
// return the real UTC time and not seconds since boot.
sl_status_t sl_zigbee_af_compact_logger_update_all_logs_with_utc_time(uint32_t currentUtcTimeSeconds)
{
  // Foreach log message, take the delta from the current clock tick seconds to the
  // clock tick seconds that was used when the log message was added.
  // Set the log message's time to be the currentUtcTimeSeconds - delta.

  sl_status_t status;
  bool keepGoing = true;
  uint16_t entryNumber = 0;

  status = sl_zigbee_af_compact_logger_init_iterator();
  if (status != SL_STATUS_OK) {
    return status;
  }

  do {
    sl_zigbee_af_plugin_compact_logger_message_info_t info;
    status = sl_zigbee_af_compact_logger_get_log_message_info_by_iterator(&info);
    if (status) {
      sl_zigbee_af_core_println("Error: Compact logger couldn't get log info to update timestamp.");
      return status;
    }

    if (!(info.bitmask & UTC_TIME_SYNCED_BITMASK)) {
      uint32_t logMessageSecondsSinceBoot = info.timestampSeconds;
      uint32_t currentSecondsSinceBoot = halCommonGetInt32uMillisecondTick() / MILLISECOND_TICKS_PER_SECOND;

      info.timestampSeconds = (currentUtcTimeSeconds
                               - (currentSecondsSinceBoot - logMessageSecondsSinceBoot));
      info.bitmask |= UTC_TIME_SYNCED_BITMASK;

      status = updateLogMessageInfoByIterator(&info);

      if (status != SL_STATUS_OK) {
        sl_zigbee_af_core_println("Error: Compact logger failed to update current time for entry %d.",
                                  entryNumber);
        return status;
      }
    }

    status = sl_zigbee_af_compact_logger_iterator_next_entry();
    entryNumber++;
    if (status == SL_STATUS_OK) {
      return SL_STATUS_OK;
    } else if (status != SL_STATUS_IN_PROGRESS) {
      sl_zigbee_af_core_println("Error: Compact logger couldn't get next entry in logs.");
      return status;
    }
  } while (keepGoing);

  // Issue a callback notifying that UTC time has been set
  sl_zigbee_af_compact_logger_utc_time_set_cb(currentUtcTimeSeconds);

  return status;
}

sl_status_t sl_zigbee_af_compact_logger_iterator_next_entry(void)
{
  return sl_zigbee_af_ring_buffer_iterator_next_entry(&ring);
}

sl_status_t sl_zigbee_af_compact_logger_get_entry_by_number(uint32_t entryNumber,
                                                            sl_zigbee_af_plugin_compact_logger_message_info_t* info)
{
  uint8_t serializedLogData[SERIALIZED_LOG_MESSAGE_LENGTH];
  uint16_t returnEntryTotalSize;
  uint16_t returnDataSize;
  sl_status_t status = sl_zigbee_af_ring_buffer_get_entry_by_entry_number(&ring,
                                                                          entryNumber,
                                                                          0, // index
                                                                          &returnEntryTotalSize,
                                                                          SERIALIZED_LOG_MESSAGE_LENGTH,
                                                                          &returnDataSize,
                                                                          serializedLogData);
  if (status != SL_STATUS_OK) {
    return status;
  }
  convertSerializedLogDataToStruct(serializedLogData, info);
  return SL_STATUS_OK;
}

bool sl_zigbee_af_compact_logger_check_facility_and_severity(sl_zigbee_compact_logger_severity_t severity,
                                                             uint16_t facility)
{
  return ((severity <= compactLoggerSeverity)
          && ((facility & compactLoggerFacility) == facility));
}

sl_status_t sl_zigbee_af_compact_logger_set_severity(sl_zigbee_compact_logger_severity_t severity)
{
  sl_status_t status = SL_STATUS_INVALID_PARAMETER;
  if ( severity <= SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_SEVERITY_DEBUG ) {
    compactLoggerSeverity = severity;
    status = SL_STATUS_OK;
  }
  return status;
}

void sl_zigbee_af_compact_logger_set_facility(uint16_t facility)
{
  compactLoggerFacility = facility;
}

//------------------------------------------------------------------------------
// Internal API

static void convertSerializedLogDataToStruct(uint8_t* serializedLogData,
                                             sl_zigbee_af_plugin_compact_logger_message_info_t* info)

{
  uint8_t i;

  memset(info, 0, sizeof(sl_zigbee_af_plugin_compact_logger_message_info_t));

  for (i = 0; i < UTC_TIME_SIZE_BYTES; i++) {
    info->timestampSeconds += ((uint32_t)(*serializedLogData)) << (i * 8);
    serializedLogData++;
  }

  info->bitmask = (sl_zigbee_af_plugin_compact_logger_message_bitmask_t)(*serializedLogData);
  serializedLogData++;
  if (info->bitmask & SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_BITMASK_MS_PRECISION) {
    info->millisecondPrecision = *serializedLogData;
    info->millisecondPrecision += (*serializedLogData) << 8;
  }
  info->messageId = *serializedLogData;
  serializedLogData++;
  info->messageId += (*serializedLogData) << 8;
}

static void serializeLogDataFromStruct(sl_zigbee_af_plugin_compact_logger_message_info_t* info,
                                       uint8_t* serializedLogData)
{
  uint8_t i;
  uint8_t* ptr;

  memset(serializedLogData, 0, SERIALIZED_LOG_MESSAGE_LENGTH);

  ptr = &serializedLogData[UTC_TIME_OFFSET];

  for (i = 0; i < UTC_TIME_SIZE_BYTES; i++) {
    *ptr = LOW_BYTE(info->timestampSeconds >> (i * 8));
    ptr++;
  }

  if (info->bitmask & UTC_TIME_SYNCED_BITMASK) {
    *ptr |= UTC_TIME_SYNCED_BITMASK;
  }
#if defined(MILLISECOND_PRECISION)
  *ptr |= SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_BITMASK_MS_PRECISION;
#endif
  ptr++;

#if defined(MILLISECOND_PRECISION)
  for (i = 0; i < MILLISECOND_PRECISION_BYTES; i++) {
    *ptr = LOW_BYTE((info.millisecondPrecision >> (i * 8)));
    ptr++;
  }
#endif

  for (i = 0; i < LOG_MESSAGE_ID_SIZE_BYTES; i++) {
    *ptr = LOW_BYTE(info->messageId >> (i * 8));
    ptr++;
  }
}

static sl_status_t updateLogMessageInfoByIterator(sl_zigbee_af_plugin_compact_logger_message_info_t* info)
{
  uint8_t serializedLogData[SERIALIZED_LOG_MESSAGE_LENGTH];

  serializeLogDataFromStruct(info, serializedLogData);

  return sl_zigbee_af_ring_buffer_update_entry_by_iterator(&ring,
                                                           0, // data index in entry
                                                           serializedLogData,
                                                           SERIALIZED_LOG_MESSAGE_LENGTH);
}
