/***************************************************************************//**
 * @file
 * @brief CLI for the Zigbee Event Logger plugin.
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
#include "stack/include/zigbee-event-logger-gen.h"
#include "zigbee-event-logger-print-gen.h"

#include "compact-logger.h"
#include "app/util/serial/sl_zigbee_command_interpreter.h"

#define ZIGBEE_EVENT_LOGGER_MAX_DATA_SIZE  32

static void printMessageTimestamp(sl_zigbee_af_plugin_compact_logger_message_info_t *msgInfo);

// plugin zigbee-event-logger print
void sli_zigbee_af_zigbee_event_logger_print_events(sl_cli_command_arg_t *arguments)
{
  UNUSED_VAR(arguments);

  sl_zigbee_af_plugin_compact_logger_message_info_t msgInfo;
  sl_status_t status;
  printFunction *pfunc;
  uint16_t msgSize;
  uint8_t  msgData[ZIGBEE_EVENT_LOGGER_MAX_DATA_SIZE];
  bool ret;

  sl_zigbee_af_cli_println("Printing All Events");
  sl_zigbee_af_cli_println("-------------------");
  status = sl_zigbee_af_compact_logger_init_iterator();
  if ( status == SL_STATUS_OK ) {
    do {
      // Get the data from the iterator
      sl_zigbee_af_compact_logger_get_log_message_info_by_iterator(&msgInfo);
      sl_zigbee_af_compact_logger_get_log_message_data_by_iterator(0,
                                                                   ZIGBEE_EVENT_LOGGER_MAX_DATA_SIZE,
                                                                   &msgSize,
                                                                   msgData);

      printMessageTimestamp(&msgInfo);
      // Now call the print function based on the messageId.
      pfunc = sl_zigbee_af_zig_bee_event_logger_lookup_print_function(msgInfo.messageId);
      if ( pfunc != NULL ) {
        ret = pfunc(msgData, msgInfo.dataLength);
        if ( ret == false ) {
          sl_zigbee_af_zig_bee_event_logger_print_hex_data(msgData, msgInfo.dataLength);
        }
      }

      // Advance the iterator.  This returns SL_STATUS_IN_PROGRESS
      // until it returns the final entry, at which point it returns
      // SL_STATUS_OK.  Continue to read as long as
      // status == SL_STATUS_IN_PROGRESS.
      status = sl_zigbee_af_compact_logger_iterator_next_entry();
    } while ( status == SL_STATUS_IN_PROGRESS );
  }
}

static void printMessageTimestamp(sl_zigbee_af_plugin_compact_logger_message_info_t *msgInfo)
{
  if ( msgInfo->bitmask & SL_ZIGBEE_AF_PLUGIN_COMPACT_LOGGER_BITMASK_MS_PRECISION ) {
    sl_zigbee_af_cli_print("[%d.%d] ", msgInfo->timestampSeconds, msgInfo->millisecondPrecision);
  }
  sl_zigbee_af_cli_print("[%d] ", msgInfo->timestampSeconds);
}
