/***************************************************************************//**
 * @file
 * @brief sl_rail_sdk_packet_asm.h
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_RAIL_SDK_PACKET_ASM_H
#define SL_RAIL_SDK_PACKET_ASM_H

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include "sl_rail_types.h"
#include "sl_common.h"
#include "stdbool.h"

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

/*
 * IEEE 802.15.4/g
 * -----------------
 *
 * NOTE 1: The CRC is generated automatically by the RAIL, but when the frame
 * is packing, the CRC size must be counted into the length of the frame.
 *
 * NOTE 2: The ACK frame is generated automatically during the auto-ACK. The
 * frame contains the received frame's sequence number. RAIL knows the frame
 * based on the current initialized specific protocol (IEEE 802.15.4/g)
 *
 * Frame without the Synchronization Header
 * ------------------------------
 * |    PHR   |   PHY payload   |
 * ------------------------------
 *
 * IEEE 802.15.4
 * --------------
 * Data frame
 * --------------------------------------------------------------------------
 * |                   PHR                  |          PHY payload          |
 * --------------------------------------------------------------------------
 * | Frame length(7 bits) | Reserved(1 bit) |MAC payload(var)|FCS/MFR/CRC(2)|
 * --------------------------------------------------------------------------
 *
 * ACK frame
 * ----------------------------------------------
 * |    PHR     |   FCF   |  seq# | FCS/MFR/CRC |
 * ----------------------------------------------
 * | Length(1B) | FCF(2B) | #(1B) |   CRC(2B)   |
 * ----------------------------------------------
 *
 * IEEE 802.15.4g
 * ---------------
 * NOTE 3: Here must be highlighted the reverse endianness of the length in the
 * PHR field. In this kind of frame the length is 11 bits long, and the order
 * of the calculated length's bit shall be changed (MSB <-> LSB), to suit to
 * standard of the frame.
 *
 * Data frame
 * ------------------------------------------------------------------------
 * |                   PHR                    |         PHY payload       |
 * ------------------------------------------------------------------------
 * |MS(1)|R1-R0(2)|FCStype(1)|DW()1|length(11)|MAC payl.(var)|FCS/CRC(2/4)|
 * ------------------------------------------------------------------------
 *
 * ACK frame
 * -----------------------------------------------------------------
 * |               PHR             |   FCF   |  seq# | FCS/MFR/CRC |
 * -----------------------------------------------------------------
 * | Same as PHR of Data frame(2B) | FCF(2B) | #(1B) |  CRC(2B/4B) |
 * -----------------------------------------------------------------
 *
 */

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#define SL_RAIL_SDK_802154_PACKET_OK                  (0)
#define SL_RAIL_SDK_802154_PACKET_ERROR               (-1)

// IEEE 802.15.4 macros
/// IEEE 802.15.4 CRC length is 2 bytes
#define SL_RAIL_SDK_IEEE802154_CRC_LENGTH               (2U)
/// IEEE 802.15.4 PHR length is 1 byte
#define SL_RAIL_SDK_IEEE802154_PHR_LENGTH               (1U)
/// Indicates the effective payload length excluding CRC (2 Bytes)
#define SL_RAIL_SDK_IEEE802154_LEN_MAX                  (127)
/// Size of FCF of MHR field
#define SL_RAIL_SDK_IEEE802154_MHR_FCF_LENGTH           (sizeof(uint16_t))
/// Size of Seq.# of MHR field
#define SL_RAIL_SDK_IEEE802154_MHR_SEQ_NUM_LENGTH       (sizeof(uint8_t))
/// Size of PAN ID of MHR field
#define SL_RAIL_SDK_IEEE802154_MHR_PAN_ID_LENGTH        (sizeof(uint16_t))
/// Size of destination address (short) of MHR field
#define SL_RAIL_SDK_IEEE802154_MHR_DEST_ADDR_LENGTH     (sizeof(uint16_t))
/// Size of source address (short) of MHR field
#define SL_RAIL_SDK_IEEE802154_MHR_SRC_ADDR_LENGTH      (sizeof(uint16_t))
/// MHR length = FCF + seq# + dest PAN ID + dest. addr. + src addr.
#define SL_RAIL_SDK_IEEE802154_MHR_LENGTH       (SL_RAIL_SDK_IEEE802154_MHR_FCF_LENGTH         \
                                                 + SL_RAIL_SDK_IEEE802154_MHR_SEQ_NUM_LENGTH   \
                                                 + SL_RAIL_SDK_IEEE802154_MHR_PAN_ID_LENGTH    \
                                                 + SL_RAIL_SDK_IEEE802154_MHR_DEST_ADDR_LENGTH \
                                                 + SL_RAIL_SDK_IEEE802154_MHR_SRC_ADDR_LENGTH)

