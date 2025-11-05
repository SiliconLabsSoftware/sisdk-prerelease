/***************************************************************************//**
 * @file storage_test.c
 * @brief Test storage interface
 * @author Silicon Labs
 * @version 1.7.0
 *******************************************************************************
 * @section License
 * <b>Copyright 2016 Silicon Laboratories, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/
#include "api/btl_interface.h"
#include "sl_memory_manager.h"
#include "debug/btl_debug.h"
#include "driver/btl_serial_driver.h"
#include "driver/btl_driver_delay.h"
#include "em_chip.h"

#ifdef BTL_CONFIG_FILE
  #include BTL_CONFIG_FILE
#else
#ifdef BTL_DRIVER_EUART_ENABLE
#include "btl_euart_driver_cfg.h"
#else
#include "btl_uart_driver_cfg.h"
#endif
#endif

// Unity test framework
#include "unity.h"
#if defined(UNITY_OUTPUT_SERIAL)
#include "unity_usart.h"
#else
#include "unity_swo.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#if ENABLE_TEST_COVERAGE
extern void __gcov_flush(void);
#endif

#define TEST_HOST_CLEAR()  UnityPrintf("{{clear}}\n")
#define TEST_HOST_SYNC()   UnityPrintf("{{sync}}\n")
#define TEST_HOST_END()    UnityPrintf("{{end}}\n")

#define TEST_HOST_START_D2H(size, data)        \
  do {                                         \
    UnityPrintf("{{device_to_host;%u;", size); \
    for (size_t i = 0; i < size; i++) {        \
      UNITY_OUTPUT_CHAR('\\');                 \
      UnityPrintNumberHex(data[i], 2);         \
    }                                          \
    UnityPrintf("}}\n");                       \
  } while (0)

#define TEST_HOST_START_H2D(size)                       \
  do {                                                  \
    UnityPrintf("{{host_to_device;start;%u}}\n", size); \
  } while (0)
#define TEST_HOST_END_H2D(size, data)     \
  do {                                    \
    UnityPrintf("{{host_to_device;end;"); \
    for (size_t i = 0; i < size; i++) {   \
      UNITY_OUTPUT_CHAR('\\');            \
      UnityPrintNumberHex(data[i], 2);    \
    }                                     \
    UnityPrintf("}}\n");                  \
  } while (0)

#define TEST_HOST_START_ECHO()  UnityPrintf("{{host_echo;start}}\n")
#define TEST_HOST_END_ECHO()    UnityPrintf("{{host_echo;end}}\n")

static void testInit(void)
{
  uart_init();
}

static void testDeinit(void)
{
  uart_deinit();
}

static void testTxClear(void)
{
  // Transmit the string "cleared" to allow the host to clear any buffered data
  // until the expected string is received

  uint8_t str[] = "cleared";
#ifdef BTL_DRIVER_EUART_ENABLE
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SPI_PERIPHERAL_ARGUMENT, uart_sendBuffer(str, SL_DRIVER_EUART_TX_BUFFER_SIZE + 1, true));
#else
  TEST_ASSERT_EQUAL(BOOTLOADER_ERROR_SPI_PERIPHERAL_ARGUMENT, uart_sendBuffer(str, SL_DRIVER_UART_TX_BUFFER_SIZE + 1, true));
#endif
  TEST_HOST_CLEAR();
  TEST_HOST_SYNC();

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, uart_sendBuffer(str, sizeof(str) - 1, true));
}

static void testTx(void)
{
  uint8_t str[] = "Hello World\n";

  TEST_HOST_START_D2H(sizeof(str) - 1, str);
  TEST_HOST_SYNC();

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, uart_sendBuffer(str, sizeof(str) - 1, true));
}

static void testRx(void)
{
  char buffer[256];
  size_t received = 0;

  delay_init();

  // Request 128 bytes from host
  TEST_HOST_START_H2D(128);
  TEST_HOST_SYNC();

  int attempts = 10;

  while (uart_getRxAvailableBytes() < 128 && attempts) {
    delay_milliseconds(1000, true);
    attempts--;
//    UnityPrintf("Rx size %d\r\n", uart_getRxAvailableBytes());
  }

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, uart_receiveBuffer((uint8_t*)buffer, 128, &received, true, 100));
  buffer[received] = 0;
  UnityPrintf("Received %" PRIu32 " bytes: %s\r\n", received, buffer);

  TEST_HOST_END_H2D(received, buffer);
  TEST_HOST_SYNC();

  TEST_HOST_START_H2D(128);
  TEST_HOST_SYNC();

  attempts = 10;

  while (uart_getRxAvailableBytes() < 128 && attempts) {
    delay_milliseconds(1000, true);
    attempts--;
//    UnityPrintf("Rx size %d\r\n", uart_getRxAvailableBytes());
  }

  TEST_ASSERT_EQUAL(BOOTLOADER_OK, uart_receiveBuffer((uint8_t*)buffer, 128, &received, true, 100));
  buffer[received] = 0;
  UnityPrintf("Received %" PRIu32 " bytes: %s\r\n", received, buffer);

  TEST_HOST_END_H2D(received, buffer);
  TEST_HOST_SYNC();
}

static void testEcho(void)
{
  uint8_t txBuffer[127];
  uint8_t rxBuffer[127];

  size_t rxPos = 0;

  // Fill TX buffer with random printable ASCII data
  for (size_t i = 0; i < sizeof(txBuffer); i++) {
    txBuffer[i] = (rand() % 96) + 32;
  }

  delay_init();

  for (int i = 0; i < 10; i++) {
    while (!uart_isTxIdle()) {
      UnityPrintf("TX not ready");
      TEST_HOST_SYNC();
    }

    // Notify host to start echo server
    TEST_HOST_START_ECHO();
    TEST_HOST_SYNC();

    TEST_ASSERT_EQUAL_MESSAGE(0, uart_getRxAvailableBytes(), "Unexpected data in buffer before test");

    // Transmit buffer without blocking
    uart_sendBuffer(txBuffer, sizeof(txBuffer), false);

    delay_milliseconds(2500, false); // TODO: fine tune the number

    // Receive buffer one byte at a time
    while (rxPos < sizeof(rxBuffer) && !delay_expired()) {
      if (uart_getRxAvailableBytes()) {
        uart_receiveBuffer(&rxBuffer[rxPos], 1, NULL, false, 0);
        rxPos++;
      }
    }

    TEST_HOST_END_ECHO();
    TEST_HOST_SYNC();

    TEST_ASSERT_FALSE(delay_expired());
    // Assert that the received data is identical to the transmitted data
    TEST_ASSERT_EQUAL_HEX8_ARRAY(txBuffer, rxBuffer, 127);
    // Assert that there are no more bytes in the RX buffer
    TEST_ASSERT_EQUAL(0, uart_getRxAvailableBytes());

    // Notify host to start echo server
    TEST_HOST_START_ECHO();
    TEST_HOST_SYNC();

    TEST_ASSERT_EQUAL_MESSAGE(0, uart_getRxAvailableBytes(), "Unexpected data in buffer before test");

    // Transmit bytes
    for (size_t i = 0; i < sizeof(txBuffer); i++) {
      uart_sendByte(txBuffer[i]);
    }

    delay_milliseconds(2500, false); // TODO: fine tune the number
    // Receive buffer one byte at a time
    rxPos = 0;
    while (rxPos < sizeof(rxBuffer) && !delay_expired()) {
      if (uart_getRxAvailableBytes()) {
        uart_receiveByte(&rxBuffer[rxPos]);
        rxPos++;
      }
    }

    TEST_HOST_END_ECHO();
    TEST_HOST_SYNC();
    TEST_ASSERT_FALSE(delay_expired());
    // Assert that the received data is identical to the transmitted data
    TEST_ASSERT_EQUAL_HEX8_ARRAY(txBuffer, rxBuffer, 127);
    // Assert that there are no more bytes in the RX buffer
    TEST_ASSERT_EQUAL(0, uart_getRxAvailableBytes());

    // Notify host to start echo server
    TEST_HOST_START_ECHO();
    TEST_HOST_SYNC();

    TEST_ASSERT_EQUAL_MESSAGE(0, uart_getRxAvailableBytes(), "Unexpected data in buffer before test");

    // Transmit bytes
    for (size_t i = 0; i < sizeof(txBuffer); i++) {
      uart_sendByte(txBuffer[i]);
    }

    // Receive buffer one byte at a time
    rxPos = 0;
    while (rxPos < sizeof(rxBuffer)) {
      if (uart_getRxAvailableBytes()) {
        uart_receiveByteTimeout(&rxBuffer[rxPos], 10);
        rxPos++;
      }
    }

    TEST_HOST_END_ECHO();
    TEST_HOST_SYNC();

    // Assert that the received data is identical to the transmitted data
    TEST_ASSERT_EQUAL_HEX8_ARRAY(txBuffer, rxBuffer, 127);
    // Assert that there are no more bytes in the RX buffer
    TEST_ASSERT_EQUAL(0, uart_getRxAvailableBytes());

    TEST_ASSERT_EQUAL(BOOTLOADER_OK, uart_flush(true, false));
    TEST_ASSERT_EQUAL(BOOTLOADER_OK, uart_flush(false, true));

    // Reset RX buffer
    for (size_t i = 0; i < sizeof(rxBuffer); i++) {
      rxBuffer[i] = 0;
    }
    rxPos = 0;
  }
}

int main(void)
{
  CHIP_Init();
  BTL_DEBUG_INIT();

#if defined(UNITY_OUTPUT_SERIAL)
  UNITY_UsartSetup();
#else
  SWO_Setup();
#endif

  sl_memory_init();

  UnityBeginGroup("UART");

  delay_init();

  RUN_TEST(testInit, __LINE__);
  RUN_TEST(testTxClear, __LINE__);
  RUN_TEST(testTx, __LINE__);
  RUN_TEST(testRx, __LINE__);
  RUN_TEST(testEcho, __LINE__);
  RUN_TEST(testDeinit, __LINE__);

  UnityEnd();
  TEST_HOST_END();
  TEST_HOST_SYNC();

#if ENABLE_TEST_COVERAGE
  __gcov_flush();
#endif

  UnityPrint("ENDSWO");
  while (1) ;
}
