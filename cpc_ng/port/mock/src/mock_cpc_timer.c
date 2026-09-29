/***************************************************************************/ /**
 * @file
 * @brief Implementation of the mock CPC timer for unit tests.
 ******************************************************************************/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sl_slist.h"
#include "sl_status.h"

#include "../../../src/sli_cpc_timer.h"
#include "mock_cpc_timer.h"

#define MOCK_CPC_TIMER_DEFAULT_FREQ_HZ 32768u

static struct {
  uint64_t current_tick;
  uint32_t frequency_hz;
  sl_slist_node_t *head;
} s_state = {
  .current_tick = 0,
  .frequency_hz = MOCK_CPC_TIMER_DEFAULT_FREQ_HZ,
  .head = NULL,
};

/*--------------------------------------------------------------------------*/
/* Active-list helpers                                                      */
/*--------------------------------------------------------------------------*/

static sli_cpc_timer_t *timer_of(sl_slist_node_t *node)
{
  return SL_SLIST_ENTRY(node, sli_cpc_timer_t, node);
}

static void list_remove(sli_cpc_timer_t *timer)
{
  sl_slist_remove(&s_state.head, &timer->node);
  timer->node.node = NULL;
  timer->running = false;
}

// `sl_slist_sort()` comparator: returns true when `l` should come before `r`
// in the active timer list (ascending by `expiration_tick`). Using `<=` keeps
// ties in insertion order, which gives newcomers FIFO semantics behind any
// existing timer with the same expiration.
static bool timer_node_ordered(sl_slist_node_t *l, sl_slist_node_t *r)
{
  return timer_of(l)->expiration_tick <= timer_of(r)->expiration_tick;
}

static void list_insert_sorted(sli_cpc_timer_t *timer)
{
  sl_slist_push_back(&s_state.head, &timer->node);
  sl_slist_sort(&s_state.head, timer_node_ordered);
  timer->running = true;
}

static sli_cpc_timer_t *list_pop_head(void)
{
  sl_slist_node_t *node = sl_slist_pop(&s_state.head);
  sli_cpc_timer_t *timer;

  if (node == NULL) {
    return NULL;
  }

  timer = timer_of(node);
  timer->node.node = NULL;
  timer->running = false;
  return timer;
}

static uint64_t ms_to_ticks_u64(uint32_t time_ms)
{
  // Round up so a delay always covers the requested time.
  return ((uint64_t)time_ms * (uint64_t)s_state.frequency_hz + 999u) / 1000u;
}

