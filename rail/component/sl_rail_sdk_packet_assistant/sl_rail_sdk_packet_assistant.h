/***************************************************************************//**
 * @file sl_rail_sdk_packet_assistant.h
 * @brief RAIL SDK - RAIL Packet Assistant Component
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_RAIL_SDK_PACKET_ASSISTANT_H
#define SL_RAIL_SDK_PACKET_ASSISTANT_H

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include "sl_component_catalog.h"
#include "sl_rail_sdk_packet_asm.h"
#include "sl_rail_util_init_inst0_config.h"
#include "sl_rail_util_protocol_types.h"
#include "sl_rail.h"
#include "sl_status.h"

#if defined(SL_CATALOG_APP_LOG_PRESENT)
#include "app_log.h"
#endif
#if defined(SL_CATALOG_APP_ASSERT_PRESENT)
#include "app_assert.h"
#endif

#include "rail_config.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// Fallback if the platform package does not yet define this deprecation tag.
#ifndef SL_DEPRECATED_API_SDK_2026_12
#ifdef SL_SUPPRESS_DEPRECATION_WARNINGS_SDK_2026_12
#define SL_DEPRECATED_API_SDK_2026_12
#else
#define SL_DEPRECATED_API_SDK_2026_12 __attribute__((deprecated))
#endif
#endif
#endif // DOXYGEN_SHOULD_SKIP_THIS

/**
 * \addtogroup rail_sdk_packet_assistant
 * @{
 */

/**************************************************************************//**
 * @addtogroup rail_sdk_packet_assistant_types Type definitions
 * @ingroup rail_sdk_packet_assistant
 * @{
 *****************************************************************************/
// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
/**
 * @enum RAIL_SDK_Protocol_t
 * @brief Enumeration of different radio protocols.
 *
 * This enumeration defines the various radio protocols that can be used.
 */
typedef enum {
  CUSTOM_AND_SUN_OQPSK = 0, /*!< Custom and SUN OQPSK protocol */
  EMBER_PHY = 1, /*!< Ember PHY protocol */
  THREAD = 2, /*!< Thread protocol */
  BLE = 3, /*!< Bluetooth Low Energy protocol */
  CONNECT = 4, /*!< Connect protocol, original, SUN-FSK and OFDM */
  ZIGBEE = 5, /*!< Zigbee protocol */
  ZWAVE = 6, /*!< Z-Wave protocol */
  WISUN = 7, /*!< Wi-SUN protocol, FAN 1.0 and FAN 1.1 with OFDM */
  BPSK = 8, /*!< BPSK protocol */
  SIDEWALK = 9, /*!< Sidewalk protocol */
  LONGRANGE = 10, /*!< Long-range protocol */
  MBUS = 11, /*!< Wireless M-Bus protocol */
  SIGFOX = 12, /*!< Sigfox protocol */
  UNDEFINED = 13 /*!< Undefined protocol */
} RAIL_SDK_Protocol_t;

/** @} (end rail_sdk_packet_assistant_types) */
// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------
/**
 * @brief Checks PHY setting to avoid errors at packet sending.
 */
void sl_packet_assistant_validation_check(void);

/**
 * @brief Selects PHY index 0.
 *
 * Registered on \c internal_app_init. Call \ref sl_packet_assistant_select_phy()
 * again when the active PHY changes.
 */
void sl_packet_assistant_init(void);

/**
 * @brief Selects the packer and unpacker functions based on the selected PHY.
 *
 * Must be called before \ref sl_packet_assistant_prepare_packet(),
 * \ref sl_packet_assistant_unpack_packet(), or \ref sl_packet_assistant_get_phr_length().
 * Call again whenever the active PHY changes. A failed selection clears the
 * previously selected PHY.
 *
 * @param[in] new_phy_index The PHY index to select.
 * @return SL_STATUS_OK on success, SL_STATUS_NOT_SUPPORTED if the PHY is not supported.
 */
sl_status_t sl_packet_assistant_select_phy(uint8_t new_phy_index);

