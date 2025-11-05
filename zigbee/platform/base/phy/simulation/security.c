/***************************************************************************//**
 * @file
 * @brief This file implements the routines for encrypting parcels and packets
 * using AES.
 * These are normally provided by the chip on our real hardware.
 * For simulation we can either use real encryption, pseudo encryption,
 * or no encryption. This gives us more flexibility in debugging.
 * We use the Rijndael encryption algorithm to do the work for
 * real encryption.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "core/sl_zigbee_stack.h"
#include "hal/hal.h"
#include "phy/security.h"
#include "micro/unix/simulation/em250/regs.h"
#include "zigbee/aps-security.h"
#include "zigbee/aps-keys.h"

#include "core/parcel.h"

#include "stack/framework/packet-header.h"

#include "phy/simulation/security.h"

//------------------------------------------------------------------------------
// Globals

// These are some Dummy MIC values we use for various tests when
// the encryption level is Pseudo.

uint8_t standardMic[] =
{
  0xF0, 0xE1, 0xD2, 0xC3,
  0xB4, 0xA5, 0x96, 0x87,
  0x78, 0x69, 0x5A, 0x4B,
  0x3C, 0x2D, 0x1E, 0x0F,
};

// This global controls how the stack behaves in simulation.
SimulatorEncryption simulatorEncryptionType = USE_PSEUDO_ENCRYPTION;

bool emPseudoEncryptUseTransientLinkKey = false;

static uint8_t transientLinkKeyMic[] =  { 0x12, 0x34, 0x56, 0x78 };

//------------------------------------------------------------------------------
// External Declarations

void encryptPayload(sli_buffer_manager_buffer_t, uint8_t, uint8_t*);

//------------------------------------------------------------------------------

void emSecuritySetNonce(uint16_t nonceIndex,
                        sl_802154_long_addr_t srcEui64,
                        uint32_t frameCounter,
                        uint8_t keySequenceNumber)
{
  UNUSED_VAR(nonceIndex);
  UNUSED_VAR(srcEui64);
  UNUSED_VAR(frameCounter);
  UNUSED_VAR(keySequenceNumber);
}

//------------------------------------------------------------------------------
// Header is verified but not encrypted and this doesn't do any verification.

#if defined SL_ZIGBEE_TEST
static void pseudoEncrypt(sli_buffer_manager_buffer_t payload,
                          uint8_t startIndex,
                          uint8_t offset)
{
  if (simulatorEncryptionType != USE_PSEUDO_ENCRYPTION) {
    return;
  }

  if (payload != SL_ZIGBEE_NULL_MESSAGE_BUFFER) {
    uint8_t i;
    uint8_t payloadLength = sl_legacy_buffer_manager_message_buffer_length(payload);

    for (i = startIndex; i < payloadLength; i++) {
      sl_legacy_buffer_manager_set_linked_buffers_byte(payload,
                                                       i,
                                                       (sl_legacy_buffer_manager_get_linked_buffers_byte(payload, i)
                                                        + offset));
    }
  }
}

#endif

//------------------------------------------------------------------------------

#if defined SL_ZIGBEE_TEST
bool emPseudoEncryptPacket(sli_zigbee_packet_header_t *header,
                           uint8_t authenticationStartOffset,
                           uint8_t encryptionStartOffset)
{
  UNUSED_VAR(authenticationStartOffset);
  bool retval;
  sli_buffer_manager_buffer_t payload = sli_legacy_packet_buffer_packet_header_payload(*header);

  pseudoEncrypt(*header, encryptionStartOffset, pseudoEncryptOffset);

  if (payload != SL_ZIGBEE_NULL_MESSAGE_BUFFER) {
    pseudoEncrypt(payload, 0, pseudoEncryptOffset);
  }
  if (payload == SL_ZIGBEE_NULL_MESSAGE_BUFFER) {
    retval = (sl_legacy_buffer_manager_append_to_linked_buffers(*header,
                                                                (emPseudoEncryptUseTransientLinkKey
                                                                 ? transientLinkKeyMic
                                                                 : standardMic),
                                                                4)
              == SL_STATUS_OK);
  } else {
    retval = (sl_legacy_buffer_manager_append_to_linked_buffers(payload,
                                                                (emPseudoEncryptUseTransientLinkKey
                                                                 ? transientLinkKeyMic
                                                                 : standardMic),
                                                                4)
              == SL_STATUS_OK);
    sli_legacy_packet_buffer_set_packet_header_payload(*header, payload);
  }

  return retval;
}

void emPseudoEncryptFlatPacket(uint8_t *packet,
                               uint8_t length,
                               uint8_t authenticationStartOffset,
                               uint8_t encryptionStartOffset)
{
  UNUSED_VAR(authenticationStartOffset);
  uint8_t i;

  if (simulatorEncryptionType == USE_PSEUDO_ENCRYPTION) {
    for (i = encryptionStartOffset; i < length; i++) {
      packet[i] += pseudoEncryptOffset;
    }
  }

  for (i = 0; i < 4; i++) {
    packet[length + i] = standardMic[i];
  }
}

#endif

//------------------------------------------------------------------------------

#if defined SL_ZIGBEE_TEST
bool emPseudoDecryptPacket(sli_zigbee_packet_header_t header,
                           uint8_t authenticationStartOffset,
                           uint8_t encryptionStartOffset)
{
  UNUSED_VAR(authenticationStartOffset);
  sli_buffer_manager_buffer_t payload = sli_legacy_packet_buffer_packet_header_payload(header);
  uint8_t payloadLength;
  uint8_t mic[4];

  if (payload == SL_ZIGBEE_NULL_MESSAGE_BUFFER) {
    payload = header;
  }

  payloadLength = sl_legacy_buffer_manager_message_buffer_length(payload);
  sl_legacy_buffer_manager_copy_from_linked_buffers(payload, payloadLength - 4, mic, 4);

  if (0 != memcmp(mic,
                  (emPseudoEncryptUseTransientLinkKey
                   ? transientLinkKeyMic
                   : standardMic),
                  4)) {
    return false;
  }

  pseudoEncrypt(header,
                encryptionStartOffset,
                -pseudoEncryptOffset);

  if (payload != header) {
    pseudoEncrypt(payload, 0, -pseudoEncryptOffset);
  }

  sl_legacy_buffer_manager_set_linked_buffers_length(payload, payloadLength - 4);

  return true;
}

#endif

//------------------------------------------------------------------------------

void sli_simulator_set_encryption(SimulatorEncryption type)
{
  simulatorEncryptionType = type;
}

//------------------------------------------------------------------------------
// Encrypting a parcel using pseudo encryption.

sli_parcel_t *encryptParcelWithSpecialMic(sli_parcel_t *clearText, PseudoEncryptMicType type)
{
  // Currently this test code doesn't support real encryption.
  assert(simulatorEncryptionType != USE_REAL_ENCRYPTION);

  return appendParcels((simulatorEncryptionType == USE_PSEUDO_ENCRYPTION
                        ? encryptParcelNoMic(clearText)
                        : clearText),
                       constructMicParcel(type == USE_TRANSIENT_KEY_MIC
                                          ? USE_TRANSIENT_KEY_MIC
                                          : USE_STANDARD_MIC),
                       NULL);
}

sli_parcel_t *encryptParcel(sli_parcel_t* clearText)
{
  return encryptParcelWithSpecialMic(clearText, USE_STANDARD_MIC);
}

sli_parcel_t *encryptParcelNoMic(sli_parcel_t *clearText)
{
  // Currently this test code doesn't support real encryption.
  assert(simulatorEncryptionType != USE_REAL_ENCRYPTION);

  if ( simulatorEncryptionType == USE_NO_ENCRYPTION ) {
    return clearText;
  }

  sli_parcel_t *encrypted = makeParcel(clearText->length);
  int i;
  for (i = 0; i < clearText->length; i++) {
    encrypted->contents[i] = clearText->contents[i] + pseudoEncryptOffset;
  }
  return encrypted;
}

sli_parcel_t *constructMicParcel(PseudoEncryptMicType type)
{
  return makeMessage("s",
                     (type == USE_TRANSIENT_KEY_MIC
                      ? transientLinkKeyMic
                      : standardMic),
                     4);
}
