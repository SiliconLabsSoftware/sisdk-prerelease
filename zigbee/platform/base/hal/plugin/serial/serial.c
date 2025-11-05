/***************************************************************************//**
 * @file
 * @brief Serial Layer, legacy support
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
#include PLATFORM_HEADER
#include STACK_TYPES_HEADER

//Host processors do not use Ember Message Buffers.
#ifndef EZSP_HOST
  #include "buffer_manager/buffer-management.h"
  #include "buffer_manager/legacy-packet-buffer.h"
#endif

#include "hal/hal.h"
#include "serial.h"
#ifdef CORTEXM3_EFM32_MICRO
  #include "com.h"
  #include "com_config.h"
#endif
#include "ember-printf.h"

#include <stdarg.h>

#ifdef SL_LEGACY_SERIAL_USE_STDIO
#include <stdio.h>
#endif //SL_LEGACY_SERIAL_USE_STDIO

#ifdef SL_LEGACY_SERIAL_CUSTOM_STDIO
#include SL_LEGACY_SERIAL_CUSTOM_STDIO
#define SL_LEGACY_SERIAL_USE_STDIO
#endif // SL_LEGACY_SERIAL_CUSTOM_STDIO

// AppBuilder and Afv2 will define the characteristics of the Serial ports here.
#if defined(ZA_GENERATED_HEADER)
  #include ZA_GENERATED_HEADER
#endif

// Crude method of mashing together com and serial layers before all the drivers
// are properly ported over into ember world. NT 2014-09-16
#ifdef CORTEXM3_EFM32_MICRO
//------------------------------------------------------
// Serial initialization

sl_status_t sli_legacy_serial_init(uint8_t port,
                                   SerialBaudRate rate,
                                   SerialParity parity,
                                   uint8_t stopBits)
{
  sl_status_t status = SL_STATUS_INITIALIZATION;
  COM_Init_t initdata;
  switch (port) {
#ifdef COM_VCP_ENABLE
    case COM_VCP:
    case comPortVcp:
      Ecode_t com_status = COM_Init((COM_Port_t) port, NULL);
      status = (com_status == ECODE_OK) ? SL_STATUS_OK : SL_STATUS_INITIALIZATION;
      break;
#endif
#ifdef COM_USART0_ENABLE
    case COM_USART0:
    case comPortUsart0:
      initdata = (COM_Init_t) COM_USART0_DEFAULT;
      break;
#endif
#ifdef COM_USART1_ENABLE
    case COM_USART1:
    case comPortUsart1:
      initdata = (COM_Init_t) COM_USART1_DEFAULT;
      break;
#endif
#ifdef COM_USART2_ENABLE
    case COM_USART2:
    case comPortUsart2:
      initdata = (COM_Init_t) COM_USART2_DEFAULT;
      break;
#endif
#ifdef COM_USART3_ENABLE
    case comPortUsart3:
      initdata = (COM_Init_t) COM_USART3_DEFAULT;
      break;
#endif
#ifdef COM_LEUART0_ENABLE
    case COM_LEUART0:
    case comPortLeuart0:
      initdata = (COM_Init_t) COM_LEUART0_DEFAULT;
      break;
#endif
#ifdef COM_LEUART1_ENABLE
    case COM_LEUART1:
    case comPortLeuart1:
      initdata = (COM_Init_t) COM_LEUART1_DEFAULT;
      break;
#endif
#ifdef COM_EUSART0_ENABLE
    case COM_EUSART0:
    case comPortEusart0:
      initdata = (COM_Init_t) COM_EUSART0_DEFAULT;
      break;
#endif
#ifdef COM_EUSART1_ENABLE
    case COM_EUSART1:
    case comPortEusart1:
      initdata = (COM_Init_t) COM_EUSART1_DEFAULT;
      break;
#endif
    default:
      return status;
  }
#if defined(COM_USART0_ENABLE) || defined (COM_USART1_ENABLE) || defined (COM_USART2_ENABLE) || defined (COM_USART3_ENABLE)
  if ((port == COM_USART0) || (port == comPortUsart0)
      || (port == COM_USART1) || (port == comPortUsart1)
      || (port == COM_USART2) || (port == comPortUsart2)
      || (port == comPortUsart3)) {
    initdata.uartdrvinit.uartinit.baudRate = rate;
    initdata.uartdrvinit.uartinit.parity = (USART_Parity_TypeDef)parity;
    if (stopBits == 1) {
      initdata.uartdrvinit.uartinit.stopBits = usartStopbits1;
    } else if (stopBits == 2) {
      initdata.uartdrvinit.uartinit.stopBits = usartStopbits2;
    }
    Ecode_t com_status = COM_Init((COM_Port_t) port, &initdata);
    status = (com_status == ECODE_OK) ? SL_STATUS_OK : SL_STATUS_INITIALIZATION;
  }
#endif
#if defined (COM_LEUART0_ENABLE) || defined (COM_LEUART1_ENABLE)
  if ((port == COM_LEUART0) || (port == comPortLeuart0)
      || (port == COM_LEUART1) || (port == comPortLeuart0)
      ) {
    initdata.uartdrvinit.leuartinit.baudRate = rate;
    initdata.uartdrvinit.leuartinit.parity = (LEUART_Parity_TypeDef)parity;
    if (stopBits == 1) {
      initdata.uartdrvinit.leuartinit.stopBits = leuartStopbits1;
    } else if (stopBits == 2) {
      initdata.uartdrvinit.leuartinit.stopBits = leuartStopbits2;
    }
    Ecode_t com_status = COM_Init((COM_Port_t) port, &initdata);
    status = (com_status == ECODE_OK) ? SL_STATUS_OK : SL_STATUS_INITIALIZATION;
  }
#endif
#if defined (COM_EUSART0_ENABLE) || defined (COM_EUSART1_ENABLE)
  if ((port == COM_EUSART0) || (port == comPortEusart0)
      || (port == COM_EUSART1) || (port == comPortEusart0)
      ) {
    initdata.uartdrvinit.eusartinit.baudRate = rate;
    initdata.uartdrvinit.eusartinit.parity = (EUSART_Parity_TypeDef)parity;
    if (stopBits == 1) {
      initdata.uartdrvinit.eusartinit.stopBits = eusartStopbits1;
    } else if (stopBits == 2) {
      initdata.uartdrvinit.eusartinit.stopBits = eusartStopbits2;
    }
    Ecode_t com_status = COM_Init((COM_Port_t) port, &initdata);
    status = (com_status == ECODE_OK) ? SL_STATUS_OK : SL_STATUS_INITIALIZATION;
  }
#endif
  return status;
}

//------------------------------------------------------
// Serial Input

// returns # bytes available for reading
uint16_t sli_legacy_serial_read_available(uint8_t port)
{
  return COM_ReadAvailable((COM_Port_t) port);
}

sl_status_t sli_legacy_serial_read_byte(uint8_t port, uint8_t *dataByte)
{
  switch (COM_ReadByte((COM_Port_t) port, dataByte)) {
    case SL_ZIGBEE_SUCCESS:
      return SL_STATUS_OK;
    case SL_LEGACY_SERIAL_RX_EMPTY:
      return SL_STATUS_EMPTY;
    case SL_LEGACY_SERIAL_RX_OVERFLOW:
      return SL_STATUS_HAS_OVERFLOWED;
    // no sl_status_t translation, treated as generic fail
    case SL_LEGACY_SERIAL_RX_FRAME_ERROR:
    case SL_LEGACY_SERIAL_RX_PARITY_ERROR:
    case SL_LEGACY_SERIAL_RX_OVERRUN_ERROR:
    default:
      return SL_STATUS_FAIL;
  }
}

sl_status_t sl_legacy_serial_read_data(uint8_t port,
                                       uint8_t *data,
                                       uint16_t length,
                                       uint16_t *bytesRead)
{
  uint16_t bytesReadInternal = 0;
  sl_status_t status;

  while (bytesReadInternal < length) {
    status = sli_legacy_serial_read_byte(port, data);

    switch (status) {
      case SL_STATUS_OK:
        ++data;
        ++bytesReadInternal;
        break;

      case SL_STATUS_EMPTY:
        // empty queue is not an error for us, we just want to keep waiting
        break;

      default:
        // only store number of bytes read if the caller provided a non-NULL pointer
        if (bytesRead) {
          *bytesRead = bytesReadInternal;
        }
        return status;
    }
  }

  // only store number of bytes read if the caller provided a non-NULL pointer
  if (bytesRead) {
    *bytesRead = bytesReadInternal;
  }

  return SL_STATUS_OK;
}

#ifndef SL_ZIGBEE_TEST
sl_status_t emberSerialReadDataTimeout(uint8_t port,
                                       uint8_t *data,
                                       uint16_t length,
                                       uint16_t *bytesRead,
                                       uint16_t firstByteTimeout,
                                       uint16_t subsequentByteTimeout)
{
  uint16_t bytesReadInternal = 0;
  sl_status_t status;
  uint16_t timeout = firstByteTimeout;
  uint16_t startTime = halCommonGetInt16uMillisecondTick();

  // loop until we read the max number of bytes or the timeout elapses
  while (bytesReadInternal < length
         && elapsedTimeInt16u(startTime, halCommonGetInt16uMillisecondTick()) < timeout) {
    status = sli_legacy_serial_read_byte(port, data);

    switch (status) {
      case SL_STATUS_OK:
        ++data;
        ++bytesReadInternal;
        // reset timer and timeout for next character
        startTime = halCommonGetInt16uMillisecondTick();
        timeout = subsequentByteTimeout;
        break;

      case SL_STATUS_EMPTY:
        // empty queue is not an error for us, we just want to keep waiting
        break;

      default:
        // only store number of bytes read if the caller provided a non-NULL pointer
        if (bytesRead) {
          *bytesRead = bytesReadInternal;
        }
        return status;
    }
  }

  // only store number of bytes read if the caller provided a non-NULL pointer
  if (bytesRead) {
    *bytesRead = bytesReadInternal;
  }

  return bytesReadInternal == length ? SL_STATUS_OK : SL_STATUS_EMPTY;
}

#endif // SL_ZIGBEE_TEST

sl_status_t emberSerialReadPartialLine(uint8_t port, char *data, uint8_t max, uint8_t *index)
{
  switch (COM_ReadPartialLine((COM_Port_t) port, data, max, index)) {
    case SL_ZIGBEE_SUCCESS:
      return SL_STATUS_OK;
    case SL_LEGACY_SERIAL_RX_EMPTY:
      return SL_STATUS_EMPTY;
    case SL_LEGACY_SERIAL_TX_OVERFLOW:
      return SL_STATUS_HAS_OVERFLOWED;
    case SL_LEGACY_SERIAL_RX_FRAME_ERROR:
    case SL_LEGACY_SERIAL_RX_PARITY_ERROR:
    case SL_LEGACY_SERIAL_RX_OVERRUN_ERROR:
    default:
      return SL_STATUS_FAIL;
  }
}

sl_status_t sli_legacy_serial_read_line(uint8_t port, char *data, uint8_t max)
{
  uint8_t index = 0;

  while (emberSerialReadPartialLine(port, data, max, &index) != SL_STATUS_OK) {
    halResetWatchdog();
  }
  return SL_STATUS_OK;
}

//------------------------------------------------------
// Serial Output

// returns # bytes (if fifo mode)/messages (if buffer mode) that can be written
uint16_t sli_legacy_serial_write_available(uint8_t port)
{
  return COM_WriteAvailable((COM_Port_t) port);
}

uint16_t emberSerialWriteUsed(uint8_t port)
{
  return COM_WriteUsed((COM_Port_t) port);
}

sl_status_t sli_legacy_serial_write_byte(uint8_t port, uint8_t dataByte)
{
  switch (COM_WriteByte((COM_Port_t) port, dataByte)) {
    case SL_ZIGBEE_SUCCESS:
      return SL_STATUS_OK;
    case SL_LEGACY_SERIAL_TX_OVERFLOW:
      return SL_STATUS_HAS_OVERFLOWED;
    default:
      return SL_STATUS_FAIL;
  }
}

sl_status_t sli_legacy_serial_write_string(uint8_t port, const char *string)
{
  switch (COM_WriteString((COM_Port_t) port, string)) {
    case SL_ZIGBEE_SUCCESS:
      return SL_STATUS_OK;
    case SL_LEGACY_SERIAL_TX_OVERFLOW:
      return SL_STATUS_HAS_OVERFLOWED;
    default:
      return SL_STATUS_FAIL;
  }
}

sl_status_t sli_legacy_serial_write_data(uint8_t port, uint8_t *data, uint8_t length)
{
  switch (COM_WriteData((COM_Port_t) port, data, length)) {
    case SL_ZIGBEE_SUCCESS:
      return SL_STATUS_OK;
    case SL_LEGACY_SERIAL_TX_OVERFLOW:
      return SL_STATUS_HAS_OVERFLOWED;
    default:
      return SL_STATUS_FAIL;
  }
}

sl_status_t emberSerialWriteBuffer(uint8_t port,
                                   sli_buffer_manager_buffer_t buffer,
                                   uint8_t start,
                                   uint8_t length)
{
  return sli_legacy_serial_write_data(port,
                                      sl_legacy_buffer_manager_message_buffer_contents(buffer) + start,
                                      length);
}

sl_status_t sli_legacy_serial_wait_send(uint8_t port)  // waits for all byte to be written out of a port
{
  switch (COM_WaitSend((COM_Port_t) port)) {
    case SL_ZIGBEE_SUCCESS:
      return SL_STATUS_OK;
    case SL_LEGACY_SERIAL_TX_OVERFLOW:
      return SL_STATUS_HAS_OVERFLOWED;
    default:
      return SL_STATUS_FAIL;
  }
}

//------------------------------------------------------
// Guaranteed output

// The _only_ Guaranteed API:  The usage model for this api
//   Does not require efficiency

#if !defined(EMBER_AF_PLUGIN_STANDARD_PRINTF_SUPPORT)

sl_status_t sli_legacy_serial_guaranteed_printf(uint8_t port, const char *formatString, ...)
{
  va_list ap;
  va_start(ap, formatString);
  (void) sli_util_printf_internal(COM_ForceWriteData, (COM_Port_t) port, formatString, ap);
  va_end(ap);
  return SL_STATUS_OK;
}

#endif

//------------------------------------------------------
// Serial buffer maintenance
void sli_legacy_serial_flush_rx(uint8_t port)
{
  COM_FlushRx((COM_Port_t) port);
}

void emberSerialBufferTick(void)
{
}

bool emberSerialUnused(uint8_t port)
{
#ifdef SL_LEGACY_SERIAL_USE_STDIO
  return false;
#else //SL_LEGACY_SERIAL_USE_STDIO
  return COM_Unused((COM_Port_t) port);
#endif //SL_LEGACY_SERIAL_USE_STDIO
}

#else //CORTEXM3_EFM32_MICRO

//Documentary comments:
// To conserve precious flash, there is very little validity checking
//  on the given parameters.  Be sure not to use an invalid port number
//  or a port that is unused.
// Blocking routines will always wait for room (but not buffers - if buffers
//  cannot be allocated, no part of the message will be sent)
// Non-blocking routines will never wait for room, and may cause partial
//  messages to be sent.  If a contiguous message needs to be sent, available
//  space should be checked _before_ calling the appropriate write API.

//------------------------------------------------------
// Determine if blocking code needs to be enabled
#if defined(SL_LEGACY_SERIAL0_BLOCKING)  \
  || defined(SL_LEGACY_SERIAL1_BLOCKING) \
  || defined(SL_LEGACY_SERIAL2_BLOCKING) \
  || defined(SL_LEGACY_SERIAL3_BLOCKING) \
  || defined(SL_LEGACY_SERIAL4_BLOCKING)
  #define EM_ENABLE_SERIAL_BLOCKING
#endif
#ifdef SL_LEGACY_SERIAL0_BLOCKING
  #define EM_SERIAL0_BLOCKSTATE true
#else
  #define EM_SERIAL0_BLOCKSTATE false
#endif
#ifdef SL_LEGACY_SERIAL1_BLOCKING
  #define EM_SERIAL1_BLOCKSTATE true
#else
  #define EM_SERIAL1_BLOCKSTATE false
#endif
#ifdef SL_LEGACY_SERIAL2_BLOCKING
  #define EM_SERIAL2_BLOCKSTATE true
#else
  #define EM_SERIAL2_BLOCKSTATE false
#endif
#ifdef SL_LEGACY_SERIAL3_BLOCKING
  #define EM_SERIAL3_BLOCKSTATE true
#else
  #define EM_SERIAL3_BLOCKSTATE false
#endif
#ifdef SL_LEGACY_SERIAL4_BLOCKING
  #define EM_SERIAL4_BLOCKSTATE true
#else
  #define EM_SERIAL4_BLOCKSTATE false
#endif

#ifndef SL_LEGACY_SERIAL_USE_STDIO
//------------------------------------------------------
// Memory allocations for Queue data structures

//Macros to define fifo and buffer queues, can't use a typedef becuase the size
// of the fifo array in the queues can change
#define DEFINE_FIFO_QUEUE(qSize, qName)          \
  static struct {                                \
    /*! Indexes of next byte to send*/           \
    uint16_t head;                               \
    /*! Index of where to enqueue next message*/ \
    uint16_t tail;                               \
    /*! Number of bytes queued*/                 \
    volatile uint16_t used;                      \
    /*! FIFO of queue data*/                     \
    uint8_t fifo[qSize];                         \
  } qName;

