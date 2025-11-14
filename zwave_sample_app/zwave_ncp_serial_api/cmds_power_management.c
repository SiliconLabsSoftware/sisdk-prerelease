/**
 * @file
 * Offers Power Management commands for Silabs targets only.
 * @attention Must be linked for Silabs build targets only.
 * @copyright 2022 Silicon Laboratories Inc.
 */
#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif
#include <stdint.h>
#include "cmd_handlers.h"
#include "SerialAPI.h"
#include "app.h"
#include "sl_power_manager.h"
#ifdef SL_CATALOG_ZW_SHUTDOWN_MANAGER_PRESENT
#include "zw_shutdown_manager.h"
#endif
#include "SwTimer.h"
#include "AppTimer.h"
#include "zpal_radio.h"
#include "zpal_log.h"

SSwTimer mWakeupTimer = { 0 }; // Timer for wakeup after sleep timeout
static zpal_radio_stay_awake_id_t sapi_stay_awake_id = 0;

/**
 * @brief wakeup after sleep timeout event
 *
 * @param pTimer Timer connected to this method
 */
static void ZCB_WakeupTimeout(__attribute__((unused)) SSwTimer *pTimer)
{
  ZPAL_LOG_DEBUG(ZPAL_LOG_APP, "ZCB_WakeupTimeout\n");
}

void cmds_power_management_init(void)
{
  AppTimerDeepSleepPersistentRegister(&mWakeupTimer, false, ZCB_WakeupTimeout);   // register for event jobs timeout event
}

ZW_ADD_CMD(FUNC_ID_PM_STAY_AWAKE)
{
#if defined(SL_CATALOG_POWER_MANAGER_NO_DEEPSLEEP_PRESENT)
  if (1 == frame->payload[0]) {
    return; // Ignore Power management requests for Controller firmwares
  }
#endif
  /* HOST->ZW: PowerLock Type, timeout of stay awake, timeout of wakeup */
  /*           Power locks type 0 for radio and 1 for peripheral*/
  uint32_t timeout = (uint32_t)(frame->payload[1] << 24);
  timeout |= (uint32_t)(frame->payload[2] << 16);
  timeout |= (uint32_t)(frame->payload[3] << 8);
  timeout |= (uint32_t)(frame->payload[4]);

  if (0 == frame->payload[0]) {
    // use relock to force acquisition of the lock
    zpal_radio_update_stay_awake(&sapi_stay_awake_id, timeout);
  }
#ifdef SL_CATALOG_ZW_SHUTDOWN_MANAGER_PRESENT
  else if (1 == frame->payload[0]) {
    if (0 == timeout) {
      zw_shutdown_manager_add_lock();
    } else {
      zw_shutdown_manager_take_temporary_lock(timeout);
    }
  }
#endif
  uint32_t timeoutwakeup = (uint32_t)(frame->payload[5] << 24);
  timeoutwakeup |= (uint32_t)(frame->payload[6] << 16);
  timeoutwakeup |= (uint32_t)(frame->payload[7] << 8);
  timeoutwakeup |= (uint32_t)(frame->payload[8]);

  if (timeout && timeoutwakeup) {
    AppTimerDeepSleepPersistentStart(&mWakeupTimer, timeoutwakeup);
  }
  set_state_and_notify(stateIdle);
}

ZW_ADD_CMD(FUNC_ID_PM_CANCEL)
{
  if (0 == frame->payload[0]) {
    zpal_radio_revoke_stay_awake(&sapi_stay_awake_id);
  }
#if defined(SL_CATALOG_POWER_MANAGER_NO_DEEPSLEEP_PRESENT)
  if (1 == frame->payload[0]) {
    set_state_and_notify(stateIdle);
    return;
  }
#endif

#ifdef SL_CATALOG_ZW_SHUTDOWN_MANAGER_PRESENT
  if (1 == frame->payload[0]) {
    zw_shutdown_manager_release_lock();
  }
#endif
  set_state_and_notify(stateIdle);
}
