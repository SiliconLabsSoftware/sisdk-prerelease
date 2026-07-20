/***************************************************************************//**
 * @file
 * @brief Routines for the Critical Message Queue plugin, which stores and
 *        transmits (with retries) messages enqueued by the user.
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
#include "app/framework/include/af-types.h"

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_WWAH_APP_EVENT_RETRY_MANAGER_PRESENT
// This is needed because this component consumes the
// SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_QUEUE_SIZE option.
#include "wwah-app-event-retry-manager-config.h"
#endif

#ifdef SL_ZIGBEE_TEST
  #define debugPrintln(...) sl_zigbee_af_core_println(__VA_ARGS__)
#else
  #define debugPrintln(...)
#endif

typedef struct {
  sl_zigbee_outgoing_message_type_t type;
  uint16_t indexOrDestination;
  sl_zigbee_aps_frame_t apsStruct;
  uint8_t zclTransactionIndex;
  uint8_t retries;
  uint8_t length;
  uint8_t data[128];  // variable length based on the previous element
} sli_zigbee_critical_message_queue_entry_t;

#define INFINITE_REDELIVERY_ATTEMPTS 0xFF
#if defined(SL_CATALOG_ZIGBEE_WWAH_APP_EVENT_RETRY_MANAGER_PRESENT)
  #define CMQ_SIZE SL_ZIGBEE_AF_PLUGIN_WWAH_APP_EVENT_RETRY_MANAGER_QUEUE_SIZE
#else
  #define CMQ_SIZE 10
#endif
#define MAX_ATTEMPTS_DEFAULT INFINITE_REDELIVERY_ATTEMPTS
#define FIRST_BACKOFF_DEFAULT 5
#define BACKOFF_RATIO_DEFAULT 2
#define MAX_BACKOFF_DEFAULT 300

#define ABS_MAX_BACKOFF_SEC \
  (24   /* hours */         \
   * 60 /* minutes */       \
   * 60) /* seconds */

static void enqueueCallback(sl_zigbee_outgoing_message_type_t type,
                            uint16_t indexOrDestination,
                            sl_zigbee_aps_frame_t *apsFrame,
                            uint16_t msgLen,
                            uint8_t *message,
                            sl_status_t status);

static void retryCallback(sl_zigbee_outgoing_message_type_t type,
                          uint16_t indexOrDestination,
                          sl_zigbee_aps_frame_t *apsFrame,
                          uint16_t msgLen,
                          uint8_t *message,
                          sl_status_t status);

const sl_zigbee_af_message_sent_function_t sl_zigbee_af_critical_message_queue_enqueue_cb = &enqueueCallback;
const sl_zigbee_af_message_sent_function_t sli_zigbee_af_critical_message_queue_retry_callback = &retryCallback;

static sli_zigbee_critical_message_queue_entry_t messageQueue[CMQ_SIZE];
static uint8_t messageQueueCount;

static uint8_t firstBackoffTimeSeconds = FIRST_BACKOFF_DEFAULT;
static uint8_t backoffSequenceRatio = BACKOFF_RATIO_DEFAULT;
static uint32_t maxBackoffTimeSeconds = MAX_BACKOFF_DEFAULT;
static uint8_t maxRedeliveryAttempts = MAX_ATTEMPTS_DEFAULT;

sl_zigbee_af_event_t sl_zigbee_af_critical_message_queue_retry_event;
#define retryEventControl (&sl_zigbee_af_critical_message_queue_retry_event)
static void retryEventHandler(sl_zigbee_af_event_t * event);

static uint32_t currentBackoff = FIRST_BACKOFF_DEFAULT;
static bool retryInProgress = false;

static void resetBackoffs(void)
{
  sl_zigbee_af_event_set_inactive(retryEventControl);
  currentBackoff = firstBackoffTimeSeconds;
}
static void increaseBackoffs(void)
{
  currentBackoff *= backoffSequenceRatio;
  if (currentBackoff > maxBackoffTimeSeconds ) {
    currentBackoff = maxBackoffTimeSeconds;
  }
  if (messageQueueCount) {
    sl_zigbee_af_event_set_delay_ms(retryEventControl, currentBackoff * MILLISECOND_TICKS_PER_SECOND);
  }
}

