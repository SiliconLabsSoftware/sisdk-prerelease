/**************************************************************************//**
 * @file cmds_rf.c
 * @brief The source file for command handling of RF related serialAPI
 * commands
 * @copyright 2022 Silicon Laboratories Inc.
 *****************************************************************************/

#include <cmds_rf.h>
#include <app.h>
#include <ZW_application_transport_interface.h>
#include <utils.h>
#include "ZAF_Common_interface.h"

#ifdef SUPPORT_ZW_SET_LISTEN_BEFORE_TALK_THRESHOLD
void func_id_set_listen_before_talk(__attribute__((unused)) uint8_t inputLength,
                                    const uint8_t *pInputBuffer,
                                    uint8_t *pOutputBuffer,
                                    uint8_t *pOutputLength)
{
  const SApplicationHandles *m_pAppHandles = ZAF_getAppHandle();
  const uint8_t channel = pInputBuffer[0];
  const int8_t level = (int8_t)pInputBuffer[1];
  SZwaveCommandPackage setLBTMode = {
    .eCommandType = EZWAVECOMMANDTYPE_ZW_SET_LBT_THRESHOLD,
    .uCommandParams.SetLBTThreshold.channel = channel,
    .uCommandParams.SetLBTThreshold.level = level
  };
  pOutputBuffer[0] = 0x00U;
  *pOutputLength = 1;
  if (EQUEUENOTIFYING_STATUS_SUCCESS != QueueNotifyingSendToBack(m_pAppHandles->pZwCommandQueue, (uint8_t *)&setLBTMode, 0)) {
    return;
  }

  SZwaveCommandStatusPackage result = { .eStatusType = EZWAVECOMMANDSTATUS_ZW_SET_LBT_THRESHOLD };
  pOutputBuffer[0] = (GetCommandResponse(&result, result.eStatusType) && result.Content.SetLBTThresholdStatus.result)
                     ? 0x01U : 0x00U;
}
#endif