// IEEE 802.15.4g macros
/// IEEE Std. 802.15.4g CRC length 4 bytes
#define SL_RAIL_SDK_IEEE802154G_CRC_LENGTH_4BYTE        (4U)
/// IEEE Std. 802.15.4g CRC length 2 bytes
#define SL_RAIL_SDK_IEEE802154G_CRC_LENGTH_2BYTE        (2U)
/// IEEE Std. 802.15.4g max. length 11Bit
#define SL_RAIL_SDK_IEEE802154G_LEN_MAX                 (2047U)
/// IEEE Std. 802.15.4g PHR length is 2 byte
#define SL_RAIL_SDK_IEEE802154G_PHR_LENGTH              (2U)
// PHR bits next to the length in case of IEEE 802.15.4g
/// Whitening switched on
#define SL_RAIL_SDK_IEEE802154G_PHR_DATA_WHITENING_ON   (0x10)
/// Whitening switched off
#define SL_RAIL_SDK_IEEE802154G_PHR_DATA_WHITENING_OFF  (0x00)
/// Set the CRC 4 byte bit (FCS type)
#define SL_RAIL_SDK_IEEE802154G_PHR_CRC_4_BYTE          (0x00)
/// Set the CRC 2 byte bit (FCS type)
#define SL_RAIL_SDK_IEEE802154G_PHR_CRC_2_BYTE          (0x08)
/// Set the MS-FSK switched on
#define SL_RAIL_SDK_IEEE802154G_PHR_MODE_SWITCH_ON      (0x01)
/// Set the MS-FSK switched off
#define SL_RAIL_SDK_IEEE802154G_PHR_MODE_SWITCH_OFF     (0x00)
/// Mask to get the PHR configuration
#define SL_RAIL_SDK_IEEE802154G_PHR_GET_PHR_CFG_MASK    (0x1F)
/// Clears the PHR setting bits (use it as a mask)
#define SL_RAIL_SDK_IEEE802154G_PHR_CLEAR_PHR_BITS      (0xE0)

// 802.15.4-2015 Frame Control Field definitions for Beacon, Ack, Data, Command
// These MAC frame defines belongs to set the FCF field of the frame.
// They comes from the railtest example
#define MAC_FRAME_SOURCE_MODE_MASK                  0xC000U
#define MAC_FRAME_SOURCE_MODE_LONG                  0xC000U
#define MAC_FRAME_SOURCE_MODE_SHORT                 0x8000U
#define MAC_FRAME_SOURCE_MODE_RESERVED              0x4000U
#define MAC_FRAME_SOURCE_MODE_NONE                  0x0000U
#define MAC_FRAME_VERSION_MASK                      0x3000U
#define MAC_FRAME_VERSION_RESERVED                  0x3000U
#define MAC_FRAME_VERSION_2012                      0x2000U
#define MAC_FRAME_VERSION_2006                      0x1000U
#define MAC_FRAME_VERSION_2003                      0x0000U
#define MAC_FRAME_DESTINATION_MODE_MASK             0x0C00U
#define MAC_FRAME_DESTINATION_MODE_LONG             0x0C00U
#define MAC_FRAME_DESTINATION_MODE_SHORT            0x0800U
#define MAC_FRAME_DESTINATION_MODE_RESERVED         0x0400U
#define MAC_FRAME_DESTINATION_MODE_NONE             0x0000U
#define MAC_FRAME_FLAG_IE_LIST_PRESENT              0x0200U
#define MAC_FRAME_FLAG_SEQ_SUPPRESSION              0x0100U
#define MAC_FRAME_FLAG_RESERVED                     0x0080U
#define MAC_FRAME_FLAG_PANID_COMPRESSION            0x0040U
#define MAC_FRAME_FLAG_ACK_REQUIRED                 0x0020U
#define MAC_FRAME_FLAG_FRAME_PENDING                0x0010U
#define MAC_FRAME_FLAG_SECURITY_ENABLED             0x0008U
#define MAC_FRAME_TYPE_MASK                         0x0007U
#define MAC_FRAME_TYPE_BEACON                       0x0000U
#define MAC_FRAME_TYPE_DATA                         0x0001U
#define MAC_FRAME_TYPE_ACK                          0x0002U
#define MAC_FRAME_TYPE_CONTROL                      0x0003U

/// ACK packet length 802.15.4
#define SL_RAIL_SDK_IEEE802154_ACK_PACKET_LENGTH  (SL_RAIL_SDK_IEEE802154_PHR_LENGTH       \
                                                   + SL_RAIL_SDK_IEEE802154_MHR_FCF_LENGTH \
                                                   + SL_RAIL_SDK_IEEE802154_MHR_SEQ_NUM_LENGTH)
/// ACK packet length 802.15.4g
#define SL_RAIL_SDK_IEEE802154G_ACK_PACKET_LENGTH (SL_RAIL_SDK_IEEE802154G_PHR_LENGTH      \
                                                   + SL_RAIL_SDK_IEEE802154_MHR_FCF_LENGTH \
                                                   + SL_RAIL_SDK_IEEE802154_MHR_SEQ_NUM_LENGTH)

#endif // DOXYGEN_SHOULD_SKIP_THIS

/**
 * \addtogroup rail_sdk_packet_asm
 * @{
 */

/**************************************************************************//**
 * @addtogroup rail_sdk_packet_asm_types Type definitions
 * @ingroup rail_sdk_packet_asm
 * @{
 *****************************************************************************/

