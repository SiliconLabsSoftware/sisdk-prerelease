/***************************************************************************//**
 * @file
 * @brief Unit test for the cryptography routines.
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
// For unit tests, the configuration default overrides must be before
// the #include of sl_zigbee.h or sl_zigbee_stack.h
//
// Not really, but this avoids the need for stubs.
#define SL_ZIGBEE_APPLICATION_HAS_GET_ENDPOINT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/sl_zigbee_stack.h"

#include "include/error.h"
#include "framework/packet-header.h"
#include "framework/zigbee-packet-header.h"
#include "phy/simulation/security.h"
#include "stack/platform/micro/aes.h"

#include "security.h"

// Unit tests cannot have seperate CONFIGURATION_HEADERs so they
//  include sl_zigbee_configuration.c directly
#include "hal/sl_zigbee_configuration.c"

sl_802154_long_addr_t sli_802154mac_local_eui64 = { 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7 };

uint8_t *theKey = (uint8_t*) "ABCDEFGHIJKLMNOP";

//----------------------------------------------------------------

static uint8_t *thePayload = (uint8_t*)
                             "Lorem ipsum dolor sit amet, consectetur adipisicing elit,"
                             " sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";

// For the network tests we use the same frame counter and source
// address in each test, so the authenticated portion of the header
// is always the same.  The encrypted payload is then the same as
// well.  All that changes from test to test is the length of the
// encrypted payload and its distribution between the header and
// the payload buffer.

uint8_t nwkEncrypted[] = {
  // This is the authenticated portion.
  0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
  0x28, 0x01, 0x02, 0x03, 0x04, 0xA0, 0xA1, 0xA2,
  0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0x05,

  // This is 80 bytes of encrypted thePayload.
  0xBE, 0x01, 0x11, 0xC9, 0x2B, 0x8E, 0xEA, 0x6D,
  0x2E, 0x5A, 0xF2, 0x96, 0x74, 0x15, 0xAD, 0x34,
  0x65, 0x1E, 0x9B, 0x95, 0x3B, 0xE4, 0xAA, 0xE4,
  0xBE, 0x1C, 0x7E, 0x4D, 0x49, 0x2F, 0x36, 0x5A,
  0xF6, 0xF5, 0x2D, 0xBA, 0x1E, 0xC7, 0x43, 0xC5,
  0xAB, 0x52, 0x4D, 0x3C, 0x8A, 0xB6, 0x01, 0xAD,
  0x79, 0xD7, 0xDD, 0xDF, 0xA9, 0x35, 0x3E, 0x9B,
  0xFA, 0x30, 0x19, 0x05, 0x34, 0xE3, 0x6F, 0x45,
  0xDD, 0x41, 0x43, 0x3A, 0x14, 0xD5, 0xE9, 0x2A,
  0x19, 0x47, 0x90, 0xD5, 0x88, 0x6E, 0x73, 0xA8
};

// The MICs for network messages with payloads of 0 to 79 bytes.

uint32_t nwkMics[] = {
  0xFFBFC64D, 0x8635A633, 0xC0B48F1D, 0xFC90BA74,
  0x2B0EA380, 0x5937AF37, 0x78F01C19, 0x8B25DB69,
  0x982CE9E9, 0xA5DC693F, 0xB3201888, 0x9DE1073F,
  0x4F425A00, 0xF6CCDF02, 0x2DAAA88A, 0xD1259B3A,
  0x1E0895A1, 0xD6052D8D, 0xC9580E9B, 0x8DE0C9FF,
  0xA747FA05, 0xBD6DD049, 0x23E4CCEF, 0x986ACB50,
  0x2E2CD3D4, 0xC43A0AD1, 0xB138FC38, 0x5CA2B70B,
  0x4C045C82, 0xA198646B, 0x5020615E, 0x5AD2339E,
  0x0957519C, 0x19F0E468, 0xE3A59050, 0x0A09ABCF,
  0x3353B1CD, 0x8ABD1177, 0x3199FFCD, 0x9655621E,
  0x6E41969D, 0xCFF5E204, 0xFAAB3F49, 0xB0874BFA,
  0x7CABB3DB, 0x123A5771, 0xE665F065, 0x1FDD43A3,
  0x19F1B7E2, 0xB7E14F67, 0x2FA69227, 0x64B7F265,
  0xADE469E0, 0x044144AA, 0x557F63B9, 0x5A913792,
  0x503FA2A1, 0xC734D442, 0x0116383B, 0x0B6376B3,
  0xF8986969, 0x9BA605F2, 0x564932FA, 0xB6D11156,
  0x699B8A4C, 0x18682743, 0x94C676BD, 0x7B5BF61F,
  0x55C25552, 0xC01AD383, 0x11AD1A50, 0x0CED3EBE,
  0xFF9543C5, 0x51E85716, 0x4714136E, 0xC8C7F848,
  0x3E37AD88, 0x3F176491, 0xCD9557EB, 0x51908C86
};

uint8_t apsEncrypted[] = {
  // This is the authenticated portion.
  0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
  0x00, 0x01, 0x02, 0x03, 0x04,

  // This is 80 bytes of encrypted thePayload.
  0x84, 0xCF, 0x16, 0x04, 0x6E, 0x2B, 0x02, 0x17,
  0x0C, 0x41, 0x0A, 0x57, 0xBE, 0x71, 0xFC, 0x4F,
  0x76, 0x5C, 0x71, 0xDF, 0xB7, 0xA4, 0xFB, 0x9E,
  0xD6, 0xD8, 0xA3, 0x0B, 0xB9, 0x69, 0x2D, 0x2E,
  0x9E, 0x52, 0x60, 0xE6, 0xF6, 0xB0, 0xA6, 0x6A,
  0xD7, 0x1D, 0x8E, 0x35, 0xAC, 0xFF, 0xA6, 0x20,
  0x78, 0xA7, 0xED, 0x0B, 0xAD, 0x0E, 0x69, 0xD3,
  0x61, 0xCD, 0x49, 0x67, 0xB0, 0x0C, 0xB3, 0xEC,
  0x27, 0x0C, 0xDF, 0x14, 0x66, 0x31, 0xD0, 0x9C,
  0x16, 0x13, 0x76, 0x0D, 0x4A, 0x26, 0xEF, 0xB4
};

// The MICs for APS messages with payloads of 0 to 79 bytes.

uint32_t apsMics[] = {
  0xBA2966B9, 0x1904C179, 0xA4DFC019, 0x68A5727A,
  0x5D51EC1C, 0x8C51F0AB, 0x7CC11F33, 0xFA587A1E,
  0x7CCB8208, 0x15571884, 0x6E154D5C, 0x4B1DD2F9,
  0x1005422F, 0x8C86B422, 0x4AA2E08B, 0x91412A81,
  0xF88F92C4, 0xA47A8A14, 0x3E28D7EA, 0x58FEBA66,
  0x2B30B799, 0x433498E7, 0xED5C4F18, 0x9482DA7A,
  0xABBD707B, 0xC1A729C6, 0x020F11EA, 0x08DBE7B3,
  0x0DBEBE23, 0x36B306F1, 0x253A3F46, 0x29DFCC40,
  0x44431985, 0x0E375A90, 0x0A33C5BA, 0x82AAD94F,
  0x2AAF186E, 0x588066C9, 0xE27C03DC, 0xD1A40446,
  0xDE77EDE4, 0xC0DB3DCF, 0xD5FAEAEC, 0xA627DEBB,
  0xE093CFC9, 0x7256DE5E, 0x74FDF921, 0x1C8764EB,
  0x84F8CC75, 0x7F49D13D, 0xE941345D, 0xDDCED398,
  0xCB029EF0, 0xBB8C4F4D, 0xE3F95731, 0xB3F0CFB2,
  0xDA92496A, 0x2266BD80, 0xF1DD5647, 0x2A2B982C,
  0x51463C76, 0xBF631840, 0x4C2C9924, 0x078E3989,
  0x7F249C6A, 0x00990CAA, 0x41C09340, 0x2140EF82,
  0x2C7A3CCC, 0x6E6AA5CA, 0xC701B708, 0x002A49E9,
  0xE5204C97, 0xDEF0F494, 0x1439B993, 0x8D73C290,
  0x5476676F, 0x059D8E5D, 0xACE39EE3, 0xCB541EA0
};

//----------------------------------------------------------------
// Adding a network frame - the contents don't matter, so long as
// they are constant.  We should really check for multiple frame
// sizes as well.

static void appendNetworkFrame(sli_buffer_manager_buffer_t buffer)
{
  uint8_t nwkFrame[ZIGBEE_NETWORK_FRAME_MINIMUM_SIZE];
  int i;

  for (i = 0; i < ZIGBEE_NETWORK_FRAME_MINIMUM_SIZE; i++) {
    nwkFrame[i] = 0xB0 + i;
  }
  assert(sl_legacy_buffer_manager_append_to_linked_buffers(buffer,
                                                           nwkFrame,
                                                           ZIGBEE_NETWORK_FRAME_MINIMUM_SIZE)
         == SL_STATUS_OK);
}

// Generic procedure for adding the auxilliary frame.

static void appendAuxFrame(sli_buffer_manager_buffer_t buffer,
                           uint8_t keyType,
                           uint32_t counter,
                           sl_802154_long_addr_t sourceAddress,
                           uint8_t keySequence)
{
  uint8_t auxFrame[ZIGBEE_NETWORK_SECURITY_SUBFRAME_SIZE];
  uint8_t size = (ZIGBEE_NETWORK_SECURITY_SUBFRAME_SIZE
                  - (sourceAddress == NULL
                     ? 8
                     : 0)
                  - (keyType == ZIGBEE_SECURITY_CONTROL_NETWORK_KEY
                     ? 0
                     : 1));

  sli_zigbee_security_frame_control(auxFrame) =
    keyType
    | ZIGBEE_SECURITY_ENC_MIC_32
    | (sourceAddress == NULL
       ? 0
       : ZIGBEE_SECURITY_CONTROL_HAVE_EXTENDED_NONCE);
  sli_zigbee_security_frame_counter(auxFrame)[0] = (counter >> 24) & 0xFF;
  sli_zigbee_security_frame_counter(auxFrame)[1] = (counter >> 16) & 0xFF;
  sli_zigbee_security_frame_counter(auxFrame)[2] = (counter >>  8) & 0xFF;
  sli_zigbee_security_frame_counter(auxFrame)[3] = (counter >>  0) & 0xFF;
  if (sourceAddress != NULL) {
    memcpy(sli_zigbee_security_frame_source_address(auxFrame),
           sli_802154mac_local_eui64,
           8);
  }
  sli_zigbee_security_frame_key_sequence(auxFrame) = keySequence;

  assert(sl_legacy_buffer_manager_append_to_linked_buffers(buffer, auxFrame, size)
         == SL_STATUS_OK);
}

// Add the first 'headerBytes' of 'thePayload' to 'header' and
// add a payload buffer containing the next 'payloadBytes' of
// 'thePayload' to 'header'.

static void addPayload(sli_buffer_manager_buffer_t header,
                       uint8_t headerBytes,
                       uint8_t payloadBytes)
{
  if (0 < headerBytes) {
    assert(sl_legacy_buffer_manager_append_to_linked_buffers(header, thePayload, headerBytes)
           == SL_STATUS_OK);
  }
  if (0 < payloadBytes) {
    sli_buffer_manager_buffer_t payload
      = sl_legacy_buffer_manager_fill_linked_buffers(thePayload + headerBytes, payloadBytes);
    assert(payload != SL_ZIGBEE_NULL_MESSAGE_BUFFER);
    sli_legacy_packet_buffer_set_packet_header_payload(header, payload);
    sl_legacy_buffer_manager_release_message_buffer(payload);
  }
}

// Verify that 'buffer' contains, starting from 'startOffset' the
// 'contentsLength' bytes in 'contents'.

static void checkBufferContents(sli_buffer_manager_buffer_t buffer,
                                uint8_t startOffset,
                                uint8_t *contents,
                                uint8_t contentsLength)
{
  int i;
  for (i = 0; i < contentsLength; i++) {
    assert(sl_legacy_buffer_manager_get_linked_buffers_byte(buffer, startOffset + i)
           == contents[i]);
  }
}

// Verify that 'mic' is found at 'offset' in 'buffer'.

static void checkMic(sli_buffer_manager_buffer_t buffer,
                     uint8_t offset,
                     uint32_t mic)
{
  uint32_t have = 0;
  int i;

  for (i = 0; i < 4; i++) {
    have = (have << 8) | sl_legacy_buffer_manager_get_linked_buffers_byte(buffer, offset + i);
  }

  if (mic != have) {
    printf("have %08X want %08X\n", have, mic);
  }
  assert(mic == have);
  //fprintf(stderr, "  0x%08X,\n", have);
}

// This tests encryption and decryption with the given number of payload
// bytes in the header and payload.  'encryptedContents' is the expected
// result of encryption, and 'mic' is the expected MIC value.

static void cryptoTest(uint8_t keyType,
                       bool extendedNonce,
                       uint8_t headerBytes,
                       uint8_t payloadBytes,
                       uint8_t *encryptedContents,
                       uint32_t mic)
{
  sli_zigbee_packet_header_t header;
  sli_buffer_manager_buffer_t payload;
  uint8_t authenticationStartOffset;
  uint8_t auxFrameOffset;
  uint8_t encryptionStartOffset;
  uint8_t authenticationLength;

//  fprintf(stderr, "\nnew test %d %d\n", headerBytes, payloadBytes);

  header = sli_zigbee_make_data_packet_header(0,
                                              BROADCAST_ADDRESS,
                                              SL_ZIGBEE_NULL_MESSAGE_BUFFER);
  assert(header != SL_ZIGBEE_NULL_MESSAGE_BUFFER);

  authenticationStartOffset = sl_legacy_buffer_manager_message_buffer_length(header);

  appendNetworkFrame(header);

  auxFrameOffset = sl_legacy_buffer_manager_message_buffer_length(header);

  appendAuxFrame(header,
                 keyType,
                 0x01020304,
                 extendedNonce ? sli_802154mac_local_eui64 : NULL,
                 0x05);

  encryptionStartOffset = sl_legacy_buffer_manager_message_buffer_length(header);

  addPayload(header, headerBytes, payloadBytes);

  payload = sli_legacy_packet_buffer_packet_header_payload(header);

  // To check sli_zigbee_network_encryptFlatPacket() we have to make a flat copy of
  // the packet.  The two encryptions should yield the same result.
  if (keyType == ZIGBEE_SECURITY_CONTROL_NETWORK_KEY) {
    uint8_t flatPacket[128];
    uint8_t headerLength = sl_legacy_buffer_manager_message_buffer_length(header);
    uint8_t i;

    sl_legacy_buffer_manager_copy_from_linked_buffers(header, 0, flatPacket, headerLength);
    if (payload != SL_ZIGBEE_NULL_MESSAGE_BUFFER) {
      sl_legacy_buffer_manager_copy_from_linked_buffers(payload,
                                                        0,
                                                        flatPacket + headerLength,
                                                        payloadBytes);
    }

    assert(sli_zigbee_encrypt_packet(header, authenticationStartOffset, auxFrameOffset));
    sli_zigbee_network_encryptFlatPacket(flatPacket,
                                         headerLength + payloadBytes,
                                         authenticationStartOffset,
                                         auxFrameOffset);

    for (i = 0; i < sl_legacy_buffer_manager_message_buffer_length(header); i++) {
      assert(flatPacket[i]
             == sl_legacy_buffer_manager_get_linked_buffers_byte(header, i));
    }
    if (payload != SL_ZIGBEE_NULL_MESSAGE_BUFFER) {
      for (i = 0; i < payloadBytes + 4; i++) {          // check MIC as well
        assert(flatPacket[headerLength + i]
               == sl_legacy_buffer_manager_get_linked_buffers_byte(payload, i));
      }
    }
  } else {
    assert(sli_zigbee_encrypt_packet(header, authenticationStartOffset, auxFrameOffset));
  }

//  printPacketBuffers(header);

  authenticationLength =
    encryptionStartOffset - authenticationStartOffset;

  checkBufferContents(header,
                      authenticationStartOffset,
                      encryptedContents,
                      authenticationLength + headerBytes);
  if (payloadBytes == 0) {
    checkMic(header, encryptionStartOffset + headerBytes, mic);
  } else {
    checkBufferContents(payload,
                        0,
                        encryptedContents + authenticationLength + headerBytes,
                        payloadBytes);
    checkMic(payload, payloadBytes, mic);
  }

  assert(sli_zigbee_decrypt_packet(header,
                                   authenticationStartOffset,
                                   auxFrameOffset,
                                   sli_802154mac_local_eui64));

  checkBufferContents(header,
                      authenticationStartOffset,
                      encryptedContents,
                      authenticationLength);
  checkBufferContents(header,
                      authenticationStartOffset + authenticationLength,
                      thePayload,
                      headerBytes);
  if (payloadBytes != 0) {
    checkBufferContents(payload,
                        0,
                        thePayload + headerBytes,
                        payloadBytes);
  }

  sl_legacy_buffer_manager_release_message_buffer(header);
}

const bool myEncryption = false;
static uint8_t myKey[] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
};
static uint8_t *myPayload = (uint8_t*)
                            "One Ring to bring them all and in the darkness bind them. "
                            "In the Land of Mordor where the Shadows lie.";

// Runs APS encryption only using the hard coded APS + AUX frame, and
// using 'payloadBytes' of the 'myPayload'
// Returns the 32-bit MIC.  It is used to create MICs for testing
// 'bin/ccm-test', which is CCM* run up on the host.
static uint32_t myCryptoTest(uint8_t payloadBytes, bool printCompleteMessage)
{
  uint8_t mic[4];
  static uint8_t headerData[] = {
    // This is the APS header
    //   FC (1-byte), destEP (1-byte), Cluster (2-bytes), Profile (2-bytes),
    //   srcEP (1-byte), APS counter (1-byte)
    0x60, 0x20, 0x07, 0x01, 0x01, 0x09, 0x01, 0x0B,

    // This the auxiliary header
    //   FC (1-byte), Frame Counter (4-bytes)
    0x00, 0x00, 0x12, 0x34, 0x56,

    // Payload appended later
  };
  uint8_t auxFrameOffset = 8;
  uint8_t payloadLength;
  uint8_t completeMessage[150];
  uint8_t headerLength;

  sli_buffer_manager_buffer_t payload;
  sli_zigbee_packet_header_t header;

  if (payloadBytes != 0) {
    payload = sl_legacy_buffer_manager_fill_linked_buffers(myPayload, payloadBytes);
    assert(payload != SL_ZIGBEE_NULL_MESSAGE_BUFFER);
  }
  header = sli_zigbee_make_data_packet_header(0,
                                              0x0000,
                                              (payloadBytes == 0
                                               ? SL_ZIGBEE_NULL_MESSAGE_BUFFER
                                               : payload));
  assert(header != SL_ZIGBEE_NULL_MESSAGE_BUFFER);
  headerLength = sl_legacy_buffer_manager_message_buffer_length(header);

  assert(SL_STATUS_OK == sl_legacy_buffer_manager_append_to_linked_buffers(header,
                                                                           headerData,
                                                                           sizeof(headerData)));
  if (payloadBytes != 0) {
    sl_legacy_buffer_manager_release_message_buffer(payload);
  }

  assert(true == sli_zigbee_encrypt_packet(header,
                                           headerLength, // authentication start index
                                           headerLength + auxFrameOffset));
  payloadLength = (payloadBytes == 0
                   ? 0
                   : sl_legacy_buffer_manager_message_buffer_length(payload));
  headerLength = sl_legacy_buffer_manager_message_buffer_length(header);
  sl_legacy_buffer_manager_copy_from_linked_buffers(payloadBytes
                                                    ? payload
                                                    : header,
                                                    ((payloadBytes
                                                      ? payloadLength
                                                      : headerLength)
                                                     - 4),
                                                    mic,
                                                    4);
  sl_legacy_buffer_manager_copy_from_linked_buffers(header,
                                                    0,
                                                    completeMessage,
                                                    headerLength);
  if (payloadLength) {
    sl_legacy_buffer_manager_copy_from_linked_buffers(payload,
                                                      0,
                                                      completeMessage + headerLength,
                                                      payloadLength);
    sl_legacy_buffer_manager_release_message_buffer(header);
  }

  if (printCompleteMessage) {
    uint8_t i;
    printf("\n");
    for (i = 0; i < (headerLength + payloadLength); i++) {
      fprintf(stderr, "%0x ", completeMessage[i]);
      if (i % 16 == 0) {
        printf("\n");
      }
    }
    printf("\n");
  }

  return (mic[0]
          + +((uint32_t)mic[1] << 8)
          + ((uint32_t)mic[2] << 16)
          + ((uint32_t)mic[3] << 24));
}

// Test various distributions of payload between the header and the payload,
// with and without an extended nonce. The MIC is different in each case.

int main(void)
{
  printf("[Testing CCM* ");

  simulatorEncryptionType = USE_REAL_ENCRYPTION;

  if (myEncryption) {
    uint8_t i;
    sli_util_load_key_into_core(myKey);
    printf("\n");
    for (i = 0; i <= strlen((char *) myPayload); i++) {
      uint32_t mic = myCryptoTest(i, false);
      printf("%2d bytes has a MIC of 0x%08X,\n", i, mic);
    }
    return 0;
  }

  sli_zigbee_security_init();
  sli_util_load_key_into_core(theKey);

  {
    uint8_t i;

    for (i = 0; i < 80; i++) {
      uint8_t j;
      for (j = 0; j <= i; j++) {
        cryptoTest(ZIGBEE_SECURITY_CONTROL_NETWORK_KEY,
                   true,
                   j,
                   i - j,
                   nwkEncrypted,
                   nwkMics[i]);
      }
    }
    printf(".");

    for (i = 0; i < 80; i++) {
      uint8_t j;
      for (j = 0; j <= i; j++) {
        cryptoTest(ZIGBEE_SECURITY_CONTROL_LINK_KEY,
                   false,
                   j,
                   i - j,
                   apsEncrypted,
                   apsMics[i]);
      }
    }

    printf(".");
  }

  printf(" done]\n");

  return 0;
}