void sl_zigbee_af_critical_message_queue_init(void)
{
  messageQueueCount = 0;
}

static void createQueueEntry(sl_zigbee_outgoing_message_type_t type,
                             uint16_t indexOrDestination,
                             sl_zigbee_aps_frame_t *apsFrame,
                             uint16_t msgLen,
                             uint8_t *message)
{
  uint8_t index;
  if (messageQueueCount < CMQ_SIZE) {
    index = messageQueueCount;
    messageQueueCount++;
  } else {
    index = CMQ_SIZE - 1;
  }
  messageQueue[index].type = type;
  messageQueue[index].indexOrDestination = indexOrDestination;
  memmove(&(messageQueue[index].apsStruct), apsFrame, sizeof(sl_zigbee_aps_frame_t));
  messageQueue[index].retries = 0;
  messageQueue[index].length = msgLen;
  memmove(messageQueue[index].data, message, msgLen);
}

static uint8_t findQueueEntry(sl_zigbee_outgoing_message_type_t type,
                              uint16_t indexOrDestination,
                              sl_zigbee_aps_frame_t *apsFrame,
                              uint16_t msgLen,
                              uint8_t *message)
{
  for (int i = 0; i <= messageQueueCount; i++) {
    if (messageQueue[i].type == type
        && messageQueue[i].indexOrDestination == indexOrDestination
        && messageQueue[i].length == msgLen
        && messageQueue[i].apsStruct.profileId == apsFrame->profileId
        && messageQueue[i].apsStruct.clusterId == apsFrame->clusterId
        && messageQueue[i].apsStruct.sourceEndpoint == apsFrame->sourceEndpoint
        && messageQueue[i].apsStruct.destinationEndpoint == apsFrame->destinationEndpoint
        //&& !memcmp(&(messageQueue[i].apsStruct), apsFrame, sizeof(sl_zigbee_aps_frame_t))
        // previously , I ran in to an issue with the aps radius not matching, so I only compare the ones that seem important
        && !memcmp(messageQueue[i].data, message, msgLen)) {
      return i;
    }
  }
  return 0xFF;
}

void deleteQueueEntry(uint8_t index)
{
  for (; index < messageQueueCount; index++) {
    memcpy(&(messageQueue[index]), &messageQueue[index + 1], sizeof(sli_zigbee_critical_message_queue_entry_t));
  }
  messageQueueCount--;
}

void sl_zigbee_af_critical_message_queue_print_queue(void)
{
  sl_zigbee_af_cli_println("%d/%d entries", messageQueueCount, CMQ_SIZE);
  for (int i = 0; i < messageQueueCount; i++) {
    sl_zigbee_af_cli_print("Entry %d: Destination: %s %02X Len: %d [ ",
                           i,
                           (messageQueue[i].type == SL_ZIGBEE_OUTGOING_DIRECT
                            ? "NODE"
                            : ((messageQueue[i].type == SL_ZIGBEE_OUTGOING_VIA_ADDRESS_TABLE)
                               ? "ADDR" : "BIND")),
                           messageQueue[i].indexOrDestination,
                           messageQueue[i].length
                           );
    sl_zigbee_af_cli_print_buffer(messageQueue[i].data, messageQueue[i].length, true);
    sl_zigbee_af_cli_println(" ]");
  }
}

void sl_zigbee_af_critical_message_queue_print_config(void)
{
  sl_zigbee_af_cli_println("First backoff time (sec): %d", firstBackoffTimeSeconds);
  sl_zigbee_af_cli_println("Backoff sequence common ratio: %d", backoffSequenceRatio);
  sl_zigbee_af_cli_println("Max backoff time (sec): %d", maxBackoffTimeSeconds);
  sl_zigbee_af_cli_println("Max redelivery attempts: %d", maxRedeliveryAttempts);
}

static void retryCMQHead(void)
{
  assert(messageQueueCount > 0);
  retryInProgress = true;
  sl_zigbee_af_send_unicast_with_cb(messageQueue[0].type,
                                    messageQueue[0].indexOrDestination,
                                    &(messageQueue[0].apsStruct),
                                    messageQueue[0].length,
                                    messageQueue[0].data,
                                    &retryCallback);
}