// Allocate Appropriate TX Queue for port 0
#if SL_LEGACY_SERIAL0_MODE == SL_LEGACY_SERIAL_FIFO
DEFINE_FIFO_QUEUE(SL_LEGACY_SERIAL0_TX_QUEUE_SIZE, emSerial0TxQueue)
  #define EM_SERIAL0_TX_QUEUE_ADDR (&emSerial0TxQueue)
#elif SL_LEGACY_SERIAL0_MODE == SL_LEGACY_SERIAL_UNUSED \
  || SL_LEGACY_SERIAL0_MODE == SL_LEGACY_SERIAL_LOWLEVEL
  #define EM_SERIAL0_TX_QUEUE_ADDR (NULL)
  #define SL_LEGACY_SERIAL0_TX_QUEUE_SIZE 0
  #define SL_LEGACY_SERIAL0_RX_QUEUE_SIZE 0
#endif

// Allocate Appropriate TX Queue for port 1
#if SL_LEGACY_SERIAL1_MODE == SL_LEGACY_SERIAL_FIFO
DEFINE_FIFO_QUEUE(SL_LEGACY_SERIAL1_TX_QUEUE_SIZE, emSerial1TxQueue)
  #define EM_SERIAL1_TX_QUEUE_ADDR (&emSerial1TxQueue)
