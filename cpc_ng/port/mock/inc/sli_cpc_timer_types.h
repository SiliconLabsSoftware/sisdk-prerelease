/***************************************************************************/ /**
 * @file sli_cpc_timer_types.h
 * @brief Mock CPC Timer handle for unit tests.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#ifndef SLI_CPC_TIMER_TYPES_H
#define SLI_CPC_TIMER_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "sl_slist.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sli_cpc_timer {
  /// Absolute tick value at which this timer should fire.
  uint64_t expiration_tick;

  /// Callback invoked when the virtual clock reaches `expiration_tick`.
  void (*callback)(struct sli_cpc_timer *timer, void *data);

  /// User cookie passed back to the callback.
  void *callback_data;

  /// Linkage in the mock's active timer list (sorted by `expiration_tick`).
  sl_slist_node_t node;

  /// True when the handle is queued in the mock's active timer list.
  bool running;
} sli_cpc_timer_t;

#ifdef __cplusplus
}
#endif

#endif // SLI_CPC_TIMER_TYPES_H