/**
 * @brief Gets the PHR length for the selected PHY.
 *
 * Requires a prior successful call to \ref sl_packet_assistant_select_phy().
 * Uses the same stackInfo-based identification as that API.
 *
 * @return PHR length in bytes.
 * @return -1 if no PHY has been selected, or if the PHY is unsupported
 *         (missing config, unknown protocol, or not implemented).
 */
int8_t sl_packet_assistant_get_phr_length(void);

/**
 * @brief Unpacks the received packet, points to the payload, and reports the length.
 *
 * Requires a prior successful call to \ref sl_packet_assistant_select_phy().
 *
 * @param[in] rail_handle The RAIL handle used for unpacking the packet.
 * @param[in] packet_information Packet information provided by RAIL.
 * @param[in] rx_buffer Caller-provided buffer to unpack the full packet into.
 * @param[out] start_of_payload Pointer where the payload starts.
 * @param[out] payload_size Length of the received payload.
 * @return SL_STATUS_OK on success.
 * @return SL_STATUS_INVALID_STATE if no PHY has been selected.
 */
sl_status_t sl_packet_assistant_unpack_packet(sl_rail_handle_t rail_handle,
                                              const sl_rail_rx_packet_info_t *packet_information,
                                              uint8_t *rx_buffer,
                                              uint8_t **start_of_payload,
                                              uint16_t *payload_size);

/**
 * @brief Prepares the packet for sending and load it in the RAIL TX FIFO.
 *
 * Requires a prior successful call to \ref sl_packet_assistant_select_phy().
 *
 * @param[in] rail_handle Which rail handlers should be used for the TX FIFO writing
 * @param[in] payload The payload buffer
 * @param[in] length The length of the payload
 * @return SL_STATUS_OK on success.
 * @return SL_STATUS_INVALID_STATE if no PHY has been selected.
 */
sl_status_t sl_packet_assistant_prepare_packet(sl_rail_handle_t rail_handle, uint8_t *payload, uint16_t length);

/**
 * @brief Get the print packet information.
 * This function retrieves the current print packet information setting.
 * @return The current print packet information setting.
 */
uint8_t sl_packet_assistant_get_print_packet_info(void);

/**
 * @brief Set the print packet information.
 * This function sets a new value for the print packet information setting.
 * @param[in] print_info_enabled The new print packet information setting to be applied.
 */
void sl_packet_assistant_set_print_packet_info(uint8_t print_info_enabled);

/**
 * @brief Prints out the received packet.
 * This function prints the contents of the received packet.
 * @param[in] rx_buffer The buffer where the packet is stored.
 * @param[in] length The length of the packet.
 */
void sl_packet_assistant_print_rx_packet(const uint8_t * const rx_buffer, uint16_t length);

/**
 * @brief Get the SUN OFDM rate.
 * Returns the current SUN OFDM rate.
 * @return The SUN OFDM rate.
 */
uint8_t sl_packet_assistant_get_sun_ofdm_rate(void);

/**
 * @brief Set the SUN OFDM rate.
 * Sets the SUN OFDM rate to the specified value.
 * @param[in] new_rate The new SUN OFDM rate to set.
 * @return 1 if the value was set successfully, 0 otherwise.
 */
uint8_t sl_packet_assistant_set_sun_ofdm_rate(uint8_t new_rate);

/**
 * @brief Get the SUN OFDM scrambler.
 * Returns the current SUN OFDM scrambler.
 * @return The SUN OFDM scrambler.
 */
uint8_t sl_packet_assistant_get_sun_ofdm_scrambler(void);

/**
 * @brief Set the SUN OFDM scrambler.
 * Sets the SUN OFDM scrambler to the specified value.
 * @param[in] new_scrambler The new SUN OFDM scrambler to set, 2 bits wide (0-3)
 * @return 1 if the value was set successfully, 0 otherwise.
 */
uint8_t sl_packet_assistant_set_sun_ofdm_scrambler(uint8_t new_scrambler);

/**
 * @brief Get the SUN FSK FCS type.
 * Returns the current SUN FSK FCS type.
 * @return 0 = 4-byte FCS, 1 = 2-byte FCS.
 */
uint8_t sl_packet_assistant_get_sun_fsk_fcs(void);