/** @brief MAC header (MHR) fields for IEEE 802.15.4 and 802.15.4 data frames. */
typedef struct {
  uint16_t frame_control;               //!< FCF (Frame Control Frame)
  uint8_t sequence_number;              //!< sequence number
  uint16_t destination_pan_id;          //!< Destination PAN ID
  uint16_t destination_address;         //!< Destination address
  uint16_t source_address;              //!< Source address (short address)
} sl_rail_sdk_802154_packet_mhr_frame_t;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/*
   BLE
 */
/// BLE Advertising type
#define BLE_ADV_NONCONN_IND                                (0x02)
#define SL_RAIL_SDK_BLE_PDU_TYPE                               (BLE_ADV_NONCONN_IND)
/// BLE setup
#define SL_RAIL_SDK_BLE_HEADER_LSB                             (SL_RAIL_SDK_BLE_PDU_TYPE)   // RFU(0)|ChSel(0)|TxAdd(0)|RxAdd(0)
#define SL_RAIL_SDK_BLE_PHYSICAL_CH                            (0U)
#define SL_RAIL_SDK_BLE_LOGICAL_CH                             (37U)
#define SL_RAIL_SDK_BLE_CRC_INIT                               (0x555555)
#define SL_RAIL_SDK_BLE_ACCESS_ADDRESS                         (0x12345678)             // 0x8E89BED6
#define SL_RAIL_SDK_BLE_COMPANY_ID                             ((uint16_t) 0x02FF)      // Company Identifier of Silicon Labs
#define SL_RAIL_SDK_BLE_DISABLE_WHITENING                      (false)

// AD Structure: Flags
#define SL_RAIL_SDK_BLE_ADSTRUCT_TYPE_FLAG                     (0x01)
#define SL_RAIL_SDK_BLE_DISABLE_BR_EDR                         (0x04)
#define SL_RAIL_SDK_BLE_GENERAL_DISCOVERABLE_MODE              (0x02)

// AD Structure: Manufacturer Specific
#define SL_RAIL_SDK_BLE_ADSTRUCT_TYPE_MANUFACTURER_SPECIFIC    (0xFF)

// Packet parameter
#define SL_RAIL_SDK_BLE_HEADER_LEN_BASE                        (0x0E)
#define SL_RAIL_SDK_BLE_PAYLOAD_LEN_MIN                        (0x06) // Minimum payload
#define SL_RAIL_SDK_BLE_PAYLOAD_LEN_MAX                        (0x25) // Maximum payload
#endif // DOXYGEN_SHOULD_SKIP_THIS

/** @brief BLE advertising packet length type. */
typedef uint8_t sl_rail_sdk_ble_packet_size_t;

/// No padding in the structures
#pragma pack(1)
/** @brief BLE advertising channel PDU header (type and length). */
typedef struct {
  uint8_t type;   //!< PDU type field.
  uint8_t length; //!< PDU length field.
} ble_advertising_channel_pdu_header_t;

/** @brief BLE advertising flags AD structure. */
typedef struct {
  uint8_t length;           //!< AD structure length.
  uint8_t advertising_type; //!< AD type (flags).
  uint8_t flags;            //!< BLE flags value.
} ble_advertising_flags_t;

/** @brief BLE manufacturer-specific AD structure with application payload. */
typedef struct {
  uint8_t length;           //!< AD structure length.
  uint8_t advertising_type; //!< AD type (manufacturer specific).
  uint16_t company_id;      //!< Company identifier.
  uint8_t version;          //!< Manufacturer-specific version byte.
  uint8_t payload[SL_RAIL_SDK_BLE_PAYLOAD_LEN_MAX]; //!< Application payload.
} ble_advertising_manufacture_specific_t;

/** @brief Complete BLE non-connectable advertising packet used by RAIL SDK samples. */
typedef struct {
  ble_advertising_channel_pdu_header_t header; //!< Advertising channel PDU header.
  uint8_t advAddr[6];                            //!< Advertiser address.
  ble_advertising_flags_t flags;                 //!< Flags AD structure.
  ble_advertising_manufacture_specific_t manufactSpec; //!< Manufacturer-specific AD structure.
} sl_rail_sdk_ble_advertising_packet_t;
#pragma pack()

/** @} (end rail_sdk_packet_asm_types) */

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//                          Public Function Declarations
// -----------------------------------------------------------------------------
/**************************************************************************//**
 * This function packs a complete IEEE 802.15.4g frame (PHY + MAC): PHR + MHR + payload.
 *
 * @param[in] phr_cfg             PHR config/information for IEEE 802.15.4g
 * @param[in] *mhr_cfg            MHR configuration
 * @param[in] payload_size        payload size
 * @param[in] *payload            payload
 * @param[out] *frame_size        frame_size that is calculated
 * @param[out] *frame_buffer      buffer of packed frame
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK if the process has been successful.
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR if the process has been failed.
 *****************************************************************************/
int16_t sl_rail_sdk_802154_packet_pack_g_opt_frame(uint8_t phr_cfg,
                                                   sl_rail_sdk_802154_packet_mhr_frame_t *mhr_cfg,
                                                   uint16_t payload_size,
                                                   void *payload,
                                                   uint16_t *frame_size,
                                                   uint8_t *frame_buffer);

