/***************************************************************************//**
 * @file
 * @brief APIs and defines for the Compact Logger plugin.
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

#ifndef _COMPACT_LOGGER_H_
#define _COMPACT_LOGGER_H_

// TODO: Properly doxygenate this file

/**
 * @defgroup compact-logger Compact Logger
 * @ingroup component
 * @brief API and Callbacks for the Compact Logger Component
 *
 * This component manages a compact logger that stores log entries encoded with
 * a 2-byte enumeration. Another component must provide the encoding/decoding of
 * the enumerations. This component stores log messages with the following:
 * UTC Time, Time Synchronization, Log Message ID, and variable log message data.
 * It stores the data in a ring-buffer stored in RAM.  When space is needed,
 * the oldest log message entries are deleted.
 *
 */

/**
 * @addtogroup compact-logger
 * @{
 */

typedef uint8_t sl_zigbee_af_plugin_compact_logger_message_bitmask_t;

typedef struct {
  uint32_t timestampSeconds;

  // Millisecond precision only available if enabled in the plugin
  uint16_t millisecondPrecision;

  uint16_t messageId;
  sl_zigbee_af_plugin_compact_logger_message_bitmask_t bitmask;
  uint16_t dataLength;
} sl_zigbee_af_plugin_compact_logger_message_info_t;

typedef enum {
  SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_SEVERITY_CRITICAL,
  SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_SEVERITY_NOTICE,
  SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_SEVERITY_DEBUG
} sl_zigbee_compact_logger_severity_t;

#define SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_FACILITY_ALL  0xFFFF
#define SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_BITMASK_UTC_TIME_SYNC 0x01
#define SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_BITMASK_MS_PRECISION  0x02

/**
 * @name API
 * @{
 */

/** @brief Initialize compact logger.
 *
 */
void sl_zigbee_af_compact_logger_init(void);

/** @brief Add compact log.
 *
 * @param logMessageId message id Ver.: always
 * @param logDataLength data length Ver.: always
 * @param logData data Ver.: always
 *
 * @return sl_status_t status
 */
sl_status_t sl_zigbee_af_compact_logger_add(uint16_t logMessageId,
                                            uint16_t logDataLength,
                                            uint8_t* logData);

/** @brief Get log count.
 *
 * @return uint16_t Log count
 */
uint16_t sl_zigbee_af_compact_logger_get_log_count(void);

/** @brief Initialize compact logger iterator.
 *
 * @return sl_status_t status
 *
 */
sl_status_t sl_zigbee_af_compact_logger_init_iterator(void);

/** @brief Get log message info by using the iterator.
 *
 * @param info pointer to struct to store log info
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_compact_logger_get_log_message_info_by_iterator(sl_zigbee_af_plugin_compact_logger_message_info_t* info);

/** @brief Get log message data by using the iterator.
 *
 * @param indexIntoEntry Ver.: always
 * @param maxMessageSize Ver.: always
 * @param returnDataSize Ver.: always
 * @param returnData Ver.: always
 *
 * @return sl_status_t status code
 */
sl_status_t sl_zigbee_af_compact_logger_get_log_message_data_by_iterator(uint16_t indexIntoEntry,
                                                                         uint16_t maxMessageSize,
                                                                         uint16_t* returnDataSize,
                                                                         uint8_t* returnData);

/** @brief Compact logger iterator next entry.
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_compact_logger_iterator_next_entry(void);

/** @brief Update all logs with UTC time.
 *
 * @param currentUtcTimeSeconds UTC time in seconds Ver.: always
 *
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_compact_logger_update_all_logs_with_utc_time(uint32_t currentUtcTimeSeconds);

/** @brief Print all messages.
 *
 */
void sl_zigbee_af_compact_logger_print_all_messages(void);

/** @brief Get entry by number.
 *
 * @param entryNumber
 * @param info pointer to struct where info will be stored Ver.: always
 *
 * @return sl_status_t status code
 */
sl_status_t sl_zigbee_af_compact_logger_get_entry_by_number(uint32_t entryNumber,
                                                            sl_zigbee_af_plugin_compact_logger_message_info_t* info);

/** @brief Check facility and severity.
 *
 * @param severity Ver.: always
 * @param facility Ver.: always
 *
 * @return bool
 *
 */
bool sl_zigbee_af_compact_logger_check_facility_and_severity(sl_zigbee_compact_logger_severity_t severity,
                                                             uint16_t facility);

/** @brief Set facility.
 *
 * @param facility Ver.: always
 *
 */
void sl_zigbee_af_compact_logger_set_facility(uint16_t facility);

/** @brief Set severity.
 *
 * @param severity Ver.: always
 * @return sl_status_t status code
 *
 */
sl_status_t sl_zigbee_af_compact_logger_set_severity(sl_zigbee_compact_logger_severity_t severity);

/** @} */ // end of name APIs

/**
 * @name Callbacks
 * @{
 */
/**
 * @defgroup compact_logger_cb Compact Logger
 * @ingroup af_callback
 * @brief Callbacks for Compact Logger Component
 *
 */

/**
 * @addtogroup compact_logger_cb
 * @{
 */

/** @brief Return the current UTC time.
 *
 * This function is called when the UTC time has been set. It returns the
 * current UTC time. This callback is fired after all applicable compact-logger
 * events have had their UTC time updated.
 *
 * @param currentUtcTimeSeconds The current UTC time in seconds.
 * Ver.: always
 */
void sl_zigbee_af_compact_logger_utc_time_set_cb(uint32_t currentUtcTimeSeconds);
/** @} */ // end of name compact_logger_cb
/** @} */ // end of name Callbacks
/** @} */ // end of compact-logger

#endif  // #ifndef _COMPACT_LOGGER_H_
