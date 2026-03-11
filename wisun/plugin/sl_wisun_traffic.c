/*
 * Copyright (c) 2023 Silicon Laboratories Inc. (www.silabs.com)
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this software
 * is governed by the terms of the Silicon Labs Master Software License Agreement (MSLA)
 * available at www.silabs.com/about-us/legal/master-software-license-agreement.
 * This software is distributed to you in Object Code format and/or Source Code format and
 * is governed by the sections of the MSLA applicable to Object Code, Source Code and
 * Modified Open Source Code. By using this software, you agree to the terms of the MSLA.
 *
 * This software is a modified version of the ARM/Pelion Wi-SUN FAN software stack which is
 * licensed under Apache 2.0 (see below). Modifications to the ARM/Pelion Wi-SUN software stack
 * within this software are subject to the above copyright notice and licensed pursuant to the MSLA.
 *
 * The original ARM/Pelion Wi-SUN FAN software stack is subject to the following copyright notice.
 *
 * Copyright (c) 2014-2018, Pelion and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include "sl_status.h"
#include "sl_wisun_types.h"
#include "sl_wisun_api.h"
#include "sl_wisun_traffic.h"
#include "sl_wisun_cli_core.h"
#include "sl_cli.h"
#include "sl_sleeptimer.h"
#include "sl_wisun_common.h"
#include "assert.h"
#include "sl_memory_manager.h"

#define TRAFFIC_TASK_PRIORITY osPriorityLow3 // Lowest priority in the system for all CLI related task
#define TRAFFIC_TASK_STACK_SIZE 200 // in units of CPU_INT32U
#define TRAFFIC_FLAG_SEND 1 // Set when the task must send a packet

struct traffic_ctx {
  int socket;
  uint16_t size;
  void *buffer;
  uint32_t number;
  uint32_t sent;
  uint16_t fail;
  sl_sleeptimer_timer_handle_t timer;
  osEventFlagsId_t task_flags;
};
static struct traffic_ctx context;

static sl_status_t traffic_start(int socket, uint16_t size, uint8_t frequency, uint32_t number);
static void send_packet(struct traffic_ctx *ctx);
static void timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);
static void traffic_task(void *argument);

/**
 * @brief Initializes the task responsible for traffic generation
 */
void sl_wisun_traffic_init(void)
{
  osThreadId_t traf_task_id;

  memset(&context, 0, sizeof(context));

  const osEventFlagsAttr_t task_flags_attr = {
    "Wi-SUN Traffic Task Flags",
    0,
    NULL,
    0
  };
  context.task_flags = osEventFlagsNew(&task_flags_attr);
  EFM_ASSERT(context.task_flags != NULL);

  const osThreadAttr_t app_task_attribute = {
    "Traffic Task",
    osThreadDetached,
    NULL,
    0,
    NULL,
    (TRAFFIC_TASK_STACK_SIZE * sizeof(void *)) & 0xFFFFFFF8u,
    TRAFFIC_TASK_PRIORITY,
    0,
    0
  };
  traf_task_id = osThreadNew(traffic_task, &context, &app_task_attribute);
  EFM_ASSERT(traf_task_id != 0);
}

/**
 * @brief Sends a UDP packet on a given event
 *
 * @param argument context of the application
 */
static void traffic_task(void *argument)
{
  uint32_t flags;
  struct traffic_ctx *ctx = argument;

  while (1) {
    flags = osEventFlagsWait(ctx->task_flags,
                             TRAFFIC_FLAG_SEND,
                             osFlagsWaitAny,
                             osWaitForever);
    EFM_ASSERT((flags & CMSIS_RTOS_ERROR_MASK) == 0);
    send_packet(ctx);
    if (ctx->number != 0 && ctx->sent >= ctx->number) {
      (void)sl_sleeptimer_stop_timer(&ctx->timer);
      printf("Traffic finished: %u failed requests\r\n", ctx->fail);
    }
  }
}

/**
 * @brief Unpacks CLI arguments and starts the traffic
 *
 * @param arguments arguments from the CLI
 */
void sl_wisun_traffic_start(sl_cli_command_arg_t *arguments)
{
  int socket;
  uint16_t size;
  uint8_t frequency;
  uint32_t number;

  app_wisun_cli_mutex_lock();
  socket = (int)sl_cli_get_argument_uint32(arguments, 0);
  size = sl_cli_get_argument_uint16(arguments, 1);
  frequency = sl_cli_get_argument_uint8(arguments, 2);
  number = sl_cli_get_argument_uint32(arguments, 3);

  (void)traffic_start(socket, size, frequency, number);
  app_wisun_cli_mutex_unlock();
}

/**
 * @brief Stops the traffic
 *
 * @param arguments arguments from the CLI
 */
void sl_wisun_traffic_stop(sl_cli_command_arg_t *arguments)
{
  (void) arguments;

  app_wisun_cli_mutex_lock();

  (void)sl_sleeptimer_stop_timer(&context.timer);

  app_wisun_cli_mutex_unlock();
  printf("Traffic stopped\r\n");
}

/**
 * @brief Starts the traffic session
 *
 * @param socket id of the socket to send on
 * @param size size in bytes of the data to send in each packet
 * @param frequency packet send call frequency in Hz
 * @param number total number of packets to send in the session
 * @return SL_STATUS_OK if the session successfully started
 */
static sl_status_t traffic_start(int socket, uint16_t size, uint8_t frequency, uint32_t number)
{
  sl_status_t ret = SL_STATUS_OK;
  bool timer_used = false;
  context.socket = socket;
  context.size = size;
  context.sent = 0;
  context.number = number;
  context.fail = 0;

  if (context.buffer != NULL) {
    // A buffer was allocated by a previous call
    sl_free(context.buffer);
  }
  context.buffer = sl_malloc(context.size);
  if (context.buffer == NULL) {
    printf("Failed: unable to allocate memory for the packet\r\n");
    goto error_handler;
  }
  memset(context.buffer, 'a', context.size);

  if (context.number > 1 || context.number == 0) {
    uint16_t period_ms = 1000 / frequency;
    uint32_t period_ticks = sl_sleeptimer_ms_to_tick(period_ms);

    ret = sl_sleeptimer_start_periodic_timer(&context.timer, period_ticks, timer_callback, &context, 0, 0);
    if (ret != SL_STATUS_OK) {
      printf("[Failed: unable to start timer]\r\n");
      goto error_handler;
    }
    timer_used = true;
  }

  send_packet(&context);
  return ret;

error_handler:
  if (context.buffer != NULL) {
    sl_free(context.buffer);
  }
  if (timer_used) {
    (void)sl_sleeptimer_stop_timer(&context.timer);
  }
  return ret;
}

/**
 * @brief Sends a packet
 *
 * @param ctx context of the application
 */
static void send_packet(struct traffic_ctx *ctx)
{
  int32_t ret;
  ctx->sent++;
  printf("Sending packet %lu/%lu\r\n", ctx->sent, ctx->number);

  ret = send(ctx->socket, ctx->buffer, ctx->size, 0);
  if (ret < 0) {
    printf("[Failed: unable to send on a socket: %ld]\r\n", ret);
    ctx->fail++;
  }
}

/**
 * @brief Called on timer, triggers the task
 *
 * @param handle timer handle
 * @param data context of the application
 */
static void timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  struct traffic_ctx *ctx = data;
  EFM_ASSERT((osEventFlagsSet(ctx->task_flags, TRAFFIC_FLAG_SEND)
               & CMSIS_RTOS_ERROR_MASK) == 0);
}