/**************************************************************************//**
 * This function packs a complete standard IEEE 802.15.4 frame (PHY + MAC): PHR + MHR + payload.
 *
 * @param[in] *mhr_cfg            MHR configuration
 * @param[in] payload_size        payload size
 * @param[in] *payload            payload
 * @param[out] *frame_size        frame_size that is calculated
 * @param[out] *frame_buffer      buffer of packed frame
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK if the process has been successful.
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR if the process has been failed.
 *****************************************************************************/
int16_t sl_rail_sdk_802154_packet_pack_std_frame(const sl_rail_sdk_802154_packet_mhr_frame_t *mhr_cfg,
                                                 uint16_t payload_size,
                                                 void *payload,
                                                 uint16_t *frame_size,
                                                 uint8_t *frame_buffer);

/**************************************************************************//**
 * This function packs an IEEE 802.15.4 SUN OFDM PPDU (PHR + payload).
 *
 * @param[in] rate                5 bits wide, The Rate field (RA4-RA0) specifies the data rate of the payload and is equal to the numerical value of the MCS
 * @param[in] scrambler           2 bits wide, The Scrambler field (S1-S0) specifies the scrambling seed
 * @param[in] payload_size        payload size
 * @param[in] *payload            payload
 * @param[out] *frame_size        frame_size that is calculated
 * @param[out] *frame_buffer      buffer of packed frame
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK if the process has been successful.
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR if the process has been failed.
 *****************************************************************************/
int16_t sl_rail_sdk_802154_packet_pack_ofdm_ppdu(uint8_t rate,
                                                 uint8_t scrambler,
                                                 uint16_t payload_size,
                                                 const uint8_t *payload,
                                                 uint16_t *frame_size,
                                                 uint8_t *frame_buffer);

/**************************************************************************//**
 * Pack SUN OFDM PHR bytes only (payload is supplied separately for TX).
 *
 * @param[in] rate                OFDM rate / MCS (5 bits)
 * @param[in] scrambler           Scrambler seed (2 bits)
 * @param[in] payload_size        Payload length in bytes
 * @param[out] phr_buffer         Buffer for PHR bytes (4 bytes)
 * @param[out] phr_size           Number of PHR bytes written
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK on success
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR on invalid argument
 *****************************************************************************/
int16_t sl_rail_sdk_802154_packet_pack_ofdm_phr(uint8_t rate,
                                                uint8_t scrambler,
                                                uint16_t payload_size,
                                                uint8_t *phr_buffer,
                                                uint8_t *phr_size);

/**************************************************************************//**
 * This function packs an IEEE 802.15.4 SUN FSK PPDU (2-byte PHR + payload).
 *
 * @param[in] fcsType             0 = 4-byte FCS, 1 = 2-byte FCS
 * @param[in] whitening           Whitening is on/off
 * @param[in] payload_size        payload size
 * @param[in] *payload            payload
 * @param[out] *frame_size        frame_size that is calculated
 * @param[out] *frame_buffer      buffer of packed frame
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK if the process has been successful.
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR if the process has been failed.
 *****************************************************************************/
int16_t sl_rail_sdk_802154_packet_pack_sunfsk_ppdu(uint8_t fcsType,
                                                   uint8_t whitening,
                                                   uint16_t payload_size,
                                                   const uint8_t *payload,
                                                   uint16_t *frame_size,
                                                   uint8_t *frame_buffer);

/**************************************************************************//**
 * Pack SUN FSK PHR bytes only (payload is supplied separately for TX).
 *
 * @param[in] fcsType             0 = 4-byte FCS, 1 = 2-byte FCS
 * @param[in] whitening           Whitening on/off
 * @param[in] payload_size        Payload length in bytes
 * @param[out] phr_buffer         Buffer for PHR bytes
 * @param[out] phr_size           Number of PHR bytes written (2)
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK on success
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR on invalid argument
 *****************************************************************************/
int16_t sl_rail_sdk_802154_packet_pack_sunfsk_phr(uint8_t fcsType,
                                                  uint8_t whitening,
                                                  uint16_t payload_size,
                                                  uint8_t *phr_buffer,
                                                  uint8_t *phr_size);

/**************************************************************************//**
 * This function packs an IEEE 802.15.4 SUN OQPSK PPDU (PHR + payload).
 *
 * @param[in] spreadingMode       spreading mode
 * @param[in] rateMode            rate mode: 2 bits wide
 * @param[in] payload_size        payload size
 * @param[in] *payload            payload
 * @param[out] *frame_size        frame_size that is calculated
 * @param[out] *frame_buffer      buffer of packed frame
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK if the process has been successful.
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR if the process has been failed.
 *****************************************************************************/
int16_t sl_rail_sdk_802154_packet_pack_oqpsk_ppdu(bool spreadingMode,
                                                  uint8_t rateMode,
                                                  uint16_t payload_size,
                                                  const uint8_t *payload,
                                                  uint16_t *frame_size,
                                                  uint8_t *frame_buffer);

/**************************************************************************//**
 * Pack SUN OQPSK PHR bytes only (payload is supplied separately for TX).
 *
 * @param[in] spreadingMode       Spreading mode
 * @param[in] rateMode            Rate mode (2 bits)
 * @param[in] payload_size        Payload length in bytes
 * @param[out] phr_buffer         Buffer for PHR bytes
 * @param[out] phr_size           Number of PHR bytes written (4)
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK on success
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR on invalid argument
 *****************************************************************************/
