/***************************************************************************//**
 * @file
 * @brief Minimal serial hardware abstraction layer interfaces.
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
#ifndef __HAL_SERIAL_MINIMAL_H__
#define __HAL_SERIAL_MINIMAL_H__

#if !defined(SL_LEGACY_SERIAL_TX_QUEUE_SIZE)
#define SL_LEGACY_SERIAL_TX_QUEUE_SIZE 255
#endif

#if !defined(SL_LEGACY_SERIAL_RX_QUEUE_SIZE)
#define SL_LEGACY_SERIAL_RX_QUEUE_SIZE 32
#endif

/**
 * @brief Macro used for declaring a Serial queue
 *
 * @param name The name assigned to the struct representing the queue
 *
 * @param size
 */
#define DECLARE_SERIAL_QUEUE(name, size) \
  static struct {                        \
    uint8_t head;                        \
    uint8_t tail;                        \
    uint8_t used;                        \
    uint8_t fifo[size];                  \
  } name

/**
 * @brief Macro that enqueues a byte of data in a FIFO queue.
 *
 * @param queue  Pointer to the FIFO queue.
 *
 * @param dataByte Data byte to be enqueued.
 *
 * @param size   Size used to control the wrap-around of the FIFO pointers.
 */
#define FIFO_ENQUEUE(queue, dataByte, size)     \
  do {                                          \
    (queue).fifo[(queue).head] = dataByte;      \
    (queue).head = (((queue).head + 1) % size); \
    (queue).used++;                             \
  } while (0)

/**
 * @brief Macro that de-queues a byte of data from a FIFO queue.
 *
 * @param queue Pointer to the FIFO queue.
 *
 * @param size  Size used to control the wrap-around of the FIFO pointers.
 */
#define FIFO_DEQUEUE(queue, size)             \
  (queue).fifo[(queue).tail];                 \
  (queue).tail = (((queue).tail + 1) % size); \
  (queue).used--

#ifdef DOXYGEN_SHOULD_SKIP_THIS

/**
 * @brief Assign numerical values for variables that hold Baud Rate
 * parameters.
 */
enum SerialBaudRate
#else
#ifndef DEFINE_BAUD
#define DEFINE_BAUD(num) BAUD_##num
#endif
typedef uint8_t SerialBaudRate;
enum
#endif //DOXYGEN_SHOULD_SKIP_THIS
{
  DEFINE_BAUD(300) = 0,  // BAUD_300
  DEFINE_BAUD(600) = 1,  // BAUD_600
  DEFINE_BAUD(900) = 2,  // etc...
  DEFINE_BAUD(1200) = 3,
  DEFINE_BAUD(2400) = 4,
  DEFINE_BAUD(4800) = 5,
  DEFINE_BAUD(9600) = 6,
  DEFINE_BAUD(14400) = 7,
  DEFINE_BAUD(19200) = 8,
  DEFINE_BAUD(28800) = 9,
  DEFINE_BAUD(38400) = 10,
  DEFINE_BAUD(50000) = 11,
  DEFINE_BAUD(57600) = 12,
  DEFINE_BAUD(76800) = 13,
  DEFINE_BAUD(100000) = 14,
  DEFINE_BAUD(115200) = 15,
  DEFINE_BAUD(230400) = 16,   /*<! define higher baud rates for the EM2XX and EM3XX */
  DEFINE_BAUD(460800) = 17,   /*<! Note: receiving data at baud rates > 115200 */
  DEFINE_BAUD(CUSTOM) = 18    /*<! may not be reliable due to interrupt latency */
};

/** @brief This function is typically called by ::halInternalPowerUpBoard()
 * and it is responsible for performing all the work internal to the UART
 * needed to restart the UART after a sleep cycle. (For example, resyncing the
 * DMA hardware and the serial FIFO.)
 */
void halInternalRestartUart(void);

/** @brief Initializes the UART to the given settings (same parameters
 * as  ::sli_legacy_serial_init() ).
 *
 *  @param rate     Baud rate (see  SerialBaudRate).
 *
 *  @return Returns false if initialization failed (invalid baud rate),
 * otherwise returns true;
 */
bool halInternalUartInit(SerialBaudRate rate);

/** @brief Called by serial code whenever anything is queued for
 * transmission to start any interrupt-driven transmission. May
 * be called when transmission is already in progess.
 */
void halInternalStartUartTx(void);

/** @brief Blocks until the UART has finished transmitting any data in
 * its hardware registers.
 */
void halInternalWaitUartTxComplete(void);

/**
 *  @brief Returns the number of free bytes in the TX queue.
 */
uint8_t halUartTxFifoAvailable(void);

/**
 *  @brief Returns the number of bytes available to be read in the RX queue.
 */
uint8_t halUartRxFifoAvailable(void);

/**
 *  @brief Writes a byte in the TX fifo. Returns true if there is available
 * space in the TX fifo and the byte has been enqueued successfully. Returns
 * false otherwise.
 *
 * @param dataByte The byte to be written
 */
bool halUartWriteByte(uint8_t dataByte);

/**
 *  @brief Reads a byte from the RX fifo. Returns true if there is at least a
 *  byte available in the TX fifo and the byte has been dequeued successfully.
 *  Returns false otherwise.
 *
 *  @param *dataByte The location where the read byte is written
 */
bool halUartReadByte(uint8_t *dataByte);

#endif // __HAL_SERIAL_MINIMAL_H__