#elif SL_LEGACY_SERIAL1_MODE == SL_LEGACY_SERIAL_UNUSED \
  || SL_LEGACY_SERIAL1_MODE == SL_LEGACY_SERIAL_LOWLEVEL
  #define EM_SERIAL1_TX_QUEUE_ADDR (NULL)
  #define SL_LEGACY_SERIAL1_TX_QUEUE_SIZE 0
  #define SL_LEGACY_SERIAL1_RX_QUEUE_SIZE 0
#endif

// Allocate Appropriate TX Queue for port 2
#if SL_LEGACY_SERIAL2_MODE == SL_LEGACY_SERIAL_FIFO
DEFINE_FIFO_QUEUE(SL_LEGACY_SERIAL2_TX_QUEUE_SIZE, emSerial2TxQueue)
  #define EM_SERIAL2_TX_QUEUE_ADDR (&emSerial2TxQueue)
#elif SL_LEGACY_SERIAL2_MODE == SL_LEGACY_SERIAL_UNUSED \
  || SL_LEGACY_SERIAL2_MODE == SL_LEGACY_SERIAL_LOWLEVEL
  #define EM_SERIAL2_TX_QUEUE_ADDR (NULL)
  #define SL_LEGACY_SERIAL2_TX_QUEUE_SIZE 0
  #define SL_LEGACY_SERIAL2_RX_QUEUE_SIZE 0