int16_t sl_rail_sdk_802154_packet_pack_oqpsk_phr(bool spreadingMode,
                                                 uint8_t rateMode,
                                                 uint16_t payload_size,
                                                 uint8_t *phr_buffer,
                                                 uint8_t *phr_size);

/**************************************************************************//**
 * This function unpacks a complete IEEE 802.15.4g frame (PHY + MAC): PHR + MHR + payload.
 *
 * @param[out] *phr_cfg           PHR config/information for IEEE 802.15.4g
 * @param[out] *mhr_cfg           MHR configuration
 * @param[out] *payload_size      payload size
 * @param[in] *frame_buffer       buffer of packed frame
 *
 * @return pointer of the payload
 *****************************************************************************/
uint8_t *sl_rail_sdk_802154_packet_unpack_g_opt_frame(uint8_t *phr_cfg,
                                                      sl_rail_sdk_802154_packet_mhr_frame_t *mhr_cfg,
                                                      uint16_t *payload_size,
                                                      uint8_t *frame_buffer);

/**************************************************************************//**
 * This function unpacks a complete standard IEEE 802.15.4 frame (PHY + MAC): PHR + MHR + payload.
 *
 * @param[out] *mhr_cfg           MHR configuration
 * @param[out] *payload_size      payload size
 * @param[in] *frame_buffer       buffer of packed frame
 *
 * @return pointer of the payload
 *****************************************************************************/
uint8_t *sl_rail_sdk_802154_packet_unpack_std_frame(sl_rail_sdk_802154_packet_mhr_frame_t *mhr_cfg,
                                                    uint16_t *payload_size,
                                                    uint8_t *frame_buffer);

/**************************************************************************//**
 * This function unpacks a received IEEE 802.15.4 SUN FSK PPDU (2-byte PHR).
 *
 * @param[in] *packet_information   packet information
 * @param[out] *fcsType             0 = 4-byte FCS, 1 = 2-byte FCS
 * @param[out] *whitening           whitening is on/off
 * @param[out] *payload_size        payload size
 * @param[in] *frame_buffer         buffer of packed frame
 *
 * @return pointer of the payload
 * @return NULL on error
 *****************************************************************************/
uint8_t *sl_rail_sdk_802154_packet_unpack_sunfsk_ppdu(const sl_rail_rx_packet_info_t *packet_information,
                                                      uint8_t *fcsType,
                                                      uint8_t *whitening,
                                                      uint16_t *payload_size,
                                                      uint8_t *frame_buffer);

/**************************************************************************//**
 * This function unpacks a received IEEE 802.15.4 SUN OFDM PPDU.
 *
 * @param[in] *packet_information   packet information
 * @param[out] *rate                5 bits wide, The Rate field (RA4-RA0) specifies the data rate of the payload and is equal to the numerical value of the MCS
 * @param[out] *scrambler           2 bits wide, The Scrambler field (S1-S0) specifies the scrambling seed
 * @param[out] *payload_size        payload size
 * @param[in] *frame_buffer         buffer of packed frame
 *
 * @return pointer of the payload
 * @return NULL on error
 *****************************************************************************/
uint8_t *sl_rail_sdk_802154_packet_unpack_ofdm_ppdu(const sl_rail_rx_packet_info_t *packet_information,
                                                    uint8_t *rate,
                                                    uint8_t *scrambler,
                                                    uint16_t *payload_size,
                                                    uint8_t *frame_buffer);

/**************************************************************************//**
 * This function unpacks a received IEEE 802.15.4 SUN OQPSK PPDU.
 *
 * @param[in] *packet_information   packet information
 * @param[out] *spreadingMode       spreading mode
 * @param[out] *rateMode            rate mode: 2 bits wide
 * @param[out] *payload_size        payload size
 * @param[in] *frame_buffer         buffer of packed frame
 *
 * @return pointer of the payload
 * @return NULL on error
 *****************************************************************************/
uint8_t *sl_rail_sdk_802154_packet_unpack_oqpsk_ppdu(const sl_rail_rx_packet_info_t *packet_information,
                                                     bool *spreadingMode,
                                                     uint8_t *rateMode,
                                                     uint16_t *payload_size,
                                                     uint8_t *frame_buffer);

/**************************************************************************//**
 * Pack the tx data buffer to get BLE advertising packet frame.
 *
 * @param[out] *packet            BLE packet frame
 * @param[in] *payload_length     payload size
 * @param[in] *payload            payload
 *
 *****************************************************************************/
void sl_rail_sdk_ble_prepare_packet(sl_rail_sdk_ble_advertising_packet_t *packet,
                                    const uint8_t *payload,
                                    const sl_rail_sdk_ble_packet_size_t payload_length);

/**************************************************************************//**
 * Get packet size
 *
 * @param[in] *packet            BLE packet frame
 *
 * @return packet size
 *****************************************************************************/
sl_rail_sdk_ble_packet_size_t
sl_rail_sdk_ble_get_packet_size(const sl_rail_sdk_ble_advertising_packet_t *packet);

/**************************************************************************//**
 * Copy BLE packet from received data
 *
 * @param[out] *packet            BLE packet frame
 * @param[in] *rx_data            received packet data
 *
 *****************************************************************************/
