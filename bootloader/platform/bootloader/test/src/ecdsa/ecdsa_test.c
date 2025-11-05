// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include "em_chip.h"
#include "security/btl_security_types.h"
#include "security/btl_security_ecdsa.h"
#include "security/btl_security_sha256.h"
#include "mbedtls/sha1.h"
#include "sl_memory_manager.h"

#include <stdlib.h>
#include <string.h>

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

#if defined(SEMAILBOX_PRESENT) || defined(CRYPTOACC_PRESENT)
// SE does not give detailed info about rejection
  #define EXPECTED_ERROR_RANGE  BOOTLOADER_ERROR_SECURITY_REJECTED
#else
// CRYPTO tells us that parameter validation failed
  #define EXPECTED_ERROR_RANGE  BOOTLOADER_ERROR_SECURITY_PARAM_OUT_RANGE
#endif

// Function under test
// int32_t btl_verifyEcdsaP256r1(const uint8_t *sha256,
//                               const uint8_t *signatureR,
//                               const uint8_t *signatureS,
//                               const uint8_t *keyX,
//                               const uint8_t *keyY)

void test_simple_invalid_pointers(void)
{
  uint8_t sha[32] = {
    0xaa, 0xaf, 0x71, 0x56, 0x61, 0x62, 0x1d, 0x0b,
    0xd8, 0x5d, 0xc7, 0xc1, 0x15, 0x8c, 0xe7, 0x1a,
    0xdb, 0x4c, 0xc9, 0x6c, 0xef, 0xfa, 0x10, 0xd8,
    0xbb, 0x86, 0xa5, 0x8d, 0xa9, 0x11, 0x45, 0xa2
  };
  uint8_t r[32] = {
    0xb4, 0x03, 0xad, 0x46, 0x51, 0x05, 0xd1, 0x9e,
    0xed, 0x8b, 0x52, 0x6d, 0x09, 0x64, 0xee, 0xc7,
    0x7e, 0x61, 0x54, 0xc4, 0x20, 0x07, 0x67, 0x64,
    0xee, 0xb1, 0x8c, 0xdc, 0x1d, 0xf0, 0x4d, 0x44,
  };
  uint8_t s[32] = {
    0xf2, 0xad, 0x85, 0x99, 0xee, 0x4b, 0x32, 0xc8,
    0x4f, 0x59, 0x8a, 0x74, 0xa7, 0x44, 0x03, 0xe2,
    0x9e, 0xa8, 0xea, 0xa7, 0x42, 0x48, 0xa7, 0x58,
    0x6a, 0xa5, 0x16, 0x5b, 0x97, 0xf0, 0xcb, 0xa1,
  };
  uint8_t x[32] = {
    0x28, 0x3d, 0x7f, 0x16, 0xf3, 0xc8, 0x26, 0x91,
    0x95, 0x86, 0x45, 0x70, 0x8b, 0xbd, 0x7a, 0x98,
    0x81, 0x28, 0x44, 0xc6, 0x3d, 0x74, 0x53, 0xb6,
    0xda, 0x70, 0x3d, 0xcd, 0xd2, 0x0f, 0x84, 0xad
  };
  uint8_t y[32] = {
    0xcb, 0x61, 0xf6, 0x61, 0x2f, 0x1b, 0x66, 0x72,
    0xcb, 0xb1, 0xe6, 0xf3, 0x3c, 0x47, 0xb4, 0x57,
    0x84, 0xbb, 0xd4, 0x48, 0x43, 0xa2, 0xf6, 0xbe,
    0xa9, 0xb6, 0xaf, 0xdc, 0xd8, 0x76, 0xb8, 0x71
  };

  // Valid signature should pass
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    btl_verifyEcdsaP256r1(sha, r, s, x, y));

  // Missing digest
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_INVALID_PARAM,
                    btl_verifyEcdsaP256r1(NULL, r, s, x, y));
  // Missing R
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_INVALID_PARAM,
                    btl_verifyEcdsaP256r1(sha, NULL, s, x, y));
  // Missing S
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_INVALID_PARAM,
                    btl_verifyEcdsaP256r1(sha, r, NULL, x, y));
#if defined(SEMAILBOX_PRESENT)
  // Missing Qx: Look for the platform key
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    btl_verifyEcdsaP256r1(sha, r, s, NULL, y));
  // Missing Qy: Look for the platform key
  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    btl_verifyEcdsaP256r1(sha, r, s, x, NULL));
#else
  // Missing Qx
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_INVALID_PARAM,
                    btl_verifyEcdsaP256r1(sha, r, s, NULL, y));
  // Missing Qy
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_INVALID_PARAM,
                    btl_verifyEcdsaP256r1(sha, r, s, x, NULL));
#endif
}

void test_simple_key_byte_error(void)
{
  uint8_t sha[32] = {
    0xaa, 0xaf, 0x71, 0x56, 0x61, 0x62, 0x1d, 0x0b,
    0xd8, 0x5d, 0xc7, 0xc1, 0x15, 0x8c, 0xe7, 0x1a,
    0xdb, 0x4c, 0xc9, 0x6c, 0xef, 0xfa, 0x10, 0xd8,
    0xbb, 0x86, 0xa5, 0x8d, 0xa9, 0x11, 0x45, 0xa2
  };
  uint8_t r[32] = {
    0xb4, 0x03, 0xad, 0x46, 0x51, 0x05, 0xd1, 0x9e,
    0xed, 0x8b, 0x52, 0x6d, 0x09, 0x64, 0xee, 0xc7,
    0x7e, 0x61, 0x54, 0xc4, 0x20, 0x07, 0x67, 0x64,
    0xee, 0xb1, 0x8c, 0xdc, 0x1d, 0xf0, 0x4d, 0x44,
  };
  uint8_t s[32] = {
    0xf2, 0xad, 0x85, 0x99, 0xee, 0x4b, 0x32, 0xc8,
    0x4f, 0x59, 0x8a, 0x74, 0xa7, 0x44, 0x03, 0xe2,
    0x9e, 0xa8, 0xea, 0xa7, 0x42, 0x48, 0xa7, 0x58,
    0x6a, 0xa5, 0x16, 0x5b, 0x97, 0xf0, 0xcb, 0xa1,
  };
  uint8_t x[32] = {
    0x28, 0x3d, 0x7f, 0x16, 0xf3, 0xc8, 0x26, 0x91,
    0x95, 0x86, 0x45, 0x70, 0x8b, 0xbd, 0x7a, 0x98,
    0x81, 0x28, 0x44, 0xc6, 0x3d, 0x74, 0x53, 0xb6,
    0xda, 0x70, 0x3d, 0xcd, 0xd2, 0x0f, 0x84, 0xad
  };
  uint8_t y[32] = {
    0xcb, 0x61, 0xf6, 0x61, 0x2f, 0x1b, 0x66, 0x72,
    0xcb, 0xb1, 0xe6, 0xf3, 0x3c, 0x47, 0xb4, 0x57,
    0x84, 0xbb, 0xd4, 0x48, 0x43, 0xa2, 0xf6, 0xbe,
    0xa9, 0xb6, 0xaf, 0xdc, 0xd8, 0x76, 0xb8, 0x71
  };

  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    btl_verifyEcdsaP256r1(sha, r, s, x, y));

  x[7] = 0xFF;

  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED,
                    btl_verifyEcdsaP256r1(sha, r, s, x, y));

  x[7] = 0x91;
  y[15] = 0x00;

  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED,
                    btl_verifyEcdsaP256r1(sha, r, s, x, y));
}

