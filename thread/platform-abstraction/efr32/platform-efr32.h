/*
 *  Copyright (c) 2023, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file includes the platform-specific initializers and PAL
 *   compile-time configuration.
 *
 */

#ifndef PLATFORM_EFR32_H_
#define PLATFORM_EFR32_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <openthread/instance.h>

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif

#include "em_device.h"

#if defined(_SILICON_LABS_32B_SERIES_1)
#error "EFR32 Series 1 parts are not supported."
#endif

#if defined(_SILICON_LABS_32B_SERIES_2)
#include "em_system.h"
#else
#include "sl_hal_system.h"
#endif

#ifdef SL_CATALOG_CLOCK_MANAGER_PRESENT
#include "sl_clock_manager_oscillator_config.h"
#else
#include "sl_device_init_hfxo.h"
#include "sl_device_init_hfxo_config.h"
#endif

#if defined(HARDWARE_BOARD_HAS_LFXO) && !defined(SL_CATALOG_CLOCK_MANAGER_PRESENT)
#include "sl_device_init_lfxo.h"
#include "sl_device_init_lfxo_config.h"
#endif

/**
 * @def SL_OPENTHREAD_CSL_TX_UNCERTAINTY
 *
 * Uncertainty of scheduling a CSL transmission, in ±10 us units.
 *
 * Note: This value was carefully configured to meet Thread certification
 * requirements for Silicon Labs devices.
 *
 */
#ifndef SL_OPENTHREAD_CSL_TX_UNCERTAINTY
#if OPENTHREAD_RADIO || OPENTHREAD_CONFIG_REFERENCE_DEVICE_ENABLE
#define SL_OPENTHREAD_CSL_TX_UNCERTAINTY 175
#elif OPENTHREAD_FTD
// Approx. ~128 us. for single CCA + some additional tx uncertainty in testing
#define SL_OPENTHREAD_CSL_TX_UNCERTAINTY 20
#else
// Approx. ~128 us. for single CCA
//
// Note: Our SSEDs "schedule" transmissions to their parent in order to know
// exactly when in the future the data packets go out so they can calculate
// the accurate CSL phase to send to their parent.
//
// The receive windows on the SSEDs scale with this value, so increasing this
// uncertainty to account for full CCA/CSMA with 0..7 backoffs
// (see RAIL_CSMA_CONFIG_802_15_4_2003_2p4_GHz_OQPSK_CSMA) will mean that the
// receive windows can get very long (~ 5ms.)
//
// We have updated SSEDs to use a single CCA (RAIL_CSMA_CONFIG_SINGLE_CCA)
// instead. If they are in very busy channels, CSL won't be reliable anyway.
#define SL_OPENTHREAD_CSL_TX_UNCERTAINTY 12
#endif
#endif

/**
 * @def SL_OPENTHREAD_HFXO_ACCURACY
 *
 * Worst case XTAL accuracy in units of ± ppm. Also used for calculations during CSL operations.
 *
 * @note Platforms may optimize this value based on operational conditions (i.e.: temperature).
 *
 */
#ifndef SL_OPENTHREAD_HFXO_ACCURACY
#ifdef SL_CATALOG_CLOCK_MANAGER_PRESENT
#define SL_OPENTHREAD_HFXO_ACCURACY SL_CLOCK_MANAGER_HFXO_PRECISION
#else
#define SL_OPENTHREAD_HFXO_ACCURACY SL_DEVICE_INIT_HFXO_PRECISION
#endif
#endif

/**
 * @def SL_OPENTHREAD_LFXO_ACCURACY
 *
 * Worst case XTAL accuracy in units of ± ppm. Also used for calculations during CSL operations.
 *
 * @note Platforms may optimize this value based on operational conditions (i.e.: temperature).
 */
#ifndef SL_OPENTHREAD_LFXO_ACCURACY
#if defined(HARDWARE_BOARD_HAS_LFXO)
#if SL_CATALOG_CLOCK_MANAGER_PRESENT
#define SL_OPENTHREAD_LFXO_ACCURACY SL_CLOCK_MANAGER_LFXO_PRECISION
#else
#define SL_OPENTHREAD_LFXO_ACCURACY SL_DEVICE_INIT_LFXO_PRECISION
#endif // SL_CATALOG_CLOCK_MANAGER_PRESENT
#else
#define SL_OPENTHREAD_LFXO_ACCURACY 0
#endif // HARDWARE_BOARD_HAS_LFXO
#endif

/**
 * @def SL_OPENTHREAD_RADIO_CCA_MODE
 *
 * Defines the CCA mode to be used by the platform.
 *
 */
#ifndef SL_OPENTHREAD_RADIO_CCA_MODE
#define SL_OPENTHREAD_RADIO_CCA_MODE SL_RAIL_IEEE802154_CCA_MODE_RSSI
#endif

/**
 * @def SL_OPENTHREAD_ECDSA_PRIVATE_KEY_SIZE
 *
 * Max Private key size supported by ECDSA Crypto handler.
 *
 */
#ifndef SL_OPENTHREAD_ECDSA_PRIVATE_KEY_SIZE
#define SL_OPENTHREAD_ECDSA_PRIVATE_KEY_SIZE 32
#endif