void sl_rail_sdk_ble_copy_packet_from_buff(sl_rail_sdk_ble_advertising_packet_t *packet,
                                           const uint8_t *rx_data);

/**************************************************************************//**
 * Get BLE packet pointer from data buffer
 *
 * @param[in] data            received packet data
 * @return Pointer to the BLE advertising packet in @p data.
 *****************************************************************************/
extern sl_rail_sdk_ble_advertising_packet_t *sl_rail_sdk_ble_get_packet(uint8_t *data);

/**************************************************************************//**
 * Copy payload to destination buffer
 *
 * @param[in] *packet             BLE packet frame
 * @param[out] *dest              destination buffer
 * @param[in] *payload_length     Length of payload
 *
 *****************************************************************************/
void sl_rail_sdk_ble_copy_payload(sl_rail_sdk_ble_advertising_packet_t *packet,
                                  uint8_t *dest,
                                  const sl_rail_sdk_ble_packet_size_t payload_length);

/**************************************************************************//**
 * This function packs an IEEE 802.15.4 SideWalk FSK PPDU (2-byte PHR + payload).
 *
 * @param[in] fcsType             0 = 4-byte FCS, 1 = 2-byte FCS
 * @param[in] whitening           Whitening is on/off
 * @param[in] payload_size        payload size
 * @param[in] *payload            payload
 * @param[out] *frame_size        frame_size that is calculated
 * @param[out] *frame_buffer      buffer of packed frame
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK if the process has been successful.
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR if the process has been failed.
 *****************************************************************************/
int16_t sl_rail_sdk_802154_packet_pack_sidewalk_ppdu(uint8_t fcsType,
                                                     uint8_t whitening,
                                                     uint16_t payload_size,
                                                     const uint8_t *payload,
                                                     uint16_t *frame_size,
                                                     uint8_t *frame_buffer);

/**************************************************************************//**
 * Pack SideWalk FSK PHR bytes only (payload is supplied separately for TX).
 *
 * @param[in] fcsType             0 = 4-byte FCS, 1 = 2-byte FCS
 * @param[in] whitening           Whitening on/off
 * @param[in] payload_size        Payload length in bytes
 * @param[out] phr_buffer         Buffer for PHR bytes
 * @param[out] phr_size           Number of PHR bytes written (2)
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK on success
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR on invalid argument
 *****************************************************************************/
int16_t sl_rail_sdk_802154_packet_pack_sidewalk_phr(uint8_t fcsType,
                                                    uint8_t whitening,
                                                    uint16_t payload_size,
                                                    uint8_t *phr_buffer,
                                                    uint8_t *phr_size);

/**************************************************************************//**
 * This function unpacks a received IEEE 802.15.4 SideWalk FSK PPDU (2-byte PHR).
 *
 * @param[in] *packet_information   packet information
 * @param[out] *fcsType             0 = 4-byte FCS, 1 = 2-byte FCS
 * @param[out] *whitening           whitening is on/off
 * @param[out] *payload_size        payload size
 * @param[in] *frame_buffer         buffer of packed frame
 *
 * @return pointer of the payload
 * @return NULL on error
 *****************************************************************************/
uint8_t *sl_rail_sdk_802154_packet_unpack_sidewalk_ppdu(const sl_rail_rx_packet_info_t *packet_information,
                                                        uint8_t *fcsType,
                                                        uint8_t *whitening,
                                                        uint16_t *payload_size,
                                                        uint8_t *frame_buffer);

/**************************************************************************//**
 * This function packs a standard IEEE 802.15.4 PPDU with a 1-byte PHR.
 *
 * @param[in] payload_size        payload size
 * @param[in] *payload            payload
 * @param[out] *frame_size        frame_size that is calculated
 * @param[out] *frame_buffer      buffer of packed frame
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK if the process has been successful.
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR if the process has been failed.
 *****************************************************************************/
int16_t sl_rail_sdk_802154_packet_pack_std_1byte_phr_ppdu(uint16_t payload_size,
                                                          const uint8_t *payload,
                                                          uint16_t *frame_size,
                                                          uint8_t *frame_buffer);

/**************************************************************************//**
 * Pack a standard IEEE 802.15.4 1-byte PHR (payload supplied separately for TX).
 *
 * @param[in] payload_size        Payload length in bytes
 * @param[out] phr_buffer         Buffer for PHR bytes
 * @param[out] phr_size           Number of PHR bytes written (1)
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK on success
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR on invalid argument
 *****************************************************************************/
int16_t sl_rail_sdk_802154_packet_pack_std_1byte_phr(uint16_t payload_size,
                                                     uint8_t *phr_buffer,
                                                     uint8_t *phr_size);

/**************************************************************************//**
 * This function unpacks a standard IEEE 802.15.4 PPDU with a 1-byte PHR.
 *
 * @param[in] *packet_information   packet information
 * @param[out] *payload_size        payload size
 * @param[in] *frame_buffer         buffer of packed frame
 *
 * @return pointer of the payload
 * @return NULL on error
 *****************************************************************************/
uint8_t *sl_rail_sdk_802154_packet_unpack_std_1byte_phr_ppdu(const sl_rail_rx_packet_info_t *packet_information,
                                                             uint16_t *payload_size,
                                                             uint8_t *frame_buffer);