void test_simple_key_zero(void)
{
  uint8_t sha[32] = {
    0xaa, 0xaf, 0x71, 0x56, 0x61, 0x62, 0x1d, 0x0b,
    0xd8, 0x5d, 0xc7, 0xc1, 0x15, 0x8c, 0xe7, 0x1a,
    0xdb, 0x4c, 0xc9, 0x6c, 0xef, 0xfa, 0x10, 0xd8,
    0xbb, 0x86, 0xa5, 0x8d, 0xa9, 0x11, 0x45, 0xa2
  };
  uint8_t r[32] = {
    0xb4, 0x03, 0xad, 0x46, 0x51, 0x05, 0xd1, 0x9e,
    0xed, 0x8b, 0x52, 0x6d, 0x09, 0x64, 0xee, 0xc7,
    0x7e, 0x61, 0x54, 0xc4, 0x20, 0x07, 0x67, 0x64,
    0xee, 0xb1, 0x8c, 0xdc, 0x1d, 0xf0, 0x4d, 0x44,
  };
  uint8_t s[32] = {
    0xf2, 0xad, 0x85, 0x99, 0xee, 0x4b, 0x32, 0xc8,
    0x4f, 0x59, 0x8a, 0x74, 0xa7, 0x44, 0x03, 0xe2,
    0x9e, 0xa8, 0xea, 0xa7, 0x42, 0x48, 0xa7, 0x58,
    0x6a, 0xa5, 0x16, 0x5b, 0x97, 0xf0, 0xcb, 0xa1,
  };
  uint8_t x[32] = {
    0x28, 0x3d, 0x7f, 0x16, 0xf3, 0xc8, 0x26, 0x91,
    0x95, 0x86, 0x45, 0x70, 0x8b, 0xbd, 0x7a, 0x98,
    0x81, 0x28, 0x44, 0xc6, 0x3d, 0x74, 0x53, 0xb6,
    0xda, 0x70, 0x3d, 0xcd, 0xd2, 0x0f, 0x84, 0xad
  };
  uint8_t y[32] = {
    0xcb, 0x61, 0xf6, 0x61, 0x2f, 0x1b, 0x66, 0x72,
    0xcb, 0xb1, 0xe6, 0xf3, 0x3c, 0x47, 0xb4, 0x57,
    0x84, 0xbb, 0xd4, 0x48, 0x43, 0xa2, 0xf6, 0xbe,
    0xa9, 0xb6, 0xaf, 0xdc, 0xd8, 0x76, 0xb8, 0x71
  };
  uint8_t zz[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  uint8_t ff[32] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  };

  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    btl_verifyEcdsaP256r1(sha, r, s, x, y));

  // All-FF Qx
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED,
                    btl_verifyEcdsaP256r1(sha, r, s, ff, y));
  // All-FF Qy
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED,
                    btl_verifyEcdsaP256r1(sha, r, s, x, ff));
  // All-FF Qx and Qy
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED,
                    btl_verifyEcdsaP256r1(sha, r, s, ff, ff));
  // All-zero Qx
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED,
                    btl_verifyEcdsaP256r1(sha, r, s, zz, y));
  // All-zero Qy
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED,
                    btl_verifyEcdsaP256r1(sha, r, s, x, zz));
  // All-zero Qx and Qy
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED,
                    btl_verifyEcdsaP256r1(sha, r, s, zz, zz));
}

void test_simple_signature_byte_error(void)
{
  uint8_t sha[32] = {
    0xaa, 0xaf, 0x71, 0x56, 0x61, 0x62, 0x1d, 0x0b,
    0xd8, 0x5d, 0xc7, 0xc1, 0x15, 0x8c, 0xe7, 0x1a,
    0xdb, 0x4c, 0xc9, 0x6c, 0xef, 0xfa, 0x10, 0xd8,
    0xbb, 0x86, 0xa5, 0x8d, 0xa9, 0x11, 0x45, 0xa2
  };
  uint8_t r[32] = {
    0xb4, 0x03, 0xad, 0x46, 0x51, 0x05, 0xd1, 0x9e,
    0xed, 0x8b, 0x52, 0x6d, 0x09, 0x64, 0xee, 0xc7,
    0x7e, 0x61, 0x54, 0xc4, 0x20, 0x07, 0x67, 0x64,
    0xee, 0xb1, 0x8c, 0xdc, 0x1d, 0xf0, 0x4d, 0x44,
  };
  uint8_t s[32] = {
    0xf2, 0xad, 0x85, 0x99, 0xee, 0x4b, 0x32, 0xc8,
    0x4f, 0x59, 0x8a, 0x74, 0xa7, 0x44, 0x03, 0xe2,
    0x9e, 0xa8, 0xea, 0xa7, 0x42, 0x48, 0xa7, 0x58,
    0x6a, 0xa5, 0x16, 0x5b, 0x97, 0xf0, 0xcb, 0xa1,
  };
  uint8_t x[32] = {
    0x28, 0x3d, 0x7f, 0x16, 0xf3, 0xc8, 0x26, 0x91,
    0x95, 0x86, 0x45, 0x70, 0x8b, 0xbd, 0x7a, 0x98,
    0x81, 0x28, 0x44, 0xc6, 0x3d, 0x74, 0x53, 0xb6,
    0xda, 0x70, 0x3d, 0xcd, 0xd2, 0x0f, 0x84, 0xad
  };
  uint8_t y[32] = {
    0xcb, 0x61, 0xf6, 0x61, 0x2f, 0x1b, 0x66, 0x72,
    0xcb, 0xb1, 0xe6, 0xf3, 0x3c, 0x47, 0xb4, 0x57,
    0x84, 0xbb, 0xd4, 0x48, 0x43, 0xa2, 0xf6, 0xbe,
    0xa9, 0xb6, 0xaf, 0xdc, 0xd8, 0x76, 0xb8, 0x71
  };

  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    btl_verifyEcdsaP256r1(sha, r, s, x, y));

  r[7] = 0xFF;

  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED,
                    btl_verifyEcdsaP256r1(sha, r, s, x, y));

  r[7] = 0x9e;
  s[15] = 0x00;

  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED,
                    btl_verifyEcdsaP256r1(sha, r, s, x, y));
}

