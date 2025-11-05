/***************************************************************************//**
 * @file
 * @brief SiLabs PRO2 API basics
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
#define LEGACY_PHY_BUILD 1
#include "phy/hal.h"
#include "sixxxx_cmd.h"

#include "spidrv.h"    // Use this API

#ifndef EMDRV_SPIDRV_INCLUDE_SLAVE
#error Need EMDRV_SPIDRV_INCLUDE_SLAVE defined in library build for worst-case-sized SPIDRV_HandleData_t
#endif//EMDRV_SPIDRV_INCLUDE_SLAVE

// eaSPI driver compatibility mappings
#define SPI_IDLE_BYTE                      0xFF

#include "sipro2.h"                         // Implement this API

#include "sl_si446x_radio.h" // replaces ext-device.h
//== COMPATIBLILTY DEFINES (EmberHAL ext-device) ==
#define HalExtDeviceIrqCB           sl_ext_device_handler_t
#define HalExtDeviceConfig          sl_ext_device_config_t
#define EXT_DEVICE_HAS_PWR          SL_EXT_DEVICE_HAS_POWER
#define EXT_DEVICE_HAS_RDY          SL_EXT_DEVICE_HAS_READY
#define EXT_DEVICE_HAS_SEL          SL_EXT_DEVICE_HAS_SELECT
#define EXT_DEVICE_HAS_INT          SL_EXT_DEVICE_HAS_INTERRUPT
#define EXT_DEVICE_HAS_RDY_IRQ      SL_EXT_DEVICE_HAS_READY_IRQ
#define EXT_DEVICE_HAS_INT_IRQ      SL_EXT_DEVICE_HAS_INTERRUPT_IRQ
#define HalExtDeviceIntLevel        sl_ext_device_interrupt_depth_t
#define EXT_DEVICE_INT_LEVEL_ON     SL_EXT_DEVICE_INTERRUPT_DEPTH_ON
#define EXT_DEVICE_INT_LEVEL_OFF    SL_EXT_DEVICE_INTERRUPT_DEPTH_OFF
#define EXT_DEVICE_INT_UNCONFIGURED SL_EXT_DEVICE_INTERRUPT_UNCONFIGURED
#define halExtDeviceInit            sl_ext_device_init
#define halExtDevicePowerDown       sl_ext_device_power_down
#define halExtDevicePowerUp         sl_ext_device_power_up
#define halExtDeviceIsReady         sl_ext_device_is_ready
#define halExtDeviceNotReady        sl_ext_device_not_ready
#define halExtDeviceWaitReady       sl_ext_device_wait_ready
#define halExtDeviceSelect          sl_ext_device_select
#define halExtDeviceDeselect        sl_ext_device_deselect
#define halExtDeviceIsSelected      sl_ext_device_is_selected
#define halExtDeviceNotSelected     sl_ext_device_not_selected
#define halExtDeviceIntPending      sl_ext_device_is_interrupt_pending
#define halExtDeviceIntNotPending   sl_ext_device_no_interrupt_pending
#define halExtDeviceIntDisable      sl_ext_device_disable_interrupt
#define halExtDeviceIntEnable       sl_ext_device_enable_interrupt
#define pro2SpiConfig               sl_ext_device_spi_config // DO NOT LOOK INSIDE
#define spiHandle                   sl_ext_device_spi_handle // DO NOT LOOK INSIDE

/** @brief SiLabs PRO2 API
 */

//== DEFINITIONS ==

#define NULL_CB NULL                        // Null callback

#define SIPRO2_CTS_MAX_RETRY_TIME_MS 300    // Be somewhat generous
//FIXME: Can app tolerate spin-delays of up to 300 ms (ints on at least)??
#define SIPRO2_INTER_CTS_PROBE_DELAY_US 5   // UGLY but helps SPI-Spy

#define SIPRO2_IS_CTS_VALUE(byte) ((byte) == 0xFF)

#define SIPRO2_CMD_NEEDS_CTS(cmd) (((cmd) < CMD_READ_CMD_BUFF) \
                                   || ((cmd) > CMD_READ_RX_FIFO))

//== LOCAL STATE

static bool ready = false; // true if PRO2 ready (CTS asserted)
static HalExtDeviceConfig devConfig; // External Device configuration from HAL
static bool (*isReadyProbeFunction)(void) = NULL; // CTS probe method
// A special static RAM buffer is needed to support split transfers
static uint8_t splitTrxBuf[150];  // Largest possible split transfer operation

// Target eaSPI driver

// Use spidrv with almost fully-configurable SPI port