/**************************************************************************//**
 * This function packs an IEEE 802.15.4 BPSK PPDU (1-byte PHR + payload).
 *
 * @param[in] payload_size        payload size
 * @param[in] *payload            payload
 * @param[out] *frame_size        frame_size that is calculated
 * @param[out] *frame_buffer      buffer of packed frame
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK if the process has been successful.
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR if the process has been failed.
 *****************************************************************************/
static inline int16_t sl_rail_sdk_802154_packet_pack_bpsk_ppdu(uint16_t payload_size,
                                                               const uint8_t *payload,
                                                               uint16_t *frame_size,
                                                               uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_pack_std_1byte_phr_ppdu(payload_size, payload, frame_size, frame_buffer);
}

/**************************************************************************//**
 * This function unpacks an IEEE 802.15.4 BPSK PPDU (1-byte PHR).
 *
 * @param[in] *packet_information   packet information
 * @param[out] *payload_size        payload size
 * @param[in] *frame_buffer         buffer of packed frame
 *
 * @return pointer of the payload
 * @return NULL on error
 *****************************************************************************/
static inline uint8_t *sl_rail_sdk_802154_packet_unpack_bpsk_ppdu(const sl_rail_rx_packet_info_t *packet_information,
                                                                  uint16_t *payload_size,
                                                                  uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_unpack_std_1byte_phr_ppdu(packet_information, payload_size, frame_buffer);
}

/**************************************************************************//**
 * This function packs an IEEE 802.15.4 Long Range PPDU (1-byte PHR + payload).
 *
 * @param[in] payload_size        payload size
 * @param[in] *payload            payload
 * @param[out] *frame_size        frame_size that is calculated
 * @param[out] *frame_buffer      buffer of packed frame
 *
 * @retval SL_RAIL_SDK_802154_PACKET_OK if the process has been successful.
 * @retval SL_RAIL_SDK_802154_PACKET_ERROR if the process has been failed.
 *****************************************************************************/
static inline int16_t sl_rail_sdk_802154_packet_pack_longrange_ppdu(uint16_t payload_size,
                                                                   const uint8_t *payload,
                                                                   uint16_t *frame_size,
                                                                   uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_pack_std_1byte_phr_ppdu(payload_size, payload, frame_size, frame_buffer);
}

/**************************************************************************//**
 * This function unpacks an IEEE 802.15.4 Long Range PPDU (1-byte PHR).
 *
 * @param[in] *packet_information   packet information
 * @param[out] *payload_size        payload size
 * @param[in] *frame_buffer         buffer of packed frame
 *
 * @return pointer of the payload
 * @return NULL on error
 *****************************************************************************/
static inline uint8_t *sl_rail_sdk_802154_packet_unpack_longrange_ppdu(const sl_rail_rx_packet_info_t *packet_information,
                                                                      uint16_t *payload_size,
                                                                      uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_unpack_std_1byte_phr_ppdu(packet_information, payload_size, frame_buffer);
}

/**************************************************************************//**
 * Get Payload pointer
 *
 * @param[in] *packet             BLE packet frame
 *
 * @return payload pointer
 *****************************************************************************/
extern  uint8_t
*sl_rail_sdk_ble_get_payload(sl_rail_sdk_ble_advertising_packet_t *packet);

/**************************************************************************//**
 * Get Payload pointer
 *
 * @param[in] *packet             BLE packet frame
 *
 * @return payload length
 *****************************************************************************/
extern  sl_rail_sdk_ble_packet_size_t
sl_rail_sdk_ble_get_payload_len(const sl_rail_sdk_ble_advertising_packet_t *packet);

