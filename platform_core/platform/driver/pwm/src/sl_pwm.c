/***************************************************************************//**
 * @file
 * @brief PWM Driver
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sl_pwm.h"

#include "sl_gpio.h"

#if defined(PWM_USES_GPIO_LATCHING)
#include "sl_gpio_peripheral_pin_latching.h"
#include "sl_hal_gpio.h"
#endif

#if defined(_SILICON_LABS_32B_SERIES_3)
#include "sl_hal_timer.h"
#else
#include "em_timer.h"
#endif

#include "sl_clock_manager.h"
#include "sl_device_peripheral.h"

#include "sl_log_helper.h"
// -----------------------------------------------------------------------------
//                                Static Functions
// -----------------------------------------------------------------------------

#define SLI_PWM_CHANNEL_COUNT_MAX  8U

#if defined(_SILICON_LABS_32B_SERIES_3)
typedef struct {
  uint8_t duty_cycle;
  uint8_t port;
  uint8_t pin;
} sli_pwm_channel_state_t;

typedef struct {
  bool configured;
  uint32_t top;
  uint8_t configured_channels;
  uint8_t active_channels;
  uint8_t output_invert_channels;
  uint16_t latched_channels;
} sli_pwm_timer_state_t;

// TIMER register state is not retained when Series 3 TIMER is disabled. Keep
// the small amount of configuration needed to rebuild PWM on the next start.
static sli_pwm_timer_state_t sli_pwm_timer_state[TIMER_COUNT];

// Keep channel metadata sized to each TIMER instance, which can have a
// different number of CC channels on the same device.
#if TIMER_COUNT > 0
static sli_pwm_channel_state_t sli_pwm_channel_state_0[TIMER0_CC_NUM];
#endif
#if TIMER_COUNT > 1
static sli_pwm_channel_state_t sli_pwm_channel_state_1[TIMER1_CC_NUM];
#endif
#if TIMER_COUNT > 2
static sli_pwm_channel_state_t sli_pwm_channel_state_2[TIMER2_CC_NUM];
#endif
#if TIMER_COUNT > 3
static sli_pwm_channel_state_t sli_pwm_channel_state_3[TIMER3_CC_NUM];
#endif
#if TIMER_COUNT > 4
static sli_pwm_channel_state_t sli_pwm_channel_state_4[TIMER4_CC_NUM];
#endif
#if TIMER_COUNT > 5
static sli_pwm_channel_state_t sli_pwm_channel_state_5[TIMER5_CC_NUM];
#endif
#if TIMER_COUNT > 6
static sli_pwm_channel_state_t sli_pwm_channel_state_6[TIMER6_CC_NUM];
#endif
#if TIMER_COUNT > 7
static sli_pwm_channel_state_t sli_pwm_channel_state_7[TIMER7_CC_NUM];
#endif
#if TIMER_COUNT > 8
static sli_pwm_channel_state_t sli_pwm_channel_state_8[TIMER8_CC_NUM];
#endif
#if TIMER_COUNT > 9
static sli_pwm_channel_state_t sli_pwm_channel_state_9[TIMER9_CC_NUM];
#endif

static sli_pwm_timer_state_t *get_pwm_timer_state(TIMER_TypeDef *timer)
{
  int32_t timer_num = TIMER_NUM(timer);
  SL_LOG_DEBUG_ASSERT((timer_num >= 0) && (timer_num < (int32_t)TIMER_COUNT));
  if ((timer_num < 0) || (timer_num >= (int32_t)TIMER_COUNT)) {
    return NULL;
  }
  return &sli_pwm_timer_state[timer_num];
}

static sli_pwm_channel_state_t *get_pwm_channel_state(TIMER_TypeDef *timer)
{
  switch (TIMER_NUM(timer)) {
#if TIMER_COUNT > 0
    case 0: return sli_pwm_channel_state_0;
#endif
#if TIMER_COUNT > 1
    case 1: return sli_pwm_channel_state_1;
#endif
#if TIMER_COUNT > 2
    case 2: return sli_pwm_channel_state_2;
#endif
#if TIMER_COUNT > 3
    case 3: return sli_pwm_channel_state_3;
#endif
#if TIMER_COUNT > 4
    case 4: return sli_pwm_channel_state_4;
#endif
#if TIMER_COUNT > 5
    case 5: return sli_pwm_channel_state_5;
#endif
#if TIMER_COUNT > 6
    case 6: return sli_pwm_channel_state_6;
#endif
#if TIMER_COUNT > 7
    case 7: return sli_pwm_channel_state_7;
#endif
#if TIMER_COUNT > 8
    case 8: return sli_pwm_channel_state_8;
#endif
#if TIMER_COUNT > 9
    case 9: return sli_pwm_channel_state_9;
#endif
    default: SL_LOG_DEBUG_ASSERT(0); return NULL;
  }
}

static bool is_pwm_channel_valid(TIMER_TypeDef *timer, uint8_t channel)
{
  bool valid = (channel < SLI_PWM_CHANNEL_COUNT_MAX)
               && (channel < SL_HAL_TIMER_CHANNEL_NUM(timer));
  SL_LOG_DEBUG_ASSERT(valid);
  return valid;
}

static uint32_t pwm_compare(uint32_t top, uint8_t duty_cycle)
{
  return (uint32_t)(((uint64_t)top * duty_cycle) / 100U);
}

#endif // _SILICON_LABS_32B_SERIES_3

#if defined(_SILICON_LABS_32B_SERIES_3) || defined(PWM_USES_GPIO_LATCHING)
static volatile uint32_t *get_pwm_route_register(TIMER_TypeDef *timer,
                                                 uint8_t channel)
{
#if defined(_GPIO_TIMER_ROUTEEN_MASK)
  volatile uint32_t *route_register = &GPIO->TIMERROUTE[TIMER_NUM(timer)].CC0ROUTE;
#else
  volatile uint32_t *route_register;
  switch (TIMER_NUM(timer)) {
#if defined(TIMER0_BASE)
    case 0: route_register = &GPIO->TIMER0ROUTE[0].CC0ROUTE; break;
#endif
#if defined(TIMER1_BASE)
    case 1: route_register = &GPIO->TIMER1ROUTE[0].CC0ROUTE; break;
#endif
#if defined(TIMER2_BASE)
    case 2: route_register = &GPIO->TIMER2ROUTE[0].CC0ROUTE; break;
#endif
#if defined(TIMER3_BASE)
    case 3: route_register = &GPIO->TIMER3ROUTE[0].CC0ROUTE; break;
#endif
#if defined(TIMER4_BASE)
    case 4: route_register = &GPIO->TIMER4ROUTE[0].CC0ROUTE; break;
#endif
#if defined(TIMER5_BASE)
    case 5: route_register = &GPIO->TIMER5ROUTE[0].CC0ROUTE; break;
#endif
#if defined(TIMER6_BASE)
    case 6: route_register = &GPIO->TIMER6ROUTE[0].CC0ROUTE; break;
#endif
    default: SL_LOG_DEBUG_ASSERT(0); return NULL;
  }
#endif
  return route_register + channel;
}
#endif

#if defined(_SILICON_LABS_32B_SERIES_3)
static void configure_pwm_route(TIMER_TypeDef *timer,
                                uint8_t channel,
                                uint8_t port,
                                uint8_t pin)
{
  volatile uint32_t *route_register = get_pwm_route_register(timer, channel);
#if defined(_GPIO_TIMER_ROUTEEN_MASK)
  *route_register = (port << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT)
                    | (pin << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);
#else
  *route_register = (port << _GPIO_TIMER0_CC0ROUTE_PORT_SHIFT)
                    | (pin << _GPIO_TIMER0_CC0ROUTE_PIN_SHIFT);
#endif
}

static void set_pwm_route_enabled(TIMER_TypeDef *timer,
                                  uint8_t channel,
                                  bool enabled)
{
#if defined(_GPIO_TIMER_ROUTEEN_MASK)
  uint32_t routeen_mask = 1U << (channel + _GPIO_TIMER_ROUTEEN_CC0PEN_SHIFT);

  if (enabled) {
    GPIO->TIMERROUTE_SET[TIMER_NUM(timer)].ROUTEEN = routeen_mask;
  } else {
    GPIO->TIMERROUTE_CLR[TIMER_NUM(timer)].ROUTEEN = routeen_mask;
  }
#else
  volatile uint32_t *routeen_register;
  uint8_t routeen_shift;
  switch (TIMER_NUM(timer)) {
#if defined(TIMER0_BASE)
    case 0:
      routeen_register = enabled ? &GPIO->TIMER0ROUTE_SET[0].ROUTEEN
                         : &GPIO->TIMER0ROUTE_CLR[0].ROUTEEN;
      routeen_shift = _GPIO_TIMER0_ROUTEEN_CC0PEN_SHIFT;
      break;
#endif
#if defined(TIMER1_BASE)
    case 1:
      routeen_register = enabled ? &GPIO->TIMER1ROUTE_SET[0].ROUTEEN
                         : &GPIO->TIMER1ROUTE_CLR[0].ROUTEEN;
      routeen_shift = _GPIO_TIMER1_ROUTEEN_CC0PEN_SHIFT;
      break;
#endif
#if defined(TIMER2_BASE)
    case 2:
      routeen_register = enabled ? &GPIO->TIMER2ROUTE_SET[0].ROUTEEN
                         : &GPIO->TIMER2ROUTE_CLR[0].ROUTEEN;
      routeen_shift = _GPIO_TIMER2_ROUTEEN_CC0PEN_SHIFT;
      break;
#endif
#if defined(TIMER3_BASE)
    case 3:
      routeen_register = enabled ? &GPIO->TIMER3ROUTE_SET[0].ROUTEEN
                         : &GPIO->TIMER3ROUTE_CLR[0].ROUTEEN;
      routeen_shift = _GPIO_TIMER3_ROUTEEN_CC0PEN_SHIFT;
      break;
#endif
#if defined(TIMER4_BASE)
    case 4:
      routeen_register = enabled ? &GPIO->TIMER4ROUTE_SET[0].ROUTEEN
                         : &GPIO->TIMER4ROUTE_CLR[0].ROUTEEN;
      routeen_shift = _GPIO_TIMER4_ROUTEEN_CC0PEN_SHIFT;
      break;
#endif
#if defined(TIMER5_BASE)
    case 5:
      routeen_register = enabled ? &GPIO->TIMER5ROUTE_SET[0].ROUTEEN
                         : &GPIO->TIMER5ROUTE_CLR[0].ROUTEEN;
      routeen_shift = _GPIO_TIMER5_ROUTEEN_CC0PEN_SHIFT;
      break;
#endif
#if defined(TIMER6_BASE)
    case 6:
      routeen_register = enabled ? &GPIO->TIMER6ROUTE_SET[0].ROUTEEN
                         : &GPIO->TIMER6ROUTE_CLR[0].ROUTEEN;
      routeen_shift = _GPIO_TIMER6_ROUTEEN_CC0PEN_SHIFT;
      break;
#endif
    default: SL_LOG_DEBUG_ASSERT(0); return;
  }
  *routeen_register = 1U << (channel + routeen_shift);
#endif
}

static void restore_pwm_timer(TIMER_TypeDef *timer,
                              sli_pwm_timer_state_t *timer_state,
                              sli_pwm_channel_state_t *channel_state)
{
  sl_hal_timer_config_t timer_init = SL_HAL_TIMER_CONFIG_DEFAULT;
  sl_hal_timer_init(timer, &timer_init);

  // Channel CFG must be written while TIMER is disabled. Initializing each
  // channel through the HAL would disable TIMER again and discard the earlier
  // channels' state on devices without TIMER register retention.
  for (uint8_t channel = 0; channel < SL_HAL_TIMER_CHANNEL_NUM(timer); channel++) {
    uint8_t channel_mask = (uint8_t)(1U << channel);
    if (timer_state->configured_channels & channel_mask) {
      timer->CC[channel].CFG = TIMER_CC_CFG_MODE_PWM;
    }
  }

  sl_hal_timer_enable(timer);
  for (uint8_t channel = 0; channel < SL_HAL_TIMER_CHANNEL_NUM(timer); channel++) {
    uint8_t channel_mask = (uint8_t)(1U << channel);
    if (timer_state->configured_channels & channel_mask) {
      timer->CC[channel].CTRL = TIMER_CC_CTRL_CMOA_TOGGLE
                                | TIMER_CC_CTRL_ICEDGE_BOTH
                                | ((timer_state->output_invert_channels & channel_mask)
                                   ? TIMER_CC_CTRL_OUTINV : 0U);
    }
  }
  sl_hal_timer_set_top(timer, timer_state->top);
  for (uint8_t channel = 0; channel < SL_HAL_TIMER_CHANNEL_NUM(timer); channel++) {
    uint8_t channel_mask = (uint8_t)(1U << channel);
    if (timer_state->configured_channels & channel_mask) {
      uint32_t compare = pwm_compare(timer_state->top, channel_state[channel].duty_cycle);
      sl_hal_timer_channel_set_compare(timer, channel, compare);
    }
  }
  sl_hal_timer_start(timer);
}
#endif // _SILICON_LABS_32B_SERIES_3

#if defined(PWM_USES_GPIO_LATCHING)
#define SLI_PWM_GPIO_ROUTES_LATCHED_MASK  (1U << SLI_PWM_CHANNEL_COUNT_MAX)

static uint8_t *get_pwm_active_channels(TIMER_TypeDef *timer)
{
  sli_pwm_timer_state_t *timer_state = get_pwm_timer_state(timer);
  return (timer_state == NULL) ? NULL : &timer_state->active_channels;
}

static uint16_t *get_pwm_latched_channels(TIMER_TypeDef *timer)
{
  sli_pwm_timer_state_t *timer_state = get_pwm_timer_state(timer);
  return (timer_state == NULL) ? NULL : &timer_state->latched_channels;
}

static void set_pwm_channel_active(TIMER_TypeDef *timer,
                                   uint8_t channel,
                                   bool active)
{
  if (channel >= SLI_PWM_CHANNEL_COUNT_MAX) {
    return;
  }

  uint8_t *active_channels = get_pwm_active_channels(timer);
  if (active_channels == NULL) {
    return;
  }

  uint8_t channel_mask = (uint8_t)(1U << channel);
  sli_pwm_timer_state_t *timer_state = get_pwm_timer_state(timer);
  if (timer_state == NULL) {
    return;
  }
  if (active && !(timer_state->configured_channels & channel_mask)) {
    return;
  }

  if (active) {
    *active_channels |= channel_mask;
  } else {
    *active_channels &= (uint8_t) ~channel_mask;
  }
}

static void unlatch_pwm_channel_if_needed(sl_pwm_instance_t *pwm)
{
  if (pwm->channel >= SLI_PWM_CHANNEL_COUNT_MAX) {
    return;
  }
  uint16_t channel_mask = (uint16_t)(1U << pwm->channel);
  uint16_t *latched_channels = get_pwm_latched_channels(pwm->timer);
  if ((latched_channels != NULL)
      && (*latched_channels & SLI_PWM_GPIO_ROUTES_LATCHED_MASK)
      && (*latched_channels & channel_mask)) {
    sl_gpio_t gpio = { .port = pwm->port, .pin = pwm->pin };
    (void)sl_hal_gpio_unlatch_pin(&gpio);
    *latched_channels &= (uint16_t) ~channel_mask;
  }
}
#endif // PWM_USES_GPIO_LATCHING

static bool get_timer_clock(TIMER_TypeDef *timer, sl_bus_clock_t *timer_clock, sl_peripheral_t *peripheral)
{
  switch ((uint32_t)timer) {
#if defined(TIMER0_BASE)
    case TIMER0_BASE:
      *timer_clock = SL_BUS_CLOCK_TIMER0;
      *peripheral = SL_PERIPHERAL_TIMER0;
      break;
#endif
#if defined(TIMER1_BASE)
    case TIMER1_BASE:
      *timer_clock = SL_BUS_CLOCK_TIMER1;
      *peripheral = SL_PERIPHERAL_TIMER1;
      break;
#endif
#if defined(TIMER2_BASE)
    case TIMER2_BASE:
      *timer_clock = SL_BUS_CLOCK_TIMER2;
      *peripheral = SL_PERIPHERAL_TIMER2;
      break;
#endif
#if defined(TIMER3_BASE)
    case TIMER3_BASE:
      *timer_clock = SL_BUS_CLOCK_TIMER3;
      *peripheral = SL_PERIPHERAL_TIMER3;
      break;
#endif
#if defined(TIMER4_BASE)
    case TIMER4_BASE:
      *timer_clock = SL_BUS_CLOCK_TIMER4;
      *peripheral = SL_PERIPHERAL_TIMER4;
      break;
#endif
#if defined(TIMER5_BASE)
    case TIMER5_BASE:
      *timer_clock = SL_BUS_CLOCK_TIMER5;
      *peripheral = SL_PERIPHERAL_TIMER5;
      break;
#endif
#if defined(TIMER6_BASE)
    case TIMER6_BASE:
      *timer_clock = SL_BUS_CLOCK_TIMER6;
      *peripheral = SL_PERIPHERAL_TIMER6;
      break;
#endif
    default:
      SL_LOG_DEBUG_ASSERT(0);
      return false;
  }
  return true;
}

sl_status_t sl_pwm_init(sl_pwm_instance_t *pwm, sl_pwm_config_t *config)
{
  sl_bus_clock_t timer_clock;
  sl_peripheral_t peripheral;
  if (!get_timer_clock(pwm->timer, &timer_clock, &peripheral)) {
    return SL_STATUS_FAIL;
  }
#if defined(_SILICON_LABS_32B_SERIES_3)
  sli_pwm_timer_state_t *timer_state = get_pwm_timer_state(pwm->timer);
  sli_pwm_channel_state_t *channel_state = get_pwm_channel_state(pwm->timer);
  if ((timer_state == NULL) || (channel_state == NULL)
      || !is_pwm_channel_valid(pwm->timer, pwm->channel)
      || (config->frequency <= 0)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  uint8_t channel_mask = (uint8_t)(1U << pwm->channel);
  if (timer_state->configured_channels & channel_mask) {
    return SL_STATUS_INVALID_STATE;
  }
  bool first_channel = (timer_state->configured_channels == 0U);
  bool timer_was_enabled = timer_state->configured
                           && ((pwm->timer->EN & _TIMER_EN_EN_MASK) != 0U);
#if defined(PWM_USES_GPIO_LATCHING)
  if ((timer_state->active_channels != 0U) && timer_was_enabled) {
    return SL_STATUS_INVALID_STATE;
  }
#endif
#endif
  sl_clock_manager_enable_bus_clock(timer_clock);

#if defined(_SILICON_LABS_32B_SERIES_3)
  uint32_t clock_freq;
  sl_clock_branch_t clock_branch = sl_device_peripheral_get_clock_branch(peripheral);
  sl_clock_manager_get_clock_branch_frequency(clock_branch, &clock_freq);
  uint32_t ticks = clock_freq / (uint32_t)config->frequency;
  if ((ticks < 2U) || ((ticks - 1U) > SL_HAL_TIMER_MAX_COUNT(pwm->timer))
      || (timer_state->configured && timer_state->top != ticks - 1U)) {
    if (!timer_state->configured) {
      sl_clock_manager_disable_bus_clock(timer_clock);
    }
    return SL_STATUS_INVALID_PARAMETER;
  }
#endif

  // Set PWM pin as output
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);

  sl_gpio_t gpio;
  gpio.port = pwm->port;
  gpio.pin = pwm->pin;
  sl_gpio_set_pin_mode(&gpio,
                       SL_GPIO_MODE_PUSH_PULL,
                       config->polarity);

  // Set CC channel parameters
#if defined(_SILICON_LABS_32B_SERIES_2)
  TIMER_InitCC_TypeDef channel_init = TIMER_INITCC_DEFAULT;
  channel_init.mode = timerCCModePWM;
  channel_init.cmoa = timerOutputActionToggle;
  channel_init.edge = timerEdgeBoth;
  channel_init.outInvert = (config->polarity == PWM_ACTIVE_LOW);
  TIMER_InitCC(pwm->timer, pwm->channel, &channel_init);
#else
  if (!timer_state->configured) {
    timer_state->configured = true;
    timer_state->top = ticks - 1U;
  }
  timer_state->configured_channels |= channel_mask;
  channel_state[pwm->channel].duty_cycle = 0U;
  channel_state[pwm->channel].port = pwm->port;
  channel_state[pwm->channel].pin = pwm->pin;
  if (config->polarity == PWM_ACTIVE_LOW) {
    timer_state->output_invert_channels |= channel_mask;
  } else {
    timer_state->output_invert_channels &= (uint8_t) ~channel_mask;
  }
#endif
  // Configure CC channel pinout
#if defined(_SILICON_LABS_32B_SERIES_3)
  configure_pwm_route(pwm->timer, pwm->channel, pwm->port, pwm->pin);
  bool restore_timer = first_channel || timer_was_enabled;
  if (restore_timer) {
    if (pwm->timer->EN & _TIMER_EN_EN_MASK) {
      sl_hal_timer_stop(pwm->timer);
      sl_hal_timer_disable(pwm->timer);
      sl_hal_timer_wait_ready(pwm->timer);
    }
    restore_pwm_timer(pwm->timer, timer_state, channel_state);
  }
#else
  volatile uint32_t * route_register;
#if defined(_GPIO_TIMER_ROUTEEN_MASK)
  route_register = &GPIO->TIMERROUTE[TIMER_NUM(pwm->timer)].CC0ROUTE;
  route_register += pwm->channel;
  *route_register = (pwm->port << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT)
                    | (pwm->pin << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);
#else
  switch (TIMER_NUM(pwm->timer)) {
    case 0:
      route_register = &GPIO->TIMER0ROUTE[0].CC0ROUTE;
      break;
    case 1:
      route_register = &GPIO->TIMER1ROUTE[0].CC0ROUTE;
      break;
    case 2:
      route_register = &GPIO->TIMER2ROUTE[0].CC0ROUTE;
      break;
    case 3:
      route_register = &GPIO->TIMER3ROUTE[0].CC0ROUTE;
      break;
    default:
      SL_LOG_DEBUG_ASSERT(0);
      return SL_STATUS_FAIL;
  }
  route_register += pwm->channel;
  *route_register = (pwm->port << _GPIO_TIMER0_CC0ROUTE_PORT_SHIFT)
                    | (pwm->pin << _GPIO_TIMER0_CC0ROUTE_PIN_SHIFT);
#endif
#endif // _SILICON_LABS_32B_SERIES_3

  // Configure TIMER frequency
#if defined(_SILICON_LABS_32B_SERIES_2)
  uint32_t clock_freq;
  sl_clock_branch_t clock_branch;

  clock_branch = sl_device_peripheral_get_clock_branch(peripheral);
  sl_clock_manager_get_clock_branch_frequency(clock_branch, &clock_freq);
  uint32_t top = (clock_freq / (config->frequency)) - 1U;
  TIMER_TopSet(pwm->timer, top);

  // Set initial duty cycle to 0%
  TIMER_CompareSet(pwm->timer, pwm->channel, 0U);

  // Initialize TIMER
  TIMER_Init_TypeDef timer_init = TIMER_INIT_DEFAULT;
  TIMER_Init(pwm->timer, &timer_init);
#endif

  return SL_STATUS_OK;
}

sl_status_t sl_pwm_deinit(sl_pwm_instance_t *pwm)
{
#if defined(_SILICON_LABS_32B_SERIES_3)
  bool disable_timer_bus_clock = true;
  sli_pwm_timer_state_t *timer_state = get_pwm_timer_state(pwm->timer);
  sli_pwm_channel_state_t *channel_state = get_pwm_channel_state(pwm->timer);
  if ((timer_state == NULL) || (channel_state == NULL)
      || !is_pwm_channel_valid(pwm->timer, pwm->channel)
      || !(timer_state->configured_channels & (1U << pwm->channel))) {
    return SL_STATUS_INVALID_PARAMETER;
  }
#endif

  // Reset TIMER routes
  sl_pwm_stop(pwm);

#if defined(_SILICON_LABS_32B_SERIES_3)
  *get_pwm_route_register(pwm->timer, pwm->channel) = 0U;
#else
  volatile uint32_t * route_register;
#if defined(_GPIO_TIMER_ROUTEEN_MASK)
  route_register = &GPIO->TIMERROUTE[TIMER_NUM(pwm->timer)].CC0ROUTE;
#else
  switch (TIMER_NUM(pwm->timer)) {
    case 0:
      route_register = &GPIO->TIMER0ROUTE[0].CC0ROUTE;
      break;
    case 1:
      route_register = &GPIO->TIMER1ROUTE[0].CC0ROUTE;
      break;
    case 2:
      route_register = &GPIO->TIMER2ROUTE[0].CC0ROUTE;
      break;
    case 3:
      route_register = &GPIO->TIMER3ROUTE[0].CC0ROUTE;
      break;
    default:
      SL_LOG_DEBUG_ASSERT(0);
      return SL_STATUS_FAIL;
  }
#endif
  route_register += pwm->channel;
  *route_register = 0;
#endif

  // Reset TIMER
#if defined(_SILICON_LABS_32B_SERIES_2)
  TIMER_Reset(pwm->timer);
#if defined(PWM_USES_GPIO_LATCHING)
  uint8_t *active_channels = get_pwm_active_channels(pwm->timer);
  uint16_t *latched_channels = get_pwm_latched_channels(pwm->timer);
  if (active_channels != NULL) {
    *active_channels = 0U;
  }
  if (latched_channels != NULL) {
    *latched_channels = 0U;
  }
#endif
#else
  uint8_t channel_mask = (uint8_t)(1U << pwm->channel);
  timer_state->configured_channels &= (uint8_t) ~channel_mask;
  timer_state->active_channels &= (uint8_t) ~channel_mask;
  timer_state->output_invert_channels &= (uint8_t) ~channel_mask;
  channel_state[pwm->channel] = (sli_pwm_channel_state_t) { 0 };
  disable_timer_bus_clock = (timer_state->configured_channels == 0U);
  if (disable_timer_bus_clock) {
    if (pwm->timer->EN & _TIMER_EN_EN_MASK) {
      sl_hal_timer_stop(pwm->timer);
      sl_hal_timer_disable(pwm->timer);
      sl_hal_timer_wait_ready(pwm->timer);
    }
    sl_hal_timer_reset(pwm->timer);
    *timer_state = (sli_pwm_timer_state_t) { 0 };
  }
#endif

  // Reset GPIO
  sl_gpio_t reset_gpio;
  reset_gpio.port = pwm->port;
  reset_gpio.pin = pwm->pin;
  sl_gpio_set_pin_mode(&reset_gpio,
                       SL_GPIO_MODE_DISABLED,
                       0);
#if defined(PWM_USES_GPIO_LATCHING)
  // A stopped output is held at its last level. Release the pad only after
  // routing has been removed and GPIO has been returned to its idle mode.
  sl_hal_gpio_unlatch_pin(&reset_gpio);
#endif

  sl_bus_clock_t timer_clock;
  sl_peripheral_t peripheral;
  if (!get_timer_clock(pwm->timer, &timer_clock, &peripheral)) {
    return SL_STATUS_FAIL;
  }
  (void) peripheral;
#if defined(_SILICON_LABS_32B_SERIES_3)
  // The clock manager bus-clock operations are not reference-counted. Keep
  // the clock enabled until every PWM channel sharing this TIMER is deinit'd.
  if (disable_timer_bus_clock) {
    sl_clock_manager_disable_bus_clock(timer_clock);
  }
#else
  sl_clock_manager_disable_bus_clock(timer_clock);
#endif

  return SL_STATUS_OK;
}

void sl_pwm_start(sl_pwm_instance_t *pwm)
{
#if defined(_SILICON_LABS_32B_SERIES_3)
  // Match the legacy API: start only enables this channel's output route.
  set_pwm_route_enabled(pwm->timer, pwm->channel, true);
#else
  // Enable PWM output
#if defined(_GPIO_TIMER_ROUTEEN_MASK)
  GPIO->TIMERROUTE_SET[TIMER_NUM(pwm->timer)].ROUTEEN = 1 << (pwm->channel + _GPIO_TIMER_ROUTEEN_CC0PEN_SHIFT);
#else
  switch (TIMER_NUM(pwm->timer)) {
    case 0:
      GPIO->TIMER0ROUTE_SET[0].ROUTEEN = 1 << (pwm->channel + _GPIO_TIMER0_ROUTEEN_CC0PEN_SHIFT);
      break;
    case 1:
      GPIO->TIMER1ROUTE_SET[0].ROUTEEN = 1 << (pwm->channel + _GPIO_TIMER1_ROUTEEN_CC0PEN_SHIFT);
      break;
    case 2:
      GPIO->TIMER2ROUTE_SET[0].ROUTEEN = 1 << (pwm->channel + _GPIO_TIMER2_ROUTEEN_CC0PEN_SHIFT);
      break;
    case 3:
      GPIO->TIMER3ROUTE_SET[0].ROUTEEN = 1 << (pwm->channel + _GPIO_TIMER3_ROUTEEN_CC0PEN_SHIFT);
      break;
    default:
      SL_LOG_DEBUG_ASSERT(0);
      break;
  }
#endif
#endif // _SILICON_LABS_32B_SERIES_3
#if defined(PWM_USES_GPIO_LATCHING)
  set_pwm_channel_active(pwm->timer, pwm->channel, true);
#endif
}

void sl_pwm_stop(sl_pwm_instance_t *pwm)
{
#if defined(_SILICON_LABS_32B_SERIES_3)
  // Match the legacy API: stop only disables this channel's output route.
  set_pwm_route_enabled(pwm->timer, pwm->channel, false);
#else
  // Disable PWM output
#if defined(_GPIO_TIMER_ROUTEEN_MASK)
  GPIO->TIMERROUTE_CLR[TIMER_NUM(pwm->timer)].ROUTEEN = 1 << (pwm->channel + _GPIO_TIMER_ROUTEEN_CC0PEN_SHIFT);
#else
  switch (TIMER_NUM(pwm->timer)) {
    case 0:
      GPIO->TIMER0ROUTE_CLR[0].ROUTEEN = 1 << (pwm->channel + _GPIO_TIMER0_ROUTEEN_CC0PEN_SHIFT);
      break;
    case 1:
      GPIO->TIMER1ROUTE_CLR[0].ROUTEEN = 1 << (pwm->channel + _GPIO_TIMER1_ROUTEEN_CC0PEN_SHIFT);
      break;
    case 2:
      GPIO->TIMER2ROUTE_CLR[0].ROUTEEN = 1 << (pwm->channel + _GPIO_TIMER2_ROUTEEN_CC0PEN_SHIFT);
      break;
    case 3:
      GPIO->TIMER3ROUTE_CLR[0].ROUTEEN = 1 << (pwm->channel + _GPIO_TIMER3_ROUTEEN_CC0PEN_SHIFT);
      break;
    default:
      SL_LOG_DEBUG_ASSERT(0);
      break;
  }
#endif
  // Keep timer running in case other channels are in use
#endif // _SILICON_LABS_32B_SERIES_3
#if defined(PWM_USES_GPIO_LATCHING)
  // If the TIMER is paused, release this pin after disabling its route so the
  // pad no longer follows the stopped TIMER output.
  unlatch_pwm_channel_if_needed(pwm);
  set_pwm_channel_active(pwm->timer, pwm->channel, false);
#endif
}

sl_status_t sl_pwm_timer_stop(TIMER_TypeDef *timer)
{
  sl_bus_clock_t timer_clock;
  sl_peripheral_t peripheral;
  if (!get_timer_clock(timer, &timer_clock, &peripheral)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  (void) timer_clock;

#if defined(_SILICON_LABS_32B_SERIES_3)
  sli_pwm_timer_state_t *timer_state = get_pwm_timer_state(timer);
  if ((timer_state == NULL) || !timer_state->configured) {
    return SL_STATUS_INVALID_STATE;
  }
#endif

#if defined(PWM_USES_GPIO_LATCHING)
  uint8_t *active_channels = get_pwm_active_channels(timer);
  uint16_t *latched_channels = get_pwm_latched_channels(timer);
  if ((active_channels == NULL) || (latched_channels == NULL)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  sl_status_t status = sl_gpio_latch_peripheral_pins(peripheral);
  if (status != SL_STATUS_OK) {
    return status;
  }
  *latched_channels = (uint16_t)(*active_channels
                                 | SLI_PWM_GPIO_ROUTES_LATCHED_MASK);
#endif

#if defined(_SILICON_LABS_32B_SERIES_3)
  if (timer->EN & _TIMER_EN_EN_MASK) {
    sl_hal_timer_stop(timer);
    sl_hal_timer_disable(timer);
    sl_hal_timer_wait_ready(timer);
  }
#else
  TIMER_Enable(timer, false);
#endif

  return SL_STATUS_OK;
}

sl_status_t sl_pwm_timer_start(TIMER_TypeDef *timer)
{
  sl_bus_clock_t timer_clock;
  sl_peripheral_t peripheral;
  if (!get_timer_clock(timer, &timer_clock, &peripheral)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  (void) timer_clock;

#if defined(_SILICON_LABS_32B_SERIES_3)
  sli_pwm_timer_state_t *timer_state = get_pwm_timer_state(timer);
  sli_pwm_channel_state_t *channel_state = get_pwm_channel_state(timer);
  if ((timer_state == NULL) || (channel_state == NULL)
      || !timer_state->configured || (timer_state->configured_channels == 0U)) {
    return SL_STATUS_INVALID_STATE;
  }
  if (!(timer->EN & _TIMER_EN_EN_MASK)) {
    restore_pwm_timer(timer, timer_state, channel_state);
  }
#else
  TIMER_Enable(timer, true);
#endif

#if defined(PWM_USES_GPIO_LATCHING)
  uint16_t *latched_channels = get_pwm_latched_channels(timer);
  if (latched_channels == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (*latched_channels & SLI_PWM_GPIO_ROUTES_LATCHED_MASK) {
    sl_status_t status = sl_gpio_unlatch_peripheral_pins(peripheral);
    if (status != SL_STATUS_OK) {
      return status;
    }
    *latched_channels = 0U;
  }
#endif

  return SL_STATUS_OK;
}

void sl_pwm_set_duty_cycle(sl_pwm_instance_t *pwm, uint8_t percent)
{
#if defined(_SILICON_LABS_32B_SERIES_2)
  uint32_t top = TIMER_TopGet(pwm->timer);

  // Set compare value
  TIMER_CompareBufSet(pwm->timer, pwm->channel, (top * percent) / 100);
#else
  sli_pwm_timer_state_t *timer_state = get_pwm_timer_state(pwm->timer);
  sli_pwm_channel_state_t *channel_state = get_pwm_channel_state(pwm->timer);
  if ((timer_state == NULL) || !is_pwm_channel_valid(pwm->timer, pwm->channel)
      || (channel_state == NULL)
      || !(timer_state->configured_channels & (1U << pwm->channel))) {
    return;
  }

  channel_state[pwm->channel].duty_cycle = percent;
  if (pwm->timer->EN & _TIMER_EN_EN_MASK) {
    uint32_t top = sl_hal_timer_get_top(pwm->timer);

    // Preserve the legacy buffered update while retaining the value for a
    // later explicit TIMER restart.
    sl_hal_timer_channel_set_compare_buffer(pwm->timer,
                                            pwm->channel,
                                            (top * percent) / 100U);
  }
#endif
}

uint8_t sl_pwm_get_duty_cycle(sl_pwm_instance_t *pwm)
{
#if defined(_SILICON_LABS_32B_SERIES_2)
  uint32_t top = TIMER_TopGet(pwm->timer);
  uint32_t compare = TIMER_CaptureGet(pwm->timer, pwm->channel);
#else
  sli_pwm_timer_state_t *timer_state = get_pwm_timer_state(pwm->timer);
  sli_pwm_channel_state_t *channel_state = get_pwm_channel_state(pwm->timer);
  if ((timer_state == NULL) || (channel_state == NULL)
      || !is_pwm_channel_valid(pwm->timer, pwm->channel)
      || !(timer_state->configured_channels & (1U << pwm->channel))) {
    return 0U;
  }
  uint32_t top = sl_hal_timer_get_top(pwm->timer);
  uint32_t compare = sl_hal_timer_channel_get_compare(pwm->timer, pwm->channel);
  uint8_t percent = (uint8_t)((compare * 100U) / top);

  return percent;
#endif

#if defined(_SILICON_LABS_32B_SERIES_2)
  uint8_t percent = (uint8_t)((compare * 100) / top);

  return percent;
#endif
}