#endif

// Allocate Appropriate TX Queue for port 3
#if SL_LEGACY_SERIAL3_MODE == SL_LEGACY_SERIAL_FIFO
DEFINE_FIFO_QUEUE(SL_LEGACY_SERIAL3_TX_QUEUE_SIZE, emSerial3TxQueue)
  #define EM_SERIAL3_TX_QUEUE_ADDR (&emSerial3TxQueue)
#elif SL_LEGACY_SERIAL3_MODE == SL_LEGACY_SERIAL_UNUSED \
  || SL_LEGACY_SERIAL3_MODE == SL_LEGACY_SERIAL_LOWLEVEL
  #define EM_SERIAL3_TX_QUEUE_ADDR (NULL)
  #define SL_LEGACY_SERIAL3_TX_QUEUE_SIZE 0
  #define SL_LEGACY_SERIAL3_RX_QUEUE_SIZE 0
#endif

// Allocate Appropriate TX Queue for port 4
#if SL_LEGACY_SERIAL4_MODE == SL_LEGACY_SERIAL_FIFO
DEFINE_FIFO_QUEUE(SL_LEGACY_SERIAL4_TX_QUEUE_SIZE, emSerial4TxQueue)
  #define EM_SERIAL4_TX_QUEUE_ADDR (&emSerial4TxQueue)
#elif SL_LEGACY_SERIAL4_MODE == SL_LEGACY_SERIAL_UNUSED \
  || SL_LEGACY_SERIAL4_MODE == SL_LEGACY_SERIAL_LOWLEVEL
  #define EM_SERIAL4_TX_QUEUE_ADDR (NULL)
  #define SL_LEGACY_SERIAL4_TX_QUEUE_SIZE 0
  #define SL_LEGACY_SERIAL4_RX_QUEUE_SIZE 0
#endif

// Allocate RX Queues (Always FIFOs)
#if SL_LEGACY_SERIAL0_MODE != SL_LEGACY_SERIAL_UNUSED
DEFINE_FIFO_QUEUE(SL_LEGACY_SERIAL0_RX_QUEUE_SIZE, emSerial0RxQueue)
  #define EM_SERIAL0_RX_QUEUE_ADDR (&emSerial0RxQueue)
#else
  #define EM_SERIAL0_RX_QUEUE_ADDR (NULL)