void test_simple_signature_zero(void)
{
  uint8_t sha[32] = {
    0xaa, 0xaf, 0x71, 0x56, 0x61, 0x62, 0x1d, 0x0b,
    0xd8, 0x5d, 0xc7, 0xc1, 0x15, 0x8c, 0xe7, 0x1a,
    0xdb, 0x4c, 0xc9, 0x6c, 0xef, 0xfa, 0x10, 0xd8,
    0xbb, 0x86, 0xa5, 0x8d, 0xa9, 0x11, 0x45, 0xa2
  };
  uint8_t r[32] = {
    0xb4, 0x03, 0xad, 0x46, 0x51, 0x05, 0xd1, 0x9e,
    0xed, 0x8b, 0x52, 0x6d, 0x09, 0x64, 0xee, 0xc7,
    0x7e, 0x61, 0x54, 0xc4, 0x20, 0x07, 0x67, 0x64,
    0xee, 0xb1, 0x8c, 0xdc, 0x1d, 0xf0, 0x4d, 0x44,
  };
  uint8_t s[32] = {
    0xf2, 0xad, 0x85, 0x99, 0xee, 0x4b, 0x32, 0xc8,
    0x4f, 0x59, 0x8a, 0x74, 0xa7, 0x44, 0x03, 0xe2,
    0x9e, 0xa8, 0xea, 0xa7, 0x42, 0x48, 0xa7, 0x58,
    0x6a, 0xa5, 0x16, 0x5b, 0x97, 0xf0, 0xcb, 0xa1,
  };
  uint8_t x[32] = {
    0x28, 0x3d, 0x7f, 0x16, 0xf3, 0xc8, 0x26, 0x91,
    0x95, 0x86, 0x45, 0x70, 0x8b, 0xbd, 0x7a, 0x98,
    0x81, 0x28, 0x44, 0xc6, 0x3d, 0x74, 0x53, 0xb6,
    0xda, 0x70, 0x3d, 0xcd, 0xd2, 0x0f, 0x84, 0xad
  };
  uint8_t y[32] = {
    0xcb, 0x61, 0xf6, 0x61, 0x2f, 0x1b, 0x66, 0x72,
    0xcb, 0xb1, 0xe6, 0xf3, 0x3c, 0x47, 0xb4, 0x57,
    0x84, 0xbb, 0xd4, 0x48, 0x43, 0xa2, 0xf6, 0xbe,
    0xa9, 0xb6, 0xaf, 0xdc, 0xd8, 0x76, 0xb8, 0x71
  };
  uint8_t zz[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  uint8_t ff[32] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  };

  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    btl_verifyEcdsaP256r1(sha, r, s, x, y));

  // All-FF R
  TEST_ASSERT_EQUAL(EXPECTED_ERROR_RANGE,
                    btl_verifyEcdsaP256r1(sha, ff, s, x, y));
  // All-FF S
  TEST_ASSERT_EQUAL(EXPECTED_ERROR_RANGE,
                    btl_verifyEcdsaP256r1(sha, r, ff, x, y));
  // All-FF R and S
  TEST_ASSERT_EQUAL(EXPECTED_ERROR_RANGE,
                    btl_verifyEcdsaP256r1(sha, ff, ff, x, y));
  // All-zero R
  TEST_ASSERT_EQUAL(EXPECTED_ERROR_RANGE,
                    btl_verifyEcdsaP256r1(sha, zz, s, x, y));
  // All-zero S
  TEST_ASSERT_EQUAL(EXPECTED_ERROR_RANGE,
                    btl_verifyEcdsaP256r1(sha, r, zz, x, y));
  // All-zero R and S
  TEST_ASSERT_EQUAL(EXPECTED_ERROR_RANGE,
                    btl_verifyEcdsaP256r1(sha, zz, zz, x, y));
}

void test_simple_signature_equals_n(void)
{
  uint8_t sha[32] = {
    0xaa, 0xaf, 0x71, 0x56, 0x61, 0x62, 0x1d, 0x0b,
    0xd8, 0x5d, 0xc7, 0xc1, 0x15, 0x8c, 0xe7, 0x1a,
    0xdb, 0x4c, 0xc9, 0x6c, 0xef, 0xfa, 0x10, 0xd8,
    0xbb, 0x86, 0xa5, 0x8d, 0xa9, 0x11, 0x45, 0xa2
  };
  uint8_t r[32] = {
    0xb4, 0x03, 0xad, 0x46, 0x51, 0x05, 0xd1, 0x9e,
    0xed, 0x8b, 0x52, 0x6d, 0x09, 0x64, 0xee, 0xc7,
    0x7e, 0x61, 0x54, 0xc4, 0x20, 0x07, 0x67, 0x64,
    0xee, 0xb1, 0x8c, 0xdc, 0x1d, 0xf0, 0x4d, 0x44,
  };
  uint8_t s[32] = {
    0xf2, 0xad, 0x85, 0x99, 0xee, 0x4b, 0x32, 0xc8,
    0x4f, 0x59, 0x8a, 0x74, 0xa7, 0x44, 0x03, 0xe2,
    0x9e, 0xa8, 0xea, 0xa7, 0x42, 0x48, 0xa7, 0x58,
    0x6a, 0xa5, 0x16, 0x5b, 0x97, 0xf0, 0xcb, 0xa1,
  };
  uint8_t x[32] = {
    0x28, 0x3d, 0x7f, 0x16, 0xf3, 0xc8, 0x26, 0x91,
    0x95, 0x86, 0x45, 0x70, 0x8b, 0xbd, 0x7a, 0x98,
    0x81, 0x28, 0x44, 0xc6, 0x3d, 0x74, 0x53, 0xb6,
    0xda, 0x70, 0x3d, 0xcd, 0xd2, 0x0f, 0x84, 0xad
  };
  uint8_t y[32] = {
    0xcb, 0x61, 0xf6, 0x61, 0x2f, 0x1b, 0x66, 0x72,
    0xcb, 0xb1, 0xe6, 0xf3, 0x3c, 0x47, 0xb4, 0x57,
    0x84, 0xbb, 0xd4, 0x48, 0x43, 0xa2, 0xf6, 0xbe,
    0xa9, 0xb6, 0xaf, 0xdc, 0xd8, 0x76, 0xb8, 0x71
  };
  // Order of P-256 curve as given by http://www.secg.org/sec2-v2.pdf section 2.4.2
  uint8_t n[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84,
    0xF3, 0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51,
  };

  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    btl_verifyEcdsaP256r1(sha, r, s, x, y));

  // n as R
  TEST_ASSERT_EQUAL(EXPECTED_ERROR_RANGE,
                    btl_verifyEcdsaP256r1(sha, n, s, x, y));
  // n as S
  TEST_ASSERT_EQUAL(EXPECTED_ERROR_RANGE,
                    btl_verifyEcdsaP256r1(sha, r, n, x, y));
  // n as R and S
  TEST_ASSERT_EQUAL(EXPECTED_ERROR_RANGE,
                    btl_verifyEcdsaP256r1(sha, n, n, x, y));
}