typedef Ecode_t SpiStatus;
#define SPISTATUS_TO_EMBERSTATUS(SpiStatus) ((SpiStatus) ? SL_STATUS_FAIL \
                                             : SL_STATUS_OK)

static sl_status_t spiInit(void)
{
  static bool spiHandleInited = false;
  sl_status_t status;

  // Deal with spiInit() being re-called, e.g. after waking from sleep.
  // On efr32, SPI peripherals retain their state through deep sleep,
  // so if we're already inited there is no need to re-init.
  if (spiHandleInited) {
    status = SL_STATUS_OK;
  } else {
    // SPIDRV_Init() only reads SPIDRV_Init_t *initData so can safely
    // cast away any const-ness of pro2SpiConfig.
    status = SPISTATUS_TO_EMBERSTATUS(SPIDRV_Init(spiHandle, (SPIDRV_Init_t *)&pro2SpiConfig));
    spiHandleInited = (status == SL_STATUS_OK);
  }

  return status;
}

typedef uint8_t EmberSpiOpFlags;
enum {
  SL_ZIGBEE_SPI_OP_NO_FLAGS  = 0x00,
  SL_ZIGBEE_SPI_OP_SPLIT_LEN = 0x0F,
  SL_ZIGBEE_SPI_OP_SPLIT_TRX = 0x10,
};

static sl_status_t spiXfer(uint8_t* trxBuf1, uint8_t* trxBuf2,
                           int16_t trxLen, EmberSpiOpFlags opFlags)
{
  SpiStatus status;
  uint8_t splitLen = (opFlags & SL_ZIGBEE_SPI_OP_SPLIT_LEN);

  if (splitLen > 0) { // Split transfer -- trxBuf1 and trxBuf2 are non-NULL
    if ((uint32_t)trxLen > sizeof(splitTrxBuf)) {
      return SL_STATUS_INVALID_PARAMETER;
    }
    // EFR32 nSEL pin connected to a GPIO outside SPI peripheral.
    halExtDeviceSelect();
    // Split transfer copy-in
    memcpy(splitTrxBuf, trxBuf1, splitLen);
    if (opFlags & SL_ZIGBEE_SPI_OP_SPLIT_TRX) { // trxBuf2 is for Transmit only
      // Safe to treat trxBuf1 as transmit only in this case
      memcpy(&splitTrxBuf[splitLen], trxBuf2, trxLen - splitLen);
      status = SPIDRV_MTransmitB(spiHandle, splitTrxBuf, trxLen);
    } else {
      // memset(&splitTrxBuf[splitLen], SPI_IDLE_BYTE, trxLen - splitLen);
      status = SPIDRV_MTransferB(spiHandle, splitTrxBuf, splitTrxBuf, trxLen);
      if (SPISTATUS_TO_EMBERSTATUS(status) == SL_STATUS_OK) {
        // Split transfer copy-out
        memcpy(trxBuf1, splitTrxBuf, splitLen);
        memcpy(trxBuf2, &splitTrxBuf[splitLen], trxLen - splitLen);
      }
    }
  } else {
    // EFR32 nSEL pin connected to a GPIO outside SPI peripheral.
    halExtDeviceSelect();
    if (trxBuf2 == NULL) {
      // Necessary because SPIDRV_MTransferB() barfs if rxBuffer is NULL :-(
      status = SPIDRV_MTransmitB(spiHandle, trxBuf1, trxLen);
    } else {
      status = SPIDRV_MTransferB(spiHandle, trxBuf1, trxBuf2, trxLen);
    }
  }
  // EFR32 nSEL pin connected to a GPIO outside SPI peripheral.
  halExtDeviceDeselect();
  return SPISTATUS_TO_EMBERSTATUS(status);
}

//== INTERNAL FUNCTIONS

/** @brief Probe for CTS via SPI transaction method
 * @return true if PRO2 is clear-to-send; false if not
 */
static bool siPro2IsReady(void)
{
  if (SIPRO2_IS_CTS_VALUE(siPro2GetFRR(CMD_READ_CMD_BUFF))) {
    return true;
  }
  //FIXME: Conditionalize the delay?
  // Provide a little delay before allowing reprobe (FBO SPI-Spy)
  halCommonDelayMicroseconds(SIPRO2_INTER_CTS_PROBE_DELAY_US); // UGLY!
  return false;
}

/** @brief Basic send-command followed by write data
 *  @param cmd      Command to send
 *  @param txLength Length of data to write
 *  @param txData   Pointer to the data to write
 * @return SL_STATUS_OK on success
 */
