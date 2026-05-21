/**
 * @file
 * Handler for Command Class Wake On Critical Message.
 * @copyright 2026 Silicon Laboratories Inc.
 */

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/
#include <assert.h>
#include <ZW_TransportEndpoint.h>
#include <ZW_TransportMulticast.h>
#include <ZAF_Common_interface.h>
#include "CC_WakeOnCriticalMessage.h"
#include "ZAF_types.h"
#include <CC_Common.h>
#include <ZAF_nvm.h>
#include <ZAF_file_ids.h>

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/

static void send_severity_to_protocol_task(const uint8_t severity_level)
{
  SZwaveCommandPackage cmdPackage = {
    .eCommandType = EZWAVECOMMANDTYPE_SET_SEVERITY_LEVEL,
    .uCommandParams.SetSeverityLevel.severity_level = severity_level
  };
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus =
    QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);
}

static bool nvm_write_severity(uint8_t severity)
{
  return ZPAL_STATUS_OK == ZAF_nvm_write(ZAF_FILE_ID_CC_WOCM_SEVERITY, &severity, sizeof(severity));
}

static bool nvm_read_severity(uint8_t *severity)
{
  return ZPAL_STATUS_OK == ZAF_nvm_read(ZAF_FILE_ID_CC_WOCM_SEVERITY, severity, sizeof(*severity));
}

/**
 * Retrieve the severity level from the protocol task via the command queue /
 * command-status queue round-trip.
 *
 * @param[out] severity  Pointer where the retrieved severity level is stored.
 */
static void retrieve_severity_from_protocol_task(uint8_t *severity)
{
  SApplicationHandles *pAppHandles = ZAF_getAppHandle();
  QueueHandle_t statusQueue = pAppHandles->ZwCommandStatusQueue;

  SZwaveCommandPackage cmdPackage = {
    .eCommandType = EZWAVECOMMANDTYPE_GET_SEVERITY_LEVEL,
  };
  __attribute__((unused)) EQueueNotifyingStatus QueueStatus =
    QueueNotifyingSendToBack(ZAF_getZwCommandQueue(), (uint8_t *)&cmdPackage, 0);
  assert(EQUEUENOTIFYING_STATUS_SUCCESS == QueueStatus);

  SZwaveCommandStatusPackage cmdStatus = { 0 };
  while (true) {
    __attribute__((unused)) BaseType_t rxStatus = xQueueReceive(statusQueue, (uint8_t *)&cmdStatus, portMAX_DELAY);
    assert(pdTRUE == rxStatus);
    if (cmdStatus.eStatusType == EZWAVECOMMANDSTATUS_GET_SEVERITY_LEVEL) {
      *severity = cmdStatus.Content.GetSeverityLevelStatus.result;
      return;
    }
    /* Re-insert non-matching message */
    __attribute__((unused)) BaseType_t result = xQueueSendToBack(statusQueue, (uint8_t *)&cmdStatus, 0);
    assert(pdTRUE == result);
  }
}

static received_frame_status_t
CC_WakeOnCriticalMessage_handler(
  cc_handler_input_t * input,
  cc_handler_output_t * output)
{
  switch (input->frame->ZW_Common.cmd) {
    case COMMAND_WAKE_ON_CRITICAL_MESSAGE_CONFIGURATION_SET:
    {
      if (input->length < sizeof(ZW_WAKE_ON_CRITICAL_MESSAGE_CONFIGURATION_SET_FRAME)) {
        return RECEIVED_FRAME_STATUS_FAIL;
      }

      const uint8_t *raw = (const uint8_t *)input->frame;
      uint8_t severity = raw[2] & WAKE_ON_CRITICAL_MESSAGE_SEVERITY_MASK;

      nvm_write_severity(severity);
      send_severity_to_protocol_task(severity);

      return RECEIVED_FRAME_STATUS_SUCCESS;
    }

    case COMMAND_WAKE_ON_CRITICAL_MESSAGE_CONFIGURATION_GET:
    {
      if (true == Check_not_legal_response_job(input->rx_options)) {
        return RECEIVED_FRAME_STATUS_FAIL;
      }

      uint8_t severity;
      retrieve_severity_from_protocol_task(&severity);

      uint8_t *raw_out = (uint8_t *)output->frame;

      raw_out[0] = COMMAND_CLASS_WAKE_ON_CRITICAL_MESSAGE;
      raw_out[1] = COMMAND_WAKE_ON_CRITICAL_MESSAGE_CONFIGURATION_REPORT;
      raw_out[2] = severity & WAKE_ON_CRITICAL_MESSAGE_SEVERITY_MASK;

      output->length = sizeof(ZW_WAKE_ON_CRITICAL_MESSAGE_CONFIGURATION_REPORT_FRAME);

      return RECEIVED_FRAME_STATUS_SUCCESS;
    }

    default:
      break;
  }

  return RECEIVED_FRAME_STATUS_NO_SUPPORT;
}

static void init(void)
{
  uint8_t severity = WAKE_ON_CRITICAL_MESSAGE_SEVERITY_DEFAULT;
  if (nvm_read_severity(&severity)) {
    severity &= WAKE_ON_CRITICAL_MESSAGE_SEVERITY_MASK;
  }
  send_severity_to_protocol_task(severity);
}

static void reset(void)
{
  uint8_t severity = WAKE_ON_CRITICAL_MESSAGE_SEVERITY_DEFAULT;
  nvm_write_severity(severity);
  send_severity_to_protocol_task(severity);
}

/****************************************************************************/
/*                           EXPORTED FUNCTIONS                             */
/****************************************************************************/

uint8_t CC_WakeOnCriticalMessage_getSeverityThreshold(void)
{
  uint8_t severity;
  retrieve_severity_from_protocol_task(&severity);
  return severity;
}

/**************************************************************************************************
 * Linker magic - Creates a section for an array of registered CCs and mapped CCs to the Basic CC.
 *************************************************************************************************/

REGISTER_CC_V5(COMMAND_CLASS_WAKE_ON_CRITICAL_MESSAGE, COMMAND_CLASS_WAKE_ON_CRITICAL_MESSAGE_VERSION_V1, CC_WakeOnCriticalMessage_handler, NULL, NULL, NULL, 0, init, reset);
