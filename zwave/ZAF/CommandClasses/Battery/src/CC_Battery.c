/**
 *
 * @brief Battery Command Class source file
 * @copyright 2019 Silicon Laboratories Inc.
 *
 */

/****************************************************************************/
/*                              INCLUDE FILES                               */
/****************************************************************************/
#include <ZW_TransportLayer.h>

#include <CC_Battery.h>
#include "cc_battery_io.h"
#include "cc_battery_config.h"
#include <ZW_TransportMulticast.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <ZAF_Common_interface.h>
#include <ZAF_file_ids.h>
#include "zpal_log.h"

/****************************************************************************/
/*                      PRIVATE TYPES and DEFINITIONS                       */
/****************************************************************************/
#define BATTERY_DATA_UNASSIGNED_VALUE (CMD_CLASS_BATTERY_LEVEL_FULL + 1)  // Just some value not defined in cc_battery_level_t

/****************************************************************************/
/*                              PRIVATE DATA                                */
/****************************************************************************/
static SBatteryData BatteryData;

/****************************************************************************/
/*                              EXPORTED DATA                               */
/****************************************************************************/

/****************************************************************************/
/*                            PRIVATE FUNCTIONS                             */
/****************************************************************************/

static void build_battery_report_v3(ZW_BATTERY_REPORT_V3_FRAME *frame, uint8_t endpoint)
{
  SBatteryReportData battery_report_data = { 0 };
  frame->cmdClass = COMMAND_CLASS_BATTERY_V3;
  frame->cmd = BATTERY_REPORT_V3;
  frame->batteryLevel = CC_Battery_BatteryGet_handler(endpoint, &battery_report_data);
  frame->properties1 =
    ((battery_report_data.battery_charging_status << BATTERY_REPORT_PROPERTIES1_CHARGING_STATUS_SHIFT_V3)
     & BATTERY_REPORT_PROPERTIES1_CHARGING_STATUS_MASK_V3)
    | (battery_report_data.rechargeable ? BATTERY_REPORT_PROPERTIES1_RECHARGEABLE_BIT_MASK_V3 : 0u)
    | (battery_report_data.backup_battery ? BATTERY_REPORT_PROPERTIES1_BACKUP_BATTERY_BIT_MASK_V3 : 0u)
    | (battery_report_data.overheating ? BATTERY_REPORT_PROPERTIES1_OVERHEATING_BIT_MASK_V3 : 0u)
    | (battery_report_data.low_fluid ? BATTERY_REPORT_PROPERTIES1_LOW_FLUID_BIT_MASK_V3 : 0u)
    | (battery_report_data.replace_recharge_status_bitmask & BATTERY_REPORT_PROPERTIES1_REPLACE_RECHARGE_MASK_V3);
  frame->properties2 =
    (battery_report_data.disconnected ? BATTERY_REPORT_PROPERTIES2_DISCONNECTED_BIT_MASK_V3 : 0u)
    | (battery_report_data.low_temperature_status ? BATTERY_REPORT_PROPERTIES2_LOW_TEMPERATURE_STATUS_BIT_MASK_V3 : 0u);
}

static uint8_t build_battery_health_report_v3(uint8_t endpoint, ZW_APPLICATION_TX_BUFFER *pFrameOut)
{
  SBatteryHealthReportData battery_health_report_data = { 0 };
  CC_Battery_BatteryHealthGet_handler(endpoint, &battery_health_report_data);

  uint8_t temperature_size = battery_health_report_data.size;
  assert(temperature_size <= 4u);
  if (temperature_size > 4u) {
    temperature_size = 0u;
  }

  uint8_t properties1 =
    (((battery_health_report_data.precision << BATTERY_HEALTH_REPORT_PROPERTIES1_PRECISION_SHIFT_V3)
      & BATTERY_HEALTH_REPORT_PROPERTIES1_PRECISION_MASK_V3)
     | ((battery_health_report_data.scale << BATTERY_HEALTH_REPORT_PROPERTIES1_SCALE_SHIFT_V3)
        & BATTERY_HEALTH_REPORT_PROPERTIES1_SCALE_MASK_V3)
     | (temperature_size & BATTERY_HEALTH_REPORT_PROPERTIES1_SIZE_MASK_V3));

  pFrameOut->ZW_Common.cmdClass = COMMAND_CLASS_BATTERY_V3;
  pFrameOut->ZW_Common.cmd = BATTERY_HEALTH_REPORT_V3;
  pFrameOut->ZW_BatteryHealthReport1byteV3Frame.maximumCapacity = battery_health_report_data.max_capacity;
  pFrameOut->ZW_BatteryHealthReport1byteV3Frame.properties1 = properties1;
  if (temperature_size > 0u) {
    memcpy(&pFrameOut->ZW_BatteryHealthReport1byteV3Frame.batteryTemperature1,
           battery_health_report_data.battery_temperature,
           temperature_size);
  }

  return (uint8_t)(offsetof(ZW_BATTERY_HEALTH_REPORT_1BYTE_V3_FRAME, batteryTemperature1) + temperature_size);
}