/**
 * @brief Set the SUN FSK FCS type.
 * Sets the SUN FSK FCS type to the specified new value.
 * @param[in] new_fcs The new SUN FSK FCS type to set (0 = 4-byte, 1 = 2-byte).
 * @return 1 if the value was set successfully, 0 otherwise.
 */
uint8_t sl_packet_assistant_set_sun_fsk_fcs(uint8_t new_fcs);

/**
 * @brief Get the SUN FSK Whitening value.
 * Returns the current SUN FSK Whitening value.
 * @return The SUN FSK Whitening value.
 */
uint8_t sl_packet_assistant_get_sun_fsk_whitening(void);

/**
 * @brief Set the SUN FSK Whitening value.
 * Sets the SUN FSK Whitening value to the specified new value.
 * @param[in] new_whitening The new SUN FSK Whitening value to set.
 * @return 1 if the value was set successfully, 0 otherwise.
 */
uint8_t sl_packet_assistant_set_sun_fsk_whitening(uint8_t new_whitening);

/**
 * @brief Get the Sun OQPSK Spreading Mode.
 * Retrieves the current Sun OQPSK Spreading Mode.
 * @return The current Sun OQPSK Spreading Mode.
 */
uint8_t sl_packet_assistant_get_sun_oqpsk_spreading_mode(void);

/**
 * @brief Set the Sun OQPSK Spreading Mode.
 * Sets the Sun OQPSK Spreading Mode to the specified new mode.
 * @param[in] new_spreading_mode The new Sun OQPSK Spreading Mode to set.
 * @return 1 if the value was set successfully, 0 otherwise.
 */
uint8_t sl_packet_assistant_set_sun_oqpsk_spreading_mode(uint8_t new_spreading_mode);

/**
 * @brief Get the Sun OQPSK Rate Mode.
 * Retrieves the current Sun OQPSK Rate Mode.
 * @return The current Sun OQPSK Rate Mode.
 */
uint8_t sl_packet_assistant_get_sun_oqpsk_rate_mode(void);

/**
 * @brief Set the Sun OQPSK Rate Mode.
 * Sets the Sun OQPSK Rate Mode to the specified new mode.
 * @param[in] new_rate_mode The new Sun OQPSK Rate Mode to set, 2 bits wide (0-3)
 * @return 1 if the value was set successfully, 0 otherwise.
 */
uint8_t sl_packet_assistant_set_sun_oqpsk_rate_mode(uint8_t new_rate_mode);

/**
 * @brief Get the Sidewalk FSK FCS type.
 * Retrieves the current Sidewalk FSK FCS type.
 * @return 0 = 4-byte FCS, 1 = 2-byte FCS.
 */
uint8_t sl_packet_assistant_get_sidewalk_fcs_type(void);

/**
 * @brief Set the Sidewalk FSK FCS type.
 * Sets the Sidewalk FSK FCS type to the specified new value.
 * @param[in] new_fcs The new Sidewalk FSK FCS type to set (0 = 4-byte, 1 = 2-byte).
 * @return 1 if the value was set successfully, 0 otherwise.
 */
uint8_t sl_packet_assistant_set_sidewalk_fcs_type(uint8_t new_fcs);

/**
 * @brief Get the Sidewalk Whitening value.
 * Retrieves the current Sidewalk Whitening value.
 * @return The current Sidewalk Whitening value.
 */
uint8_t sl_packet_assistant_get_sidewalk_whitening(void);

/**
 * @brief Set the Sidewalk Whitening value.
 * Sets the Sidewalk Whitening value to the specified new value.
 * @param[in] new_whitening The new Sidewalk Whitening value to set.
 * @return 1 if the value was set successfully, 0 otherwise.
 */
uint8_t sl_packet_assistant_set_sidewalk_whitening(uint8_t new_whitening);