static sl_status_t schedule(sli_cpc_timer_t *timer, uint64_t timeout_ticks, sli_cpc_timer_callback_t callback,
                            void *callback_data)
{
  if (timer == NULL || callback == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (timer->running) {
    list_remove(timer);
  }

  timer->callback = callback;
  timer->callback_data = callback_data;
  timer->expiration_tick = s_state.current_tick + timeout_ticks;
  list_insert_sorted(timer);

  return SL_STATUS_OK;
}

static void fire_one(sli_cpc_timer_t *timer)
{
  // The handle has been removed from the list and `running` is false. The
  // callback is allowed to re-schedule it; doing so will put it back into
  // the sorted list under its new expiration.
  if (timer->expiration_tick > s_state.current_tick) {
    s_state.current_tick = timer->expiration_tick;
  }

  if (timer->callback != NULL) {
    timer->callback(timer, timer->callback_data);
  }
}

/*--------------------------------------------------------------------------*/
/* sli_cpc_timer.h ABI                                                       */
/*--------------------------------------------------------------------------*/

sl_status_t sli_cpc_timer_init(sli_cpc_timer_t *timer)
{
  if (timer == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  memset(timer, 0, sizeof(*timer));

  return SL_STATUS_OK;
}

uint32_t sli_cpc_timer_get_timer_frequency(void)
{
  return s_state.frequency_hz;
}

uint32_t sli_cpc_timer_ms_to_tick(uint16_t time_ms)
{
  return (uint32_t)ms_to_ticks_u64((uint32_t)time_ms);
}

uint32_t sli_cpc_timer_tick_to_ms(uint32_t tick)
{
  uint64_t ms = ((uint64_t)tick * 1000u) / (uint64_t)s_state.frequency_hz;
  return (uint32_t)ms;
}

uint32_t sli_cpc_timer_get_tick_count(void)
{
  return (uint32_t)s_state.current_tick;
}

uint64_t sli_cpc_timer_get_tick_count64(void)
{
  return s_state.current_tick;
}

sl_status_t sli_cpc_timer_start(sli_cpc_timer_t *timer, uint32_t timeout, sli_cpc_timer_callback_t callback,
                                void *callback_data)
{
  return schedule(timer, (uint64_t)timeout, callback, callback_data);
}

sl_status_t sli_cpc_timer_start_timer_ms(sli_cpc_timer_t *timer, uint32_t timeout_ms, sli_cpc_timer_callback_t callback,
                                         void *callback_data)
{
  return schedule(timer, ms_to_ticks_u64(timeout_ms), callback, callback_data);
}

sl_status_t sli_cpc_timer_restart(sli_cpc_timer_t *timer, uint32_t timeout, sli_cpc_timer_callback_t callback,
                                  void *callback_data)
{
  return schedule(timer, (uint64_t)timeout, callback, callback_data);
}

sl_status_t sli_cpc_timer_stop(sli_cpc_timer_t *timer)
{
  if (timer == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (!timer->running) {
    return SL_STATUS_OK;
  }

  list_remove(timer);

  return SL_STATUS_OK;
}

bool sli_cpc_timer_is_running(const sli_cpc_timer_t *timer)
{
  if (timer == NULL) {
    return false;
  }

  return timer->running;
}

void sli_cpc_timer_delay_millisecond(uint16_t delay_ms)
{
  mock_cpc_timer_advance_ms((uint32_t)delay_ms);
}

/*--------------------------------------------------------------------------*/
/* mock_cpc_timer.h control API                                              */
/*--------------------------------------------------------------------------*/

void mock_cpc_timer_reset(void)
{
  for (sl_slist_node_t *node = sl_slist_pop(&s_state.head); node != NULL; node = sl_slist_pop(&s_state.head)) {
    sli_cpc_timer_t *timer = timer_of(node);
    timer->node.node = NULL;
    timer->running = false;
  }

  s_state.current_tick = 0;
  s_state.frequency_hz = MOCK_CPC_TIMER_DEFAULT_FREQ_HZ;
}

void mock_cpc_timer_set_frequency(uint32_t freq_hz)
{
  if (freq_hz == 0u) {
    return;
  }

  s_state.frequency_hz = freq_hz;
}

uint64_t mock_cpc_timer_get_tick_count(void)
{
  return s_state.current_tick;
}

size_t mock_cpc_timer_count_running(void)
{
  sl_slist_node_t *node;
  size_t n = 0;

  SL_SLIST_FOR_EACH(s_state.head, node)
  {
    n++;
  }

  return n;
}

bool mock_cpc_timer_has_pending(void)
{
  return s_state.head != NULL;
}

bool mock_cpc_timer_next_expiration_tick(uint64_t *tick)
{
  if (s_state.head == NULL) {
    return false;
  }

  if (tick != NULL) {
    *tick = timer_of(s_state.head)->expiration_tick;
  }

  return true;
}

size_t mock_cpc_timer_advance_ticks(uint64_t delta_ticks)
{
  uint64_t target = s_state.current_tick + delta_ticks;
  size_t fired = 0;

  while (s_state.head != NULL && timer_of(s_state.head)->expiration_tick <= target) {
    sli_cpc_timer_t *h = list_pop_head();
    fire_one(h);
    fired++;
  }

  if (target > s_state.current_tick) {
    s_state.current_tick = target;
  }

  return fired;
}

size_t mock_cpc_timer_advance_ms(uint32_t delta_ms)
{
  return mock_cpc_timer_advance_ticks(ms_to_ticks_u64(delta_ms));
}

bool mock_cpc_timer_fire_next(void)
{
  if (s_state.head == NULL) {
    return false;
  }

  sli_cpc_timer_t *timer = list_pop_head();
  fire_one(timer);

  return true;
}

size_t mock_cpc_timer_fire_all_pending(void)
{
  // Snapshot the latest expiration currently scheduled. Timers that the
  // callbacks schedule with an expiration past this point stay queued for
  // the next test step.
  uint64_t max_processed = s_state.current_tick;
  sli_cpc_timer_t *timer;
  size_t fired = 0;

  SL_SLIST_FOR_EACH_ENTRY(s_state.head, timer, sli_cpc_timer_t, node)
  {
    if (timer->expiration_tick > max_processed) {
      max_processed = timer->expiration_tick;
    }
  }

  while (s_state.head != NULL && timer_of(s_state.head)->expiration_tick <= max_processed) {
    sli_cpc_timer_t *next = list_pop_head();
    fire_one(next);
    fired++;
  }

  return fired;
}