void test_simple_signature_equals_q(void)
{
  uint8_t sha[32] = {
    0xaa, 0xaf, 0x71, 0x56, 0x61, 0x62, 0x1d, 0x0b,
    0xd8, 0x5d, 0xc7, 0xc1, 0x15, 0x8c, 0xe7, 0x1a,
    0xdb, 0x4c, 0xc9, 0x6c, 0xef, 0xfa, 0x10, 0xd8,
    0xbb, 0x86, 0xa5, 0x8d, 0xa9, 0x11, 0x45, 0xa2
  };
  uint8_t r[32] = {
    0xb4, 0x03, 0xad, 0x46, 0x51, 0x05, 0xd1, 0x9e,
    0xed, 0x8b, 0x52, 0x6d, 0x09, 0x64, 0xee, 0xc7,
    0x7e, 0x61, 0x54, 0xc4, 0x20, 0x07, 0x67, 0x64,
    0xee, 0xb1, 0x8c, 0xdc, 0x1d, 0xf0, 0x4d, 0x44,
  };
  uint8_t s[32] = {
    0xf2, 0xad, 0x85, 0x99, 0xee, 0x4b, 0x32, 0xc8,
    0x4f, 0x59, 0x8a, 0x74, 0xa7, 0x44, 0x03, 0xe2,
    0x9e, 0xa8, 0xea, 0xa7, 0x42, 0x48, 0xa7, 0x58,
    0x6a, 0xa5, 0x16, 0x5b, 0x97, 0xf0, 0xcb, 0xa1,
  };
  uint8_t x[32] = {
    0x28, 0x3d, 0x7f, 0x16, 0xf3, 0xc8, 0x26, 0x91,
    0x95, 0x86, 0x45, 0x70, 0x8b, 0xbd, 0x7a, 0x98,
    0x81, 0x28, 0x44, 0xc6, 0x3d, 0x74, 0x53, 0xb6,
    0xda, 0x70, 0x3d, 0xcd, 0xd2, 0x0f, 0x84, 0xad
  };
  uint8_t y[32] = {
    0xcb, 0x61, 0xf6, 0x61, 0x2f, 0x1b, 0x66, 0x72,
    0xcb, 0xb1, 0xe6, 0xf3, 0x3c, 0x47, 0xb4, 0x57,
    0x84, 0xbb, 0xd4, 0x48, 0x43, 0xa2, 0xf6, 0xbe,
    0xa9, 0xb6, 0xaf, 0xdc, 0xd8, 0x76, 0xb8, 0x71
  };

  TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                    btl_verifyEcdsaP256r1(sha, r, s, x, y));

  // Qx as R
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED,
                    btl_verifyEcdsaP256r1(sha, x, s, x, y));
  // Qy as S
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED,
                    btl_verifyEcdsaP256r1(sha, r, x, x, y));
  // Qx and Qy as R and S
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED,
                    btl_verifyEcdsaP256r1(sha, x, y, x, y));
}

static void hex_to_bytearray(char * hex, uint8_t *bytes, int len)
{
  char c, k, l;
  for (int i = 0; i < len; i++) {
    c = hex[i * 2];
    if ((c >= '0') && (c <= '9')) {
      k = c - '0';
    } else if ((c >= 'a') && (c <= 'f')) {
      k = c - 'a' + 10;
    } else if ((c >= 'A') && (c <= 'F')) {
      k = c - 'A' + 10;
    } else {
      k = 0;
    }
    c = hex[i * 2 + 1];
    if ((c >= '0') && (c <= '9')) {
      l = c - '0';
    } else if ((c >= 'a') && (c <= 'f')) {
      l = c - 'a' + 10;
    } else if ((c >= 'A') && (c <= 'F')) {
      l = c - 'A' + 10;
    } else {
      l = 0;
    }

    bytes[i] = (k << 4) | l;
  }
}