#endif

#if SL_LEGACY_SERIAL1_MODE != SL_LEGACY_SERIAL_UNUSED
DEFINE_FIFO_QUEUE(SL_LEGACY_SERIAL1_RX_QUEUE_SIZE, emSerial1RxQueue)
  #define EM_SERIAL1_RX_QUEUE_ADDR (&emSerial1RxQueue)
#else
  #define EM_SERIAL1_RX_QUEUE_ADDR (NULL)
  #define emSerial1RxQueue (NULL)
#endif

#if SL_LEGACY_SERIAL2_MODE != SL_LEGACY_SERIAL_UNUSED
DEFINE_FIFO_QUEUE(SL_LEGACY_SERIAL2_RX_QUEUE_SIZE, emSerial2RxQueue)
  #define EM_SERIAL2_RX_QUEUE_ADDR (&emSerial2RxQueue)
#else
  #define EM_SERIAL2_RX_QUEUE_ADDR (NULL)
  #define emSerial2RxQueue (NULL)
#endif

#if SL_LEGACY_SERIAL3_MODE != SL_LEGACY_SERIAL_UNUSED
DEFINE_FIFO_QUEUE(SL_LEGACY_SERIAL3_RX_QUEUE_SIZE, emSerial3RxQueue)
  #define EM_SERIAL3_RX_QUEUE_ADDR (&emSerial3RxQueue)
#else
  #define EM_SERIAL3_RX_QUEUE_ADDR (NULL)
  #define emSerial3RxQueue (NULL)
#endif

#if SL_LEGACY_SERIAL4_MODE != SL_LEGACY_SERIAL_UNUSED
DEFINE_FIFO_QUEUE(SL_LEGACY_SERIAL4_RX_QUEUE_SIZE, emSerial4RxQueue)
  #define EM_SERIAL4_RX_QUEUE_ADDR (&emSerial4RxQueue)
#else
  #define EM_SERIAL4_RX_QUEUE_ADDR (NULL)
  #define emSerial4RxQueue (NULL)
#endif

//------------------------------------------------------
// Easy access to data structures for a particular port

// The FOR_EACH_PORT(CAST,PREFIX_,_SUFFIX) macro will expand in to something like:
//    CAST(PREFIX_0_SUFFIX),
//    CAST(PREFIX_1_SUFFIX)
// with a line & number for each port of EM_NUM_SERIAL_PORTS

// Data structure for referencing TX Queues
//  (allows for different modes and queue sizes)
void *emSerialTxQueues[EM_NUM_SERIAL_PORTS] =
{ FOR_EACH_PORT((void *), EM_SERIAL, _TX_QUEUE_ADDR) };

uint16_t const emSerialTxQueueSizes[EM_NUM_SERIAL_PORTS] =
{ FOR_EACH_PORT((uint16_t), SL_LEGACY_SERIAL, _TX_QUEUE_SIZE) };

uint16_t const emSerialTxQueueMasks[EM_NUM_SERIAL_PORTS] =
{ FOR_EACH_PORT((uint16_t), SL_LEGACY_SERIAL, _TX_QUEUE_SIZE - 1) };

// Data structure for referencing RX Queues
//  (allows for different queue sizes)
EmSerialFifoQueue *emSerialRxQueues[EM_NUM_SERIAL_PORTS] =
{ FOR_EACH_PORT((EmSerialFifoQueue *), EM_SERIAL, _RX_QUEUE_ADDR) };

uint16_t const emSerialRxQueueSizes[EM_NUM_SERIAL_PORTS] =
{ FOR_EACH_PORT((uint16_t), SL_LEGACY_SERIAL, _RX_QUEUE_SIZE) };

// In-flash data structure for determined port mode
uint8_t const emSerialPortModes[EM_NUM_SERIAL_PORTS] =
{ FOR_EACH_PORT((uint8_t), SL_LEGACY_SERIAL, _MODE) };

uint16_t const emSerialTxQueueWraps[EM_NUM_SERIAL_PORTS] =
{ FOR_EACH_PORT((uint16_t), SL_LEGACY_SERIAL, _TX_QUEUE_SIZE) };
uint16_t const emSerialRxQueueWraps[EM_NUM_SERIAL_PORTS] =
{ FOR_EACH_PORT((uint16_t), SL_LEGACY_SERIAL, _RX_QUEUE_SIZE) };

#ifdef EM_ENABLE_SERIAL_BLOCKING
// In-flash data structure for blocking mode
bool const emSerialBlocking[EM_NUM_SERIAL_PORTS] =
{ FOR_EACH_PORT((bool), EM_SERIAL, _BLOCKSTATE) };
#endif

sl_status_t emSerialRxError[EM_NUM_SERIAL_PORTS] = { SL_STATUS_OK, };
uint16_t emSerialRxErrorIndex[EM_NUM_SERIAL_PORTS] = { 0, };
#endif //SL_LEGACY_SERIAL_USE_STDIO

//------------------------------------------------------
// Serial initialization

