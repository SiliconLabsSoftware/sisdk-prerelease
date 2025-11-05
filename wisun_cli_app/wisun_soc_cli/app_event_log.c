/***************************************************************************//**
 * @file app_event_log.c
 * @brief Application Event Log
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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

#include <stdio.h>
#include <string.h>
#include <cmsis_os2.h>
#include "sl_slist.h"
#include "sl_assert.h"
#include "sl_memory_manager.h"
#include "sl_wisun_api.h"
#include "sl_event_system.h"
#include "sl_sleeptimer.h"
#include "app_event_log.h"

#define APP_EVENT_LOG_TASK_PRIORITY osPriorityBelowNormal7
#define APP_EVENT_LOG_TASK_STACK_SIZE 500 // in units of CPU_INT32U
#define APP_EVENT_LOG_POOL_SIZE 10
#define APP_EVENT_QUEUE_SIZE 5

typedef struct {
  sl_slist_node_t node;
  uint32_t timestamp;
  sl_wisun_logger_event_t event;
} app_event_log_event_t;

static sl_slist_node_t *app_event_log_list;

static const char * const APP_EVENT_FRAME_TYPE[] =
{
  "PAS",
  "PA",
  "LPAS",
  "LPA",
  "EAPOL",
  "PCS",
  "PC",
  "LPCS",
  "LPC",
  "LTS",
  "DATA",
};

static void app_event_log_print_event(const app_event_log_event_t *evt)
{
  char address[24];
  snprintf(address, sizeof(address), "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
    evt->event.address.address[0], evt->event.address.address[1],
    evt->event.address.address[2], evt->event.address.address[3],
    evt->event.address.address[4], evt->event.address.address[5],
    evt->event.address.address[6], evt->event.address.address[7]);

  switch (evt->event.type) {
    case SL_WISUN_LOGGER_EVENT_TYPE_NEIGHBOR_LIFETIME_CHANGED:
      if (evt->event.u.neighbor_lifetime_changed.lifetime) {
        printf("Neighbor %s lifetime changed to %lu", address, evt->event.u.neighbor_lifetime_changed.lifetime);
      } else {
        printf("Neighbor %s expired", address);
      }
      break;
    case SL_WISUN_LOGGER_EVENT_TYPE_FRAME_RECEIVED:
      printf("RX %s from %s @ %d dBm",
        APP_EVENT_FRAME_TYPE[evt->event.u.frame_received.type],
        address, evt->event.u.frame_received.rssi);
        if (evt->event.u.frame_received.type == SL_WISUN_LOGGER_EVENT_FRAME_TYPE_PA) {
          printf(", PAN_ID: %d, load_factor: %d, routing_cost: %d, pan_size: %d", evt->event.u.frame_received.data.pa.pan_id,
          evt->event.u.frame_received.data.pa.load_factor,
          evt->event.u.frame_received.data.pa.routing_cost,
          evt->event.u.frame_received.data.pa.pan_size);
        }
      break;
    case SL_WISUN_LOGGER_EVENT_TYPE_FRAME_COUNTER_FAILURE:
      printf("RX from %s discarded due to frame counter", address);
      break;
    case SL_WISUN_LOGGER_EVENT_TYPE_TX_FAILURE:
      printf("TX %s to %s failed",
        APP_EVENT_FRAME_TYPE[evt->event.u.tx_failure.type],
        address);
      break;
    default:
      printf("Unknown event %llu", evt->event.type);
      break;
  }
}

static void app_event_log_task(void *argument)
{
  sl_status_t status, ret;
  sl_event_queue_t queue;
  uint8_t event_prio = 0;
  sl_event_t *event;
  app_event_log_event_t *eventlog_event;
  static sl_memory_pool_t app_event_log_pool;
  sl_slist_node_t *item;
  (void)argument;

  status = sl_event_queue_create(APP_EVENT_QUEUE_SIZE, &queue);
  if (status != SL_STATUS_OK) {
    printf("[Failed: unable to create an event queue]\r\n");
  } else {
    status = sl_event_subscribe(SL_EVENT_CLASS_WISUN, 0xFFFFFFFF, queue);
    if (status != SL_STATUS_OK) {
      printf("[Failed: unable to subscribe to events]\r\n");
    }
  }

  if (status == SL_STATUS_OK) {
    status = sl_memory_create_pool(sizeof(app_event_log_event_t), APP_EVENT_LOG_POOL_SIZE, &app_event_log_pool);
  }

  while (status == SL_STATUS_OK) {
    status = sl_event_queue_get(queue,
                                &event_prio,
                                osWaitForever,
                                &event);
    if (status == SL_STATUS_OK) {
      ret = sl_memory_pool_alloc(&app_event_log_pool, (void **)&eventlog_event);
      if (ret != SL_STATUS_OK) {
        // No more free entries, discard the oldest entry
        item = sl_slist_pop(&app_event_log_list);
        EFM_ASSERT(item != NULL);
        eventlog_event = SL_SLIST_ENTRY(item, app_event_log_event_t, node);
      }
      eventlog_event->timestamp = sl_sleeptimer_get_time();
      memcpy(&eventlog_event->event, event->event_data, sizeof(sl_wisun_logger_event_t));
      // List is oldest-first
      sl_slist_push_back(&app_event_log_list, &eventlog_event->node);
      printf("[");
      app_event_log_print_event(eventlog_event);
      printf("]\r\n");
      sl_event_process(&event);
    }
  }

  (void)sl_event_queue_delete(queue);
  (void)sl_memory_delete_pool(&app_event_log_pool);

  osThreadExit();
}

sl_status_t app_event_log_init(void)
{
  osThreadId_t app_event_log_task_id;

  const osThreadAttr_t app_task_attribute = {
    "App Event Log",
    osThreadDetached,
    NULL,
    0,
    NULL,
    (APP_EVENT_LOG_TASK_STACK_SIZE * sizeof(void *)) & 0xFFFFFFF8u,
    APP_EVENT_LOG_TASK_PRIORITY,
    0,
    0
  };

  app_event_log_task_id = osThreadNew(app_event_log_task, NULL, &app_task_attribute);
  EFM_ASSERT(app_event_log_task_id != 0);

  return SL_STATUS_OK;
}

void app_event_log_print(const sl_wisun_mac_address_t *address)
{
  app_event_log_event_t *evt = NULL;
  (void)address;

  printf("!  Age\t\tEvent\r\n");
  SL_SLIST_FOR_EACH_ENTRY(app_event_log_list, evt, app_event_log_event_t, node) {
    // List is oldest-first
    if (!memcmp(&evt->event.address, address, SL_WISUN_MAC_ADDRESS_SIZE)) {
      printf("!  %lus ago\t", sl_sleeptimer_get_time() - evt->timestamp);
      app_event_log_print_event(evt);
      printf("\r\n");
    }
  }
  printf("!\r\n");
}