static void enqueueCallback(sl_zigbee_outgoing_message_type_t type,
                            uint16_t indexOrDestination,
                            sl_zigbee_aps_frame_t *apsFrame,
                            uint16_t msgLen,
                            uint8_t *message,
                            sl_status_t status)
{
  resetBackoffs();
  if (status != SL_STATUS_OK) {
    debugPrintln("Error: CMQ message enqueue error 0x%02X", status);
    createQueueEntry(type, indexOrDestination, apsFrame, msgLen, message);
  }
  if (messageQueueCount
      && !retryInProgress) {
    sl_zigbee_af_event_set_active(retryEventControl);
  }
}

static void retryCallback(sl_zigbee_outgoing_message_type_t type,
                          uint16_t indexOrDestination,
                          sl_zigbee_aps_frame_t *apsFrame,
                          uint16_t msgLen,
                          uint8_t *message,
                          sl_status_t status)
{
  retryInProgress = false;
  uint8_t index = findQueueEntry(type, indexOrDestination, apsFrame, msgLen, message);
  if (index == 0xFF) {
    sl_zigbee_af_core_println("ERROR: Retry Callback for unknown message");
    return;
  }
  if (status == SL_STATUS_OK) {
    deleteQueueEntry(index);
    resetBackoffs();
    if (messageQueueCount) {
      sl_zigbee_af_event_set_active(retryEventControl);
    }
  } else {
    debugPrintln("Error: CMQ message retry error 0x%02X", status);
    messageQueue[index].retries++;
    if (maxRedeliveryAttempts != INFINITE_REDELIVERY_ATTEMPTS
        && messageQueue[index].retries >= maxRedeliveryAttempts) {
      deleteQueueEntry(index);
    }
    increaseBackoffs();
  }
}

void sli_zigbee_af_critical_message_queue_retry_init_callback(uint8_t init_level)
{
  (void)init_level;

  sl_zigbee_af_event_init(&sl_zigbee_af_critical_message_queue_retry_event,
                          retryEventHandler);
}

void retryEventHandler(sl_zigbee_af_event_t * event)
{
  UNUSED_VAR(event);

  sl_zigbee_af_event_set_inactive(retryEventControl);
  retryCMQHead();
}

#if defined(SL_CATALOG_ZIGBEE_WWAH_APP_EVENT_RETRY_MANAGER_PRESENT)
sl_status_t sl_zigbee_af_wwah_app_event_retry_manager_config_backoff_params_cb(uint8_t inFirstBackoffTimeSeconds,
                                                                               uint8_t inBackoffSeqCommonRatio,
                                                                               uint32_t inMaxBackoffTimeSeconds,
                                                                               uint8_t inMaxRedeliveryAttempts)
{
  if (!(inFirstBackoffTimeSeconds)
      || (!inBackoffSeqCommonRatio)
      || (inMaxBackoffTimeSeconds < inFirstBackoffTimeSeconds)
      || (inMaxBackoffTimeSeconds > ABS_MAX_BACKOFF_SEC)
      || (!inMaxRedeliveryAttempts) ) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  firstBackoffTimeSeconds = inFirstBackoffTimeSeconds;
  backoffSequenceRatio = inBackoffSeqCommonRatio;
  maxBackoffTimeSeconds = inMaxBackoffTimeSeconds;
  maxRedeliveryAttempts = inMaxRedeliveryAttempts;

  return SL_STATUS_OK;
}

void sl_zigbee_af_wwah_app_event_retry_manager_set_backoff_params_to_default(void)
{
  sl_zigbee_af_critical_message_queue_init();
  firstBackoffTimeSeconds = FIRST_BACKOFF_DEFAULT;
  backoffSequenceRatio = BACKOFF_RATIO_DEFAULT;
  maxBackoffTimeSeconds = MAX_BACKOFF_DEFAULT;
  maxRedeliveryAttempts = MAX_ATTEMPTS_DEFAULT;
  currentBackoff = FIRST_BACKOFF_DEFAULT;
  retryInProgress = false;
}
#endif // WWAH_APP_EVENT_RETRY_MANAGER_PRESENT