static const char * nist_fips_186_4[] = {
  "Msg = e4796db5f785f207aa30d311693b3702821dff1168fd2e04c0836825aefd850d9aa60326d88cde1a23c7745351392ca2288d632c264f197d05cd424a30336c19fd09bb229654f0222fcb881a4b35c290a093ac159ce13409111ff0358411133c24f5b8e2090d6db6558afc36f06ca1f6ef779785adba68db27a409859fc4c4a0\
  Qx = 87f8f2b218f49845f6f10eec3877136269f5c1a54736dbdf69f89940cad41555\
  Qy = e15f369036f49842fac7a86c8a2b0557609776814448b8f5e84aa9f4395205e9\
  R = d19ff48b324915576416097d2544f7cbdf8768b1454ad20e0baac50e211f23b0\
  S = a3e81e59311cdfff2d4784949f7a2cb50ba6c3a91fa54710568e61aca3e847c6\
  Result = F (3 - S changed)",
  "Msg = 069a6e6b93dfee6df6ef6997cd80dd2182c36653cef10c655d524585655462d683877f95ecc6d6c81623d8fac4e900ed0019964094e7de91f1481989ae1873004565789cbf5dc56c62aedc63f62f3b894c9c6f7788c8ecaadc9bd0e81ad91b2b3569ea12260e93924fdddd3972af5273198f5efda0746219475017557616170e\
  Qx = 5cf02a00d205bdfee2016f7421807fc38ae69e6b7ccd064ee689fc1a94a9f7d2\
  Qy = ec530ce3cc5c9d1af463f264d685afe2b4db4b5828d7e61b748930f3ce622a85\
  R = dc23d130c6117fb5751201455e99f36f59aba1a6a21cf2d0e7481a97451d6693\
  S = d6ce7708c18dbf35d4f8aa7240922dc6823f2e7058cbc1484fcad1599db5018c\
  Result = F (2 - R changed)",
  "Msg = df04a346cf4d0e331a6db78cca2d456d31b0a000aa51441defdb97bbeb20b94d8d746429a393ba88840d661615e07def615a342abedfa4ce912e562af714959896858af817317a840dcff85a057bb91a3c2bf90105500362754a6dd321cdd86128cfc5f04667b57aa78c112411e42da304f1012d48cd6a7052d7de44ebcc01de\
  Qx = 2ddfd145767883ffbb0ac003ab4a44346d08fa2570b3120dcce94562422244cb\
  Qy = 5f70c7d11ac2b7a435ccfbbae02c3df1ea6b532cc0e9db74f93fffca7c6f9a64\
  R = 9913111cff6f20c5bf453a99cd2c2019a4e749a49724a08774d14e4c113edda8\
  S = 9467cd4cd21ecb56b0cab0a9a453b43386845459127a952421f5c6382866c5cc\
  Result = F (4 - Q changed)",
  "Msg = e1130af6a38ccb412a9c8d13e15dbfc9e69a16385af3c3f1e5da954fd5e7c45fd75e2b8c36699228e92840c0562fbf3772f07e17f1add56588dd45f7450e1217ad239922dd9c32695dc71ff2424ca0dec1321aa47064a044b7fe3c2b97d03ce470a592304c5ef21eed9f93da56bb232d1eeb0035f9bf0dfafdcc4606272b20a3\
  Qx = e424dc61d4bb3cb7ef4344a7f8957a0c5134e16f7a67c074f82e6e12f49abf3c\
  Qy = 970eed7aa2bc48651545949de1dddaf0127e5965ac85d1243d6f60e7dfaee927\
  R = bf96b99aa49c705c910be33142017c642ff540c76349b9dab72f981fd9347f4f\
  S = 17c55095819089c2e03b9cd415abdf12444e323075d98f31920b9e0f57ec871c\
  Result = P (0 )",
  "Msg = 73c5f6a67456ae48209b5f85d1e7de7758bf235300c6ae2bdceb1dcb27a7730fb68c950b7fcada0ecc4661d3578230f225a875e69aaa17f1e71c6be5c831f22663bac63d0c7a9635edb0043ff8c6f26470f02a7bc56556f1437f06dfa27b487a6c4290d8bad38d4879b334e341ba092dde4e4ae694a9c09302e2dbf443581c08\
  Qx = e0fc6a6f50e1c57475673ee54e3a57f9a49f3328e743bf52f335e3eeaa3d2864\
  Qy = 7f59d689c91e463607d9194d99faf316e25432870816dde63f5d4b373f12f22a\
  R = 1d75830cd36f4c9aa181b2c4221e87f176b7f05b7c87824e82e396c88315c407\
  S = cb2acb01dac96efc53a32d4a0d85d0c2e48955214783ecf50a4f0414a319c05a\
  Result = P (0 )",
  "Msg = 666036d9b4a2426ed6585a4e0fd931a8761451d29ab04bd7dc6d0c5b9e38e6c2b263ff6cb837bd04399de3d757c6c7005f6d7a987063cf6d7e8cb38a4bf0d74a282572bd01d0f41e3fd066e3021575f0fa04f27b700d5b7ddddf50965993c3f9c7118ed78888da7cb221849b3260592b8e632d7c51e935a0ceae15207bedd548\
  Qx = a849bef575cac3c6920fbce675c3b787136209f855de19ffe2e8d29b31a5ad86\
  Qy = bf5fe4f7858f9b805bd8dcc05ad5e7fb889de2f822f3d8b41694e6c55c16b471\
  R = 25acc3aa9d9e84c7abf08f73fa4195acc506491d6fc37cb9074528a7db87b9d6\
  S = 9b21d5b5259ed3f2ef07dfec6cc90d3a37855d1ce122a85ba6a333f307d31537\
  Result = F (2 - R changed)",
  "Msg = 7e80436bce57339ce8da1b5660149a20240b146d108deef3ec5da4ae256f8f894edcbbc57b34ce37089c0daa17f0c46cd82b5a1599314fd79d2fd2f446bd5a25b8e32fcf05b76d644573a6df4ad1dfea707b479d97237a346f1ec632ea5660efb57e8717a8628d7f82af50a4e84b11f21bdff6839196a880ae20b2a0918d58cd\
  Qx = 3dfb6f40f2471b29b77fdccba72d37c21bba019efa40c1c8f91ec405d7dcc5df\
  Qy = f22f953f1e395a52ead7f3ae3fc47451b438117b1e04d613bc8555b7d6e6d1bb\
  R = 548886278e5ec26bed811dbb72db1e154b6f17be70deb1b210107decb1ec2a5a\
  S = e93bfebd2f14f3d827ca32b464be6e69187f5edbd52def4f96599c37d58eee75\
  Result = F (4 - Q changed)",
  "Msg = 1669bfb657fdc62c3ddd63269787fc1c969f1850fb04c933dda063ef74a56ce13e3a649700820f0061efabf849a85d474326c8a541d99830eea8131eaea584f22d88c353965dabcdc4bf6b55949fd529507dfb803ab6b480cd73ca0ba00ca19c438849e2cea262a1c57d8f81cd257fb58e19dec7904da97d8386e87b84948169\
  Qx = 69b7667056e1e11d6caf6e45643f8b21e7a4bebda463c7fdbc13bc98efbd0214\
  Qy = d3f9b12eb46c7c6fda0da3fc85bc1fd831557f9abc902a3be3cb3e8be7d1aa2f\
  R = 288f7a1cd391842cce21f00e6f15471c04dc182fe4b14d92dc18910879799790\
  S = 247b3c4e89a3bcadfea73c7bfd361def43715fa382b8c3edf4ae15d6e55e9979\
  Result = F (1 - Message changed)",
  "Msg = 3fe60dd9ad6caccf5a6f583b3ae65953563446c4510b70da115ffaa0ba04c076115c7043ab8733403cd69c7d14c212c655c07b43a7c71b9a4cffe22c2684788ec6870dc2013f269172c822256f9e7cc674791bf2d8486c0f5684283e1649576efc982ede17c7b74b214754d70402fb4bb45ad086cf2cf76b3d63f7fce39ac970\
  Qx = bf02cbcf6d8cc26e91766d8af0b164fc5968535e84c158eb3bc4e2d79c3cc682\
  Qy = 069ba6cb06b49d60812066afa16ecf7b51352f2c03bd93ec220822b1f3dfba03\
  R = f5acb06c59c2b4927fb852faa07faf4b1852bbb5d06840935e849c4d293d1bad\
  S = 049dab79c89cc02f1484c437f523e080a75f134917fda752f2d5ca397addfe5d\
  Result = F (3 - S changed)",
  "Msg = 983a71b9994d95e876d84d28946a041f8f0a3f544cfcc055496580f1dfd4e312a2ad418fe69dbc61db230cc0c0ed97e360abab7d6ff4b81ee970a7e97466acfd9644f828ffec538abc383d0e92326d1c88c55e1f46a668a039beaa1be631a89129938c00a81a3ae46d4aecbf9707f764dbaccea3ef7665e4c4307fa0b0a3075c\
  Qx = 224a4d65b958f6d6afb2904863efd2a734b31798884801fcab5a590f4d6da9de\
  Qy = 178d51fddada62806f097aa615d33b8f2404e6b1479f5fd4859d595734d6d2b9\
  R = 87b93ee2fecfda54deb8dff8e426f3c72c8864991f8ec2b3205bb3b416de93d2\
  S = 4044a24df85be0cc76f21a4430b75b8e77b932a87f51e4eccbc45c263ebf8f66\
  Result = F (2 - R changed)",
  "Msg = 4a8c071ac4fd0d52faa407b0fe5dab759f7394a5832127f2a3498f34aac287339e043b4ffa79528faf199dc917f7b066ad65505dab0e11e6948515052ce20cfdb892ffb8aa9bf3f1aa5be30a5bbe85823bddf70b39fd7ebd4a93a2f75472c1d4f606247a9821f1a8c45a6cb80545de2e0c6c0174e2392088c754e9c8443eb5af\
  Qx = 43691c7795a57ead8c5c68536fe934538d46f12889680a9cb6d055a066228369\
  Qy = f8790110b3c3b281aa1eae037d4f1234aff587d903d93ba3af225c27ddc9ccac\
  R = 8acd62e8c262fa50dd9840480969f4ef70f218ebf8ef9584f199031132c6b1ce\
  S = cfca7ed3d4347fb2a29e526b43c348ae1ce6c60d44f3191b6d8ea3a2d9c92154\
  Result = F (3 - S changed)",
  "Msg = 0a3a12c3084c865daf1d302c78215d39bfe0b8bf28272b3c0b74beb4b7409db0718239de700785581514321c6440a4bbaea4c76fa47401e151e68cb6c29017f0bce4631290af5ea5e2bf3ed742ae110b04ade83a5dbd7358f29a85938e23d87ac8233072b79c94670ff0959f9c7f4517862ff829452096c78f5f2e9a7e4e9216\
  Qx = 9157dbfcf8cf385f5bb1568ad5c6e2a8652ba6dfc63bc1753edf5268cb7eb596\
  Qy = 972570f4313d47fc96f7c02d5594d77d46f91e949808825b3d31f029e8296405\
  R = dfaea6f297fa320b707866125c2a7d5d515b51a503bee817de9faa343cc48eeb\
  S = 8f780ad713f9c3e5a4f7fa4c519833dfefc6a7432389b1e4af463961f09764f2\
  Result = F (1 - Message changed)",
  "Msg = 785d07a3c54f63dca11f5d1a5f496ee2c2f9288e55007e666c78b007d95cc28581dce51f490b30fa73dc9e2d45d075d7e3a95fb8a9e1465ad191904124160b7c60fa720ef4ef1c5d2998f40570ae2a870ef3e894c2bc617d8a1dc85c3c55774928c38789b4e661349d3f84d2441a3b856a76949b9f1f80bc161648a1cad5588e\
  Qx = 072b10c081a4c1713a294f248aef850e297991aca47fa96a7470abe3b8acfdda\
  Qy = 9581145cca04a0fb94cedce752c8f0370861916d2a94e7c647c5373ce6a4c8f5\
  R = 09f5483eccec80f9d104815a1be9cc1a8e5b12b6eb482a65c6907b7480cf4f19\
  S = a4f90e560c5e4eb8696cb276e5165b6a9d486345dedfb094a76e8442d026378d\
  Result = F (4 - Q changed)",
  "Msg = 76f987ec5448dd72219bd30bf6b66b0775c80b394851a43ff1f537f140a6e7229ef8cd72ad58b1d2d20298539d6347dd5598812bc65323aceaf05228f738b5ad3e8d9fe4100fd767c2f098c77cb99c2992843ba3eed91d32444f3b6db6cd212dd4e5609548f4bb62812a920f6e2bf1581be1ebeebdd06ec4e971862cc42055ca\
  Qx = 09308ea5bfad6e5adf408634b3d5ce9240d35442f7fe116452aaec0d25be8c24\
  Qy = f40c93e023ef494b1c3079b2d10ef67f3170740495ce2cc57f8ee4b0618b8ee5\
  R = 5cc8aa7c35743ec0c23dde88dabd5e4fcd0192d2116f6926fef788cddb754e73\
  S = 9c9c045ebaa1b828c32f82ace0d18daebf5e156eb7cbfdc1eff4399a8a900ae7\
  Result = F (1 - Message changed)",
  "Msg = 60cd64b2cd2be6c33859b94875120361a24085f3765cb8b2bf11e026fa9d8855dbe435acf7882e84f3c7857f96e2baab4d9afe4588e4a82e17a78827bfdb5ddbd1c211fbc2e6d884cddd7cb9d90d5bf4a7311b83f352508033812c776a0e00c003c7e0d628e50736c7512df0acfa9f2320bd102229f46495ae6d0857cc452a84\
  Qx = 2d98ea01f754d34bbc3003df5050200abf445ec728556d7ed7d5c54c55552b6d\
  Qy = 9b52672742d637a32add056dfd6d8792f2a33c2e69dafabea09b960bc61e230a\
  R = 06108e525f845d0155bf60193222b3219c98e3d49424c2fb2a0987f825c17959\
  S = 62b5cdd591e5b507e560167ba8f6f7cda74673eb315680cb89ccbc4eec477dce\
  Result = P (0 )",
};