static received_frame_status_t
CC_Battery_handler(
  RECEIVE_OPTIONS_TYPE_EX *rxOpt,
  ZW_APPLICATION_TX_BUFFER *pCmd,
  __attribute__((unused)) uint8_t cmdLength,
  ZW_APPLICATION_TX_BUFFER *pFrameOut,
  uint8_t * pFrameOutLength)
{
  if (true == Check_not_legal_response_job(rxOpt)) {
    return RECEIVED_FRAME_STATUS_FAIL;
  }

  if (pCmd->ZW_Common.cmd == BATTERY_GET_V3) {
    build_battery_report_v3(&pFrameOut->ZW_BatteryReportV3Frame, rxOpt->destNode.endpoint);
    *pFrameOutLength = sizeof(ZW_BATTERY_REPORT_V3_FRAME);

    return RECEIVED_FRAME_STATUS_SUCCESS;
  } else if (pCmd->ZW_Common.cmd == BATTERY_HEALTH_GET_V3) {
    *pFrameOutLength = build_battery_health_report_v3(rxOpt->destNode.endpoint, pFrameOut);
    return RECEIVED_FRAME_STATUS_SUCCESS;
  }
  return RECEIVED_FRAME_STATUS_NO_SUPPORT;
}

static uint8_t lifeline_reporting(ccc_pair_t * p_ccc_pair)
{
  p_ccc_pair->cmdClass = COMMAND_CLASS_BATTERY_V3;
  p_ccc_pair->cmd      = BATTERY_REPORT_V3;
  return 1;
}

static void reset(void)
{
  BatteryData.lastReportedBatteryLevel = BATTERY_DATA_UNASSIGNED_VALUE;
  cc_battery_write(&BatteryData);
}

static void init(void)
{
  if (!cc_battery_read(&BatteryData)) {
    reset();
  }
}

/****************************************************************************/
/*                            PUBLIC FUNCTIONS                              */
/****************************************************************************/

bool cc_battery_check_level_changed(void)
{
  uint8_t currentBatteryLevel;

  if (EINCLUSIONSTATE_EXCLUDED == ZAF_GetInclusionState()) {
    // We are not network included. Nothing to do.
    ZPAL_LOG_DEBUG(ZPAL_LOG_CC_BATTERY, "\r\n%s: Not included\r\n", __func__);
    return false;
  }

  SBatteryReportData battery_report_data = { 0 };
  currentBatteryLevel = CC_Battery_BatteryGet_handler(ENDPOINT_ROOT, &battery_report_data);
  ZPAL_LOG_DEBUG(ZPAL_LOG_CC_BATTERY, "\r\n%s: Current Level=%d, Last reported level=%d\r\n", __func__, currentBatteryLevel, BatteryData.lastReportedBatteryLevel);

  if ((currentBatteryLevel == BatteryData.lastReportedBatteryLevel)
      || (currentBatteryLevel == BatteryData.lastReportedBatteryLevel + cc_battery_config_get_reporting_decrements())) { // Hysteresis
    // Battery level hasn't changed (significantly) since last reported. Do nothing
    return false;
  }

  return true;
}

bool
CC_Battery_LevelReport_tx(
  const AGI_PROFILE* pProfile,
  uint8_t sourceEndpoint,
  VOID_CALLBACKFUNC(pCbFunc)(TRANSMISSION_RESULT * pTransmissionResult))
{
  ZW_BATTERY_REPORT_V3_FRAME batteryReportFrame;
  build_battery_report_v3(&batteryReportFrame, sourceEndpoint);

  size_t battery_report_size_without_cmd_class_grp = sizeof(ZW_BATTERY_REPORT_V3_FRAME) - sizeof(CMD_CLASS_GRP);

  JOB_STATUS zaf_job_status =
    cc_engine_multicast_request(
      pProfile,
      sourceEndpoint,
      (CMD_CLASS_GRP*)&batteryReportFrame.cmdClass,
      &batteryReportFrame.batteryLevel,
      battery_report_size_without_cmd_class_grp,
      false,
      pCbFunc
      );

  if (JOB_STATUS_SUCCESS == zaf_job_status) {
    BatteryData.lastReportedBatteryLevel = batteryReportFrame.batteryLevel;
    cc_battery_write(&BatteryData);
    return true;
  } else {
    return false;
  }
}

ZW_WEAK uint8_t
CC_Battery_BatteryGet_handler(__attribute__((unused)) uint8_t endpoint, SBatteryReportData *data)
{
  data->rechargeable = CC_BATTERY_RECHARGEABLE;
  data->backup_battery = CC_BATTERY_BACKUP_BATTERY;
  return (uint8_t)CMD_CLASS_BATTERY_LEVEL_FULL;
}

ZW_WEAK void
CC_Battery_BatteryHealthGet_handler(__attribute__((unused)) uint8_t endpoint, SBatteryHealthReportData *data)
{
  memset(data->battery_temperature, 0, sizeof(data->battery_temperature));
  data->max_capacity = 0xFF; //Unknown
  data->precision = CC_BATTERY_HEALTH_TEMPERATURE_PRECISION;
  data->scale = CC_BATTERY_HEALTH_TEMPERATURE_SCALE;
  data->size = CC_BATTERY_HEALTH_TEMPERATURE_VALUE_SIZE;
}

REGISTER_CC_V4(COMMAND_CLASS_BATTERY, BATTERY_VERSION_V3, CC_Battery_handler, NULL, NULL, lifeline_reporting, 0, init, reset);
