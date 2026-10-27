/***************************************************************************/ /**
 * @file sl_cpc_wake.h
 * @brief CPC public wake / power-mode definitions.
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

#ifndef SL_CPC_WAKE_H
#define SL_CPC_WAKE_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/ /**
 * @addtogroup cpc CPC
 * @{
 ******************************************************************************/

/******************************************************************************/
/*                                   Types                                    */
/******************************************************************************/

/**
 * @brief How this CPC end manages sleep and wake-up.
 *
 * This is local to the CPC instance. The peer must be configured with matching
 * configuration for the wake mechanism to work.
 */
typedef enum sl_cpc_power_mode {
  /// Keep instance always on. Note that this prevents the whole system from
  /// entering sleep state. Safe default.
  SL_CPC_POWER_MODE_ALWAYS_ON,

  /// Instance may sleep with bus shut down, it requires an external GPIO to
  /// make the bus available again. The peer must drive the matching wake-out
  /// line.
  SL_CPC_POWER_MODE_WAKE_FROM_GPIO,
} sl_cpc_power_mode_t;

/**
 * @brief Control the wake-out mode.
 *
 * Set on the end that has a wake-out GPIO (typically the primary).
 */
typedef enum sl_cpc_wake_out_mode {
  /// No wake out signal to drive
  SL_CPC_WAKE_OUT_DISABLED,

  /// Transmit immediately when waking up. Use for SPI or SDIO peripherals
  /// where the bus is controlled by the host.
  SL_CPC_WAKE_OUT_MODE_NO_DELAY,

  /// Wait a configured fixed delay, then transmit. No proof the peer is ready.
  SL_CPC_WAKE_OUT_MODE_DELAY,

  /// Wait for a peer frame with the AWAKE indication, then transmit.
  SL_CPC_WAKE_OUT_MODE_CONFIRM,
} sl_cpc_wake_out_mode_t;

/**
 * @brief What this end does when woken on wake-in.
 *
 * Must be paired in the project with the peer's
 * @ref sl_cpc_wake_out_mode_t: delay ↔ none, confirm ↔ awake.
 */
typedef enum sl_cpc_wake_in_response {
  /// No action taken upon waking up.
  SL_CPC_WAKE_IN_RESPONSE_NONE,

  /// Acknowledge that device has woken up by sending a special frame.
  SL_CPC_WAKE_IN_RESPONSE_AWAKE,
} sl_cpc_wake_in_response_t;

/** @} (end addtogroup cpc) */

#ifdef __cplusplus
}
#endif

#endif // SL_CPC_WAKE_H
