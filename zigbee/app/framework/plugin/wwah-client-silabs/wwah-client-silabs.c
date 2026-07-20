/***************************************************************************//**
 * @file
 * @brief Routines for the WWAH Client Silabs plugin, which is the client
 *        implementation of the WWAH Silabs cluster.
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
#include "app/framework/util/util.h"

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif
#include "zap-cluster-command-parser.h"
#ifdef SL_CATALOG_ZIGBEE_DIDO_STORAGE_POSIX_FILESYSTEM_PRESENT
#define DIDO_STORAGE_POSIX_FILESYSTEM_PRESENT
#include "dido-storage-linux.h"
#endif

void sl_zigbee_af_sl_wwah_cluster_client_init_cb(uint8_t endpoint)
{
#ifdef DIDO_STORAGE_POSIX_FILESYSTEM_PRESENT
  sl_zigbee_af_dido_storage_init_cb();
#endif
}

//-----------------------
// ZCL commands callbacks

// Used to handle debug report query response.
sl_zigbee_af_zcl_request_status_t sl_zigbee_af_sl_wwah_cluster_debug_report_query_response_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_sl_works_with_all_hubs_cluster_debug_report_query_response_command_t cmd_data;
  uint16_t bufferLength;
  uint32_t crc;
  uint32_t readCrc;
  uint32_t preCrcLength;

  if (zcl_decode_sl_works_with_all_hubs_cluster_debug_report_query_response_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }

  if (cmd->bufLen > cmd->payloadStartIndex ) {
    bufferLength = cmd->bufLen - cmd->payloadStartIndex - 1;
    // Calculate the CRC over the entire packet except the last 4 (CRC) bytes.
    preCrcLength = bufferLength - sizeof(crc);
    crc = sl_zigbee_af_get_buffer_crc(cmd_data.debugReportData, preCrcLength, 0);
    readCrc = sl_util_fetch_low_high_int32u(&cmd_data.debugReportData[preCrcLength]);
    if ( crc == readCrc ) {
      sl_zigbee_af_core_println("Rx ReportId=%d, data:", cmd_data.debugReportId);
      sl_zigbee_af_core_print_buffer(cmd_data.debugReportData, bufferLength, TRUE);
      // Upon removing the print lines below the above data is not being printed
      // on the console during simulation
      sl_zigbee_af_core_println("");
      sl_zigbee_af_core_println("End of Data");
#ifdef DIDO_STORAGE_POSIX_FILESYSTEM_PRESENT
      sl_zigbee_af_dido_storage_write_report(cmd_data.debugReportData, bufferLength);
#endif
    } else {
      sl_zigbee_af_core_println("Error: Debug Report CRC Mismatch");
    }
  }
  return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
}

uint32_t sl_zigbee_af_sl_wwah_cluster_client_command_parse(sl_service_opcode_t opcode,
                                                           sl_service_function_context_t *context)
{
  (void)opcode;
  sl_zigbee_af_zcl_request_status_t status = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  sl_zigbee_af_cluster_command_t *cmd = (sl_zigbee_af_cluster_command_t *)context->data;
  if (cmd->mfgSpecific && cmd->commandId == ZCL_DEBUG_REPORT_QUERY_RESPONSE_COMMAND_ID) {
    status = sl_zigbee_af_sl_wwah_cluster_debug_report_query_response_cb(cmd);
  }

  return status;
}
