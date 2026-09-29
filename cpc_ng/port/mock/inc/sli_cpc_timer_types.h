/***************************************************************************/ /**
 * @file
 * @brief Mock CPC Timer handle for unit tests.
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