static sl_status_t siPro2CmdWrite(uint8_t cmd, uint8_t txLength, uint8_t* txData)
{
  sl_status_t status;
  siPro2IntDisable();
  // Use a single split transfer to be as quick as possible
  status = spiXfer(&cmd, txData, sizeof(cmd) + txLength,
                   sizeof(cmd) | SL_ZIGBEE_SPI_OP_SPLIT_TRX);
  siPro2IntEnable();
  return status;
}

/** @brief Basic send-command followed by read data
 *  @param cmd      Command to send
 *  @param rxLength Length of data to read
 *  @param rxData   Pointer to where the read data will be stored
 * @return SL_STATUS_OK on success
 */
static sl_status_t siPro2CmdRead(uint8_t cmd, uint8_t rxLength, uint8_t* rxData)
{
  sl_status_t status;
  siPro2IntDisable();
  // Use a single split transfer to be as quick as possible
  status = spiXfer(&cmd, rxData, sizeof(cmd) + rxLength, sizeof(cmd));
  siPro2IntEnable();
  return status;
}

//== API FUNCTIONS ==

/** @brief Activate PRO2
 * @return SL_STATUS_OK if successful
 */
void siPro2PowerUp(void)
{
  ready = false; // Reinit on power up so first command forces CTS probe
  halExtDevicePowerUp();
}

/** @brief Shutdown PRO2
 * @return SL_STATUS_OK if successful
 */
void siPro2PowerDown(void)
{
  halExtDevicePowerDown();
}

/** @brief Initialize access to PRO2, activate it, and configure ISR callback
 * @return SL_STATUS_OK if successful
 */
sl_status_t siPro2Init(void (*isr)(void))
{
  devConfig = halExtDeviceInit((HalExtDeviceIrqCB) isr, NULL_CB);
  isReadyProbeFunction = (devConfig & EXT_DEVICE_HAS_RDY)
                         ? &halExtDeviceIsReady
                         : &siPro2IsReady;
  siPro2PowerDown();
  // Per PRO2 datasheet, its SPI operates in Mode 0, MSB first, max 10 MHz.
  sl_status_t status = spiInit();
  if (status != SL_STATUS_OK) {
    return status;
  }
  halCommonDelayMicroseconds(PRO2PLUS_SDN_ASSERT_DELAY_US);
  siPro2PowerUp();
  //TODO: Use a fixed delay here, or probe somehow for readiness?
  halCommonDelayMicroseconds(PRO2PLUS_SDN_DEASSERT_DELAY_US);
  return status;
}

/** @brief Disable PRO2 interrupt
 */
void siPro2IntDisable(void)
{
  (void) halExtDeviceIntDisable();
}

/** @brief Enable PRO2 interrupt
 */
void siPro2IntEnable(void)
{
  (void) halExtDeviceIntEnable(false);
}

/** @brief Clear any pending PRO2 interrupt
 */
void siPro2IntClearPend(void)
{
  (void) halExtDeviceIntDisable();
  (void) halExtDeviceIntEnable(true);
}

/** @brief Get a single Fast Read Register (FRR)
 * @return The value of the FRR register
 */
uint8_t siPro2GetFRR(uint8_t reg)
{
  uint8_t cmdRsp[2] = { reg, SPI_IDLE_BYTE };
  siPro2IntDisable();
  // Use a single transfer to be as quick as possible
  (void) spiXfer(cmdRsp, cmdRsp, sizeof(cmdRsp), SL_ZIGBEE_SPI_OP_NO_FLAGS);
  siPro2IntEnable();
  return cmdRsp[1];
}

/** @brief Get multiple Fast Read Registers (FRRs) in one operation
 *  @param startReg  The FRR register to start with
 *  @param numRegs   The number of FRR registers to read
 *  @param regValues Pointer to buffer to store numRegs values
 * @return SL_STATUS_OK on success
 */
sl_status_t siPro2GetFRRs(uint8_t startReg, uint8_t numRegs, uint8_t* regValues)
{
  return siPro2CmdRead(startReg, numRegs, regValues);
}

/** @brief Read data from the PRO2 RX FIFO
 *  @param rxLength Length in bytes of the data to retrieve
 *  @param rxData   Pointer to where rxLength bytes of retrieved data will
 *                  be written
 * @return SL_STATUS_OK on success
 */
sl_status_t siPro2ReadRxFifo(uint8_t rxLength, uint8_t* rxData)
{
  return siPro2CmdRead(CMD_READ_RX_FIFO, rxLength, rxData);
}

/** @brief Write data to the PRO2 TX FIFO
 *  @param txLength Length in bytes of the data to put
 *  @param txData   Pointer to the data to put
 * @return SL_STATUS_OK on success
 */
sl_status_t siPro2WriteTxFifo(uint8_t txLength, uint8_t* txData)
{
  return siPro2CmdWrite(CMD_WRITE_TX_FIFO, txLength, txData);
}