/** @} */ // end of rail_sdk_packet_asm

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/// @deprecated Use \ref sl_rail_sdk_802154_packet_pack_g_opt_frame() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline int16_t sl_rail_sdk_802154_packet_pack_g_opt_data_frame(uint8_t phr_cfg,
                                                                      sl_rail_sdk_802154_packet_mhr_frame_t *mhr_cfg,
                                                                      uint16_t payload_size,
                                                                      void *payload,
                                                                      uint16_t *frame_size,
                                                                      uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_pack_g_opt_frame(phr_cfg, mhr_cfg, payload_size,
                                                   payload, frame_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_pack_std_frame() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline int16_t sl_rail_sdk_802154_packet_pack_data_frame(const sl_rail_sdk_802154_packet_mhr_frame_t *mhr_cfg,
                                                                uint16_t payload_size,
                                                                void *payload,
                                                                uint16_t *frame_size,
                                                                uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_pack_std_frame(mhr_cfg, payload_size, payload,
                                                 frame_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_pack_ofdm_ppdu() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline int16_t sl_rail_sdk_802154_packet_pack_ofdm_data_frame(uint8_t rate,
                                                                     uint8_t scrambler,
                                                                     uint16_t payload_size,
                                                                     const uint8_t *payload,
                                                                     uint16_t *frame_size,
                                                                     uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_pack_ofdm_ppdu(rate, scrambler, payload_size,
                                                  payload, frame_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_pack_sunfsk_ppdu() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline int16_t sl_rail_sdk_802154_packet_pack_sunfsk_2bytes_data_frame(uint8_t fcsType,
                                                                              uint8_t whitening,
                                                                              uint16_t payload_size,
                                                                              const uint8_t *payload,
                                                                              uint16_t *frame_size,
                                                                              uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_pack_sunfsk_ppdu(fcsType, whitening, payload_size,
                                                    payload, frame_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_pack_oqpsk_ppdu() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline int16_t sl_rail_sdk_802154_packet_pack_oqpsk_data_frame(bool spreadingMode,
                                                                      uint8_t rateMode,
                                                                      uint16_t payload_size,
                                                                      const uint8_t *payload,
                                                                      uint16_t *frame_size,
                                                                      uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_pack_oqpsk_ppdu(spreadingMode, rateMode, payload_size,
                                                   payload, frame_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_unpack_g_opt_frame() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t *sl_rail_sdk_802154_packet_unpack_g_opt_data_frame(uint8_t *phr_cfg,
                                                                         sl_rail_sdk_802154_packet_mhr_frame_t *mhr_cfg,
                                                                         uint16_t *payload_size,
                                                                         uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_unpack_g_opt_frame(phr_cfg, mhr_cfg, payload_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_unpack_std_frame() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t *sl_rail_sdk_802154_packet_unpack_data_frame(sl_rail_sdk_802154_packet_mhr_frame_t *mhr_cfg,
                                                                   uint16_t *payload_size,
                                                                   uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_unpack_std_frame(mhr_cfg, payload_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_unpack_sunfsk_ppdu() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t *sl_rail_sdk_802154_packet_unpack_sunfsk_2byte_data_frame(const sl_rail_rx_packet_info_t *packet_information,
                                                                                uint8_t *fcsType,
                                                                                uint8_t *whitening,
                                                                                uint16_t *payload_size,
                                                                                uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_unpack_sunfsk_ppdu(packet_information, fcsType, whitening,
                                                      payload_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_unpack_ofdm_ppdu() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t *sl_rail_sdk_802154_packet_unpack_ofdm_data_frame(const sl_rail_rx_packet_info_t *packet_information,
                                                                        uint8_t *rate,
                                                                        uint8_t *scrambler,
                                                                        uint16_t *payload_size,
                                                                        uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_unpack_ofdm_ppdu(packet_information, rate, scrambler,
                                                    payload_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_unpack_oqpsk_ppdu() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t *sl_rail_sdk_802154_packet_unpack_oqpsk_data_frame(const sl_rail_rx_packet_info_t *packet_information,
                                                                         bool *spreadingMode,
                                                                         uint8_t *rateMode,
                                                                         uint16_t *payload_size,
                                                                         uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_unpack_oqpsk_ppdu(packet_information, spreadingMode, rateMode,
                                                     payload_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_pack_sidewalk_ppdu() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline int16_t sl_rail_sdk_802154_packet_pack_sidewalk_data_frame(uint8_t fcsType,
                                                                         uint8_t whitening,
                                                                         uint16_t payload_size,
                                                                         const uint8_t *payload,
                                                                         uint16_t *frame_size,
                                                                         uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_pack_sidewalk_ppdu(fcsType, whitening, payload_size,
                                                      payload, frame_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_unpack_sidewalk_ppdu() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t *sl_rail_sdk_802154_packet_unpack_sidewalk_data_frame(const sl_rail_rx_packet_info_t *packet_information,
                                                                            uint8_t *fcsType,
                                                                            uint8_t *whitening,
                                                                            uint16_t *payload_size,
                                                                            uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_unpack_sidewalk_ppdu(packet_information, fcsType, whitening,
                                                        payload_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_pack_bpsk_ppdu() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline int16_t sl_rail_sdk_802154_packet_pack_bpsk_data_frame(uint16_t payload_size,
                                                                       const uint8_t *payload,
                                                                       uint16_t *frame_size,
                                                                       uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_pack_bpsk_ppdu(payload_size, payload, frame_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_unpack_bpsk_ppdu() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t *sl_rail_sdk_802154_packet_unpack_bpsk_data_frame(const sl_rail_rx_packet_info_t *packet_information,
                                                                          uint16_t *payload_size,
                                                                          uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_unpack_bpsk_ppdu(packet_information, payload_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_pack_longrange_ppdu() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline int16_t sl_rail_sdk_802154_packet_pack_longrange_data_frame(uint16_t payload_size,
                                                                           const uint8_t *payload,
                                                                           uint16_t *frame_size,
                                                                           uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_pack_longrange_ppdu(payload_size, payload, frame_size, frame_buffer);
}

/// @deprecated Use \ref sl_rail_sdk_802154_packet_unpack_longrange_ppdu() instead.
SL_DEPRECATED_API_SDK_2026_12
static inline uint8_t *sl_rail_sdk_802154_packet_unpack_longrange_data_frame(const sl_rail_rx_packet_info_t *packet_information,
                                                                             uint16_t *payload_size,
                                                                             uint8_t *frame_buffer)
{
  return sl_rail_sdk_802154_packet_unpack_longrange_ppdu(packet_information, payload_size, frame_buffer);
}

#endif // DOXYGEN_SHOULD_SKIP_THIS

#endif // SL_RAIL_SDK_PACKET_ASM_H