void test_nist_fips_186_4_vectors(void)
{
  uint8_t msg[128];
  uint8_t x[32];
  uint8_t y[32];
  uint8_t r[32];
  uint8_t s[32];
  Sha256Context_t sha_ctx;
  Sha256Context_t sha_ctx_two;

  for (size_t i = 0; i < sizeof(nist_fips_186_4) / sizeof(nist_fips_186_4[0]); i++) {
    hex_to_bytearray(strstr(nist_fips_186_4[i], "Msg = ") + 6, msg, 128);
    hex_to_bytearray(strstr(nist_fips_186_4[i], "Qx = ") + 5, x, 32);
    hex_to_bytearray(strstr(nist_fips_186_4[i], "Qy = ") + 5, y, 32);
    hex_to_bytearray(strstr(nist_fips_186_4[i], "R = ") + 4, r, 32);
    hex_to_bytearray(strstr(nist_fips_186_4[i], "S = ") + 4, s, 32);

    char expect_pass = *(strstr(nist_fips_186_4[i], "Result = ") + 9);

    // FIPS-186-4 uses SHA-256 hash on message
    btl_initSha256(&sha_ctx);
    btl_updateSha256(&sha_ctx, msg, 128);
    btl_finalizeSha256(&sha_ctx);

    btl_initSha256(&sha_ctx_two);
    btl_updateSha256(&sha_ctx_two, msg, 128);
    btl_finalizeSha256(&sha_ctx_two);

    // Verify that we get the same SHA hash given the same message.
    TEST_ASSERT_EQUAL(BOOTLOADER_OK, btl_verifySha256(&sha_ctx, &sha_ctx_two.sha));
    sha_ctx_two.sha[0] = 0xFF;
    TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_REJECTED, btl_verifySha256(&sha_ctx, &sha_ctx_two.sha));
    TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_INVALID_PARAM, btl_verifySha256(NULL, &sha_ctx_two.sha));
    TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SECURITY_INVALID_PARAM, btl_verifySha256(&sha_ctx, NULL));

    UnityPrintf("FIPS-186-4 vector %u, expect %c\n", i, expect_pass);
    if (expect_pass == 'P') {
      TEST_ASSERT_EQUAL(BOOTLOADER_OK,
                        btl_verifyEcdsaP256r1(sha_ctx.sha, r, s, x, y));
    } else {
      TEST_ASSERT_NOT_EQUAL(BOOTLOADER_OK,
                            btl_verifyEcdsaP256r1(sha_ctx.sha, r, s, x, y));
    }
  }
}