sl_status_t sli_legacy_serial_init(uint8_t port,
                                   SerialBaudRate rate,
                                   SerialParity parity,
                                   uint8_t stopBits)
{
#ifdef SL_LEGACY_SERIAL_USE_STDIO
  //When using stdio, bypass the serial library initialization
  //and just initialize the low level UART driver.
  return halInternalUartInit(port, rate, parity, stopBits);
#else //SL_LEGACY_SERIAL_USE_STDIO

  if (emberSerialUnused(port)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  switch (emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
    case SL_LEGACY_SERIAL_FIFO: {
      EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[port];
      {
        DECLARE_INTERRUPT_STATE_LITE;
        DISABLE_INTERRUPTS_LITE();
        q->used = 0;
        q->head = 0;
        q->tail = 0;
        RESTORE_INTERRUPTS_LITE();
      }
      break;
    }
#endif
    default:
      return SL_STATUS_INVALID_PARAMETER;
      //break;  //statement is unreachable
  }

#if     (defined(EM_ENABLE_SERIAL_FIFO))
  EmSerialFifoQueue *rq = emSerialRxQueues[port];
  {
    DECLARE_INTERRUPT_STATE_LITE;
    DISABLE_INTERRUPTS_LITE();
    rq->used = 0;
    rq->head = 0;
    rq->tail = 0;
    emSerialRxError[port] = SL_STATUS_OK;
    RESTORE_INTERRUPTS_LITE();
  }

  return halInternalUartInit(port, rate, parity, stopBits);
#endif//(defined(EM_ENABLE_SERIAL_FIFO))
#endif //SL_LEGACY_SERIAL_USE_STDIO
}

//------------------------------------------------------
// Serial Input

// returns # bytes available for reading
uint16_t sli_legacy_serial_read_available(uint8_t port)
{
#ifdef SL_LEGACY_SERIAL_USE_STDIO
  return halInternalPrintfReadAvailable();
#else //SL_LEGACY_SERIAL_USE_STDIO
  halInternalUartRxPump(port);
  return emSerialRxQueues[port]->used;
#endif //SL_LEGACY_SERIAL_USE_STDIO
}

sl_status_t sli_legacy_serial_read_byte(uint8_t port, uint8_t *dataByte)
{
#ifdef SL_LEGACY_SERIAL_USE_STDIO
  int ch = -1;
  ch = getchar();
  if (ch < 0) {
    return SL_STATUS_EMPTY;
  }
  *dataByte = (uint8_t)ch;
  return SL_STATUS_OK;
#else //SL_LEGACY_SERIAL_USE_STDIO
  uint32_t retval;
  EmSerialFifoQueue *q = emSerialRxQueues[port];

  if (emberSerialUnused(port)) {
    return SL_STATUS_FAIL;
  }

  if (emSerialRxError[port] != SL_STATUS_OK) {
    if (emSerialRxErrorIndex[port] == q->tail) {
      {
        DECLARE_INTERRUPT_STATE_LITE;
        DISABLE_INTERRUPTS_LITE();
        retval = emSerialRxError[port];
        emSerialRxError[port] = SL_STATUS_OK;
        RESTORE_INTERRUPTS_LITE();
      }
      return retval;
    }
  }

  halInternalUartRxPump(port);
  halInternalUartFlowControl(port);

  if ((q->used > 0) && (emSerialRxQueueWraps[port] > 0)) {
    {
      DECLARE_INTERRUPT_STATE_LITE;
      DISABLE_INTERRUPTS_LITE();
      *dataByte = FIFO_DEQUEUE(q, emSerialRxQueueWraps[port]);
      RESTORE_INTERRUPTS_LITE();
    }
    if (emSerialRxError[port] != SL_STATUS_OK) {
      //This index is used when there is an error when the FIFO is full.
      if (emSerialRxErrorIndex[port] == RX_FIFO_FULL) {
        //q->tail has advanced by one, we can now mark the head as the error
        emSerialRxErrorIndex[port] = q->head;
      }
    }
    return SL_STATUS_OK;
  } else {
    return SL_STATUS_EMPTY;
  }
#endif //SL_LEGACY_SERIAL_USE_STDIO
}

sl_status_t sl_legacy_serial_read_data(uint8_t port,
                                       uint8_t *data,
                                       uint16_t length,
                                       uint16_t *bytesRead)
{
  uint16_t bytesReadInternal = 0;
  sl_status_t status;

  while (bytesReadInternal < length) {
    status = sli_legacy_serial_read_byte(port, data);

    switch (status) {
      case SL_STATUS_OK:
        ++data;
        ++bytesReadInternal;
        break;

      case SL_STATUS_EMPTY:
        // empty queue is not an error for us, we just want to keep waiting
        break;

      default:
        // only store number of bytes read if the caller provided a non-NULL pointer
        if (bytesRead != NULL) {
          *bytesRead = bytesReadInternal;
        }
        return status;
    }
  }

  // only store number of bytes read if the caller provided a non-NULL pointer
  if (bytesRead != NULL) {
    *bytesRead = bytesReadInternal;
  }

  return SL_STATUS_OK;
}

#ifndef SL_ZIGBEE_TEST
sl_status_t emberSerialReadDataTimeout(uint8_t port,
                                       uint8_t *data,
                                       uint16_t length,
                                       uint16_t *bytesRead,
                                       uint16_t firstByteTimeout,
                                       uint16_t subsequentByteTimeout)
{
  uint16_t bytesReadInternal = 0;
  sl_status_t status;
  uint16_t timeout = firstByteTimeout;
  uint16_t startTime = halCommonGetInt16uMillisecondTick();

  // loop until we read the max number of bytes or the timeout elapses
  while (bytesReadInternal < length
         && elapsedTimeInt16u(startTime, halCommonGetInt16uMillisecondTick()) < timeout) {
    status = sli_legacy_serial_read_byte(port, data);

    switch (status) {
      case SL_STATUS_OK:
        ++data;
        ++bytesReadInternal;
        // reset timer and timeout for next character
        startTime = halCommonGetInt16uMillisecondTick();
        timeout = subsequentByteTimeout;
        break;

      case SL_STATUS_EMPTY:
        // empty queue is not an error for us, we just want to keep waiting
        break;

      default:
        // only store number of bytes read if the caller provided a non-NULL pointer
        if (bytesRead != 0U) {
          *bytesRead = bytesReadInternal;
        }
        return status;
    }
  }

  // only store number of bytes read if the caller provided a non-NULL pointer
  if (bytesRead != 0U) {
    *bytesRead = bytesReadInternal;
  }

  return bytesReadInternal == length ? SL_STATUS_OK : SL_STATUS_EMPTY;
}

#endif // SL_ZIGBEE_TEST

sl_status_t emberSerialReadPartialLine(uint8_t port, char *data, uint8_t max, uint8_t * index)
{
  sl_status_t err;
  uint8_t ch;

  if (((*index) == 0) || ((*index) >= max)) {
    data[0] = '\0';
  }

  for (;; ) {
    err = sli_legacy_serial_read_byte(port, &ch);

    // no new serial port char?, keep looping
    if (err != SL_STATUS_OK) {
      return err;
    }

    // handle bogus characters
    if ( ch > 0x7F ) {
      continue;
    }

    // handle leading newline - fogBUGZ # 584
    if (((*index) == 0)
        && ((ch == '\n') || (ch == 0))) {
      continue;
    }

    // Drop the CR, or NULL that is part of EOL sequence.
    if ((*index) >= max) {
      *index = 0;
      if ((ch == '\r') || (ch == 0)) {
        continue;
      }
    }

    // handle backspace
    if ( ch == 0x8 || ch == 0x7F ) {
      if ((*index) > 0 ) {
        // delete the last character from our string
        (*index)--;
        data[*index] = '\0';
        // echo backspace
        sli_legacy_serial_write_string(port, "\b \b");
      }
      // don't add or process this character
      continue;
    }

    //if the string is about to overflow, fake in a CR
    if ((*index) + 2 > max ) {
      ch = '\r';
    }

    sli_legacy_serial_write_byte(port, ch);  // term char echo

    //upcase that char
    if ( ch >= 'a' && ch <= 'z') {
      ch = ch - ('a' - 'A');
    }

    // build a string until we press enter
    if ((ch == '\r') || (ch == '\n')) {
      data[*index] = '\0';

      if (ch == '\r') {
        sli_legacy_serial_write_byte(port, '\n');  // "append" LF
        *index = 0;                       // Reset for next line; \n next
      } else {
        sli_legacy_serial_write_byte(port, '\r');  // "append" CR
        *index = max;                     // Reset for next line; \r,\0 next
      }

      return SL_STATUS_OK;
    }

    data[(*index)++] = ch;
  }
}

sl_status_t sli_legacy_serial_read_line(uint8_t port, char *data, uint8_t max)
{
  uint8_t index = 0;

  while (emberSerialReadPartialLine(port, data, max, &index) != SL_STATUS_OK) {
    halResetWatchdog();
  }
  return SL_STATUS_OK;
}

//------------------------------------------------------
// Serial Output

// returns # bytes (if fifo mode)/messages (if buffer mode) that can be written
uint16_t sli_legacy_serial_write_available(uint8_t port)
{
#ifdef SL_LEGACY_SERIAL_USE_STDIO
  return halInternalPrintfWriteAvailable();
#else //SL_LEGACY_SERIAL_USE_STDIO
  switch (emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
    case SL_LEGACY_SERIAL_FIFO:
      return emSerialTxQueueSizes[port]
             - ((EmSerialFifoQueue*)emSerialTxQueues[port])->used;
#endif
    default: {
    }
  }
  return 0;
#endif //SL_LEGACY_SERIAL_USE_STDIO
}

uint16_t emberSerialWriteUsed(uint8_t port)
{
#ifdef SL_LEGACY_SERIAL_USE_STDIO
  return 0;
#else
  return emSerialTxQueueSizes[port] - sli_legacy_serial_write_available(port);
#endif //SL_LEGACY_SERIAL_USE_STDIO
}

sl_status_t sli_legacy_serial_write_byte(uint8_t port, uint8_t dataByte)
{
  return sli_legacy_serial_write_data(port, &dataByte, 1);
}

#ifndef SL_LEGACY_SERIAL_USE_STDIO
#ifdef EM_ENABLE_SERIAL_FIFO
static bool getOutputFifoSpace(EmSerialFifoQueue *q,
                               uint8_t port,
                               uint16_t extraByteCount)
{
  return (q->used < emSerialTxQueueSizes[port] - extraByteCount);
}

#endif
#endif //SL_LEGACY_SERIAL_USE_STDIO

sl_status_t sli_legacy_serial_write_string(uint8_t port, const char *string)
{
#ifdef SL_LEGACY_SERIAL_USE_STDIO
  while (*string != '\0') {
    putchar(*string);
    string++;
  }
  return SL_STATUS_OK;
#else //SL_LEGACY_SERIAL_USE_STDIO
  switch (emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
    case SL_LEGACY_SERIAL_FIFO:
    {
      EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[port];
      bool kickStartTx = false;

      while (*string != '\0') {
        while (!getOutputFifoSpace(q, port, 0)) {
          if (kickStartTx) {
            halInternalStartUartTx(port);
            kickStartTx = false;
          }
          #ifdef EM_ENABLE_SERIAL_BLOCKING
          if (emSerialBlocking[port]) {
            simulatedSerialTimePasses();
            // Pet the watchdog here?
            continue;
          }
          #endif
          return SL_STATUS_HAS_OVERFLOWED;
        }
        {
          DECLARE_INTERRUPT_STATE_LITE;
          DISABLE_INTERRUPTS_LITE();
          if (q->used == 0) {
            kickStartTx = true;
          }
          if (emSerialTxQueueWraps[port] > 0) {
            FIFO_ENQUEUE(q, *string, emSerialTxQueueWraps[port]);
          }
          RESTORE_INTERRUPTS_LITE();
        }
        string++;
      }
      // make sure the interrupt is enabled so it will be sent
      halInternalStartUartTx(port);
      return SL_STATUS_OK;
    }
#endif
    default:
      return SL_STATUS_FAIL;
  }
#endif //SL_LEGACY_SERIAL_USE_STDIO
}

sl_status_t sli_legacy_serial_write_data(uint8_t port, uint8_t *data, uint8_t length)
{
#ifdef SL_LEGACY_SERIAL_USE_STDIO
  while (length--) {
    putchar(*data);
    data++;
  }
  return SL_STATUS_OK;
#else //SL_LEGACY_SERIAL_USE_STDIO
  switch (emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
    case SL_LEGACY_SERIAL_FIFO:
    {
      EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[port];
      bool kickStartTx = false;

      while (length-- != 0U) {
        while (!getOutputFifoSpace(q, port, 0)) {
          if (kickStartTx) {
            halInternalStartUartTx(port);
            kickStartTx = false;
          }
          #ifdef EM_ENABLE_SERIAL_BLOCKING
          if (emSerialBlocking[port]) {
            simulatedSerialTimePasses();
            // Pet the watchdog here?
            continue;
          }
          #endif
          return SL_STATUS_HAS_OVERFLOWED;
        }
        {
          DECLARE_INTERRUPT_STATE_LITE;
          DISABLE_INTERRUPTS_LITE();
          if (q->used == 0) {
            kickStartTx = true;
          }
          if (emSerialTxQueueWraps[port] > 0) {
            FIFO_ENQUEUE(q, *data, emSerialTxQueueWraps[port]);
          }
          RESTORE_INTERRUPTS_LITE();
        }
        data++;
      }
      // make sure the interrupt is enabled so it will be sent
      halInternalStartUartTx(port);
      return SL_STATUS_OK;
    }
#endif
    default:
      return SL_STATUS_FAIL;
  }
#endif //SL_LEGACY_SERIAL_USE_STDIO
}

sl_status_t sli_legacy_serial_wait_send(uint8_t port)  // waits for all byte to be written out of a port
{
#ifdef SL_LEGACY_SERIAL_USE_STDIO
  fflush(stdout);
#else //SL_LEGACY_SERIAL_USE_STDIO
  switch (emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
    case SL_LEGACY_SERIAL_FIFO: {
      EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[port];
      while (q->used != 0U) {
        simulatedSerialTimePasses();
      }
      break;
    }
#endif
    default: {
    }
  } // close switch.
  halInternalWaitUartTxComplete(port);
#endif //SL_LEGACY_SERIAL_USE_STDIO
  return SL_STATUS_OK;
}

//------------------------------------------------------
// Guaranteed output

// The _only_ Guaranteed API:  The usage model for this api
//   Does not require efficiency

#if !defined(EMBER_AF_PLUGIN_STANDARD_PRINTF_SUPPORT)

sl_status_t sli_legacy_serial_guaranteed_printf(uint8_t port, const char *formatString, ...)
{
#ifdef SL_LEGACY_SERIAL_USE_STDIO
  //TODO: This function might have to break stdio abstraction to operate
  //      with interrupts off and guarantee a print.
  //NOTE: The following code is the same code found in sli_legacy_serial_printf
  sl_status_t status;
  halInternalForcePrintf(true);
  va_list ap;
  va_start(ap, formatString);
  status = sli_legacy_serial_printf_var_arg(port, formatString, ap);
  va_end(ap);
  halInternalForcePrintf(false);
  return status;
#else //SL_LEGACY_SERIAL_USE_STDIO
  va_list ap;

  // prevent interrupt driven transmission from intermixing
  halInternalStopUartTx(port);
  va_start(ap, formatString);
  sli_util_printf_internal(halInternalForceWriteUartData, port, formatString, ap);
  va_end(ap);

  // re-enable interrupt driven transmission if needed
  switch (emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
    case SL_LEGACY_SERIAL_FIFO: {
      EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[port];
      if (q->used != 0U) {
        halInternalStartUartTx(port);
      }
      break;
    }
#endif
    default: {
    }
  } //close switch.

  return SL_STATUS_OK;
#endif //SL_LEGACY_SERIAL_USE_STDIO
}

#endif

//------------------------------------------------------
// Serial buffer maintenance
void sli_legacy_serial_flush_rx(uint8_t port)
{
#ifdef SL_LEGACY_SERIAL_USE_STDIO
  {
    DECLARE_INTERRUPT_STATE;
    DISABLE_INTERRUPTS();
    while (halInternalPrintfReadAvailable()) {
      (int)getchar();
    }
    RESTORE_INTERRUPTS();
  }
#else //SL_LEGACY_SERIAL_USE_STDIO
  EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialRxQueues[port];

  {
    DECLARE_INTERRUPT_STATE_LITE;
    DISABLE_INTERRUPTS_LITE();
    q->used = 0;
    q->head = 0;
    q->tail = 0;
    emSerialRxError[port] = SL_STATUS_OK;
    emSerialRxErrorIndex[port] = 0;
    RESTORE_INTERRUPTS_LITE();
  }
#endif //SL_LEGACY_SERIAL_USE_STDIO
}

//------------------------------------------------------
// Serial Buffer Cleanup Tick

bool emberSerialUnused(uint8_t port)
{
#ifdef SL_LEGACY_SERIAL_USE_STDIO
  return false;
#else //SL_LEGACY_SERIAL_USE_STDIO
  return (EM_NUM_SERIAL_PORTS <= port
          || emSerialPortModes[port] == SL_LEGACY_SERIAL_UNUSED);
#endif //SL_LEGACY_SERIAL_USE_STDIO
}

#endif //CORTEXM3_EFM32_MICRO