/**
 * @def SL_OPENTHREAD_ENABLE_HOST_WAKE_GPIO
 *
 * Define to 1 to enable the host wakeup GPIO functionality.
 * This feature allows the platform to wake up the host using a GPIO pin.
 *
 * Default value is 0 (disabled).
 */
#ifndef SL_OPENTHREAD_ENABLE_HOST_WAKE_GPIO
#define SL_OPENTHREAD_ENABLE_HOST_WAKE_GPIO 0
#endif

/**
 * @def SL_OPENTHREAD_HOST_WAKEUP_GPIO_PORT
 *
 * Defines the GPIO port for host wakeup.
 *
 */

#ifndef SL_OPENTHREAD_HOST_WAKEUP_GPIO_PORT
#define SL_OPENTHREAD_HOST_WAKEUP_GPIO_PORT SL_GPIO_PORT_C
#endif

/**
 * @def SL_OPENTHREAD_HOST_WAKEUP_GPIO_PIN
 *
 * Defines the GPIO pin for host wakeup.
 *
 */

#ifndef SL_OPENTHREAD_HOST_WAKEUP_GPIO_PIN
#define SL_OPENTHREAD_HOST_WAKEUP_GPIO_PIN 0
#endif

/**
 * @def SL_OPENTHREAD_HOST_CLEAR_PIN_TIMEOUT_MS
 *
 * Defines the timeout duration (in milliseconds) for clearing the host wakeup GPIO pin.
 *
 * This value specifies the amount of time the system will wait before clearing the host wakeup GPIO pin.
 *
 */
#ifndef SL_OPENTHREAD_HOST_CLEAR_PIN_TIMEOUT_MS
#define SL_OPENTHREAD_HOST_CLEAR_PIN_TIMEOUT_MS 10
#endif

#include "sl_rail.h"

#include "alarm.h"
#include "uart.h"

#ifndef SL_CATALOG_KERNEL_PRESENT
#define sl_ot_rtos_task_can_access_pal() (true)
#else
#include "sl_ot_rtos_adaptation.h"
#endif

// Global reference to rail handle
#ifndef SL_CATALOG_RAIL_MULTIPLEXER_PRESENT
#define gRailHandle emPhyRailHandle // use gRailHandle in the OpenThread PAL.
#endif

/**
 * This function performs all platform-specific initialization of
 * OpenThread's drivers.
 *
 */
void sl_ot_sys_init(void);

/**
 * This function initializes the radio service used by OpenThead.
 *
 */
void efr32RadioInit(void);

/**
 * This function deinitializes the radio service used by OpenThead.
 *
 */
void efr32RadioDeinit(void);

/**
 * This function performs radio driver processing.
 *
 * @param[in]  aInstance  The OpenThread instance structure.
 *
 */
void efr32RadioProcess(otInstance *aInstance);

/**
 * This function indicates whether radio work is pending for the main loop.
 *
 * @retval true   An RX packet and/or TX completion event is queued.
 * @retval false  No radio work is pending.
 *
 */
bool efr32RadioIsDataReady(void);

/**
 * This function performs CPC driver processing.
 *
 */
void efr32CpcProcess(void);

/**
 * This function performs SPI driver processing.
 *
 */
void efr32SpiProcess(void);

/**
 * Initialization of Misc module.
 *
 */
void efr32MiscInit(void);

/**
 * Initialization of ADC module for random number generator.
 *
 */
void efr32RandomInit(void);

/**
 * Initialization of Logger driver.
 *
 */
void efr32LogInit(void);

/**
 * Deinitialization of Logger driver.
 *
 */
void efr32LogDeinit(void);

/**
 * Print reset info.
 *
 */
void efr32PrintResetInfo(void);

/**
 * Set 802.15.4 CCA mode
 *
 * A call to this function should be made after RAIL has been
 * initialized and a valid handle is available. On platforms that
 * don't support different CCA modes, a call to this function with
 * non-Default CCA mode (i.e. with any value except
 * SL_RAIL_IEEE802154_CCA_MODE_RSSI) will return a failure.
 *
 * @param[in] aMode Mode of CCA operation.
 * @return RAIL Status code indicating success of the function call.
 */
sl_rail_status_t efr32RadioSetCcaMode(uint8_t aMode);

/**
 * This callback is used to check if is safe to put the EFR32 into a
 * low energy sleep mode.
 *
 * The callback should return true if it is ok to enter sleep mode.
 * Note that the callback must add an EM1 requirement if it intends
 * to idle (EM1) instead of entering a deep sleep (EM2) mode.
 */

bool efr32AllowSleepCallback(void);

/**
 * Load the channel configurations.
 *
 * @param[in]  aChannel   The radio channel.
 * @param[in]  aTxPower   The radio transmit power in dBm.
 *
 * @retval OT_ERROR_NONE         Successfully enabled/disabled .
 * @retval OT_ERROR_INVALID_ARGS Invalid channel.
 *
 */
otError efr32RadioLoadChannelConfig(uint8_t aChannel, int8_t aTxPower);

otError railStatusToOtError(sl_rail_status_t status);

/**
 * This function performs Serial processing.
 *
 */
void efr32SerialProcess(void);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_EFR32_H_