static const char * nist_fips_186_2[] = {
  "Msg = d5033790158b091cb32bf6311c3160bb62f78c0081150075e6d530d632e85a753b57afd353bcb36dceb1e6d8be89a410013e4a0e7eab9a98abfb615d53625641960b46f631516eccff3129cac0ffcbfd1a59cc10dd5a291d8b0f07c3182f9979ee06452134b302e15093052eaafd9c29b3305a2f278974ab8ffa6b2f09b7ebc4\
  Qx = 5a3dbbf50abd417dab31bd26a0f2e5a1c420c990d970330d0644d6b131ad630c\
  Qy = f2ac79331870210c540dee8cd1b852d0727fee61fe3865a3cae0c523837a29bc\
  R = 5109dc0f1e3d0d4b042bd26388d1ea66b335e796b5050e381bbf820106898271\
  S = 2cb0ea4c36b7d7c4fe6e619f8deea4f828444f47547a850541885ee542a04920\
  Result = F (3 - S changed)",
  "Msg = 11eac1050cf2008321439b50282b34d2fce0b98f19c597966ae92a1b5cd07861377720ddae928d98b5186fd592016cf4374f1296cf4b11029711a7c7ef4e5ba3b149eea4c1208f8de5544e7bd788d3c8998650300983b432b5a422b9f0c1a1fc266815a36c256e2b5b001f8b1f48d118cb8f59a6eff6e8f06dab823a88afb234\
  Qx = 638bcadf69329f6a3a98500e960ed681cc8735650bbf153967b86b42b3af8677\
  Qy = 3dc59968a493f4a40f0e51d05f41bf95b503ee1fa8e97c65f5ccce4492eb07c8\
  R = 6402b171a0d735dca36b0c6dba8f201927967b394be9324d89216e3432455461\
  S = c927116cf52ed0ded699d050eacd0d2168bcc7aa156334049da51b350d9e4942\
  Result = F (4 - Q changed)",
  "Msg = 3823b1863976ed72dab033081f0be100729dc8b55337822a4b8e054b219879765139473aba1f735f97eb2b26b091a0d1d20114667c0734b1db6fa988f86eea53313d54cbe6077c017405c4a267e82c7aeb776b3884793f71ffd501e7a9f87c0abe77ffbf24f5b16159482505abd72e03a746f5b2d3564872a00635f09affd8a5\
  Qx = 2e461b92f67bb5deeea8c83dbdca83ed05785badc0832e31c1701a45e873067f\
  Qy = 9c43261e8ea57e7d52ba13117129f6af0c87a49d00f82d4eae2849398399ea5f\
  R = 379d0764114009f7222eb211d1952f65c270228c11e0d07fa5dc3cf2ee202207\
  S = 0aa1803112fbd501ee6b87031d497e25c5d9d4993b60c3e7abf5e53f2b8bb1cc\
  Result = F (4 - Q changed)",
  "Msg = acc899a726217a7e77ee17e0e52d0070943fa62ba0a7104b690fcf6b2e8b33edde22bf992a8bdb86df34c93e7389da1ddd72e2faaa1de90a4eff2300b815237929704b9845f087ed6b503b9c426279bb949c78fb60d4418b2a48ac3996e18bdb35b925ed9fdae67bc24954427943451261c64f76e8538197a6f3505ef178d4d9\
  Qx = ba8ff3bb996ec1daefdf09c69f4ebac001e9709dc8afc0905e7fc00f4305b79d\
  Qy = a36ef343e37f4c3da4b5e93b65c575b16a2b6916ad62976a6283d98cc73928be\
  R = 934e43ed17c7587fcc6c0b5d99ad73d6942df37c3d065caa7f8a436d91744e6a\
  S = d9a0c8e453fd814743b1f41f1fa6ad08929d3c7aaf716117440b6984cc5af657\
  Result = F (2 - R changed)",
  "Msg = fb5088ab8df5c3b4851b6cad00b8ace89a481f65e172e2990ae8ee6fe38fa48d09e0a8d85b8ef33aae6824a3c382174d768fb94537921b27beb6c528fb794c5a4c10272cf307be30c729c422299fc7253a5c054f4cb88268965a57ef2b5c4d5e2daaaab03dab4b1f3724f3321c59e3f75075ab8d062b3fe6f89e4dcb5b7ece21\
  Qx = 9e628d5ac881b641d4165cb5de0910ef306b61ac5bb3476d50c6151e311d0827\
  Qy = 53721782614614ff7d7ad0f7b692ff63078d55be2c807e345d0557fc80b64128\
  R = b6ceb721e55668728372c9718d170d4b9bc363336c492d82d0f66eb20783293d\
  S = bc94962cb3fb9aec05badbe60653a9951eb59137747f535a1b6982859abc79d1\
  Result = F (3 - S changed)",
  "Msg = 6755ec34a8194e421930075b43918445a358c020aabc67f608413a932a2eb5970142efc906a3c3ae7b172d5bf376f687146094e49c2b4cb0a21f99861fe6d17bac3e06668df12a7fef07772c6e3a0e17daaadf853bd2c8e212bb019fe26fb741efb1f7b346ebcedcfd68af76bcc3141e7da068a78f9925b4e17c3a867db3af12\
  Qx = f17d062896a2fbf1e304667e1ca4e0a1783e70880ede16bfc1033d2fda8a9000\
  Qy = a39b197285e38179c59aca4ce8b83604b865fccef71872f9843122a338b41124\
  R = 8e5f7e06543b2484244549652a73db444270ead0416e2a370e41da12b491ebf0\
  S = 608ce091e33f05fb61f0b2b6b4520046eaf84ebee27a0cb007216af956f124fc\
  Result = F (1 - Message changed)",
  "Msg = 8a0415918522ce78de2dab1e721b252b1dea89c5371f9fa95e728a7a76c8ef58ef6159b54653dc64cf37f9f3b122c23b32d73e35353a0f100008528043c6c75856f7325bc392014f04b32e548e9167740eb06d7bf258ba092b4566e531a04b94482e682b03c88361060b12e80f505cfe38dedc3f765fe6bd66918d52493ec335\
  Qx = 2acb25567e9d795ee199f94fcfe51bf5756e36cee13b4a2499d79e409271fd0c\
  Qy = 05c0cbe648c1dd0265336e79f97192990f95f4277fa05c1862946c9c91f32d8d\
  R = a6c8e76734bb77955f592e3ded6bc7d15882627abf975e32cb3c8262eb967268\
  S = 230b1ea88f6b6dd82b075ddc1f9f308441173f43350c50d369dadca26575a5d8\
  Result = P (0 )",
  "Msg = f4d96ed464dea0bd67d305fe94c40d99ee76f428b8f1387e8b3c508ba6689b8084cf9734b49e53c6e591278540fea20cfb15c4b12b666d21daa416445aa8bf67d8b82899340a44551184eb6228c6cff8e16ab124248ad56d6c83b3b81ef9ef691a1c8bcb8f295cb21333ba5ff3237ca3c7fe24484a6656ac69648faf9a6413cf\
  Qx = 67d699ff2177e0d1ef141baf80dbce72fb3c54adf2019f9f12211ee11f3d07e3\
  Qy = c87ecf77733943d9167938e95ac450c1d6c84ab8e87e861202dcef6a22869314\
  R = 5ad641c4ac9e26a831252e3abeec9c480e3e7b425cc845d0b7d3bd9ffacea3a0\
  S = dd9c341000584579e402c49449745d348eadbd284fbd1c2e960c6b194d07c206\
  Result = F (1 - Message changed)",
  "Msg = af2ae94abd4fc8fcf5fd01ce46f49cb89a0777305b88e7423501a2e31e24dd839405b1e12687c32336142426fd927613d0925133a1cae504c8c5e08c04d992edbc5a4e3a8b0d1489fcdf6e4992d798d60c4ae34be64e5b982370a8d44aaa32d4af8f89fcf3c90355cec5a7e00cad492ad697f72133fc9426cf6bc363fa7e075f\
  Qx = 179bb0468afd199c2207e4a3b343e3343d1f9a2dfdcc9a8b42ecf362278d5198\
  Qy = 5205696e57c0a65ef6adb541947c584492250e98f6d7161c096f6d5741e12f2e\
  R = 7a2144c502f996d67d4e43dc6e1485d2e07b87369ab0ff067fdde9d100888446\
  S = 13123861c90264bb35f9bf9a091a6e261ad9df85214f7f9741abf92056e2efcb\
  Result = F (3 - S changed)",
  "Msg = 443a9aa336e64da043fb9e43d7027b146f5763e3ace81daca53aabbe82ad7a1c4700db24ed17b76eb7fc7df7c325607fe0f5c02f9f84e57fada3f3575f1b1a748f360e0ea781b7b8fbcf29875c81d676e9050209299d783157627c70fd88003dc5e31baa8baab644b67c893bc9a046c3359113b972ba3e8a4d7e2c51847b933f\
  Qx = 7a986042092626d048bf5d86d2003ef9f7602a0721f612bfeca585a56366d3f6\
  Qy = 1521d156e250b5c0e8ef2e14a9c40a6cc41b074ccc393c5f35ef4c99fae289cd\
  R = 83dcd16bbcaae8c691ece763e795b342085599358f8014295c555a5aa5c07e1b\
  S = f5691305b5b7ecab9726430fa09d025ce4a7681b05ca2eae10ba970b3d03e1f2\
  Result = P (0 )",
  "Msg = cc7a01e541c271fd01af633d4132553a11a97a6ae0565f384310ce54f238f34d13efbd61bc868794b336f8ff7812276b8539031ed450f3f5a230b9b373db9c9956b5c6376af1eb81a1492b25c4f23a1cb7cfbe7fffe5f89d810fa74a8cf2297a1a34e40e53e9e4ed534ec5572dbad47630d1713669fbb857681560c14766e217\
  Qx = 5add7fd2f1760cb4670d102d12fd1085808c553d743bcb10cb2e31013f8262d9\
  Qy = f5c974dd4656e598d4c6cd98604a2d359b53cc4d275b3d2d5685bb311972ad0d\
  R = 189a42df5fe3602c6c93d8bfe39c7e2d44a230175157a9b5f365f331256d1e89\
  S = 08f07eeed5c85ad4a5760c7ae7daaf18e3274c0c83f346e46e3b9fcb772cf169\
  Result = F (4 - Q changed)",
  "Msg = 5b7dc78662b578ae7cf35e5b997397c36544c59427a00ee5e879686cd95413f2ef5a6605d0accfedf2f816e1014ae17a82247d67b7d0db63c2f5d9640c69c315e8461a8774409423c03563e43306bac32aa68e72e1709a4c97bea58dedb707be90686a741717ce3a2fd3d0ccf5502b86a3f27f1b6f28518ab0fb534e43e7e6d5\
  Qx = 83c0c0c2df3b8cdd2b552d0802b9dce740c355bb7540dad83e2c4b5b9da842fb\
  Qy = 3a65b7999c96beb7f4d38a63fd0a00eb43388636b18f6ed67d38d12183091359\
  R = 3f09a01ec1f2aeb95a7fdca9fe977a47703f6ec9f8b47b5deafd98eff30da981\
  S = c16023447bbc953795dce68111807ccf04dd9372759f27b7b9ec56bc4fddfdd1\
  Result = P (0 )",
  "Msg = c84ee8c88cc773848594256636be30eaf8ca56db3fea4f2020756bd9109578abef40e65745692992cb775f112df633e30e8c01034c2f3b2e86960bf2dced3908f6cd50e3486bfc899410b7a2b0c9a4ce895fbb3c688927797461ec857915cd2ce7df6d931e37b412e8477441479f4d56722aceae1a60dca9f1be58bb6d73fcc0\
  Qx = 87eed479378387613c548638cd40387ef6a7a620d325ece0a567c1a0cbef48f3\
  Qy = 0090b2f0eb2405eba809e0154be5f7060ea5cb257574be28f7b37afc87e30f8c\
  R = a9b7e252c1a35bad60c40da649d44bfce61aabbfa8761d741e6bce81e5a45544\
  S = aa34a416ab2d7d7b997c12e337ccf1dc8c360d5be697dd2bdfab1adf635b4502\
  Result = F (2 - R changed)",
  "Msg = b8a1b86f09e96fda55a0b7fc402c6e0bb8130d6a27647f28ed5334ca0d23714a2649ea5fa6ce92ff3af5b222891e38ca21ea5240b1a03ca07acf579700357a6c05e08c3db793538c296b87fa00c22b516f541dc865a0eb830492e85a565e7fdb3bafc892a6015988094b60090ee2edaa889a938ba92e33da75e21098766628a0\
  Qx = dc3a7c47a4a43eeea2620971856d952feb7687d7c5b1e0aaae8bd26e5b4a3a94\
  Qy = 23b3374a44f210e24095bfa0ad1d99bdadcebcf2fbf5ad516e4ecfe767d19167\
  R = 4424c50987cbe2dce7aa842fbfdfc78d8e99133155f19eff5c9aeff4ba752038\
  S = 9e8289344d99292f03808621ff9edb0e69637821bd5203f75182aa3f6fd15ad3\
  Result = F (1 - Message changed)",
  "Msg = 45f5efd2e3f98b47ece4f8b9a9aae668711d7546776ba69fe6e80fff300ed451a2c2d2443dd91df3c1dbadc40a0754909f97f3058945ee49610ddde0ad3585d06d8b0603f0ee55ecec0fed6ece79c2f94fcf77f64855d065c7b26ac462f5aa33cbd5f3d7a53d1c7c9608217e9e0918e9a74e67df5da5353a3f601e73f6562bc3\
  Qx = f4f63a3e2458a4d6be45581af2a7eafb5aa4c250182bfb8d8e0f6970ef3d9f41\
  Qy = 8079acd05d41fd77a4158f23548792482d3bcb8bd8092abb3dc7e8ef87b7f2b1\
  R = c014ac65e3a03f1d62cd5be5a8b97923c05f72bf560ac2c416a7f5e4af4d5289\
  S = b87074431a283a902184414f1e0eba25276a1b488e6bb19dbc0a6a26867c0f50\
  Result = F (2 - R changed)",
};