/** @brief Wait for CTS (Ready)
 * @return SL_STATUS_OK on success, SL_STATUS_FAIL on CTS wait timeout
 * @note Will use CTS GPIO if that's configured
 */
sl_status_t siPro2WaitForReady(void)
{
  if (!ready) {
    uint32_t probeStartTimeMs = halCommonGetInt32uMillisecondTick();
    halResetWatchdog();
    do {
      ready = (*isReadyProbeFunction)();
    } while ((!ready)
             && (elapsedTimeInt32u(probeStartTimeMs, halCommonGetInt32uMillisecondTick())
                 < SIPRO2_CTS_MAX_RETRY_TIME_MS));
    halResetWatchdog();
  }
  return ready ? SL_STATUS_OK : SL_STATUS_FAIL;
}

/** @brief Send a CTS-needing command and its parameters to the PRO2
 *  @param commandLength  Length in bytes of the command and its parameters
 *  @param commandData    Buffer containing command and its parameters
 * @return SL_STATUS_OK on success
 * @note Will wait for CTS prior to sending command, if command needs it
 */
sl_status_t siPro2SendCommand(uint8_t commandLength, uint8_t* commandData)
{
  bool cmdNeedsCts = SIPRO2_CMD_NEEDS_CTS(*commandData);
  sl_status_t status;

  if (cmdNeedsCts) {
    status = siPro2WaitForReady();
    if (status != SL_STATUS_OK) {
      return status;
    }
    ready = false; // The command will make the PRO2 not ready
  }
  siPro2IntDisable();
  status = spiXfer(commandData, NULL, commandLength, SL_ZIGBEE_SPI_OP_NO_FLAGS);
  siPro2IntEnable();
  return status;
}

/** @brief Get a command response from the PRO2
 *  @param responseLength Length in bytes of the response to get
 *  @param responseData   Pointer to where response bytes will be stored
 * @return SL_STATUS_OK on success
 * @note Will always wait for CTS prior to reading response
 */
sl_status_t siPro2GetResponse(uint8_t responseLength, uint8_t* responseData)
{
  sl_status_t status;
  uint32_t probeStartTimeMs = halCommonGetInt32uMillisecondTick();
  // It doesn't matter if we're already marked ready, we need to initiate
  // a CMD_READ_CMD_BUFF command anyway -- by doing it here rather than
  // using siPro2WaitForReady() we can merge the probe into the response
  // read, hopefully saving some time and avoiding multiple *successful*
  // CTS probes on the SPI bus.
  halResetWatchdog();
  do {
    uint8_t cmdRsp[2] = { CMD_READ_CMD_BUFF, SPI_IDLE_BYTE };
    siPro2IntDisable();
    // Use a single split transfer to be as quick as possible
    status = spiXfer(cmdRsp, responseData, sizeof(cmdRsp) + responseLength,
                     sizeof(cmdRsp));
    if ((status == SL_STATUS_OK) && SIPRO2_IS_CTS_VALUE(cmdRsp[1])) {
      ready = true;
      siPro2IntEnable();
      break;
    } else {
      siPro2IntEnable();
      status = SL_STATUS_FAIL; // In case we exit timed loop failing CTS
      // Provide a little delay before allowing reprobe (FBO SPI-Spy)
      halCommonDelayMicroseconds(SIPRO2_INTER_CTS_PROBE_DELAY_US); // UGLY!
    }
  } while (elapsedTimeInt32u(probeStartTimeMs, halCommonGetInt32uMillisecondTick())
           < SIPRO2_CTS_MAX_RETRY_TIME_MS);
  halResetWatchdog();
  return status;
}

/** @brief Send a CTS-needing command and its parameters to the PRO2 then
 *         get the PRO2's response.
 *  @param commandLength  Length in bytes of the command and its parameters
 *  @param commandData    Buffer containing command and its parameters
 *  @param responseLength Length in bytes of the response to get
 *  @param responseData   Pointer to where response bytes will be stored
 * @return SL_STATUS_OK on success
 * @note Will wait for CTS prior to sending command, and of course for getting
 *       response.
 */
sl_status_t siPro2SendCommandGetResponse(uint8_t commandLength,
                                         uint8_t* commandData,
                                         uint8_t responseLength,
                                         uint8_t* responseData)
{
  sl_status_t status;
  siPro2IntDisable(); // Ensure response is for the requested command!
  status = siPro2SendCommand(commandLength, commandData);
  if (status == SL_STATUS_OK) {
    status = siPro2GetResponse(responseLength, responseData);
  }
  siPro2IntEnable();
  return status;
}
