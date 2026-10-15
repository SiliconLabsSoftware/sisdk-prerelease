/***************************************************************************//**
 * @file
 * @brief Routines built on PSA Crypto AEAD (CCM) functions used by the GPD.
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
#include "gpd-components-common.h"

#define NONCE_SIZE                          13
#define GPD_CCM_KEY_BITS                    128
#define GPD_CCM_KEY_BYTES                   16
#define GPD_CCM_MAX_PAYLOAD                 80
#define GPD_CCM_ALG PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM, TAG_SIZE)

#define ZIGBEE_SECURITY_CONTROL               0x05 // For all Tx and Rx for AppId = 0
#define ZIGBEE_SECURITY_CONTROL_IEEE_RX       0xC5 // For Rx with AppId = 2

// ZigBee Alliance Pre-configured TC Link Key - 'ZigBeeAlliance09'
static const uint8_t keyDefaultTCLink[] = {
  0x5a, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6c,
  0x6c, 0x69, 0x61, 0x6e, 0x63, 0x65, 0x30, 0x39
};

static uint8_t nonceTx[NONCE_SIZE];
static uint8_t nonceRx[NONCE_SIZE];
static psa_key_id_t gpd_ccm_key_id = 0;

static int8_t gpd_psa_status_to_result(psa_status_t status)
{
  return (status == PSA_SUCCESS) ? SUCCESS : FAILED;
}

static int8_t gpd_ccm_encrypt_and_tag(const uint8_t *nonce,
                                      const uint8_t *ad,
                                      size_t ad_len,
                                      const uint8_t *plaintext,
                                      size_t plaintext_len,
                                      uint8_t *ciphertext,
                                      uint8_t *tag)
{
  uint8_t output[GPD_CCM_MAX_PAYLOAD + TAG_SIZE];
  size_t output_length = 0;
  const uint8_t empty = 0;
  psa_status_t status;

  if (plaintext_len > GPD_CCM_MAX_PAYLOAD) {
    return FAILED;
  }

  status = psa_aead_encrypt(gpd_ccm_key_id,
                            GPD_CCM_ALG,
                            nonce,
                            NONCE_SIZE,
                            ad,
                            ad_len,
                            (plaintext_len > 0) ? plaintext : &empty,
                            plaintext_len,
                            output,
                            plaintext_len + TAG_SIZE,
                            &output_length);
  if (status != PSA_SUCCESS || output_length != (plaintext_len + TAG_SIZE)) {
    return FAILED;
  }

  if ((plaintext_len > 0) && (ciphertext != NULL)) {
    (void)memcpy(ciphertext, output, plaintext_len);
  }
  if (tag != NULL) {
    (void)memcpy(tag, &output[plaintext_len], TAG_SIZE);
  }
  return SUCCESS;
}

static int8_t gpd_ccm_auth_decrypt(const uint8_t *nonce,
                                   const uint8_t *ad,
                                   size_t ad_len,
                                   const uint8_t *ciphertext,
                                   size_t ciphertext_len,
                                   const uint8_t *tag,
                                   uint8_t *plaintext)
{
  uint8_t input[GPD_CCM_MAX_PAYLOAD + TAG_SIZE];
  uint8_t output[GPD_CCM_MAX_PAYLOAD];
  size_t output_length = 0;
  psa_status_t status;

  if (ciphertext_len > GPD_CCM_MAX_PAYLOAD) {
    return FAILED;
  }

  (void)memcpy(input, ciphertext, ciphertext_len);
  (void)memcpy(&input[ciphertext_len], tag, TAG_SIZE);

  status = psa_aead_decrypt(gpd_ccm_key_id,
                            GPD_CCM_ALG,
                            nonce,
                            NONCE_SIZE,
                            ad,
                            ad_len,
                            input,
                            ciphertext_len + TAG_SIZE,
                            output,
                            sizeof(output),
                            &output_length);
  if (status != PSA_SUCCESS || output_length != ciphertext_len) {
    return FAILED;
  }

  (void)memcpy(plaintext, output, ciphertext_len);
  return SUCCESS;
}

static int8_t gpdEncryptTagKeyWrapper(uint8_t* data,
                                      uint8_t* encryptedOut,
                                      uint8_t* mic)
{
  return gpd_ccm_encrypt_and_tag(nonceTx,
                                 data,
                                 4,
                                 &data[4],
                                 GPD_CCM_KEY_BYTES,
                                 encryptedOut,
                                 mic);
}

static int8_t gpdAuthDecryptKeyWrapper(uint8_t* data,
                                       uint8_t* decryptedOut)
{
  return gpd_ccm_auth_decrypt(nonceRx,
                              data,
                              4,
                              &data[4],
                              GPD_CCM_KEY_BYTES,
                              &data[20],
                              decryptedOut);
}

void sl_zigbee_gpd_set_fc_into_tx_nonce(uint32_t value)
{
  sl_zigbee_gpd_utility_copy_4_bytes(&nonceTx[8], value);
}

void sl_zigbee_gpd_set_fc_into_rx_nonce(uint32_t value)
{
  sl_zigbee_gpd_utility_copy_4_bytes(&nonceRx[8], value);
}

int8_t sl_zigbee_gpd_mbedtls_ccm_setkey_wraper(const uint8_t* key)
{
  psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
  psa_status_t status;

  if (gpd_ccm_key_id != 0) {
    (void)psa_destroy_key(gpd_ccm_key_id);
    gpd_ccm_key_id = 0;
  }

  psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
  psa_set_key_bits(&attributes, GPD_CCM_KEY_BITS);
  psa_set_key_algorithm(&attributes, GPD_CCM_ALG);
  psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);

  status = psa_import_key(&attributes, key, GPD_CCM_KEY_BYTES, &gpd_ccm_key_id);
  psa_reset_key_attributes(&attributes);
  return gpd_psa_status_to_result(status);
}

int8_t sl_zigbee_tag_tx_buff(uint8_t* data,
                             uint8_t length,
                             uint8_t authStart,
                             uint8_t authLength)
{
  return gpd_ccm_encrypt_and_tag(nonceTx,
                                 &(data[authStart]),
                                 authLength,
                                 NULL,
                                 0,
                                 NULL,
                                 (data + length));
}

int8_t sl_zigbee_encrypt_and_tag_tx_buff(uint8_t* data,
                                         uint8_t length,
                                         uint8_t payloadLength,
                                         uint8_t authStart,
                                         uint8_t authLength)
{
  uint8_t tempDataOut[GPD_CCM_MAX_PAYLOAD];
  int8_t status;

  status = gpd_ccm_encrypt_and_tag(nonceTx,
                                   &(data[authStart]),
                                   authLength,
                                   &(data[authStart + authLength]),
                                   payloadLength,
                                   tempDataOut,
                                   (data + length));
  if (status == SUCCESS) {
    (void)memcpy(&(data[authStart + authLength]), tempDataOut, payloadLength);
  }
  return status;
}

int8_t sl_zigbee_gpd_security_encrypt_key(sl_zigbee_gpd_addr_t_t * addr,
                                          const uint8_t* oobKey,
                                          uint8_t* oobScured,
                                          uint8_t* mic)
{
  // temporary input buffer to hold a = (header || key)
  uint8_t data[20];
  uint32_t frameCounter;

  if (addr->appId == SL_ZIGBEE_GPD_APP_ID_IEEE_ID) {
    // Copy IEEE address in work buffer
    (void) memcpy(data, addr->id.ieee, 4);
    // set Security Frame Counter = 4 LSB IEEE
    frameCounter = (uint32_t)(*((uint32_t*)addr->id.ieee));
  } else {
    // Copy SrcID address in work buffer
    (void) memcpy(data, (uint8_t *)(&(addr->id.srcId)), 4);
    // set Security Frame Counter = SrcID
    frameCounter = addr->id.srcId;
  }
  // Copy OOb Key to be secured in the input buffer
  (void) memcpy(&data[4], oobKey, 16);

  // Update Nonce TX
  sl_zigbee_gpd_set_fc_into_tx_nonce(frameCounter);
  // Secure OOB key data with default TC_LK ZA09
  sl_zigbee_gpd_mbedtls_ccm_setkey_wraper(keyDefaultTCLink);
  return gpdEncryptTagKeyWrapper(data, oobScured, mic);
}

#if TEST_KEY_DECRYPTION
//set the following as input data and the decrypted keys will be [c0 c1...cf]
uint8_t decryptedKey[16] = { 0x88 };
uint8_t inData[24] = { 0x78, 0x56, 0x34, 0x12, 0xE9, 0x00, 0x06, 0x63, 0x1D, 0x0D, 0xFD, 0xC6, 0x38, 0x06, 0x8E, 0x5E, 0x69, 0x67, 0xD3, 0x25, 0x27, 0x55, 0x9F, 0x75 };
uint8_t inFc[4] = { 0x04, 0x00, 0x00, 0x00 };
#endif
int8_t sl_zigbee_gpd_security_decrypt_key(sl_zigbee_gpd_addr_t_t * addr,
                                          uint8_t * pKeyEncrypted,
                                          uint8_t * pKeyMic,
                                          uint8_t * pSecCounter,
                                          uint8_t * decryptedKey)
{
  // local variables
  uint8_t inData[24];
  uint32_t frameCounter = *((uint32_t *)pSecCounter);

  // Copy SrcID / IEEE + OOB Key in work buffer
  if (addr->appId == SL_ZIGBEE_GPD_APP_ID_IEEE_ID) {
    (void) memcpy(inData, addr->id.ieee, 4);
  } else {
    (void) memcpy(&inData[0], &(addr->id.srcId), 4);
  }

  (void) memcpy(&inData[4], pKeyEncrypted, 16);
  (void) memcpy(&inData[20], pKeyMic, 4);

  // Update Nonce RX with Rx Security Counter
  sl_zigbee_gpd_set_fc_into_rx_nonce(frameCounter);
  // Set security Key with default TC_LK ZA09
  sl_zigbee_gpd_mbedtls_ccm_setkey_wraper(keyDefaultTCLink);
  return gpdAuthDecryptKeyWrapper(inData, decryptedKey);
}

int8_t sl_zigbee_decrypt_rx_buffer(uint8_t *rxMpdu,
                                   uint8_t authStart,
                                   uint8_t encrLength,
                                   uint8_t authLength)
{
  uint8_t tempDataOut[GPD_CCM_MAX_PAYLOAD];
  int8_t status;

  status = gpd_ccm_auth_decrypt(nonceRx,
                                &rxMpdu[authStart],
                                authLength,
                                &rxMpdu[authStart + authLength],
                                encrLength,
                                &rxMpdu[authStart + authLength + encrLength],
                                tempDataOut);
  if (status == SUCCESS) {
    (void)memcpy(&rxMpdu[authStart + authLength], tempDataOut, encrLength);
  }
  return status;
}

void sl_zigbee_gpd_security_init(sl_zigbee_gpd_addr_t_t * addr,
                                 uint8_t* securityKey,
                                 uint32_t frameCounter)
{
  (void)psa_crypto_init();

  if (gpd_ccm_key_id != 0) {
    (void)psa_destroy_key(gpd_ccm_key_id);
    gpd_ccm_key_id = 0;
  }

  // Initialise nonce bytes to 0
  for (int i = 0; i < NONCE_SIZE; i++) {
    nonceRx[i] = 0;
    nonceTx[i] = 0;
  }
  // Tx Nonce set up
  if (addr->appId == SL_ZIGBEE_GPD_APP_ID_IEEE_ID) {
    (void) memcpy(&nonceTx[0], addr->id.ieee, 8);
    nonceTx[12] = ZIGBEE_SECURITY_CONTROL;
    // Rx Nonce for IEEE (outgoing from proxy uses IEEE address)
    (void) memcpy(&nonceRx[0], addr->id.ieee, 8);
    nonceRx[12] = ZIGBEE_SECURITY_CONTROL_IEEE_RX;
  } else {
    sl_zigbee_gpd_utility_copy_4_bytes(&nonceTx[0], addr->id.srcId);
    sl_zigbee_gpd_utility_copy_4_bytes(&nonceTx[4], addr->id.srcId);
    nonceTx[12] = ZIGBEE_SECURITY_CONTROL;
    // Rx Nonce for SrcId
    sl_zigbee_gpd_utility_copy_4_bytes(&nonceRx[0], 0);
    sl_zigbee_gpd_utility_copy_4_bytes(&nonceRx[4], addr->id.srcId);
    nonceRx[12] = ZIGBEE_SECURITY_CONTROL;
  }

  // Set the Frane counters in the Nonce
  sl_zigbee_gpd_set_fc_into_tx_nonce(frameCounter);
  sl_zigbee_gpd_set_fc_into_rx_nonce(frameCounter);
  // Load the key into the PSA key store for CCM
  sl_zigbee_gpd_mbedtls_ccm_setkey_wraper(securityKey);
}