// void test_nist_fips_186_2_vectors(void)
// {
//   uint8_t msg[128];
//   uint8_t x[32];
//   uint8_t y[32];
//   uint8_t r[32];
//   uint8_t s[32];
//   uint8_t sha1[32] = { 0 };
//   mbedtls_sha1_context sha_ctx;
//   for (size_t i = 0; i < sizeof(nist_fips_186_2) / sizeof(nist_fips_186_2[0]); i++) {
//     hex_to_bytearray(strstr(nist_fips_186_2[i], "Msg = ") + 6, msg, 128);
//     hex_to_bytearray(strstr(nist_fips_186_2[i], "Qx = ") + 5, x, 32);
//     hex_to_bytearray(strstr(nist_fips_186_2[i], "Qy = ") + 5, y, 32);
//     hex_to_bytearray(strstr(nist_fips_186_2[i], "R = ") + 4, r, 32);
//     hex_to_bytearray(strstr(nist_fips_186_2[i], "S = ") + 4, s, 32);
//     char expect_pass = *(strstr(nist_fips_186_2[i], "Result = ") + 9);
//     // FIPS-186-2 uses SHA-1 hash on message
//     mbedtls_sha1_init(&sha_ctx);
//     mbedtls_sha1_update(&sha_ctx, msg, 128);
//     mbedtls_sha1_finish(&sha_ctx, &sha1[12]);
//     UnityPrintf("FIPS-186-2 vector %u, expect %c\n", i, expect_pass);
//     if (expect_pass == 'P') {
//       TEST_ASSERT_EQUAL(BOOTLOADER_OK,
//                         btl_verifyEcdsaP256r1(sha1, r, s, x, y));
//     } else {
//       TEST_ASSERT_NOT_EQUAL(BOOTLOADER_OK,
//                             btl_verifyEcdsaP256r1(sha1, r, s, x, y));
//     }
//   }
// }

int main(void)
{
  CHIP_Init();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif
  sl_memory_init();

  UnityBeginGroup("ECDSA");

  RUN_TEST(test_simple_invalid_pointers, __LINE__);
  RUN_TEST(test_simple_key_byte_error, __LINE__);
  RUN_TEST(test_simple_key_zero, __LINE__);
  RUN_TEST(test_simple_signature_byte_error, __LINE__);
  RUN_TEST(test_simple_signature_zero, __LINE__);
  RUN_TEST(test_simple_signature_equals_n, __LINE__);
  RUN_TEST(test_simple_signature_equals_q, __LINE__);
  RUN_TEST(test_nist_fips_186_4_vectors, __LINE__);
  (void)nist_fips_186_2;
  // RUN_TEST(test_nist_fips_186_2_vectors, __LINE__); TODO: Re-enable this test
  UnityEnd();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