/** @} */ // end of packet_assistant group

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/**
 * @deprecated Use \ref sl_packet_assistant_validation_check() instead.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline void validation_check(void)
{
  sl_packet_assistant_validation_check();
}

/**
 * @deprecated Use \ref sl_packet_assistant_select_phy() instead, which reports unsupported PHYs.
 * @param[in] new_phy_index The PHY index to select.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline void update_assistant_pointers(uint8_t new_phy_index)
{
  (void)sl_packet_assistant_select_phy(new_phy_index);
}

/**
 * @deprecated Use \ref sl_packet_assistant_unpack_packet() instead. Call \ref sl_packet_assistant_select_phy() first.
 * @param[in] rail_handle The RAIL handle used for unpacking the packet.
 * @param[in] rx_destination Caller-provided buffer to unpack the full packet into.
 * @param[in] packet_information Packet information provided by RAIL.
 * @param[out] start_of_payload Pointer where the payload starts.
 * @return Length of the received payload.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint16_t unpack_packet(sl_rail_handle_t rail_handle,
                                     uint8_t *rx_destination,
                                     const sl_rail_rx_packet_info_t *packet_information,
                                     uint8_t **start_of_payload)
{
  uint16_t payload_size = 0;
  (void)sl_packet_assistant_unpack_packet(rail_handle,
                                          packet_information,
                                          rx_destination,
                                          start_of_payload,
                                          &payload_size);
  return payload_size;
}

/**
 * @deprecated Use \ref sl_packet_assistant_prepare_packet() instead. Call \ref sl_packet_assistant_select_phy() first.
 * @param[in] rail_handle RAIL handle used for TX FIFO writing.
 * @param[in] out_data The payload buffer.
 * @param[in] length The length of the payload.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline void prepare_packet(sl_rail_handle_t rail_handle, uint8_t *out_data, uint16_t length)
{
  (void)sl_packet_assistant_prepare_packet(rail_handle, out_data, length);
}

/**
 * @deprecated Use \ref sl_packet_assistant_get_print_packet_info() instead.
 * @return The current print packet information setting.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t get_print_packet_info(void)
{
  return sl_packet_assistant_get_print_packet_info();
}

/**
 * @deprecated Use \ref sl_packet_assistant_set_print_packet_info() instead.
 * @param[in] new_print_packet_info The new print packet information setting to be applied.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline void set_print_packet_info(uint8_t new_print_packet_info)
{
  sl_packet_assistant_set_print_packet_info(new_print_packet_info);
}

/**
 * @deprecated Use \ref sl_packet_assistant_print_rx_packet() instead.
 * @param[in] rx_buffer The buffer where the packet is stored.
 * @param[in] length The length of the packet.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline void printf_rx_packet(const uint8_t * const rx_buffer, uint16_t length)
{
  sl_packet_assistant_print_rx_packet(rx_buffer, length);
}

/**
 * @deprecated Use \ref sl_packet_assistant_get_sun_ofdm_rate() instead.
 * @return The SUN OFDM rate.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t get_wisun_ofdm_rate(void)
{
  return sl_packet_assistant_get_sun_ofdm_rate();
}

/**
 * @deprecated Use \ref sl_packet_assistant_set_sun_ofdm_rate() instead.
 * @param[in] new_rate The new SUN OFDM rate to set.
 * @return 1 if the value was set successfully, 0 otherwise.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t set_wisun_ofdm_rate(uint8_t new_rate)
{
  return sl_packet_assistant_set_sun_ofdm_rate(new_rate);
}

/**
 * @deprecated Use \ref sl_packet_assistant_get_sun_ofdm_scrambler() instead.
 * @return The SUN OFDM scrambler.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t get_wisun_ofdm_scrambler(void)
{
  return sl_packet_assistant_get_sun_ofdm_scrambler();
}

/**
 * @deprecated Use \ref sl_packet_assistant_set_sun_ofdm_scrambler() instead.
 * @param[in] new_scrambler The new SUN OFDM scrambler to set, 2 bits wide (0-3).
 * @return 1 if the value was set successfully, 0 otherwise.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t set_wisun_ofdm_scrambler(uint8_t new_scrambler)
{
  return sl_packet_assistant_set_sun_ofdm_scrambler(new_scrambler);
}

/**
 * @deprecated Use \ref sl_packet_assistant_get_sun_fsk_fcs() instead.
 * @return 0 = 4-byte FCS, 1 = 2-byte FCS.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t get_wisun_fsk_fcs(void)
{
  return sl_packet_assistant_get_sun_fsk_fcs();
}

/**
 * @deprecated Use \ref sl_packet_assistant_set_sun_fsk_fcs() instead.
 * @param[in] new_fcs The new SUN FSK FCS type to set (0 = 4-byte, 1 = 2-byte).
 * @return 1 if the value was set successfully, 0 otherwise.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t set_wisun_fsk_fcs(uint8_t new_fcs)
{
  return sl_packet_assistant_set_sun_fsk_fcs(new_fcs);
}

/**
 * @deprecated Use \ref sl_packet_assistant_get_sun_fsk_whitening() instead.
 * @return The SUN FSK Whitening value.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t get_wisun_fsk_whitening(void)
{
  return sl_packet_assistant_get_sun_fsk_whitening();
}

/**
 * @deprecated Use \ref sl_packet_assistant_set_sun_fsk_whitening() instead.
 * @param[in] new_whitening The new SUN FSK Whitening value to set.
 * @return 1 if the value was set successfully, 0 otherwise.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t set_wisun_fsk_whitening(uint8_t new_whitening)
{
  return sl_packet_assistant_set_sun_fsk_whitening(new_whitening);
}

/**
 * @deprecated Use \ref sl_packet_assistant_get_sun_oqpsk_spreading_mode() instead.
 * @return The current Sun OQPSK Spreading Mode.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t get_sun_oqpsk_spreading_mode(void)
{
  return sl_packet_assistant_get_sun_oqpsk_spreading_mode();
}

/**
 * @deprecated Use \ref sl_packet_assistant_set_sun_oqpsk_spreading_mode() instead.
 * @param[in] new_spreading_mode The new Sun OQPSK Spreading Mode to set.
 * @return 1 if the value was set successfully, 0 otherwise.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t set_sun_oqpsk_spreading_mode(uint8_t new_spreading_mode)
{
  return sl_packet_assistant_set_sun_oqpsk_spreading_mode(new_spreading_mode);
}

/**
 * @deprecated Use \ref sl_packet_assistant_get_sun_oqpsk_rate_mode() instead.
 * @return The current Sun OQPSK Rate Mode.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t get_sun_oqpsk_rate_mode(void)
{
  return sl_packet_assistant_get_sun_oqpsk_rate_mode();
}

/**
 * @deprecated Use \ref sl_packet_assistant_set_sun_oqpsk_rate_mode() instead.
 * @param[in] new_rate_mode The new Sun OQPSK Rate Mode to set, 2 bits wide (0-3).
 * @return 1 if the value was set successfully, 0 otherwise.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t set_sun_oqpsk_rate_mode(uint8_t new_rate_mode)
{
  return sl_packet_assistant_set_sun_oqpsk_rate_mode(new_rate_mode);
}

/**
 * @deprecated Use \ref sl_packet_assistant_get_sidewalk_fcs_type() instead.
 * @return 0 = 4-byte FCS, 1 = 2-byte FCS.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t get_sidewalk_fcs_type(void)
{
  return sl_packet_assistant_get_sidewalk_fcs_type();
}

/**
 * @deprecated Use \ref sl_packet_assistant_set_sidewalk_fcs_type() instead.
 * @param[in] new_fcs The new Sidewalk FSK FCS type to set (0 = 4-byte, 1 = 2-byte).
 * @return 1 if the value was set successfully, 0 otherwise.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t set_sidewalk_fcs_type(uint8_t new_fcs)
{
  return sl_packet_assistant_set_sidewalk_fcs_type(new_fcs);
}

/**
 * @deprecated Use \ref sl_packet_assistant_get_sidewalk_whitening() instead.
 * @return The current Sidewalk Whitening value.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t get_sidewalk_whitening(void)
{
  return sl_packet_assistant_get_sidewalk_whitening();
}

/**
 * @deprecated Use \ref sl_packet_assistant_set_sidewalk_whitening() instead.
 * @param[in] new_whitening The new Sidewalk Whitening value to set.
 * @return 1 if the value was set successfully, 0 otherwise.
 */
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t set_sidewalk_whitening(uint8_t new_whitening)
{
  return sl_packet_assistant_set_sidewalk_whitening(new_whitening);
}

#endif // DOXYGEN_SHOULD_SKIP_THIS

#endif // SL_RAIL_SDK_PACKET_ASSISTANT_H
