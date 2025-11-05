/***************************************************************************//**
 * @file
 * @brief SiLabs PRO2 API
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
#ifndef __PHY_SIPRO2_H__
#define __PHY_SIPRO2_H__

/** @brief SiLabs PRO2 API
 */

//== DEFINITIONS ==

#define PRO2PLUS_SDN_ASSERT_DELAY_US    10  // 10 us to discharge caps and whatnot
#define PRO2PLUS_SDN_DEASSERT_DELAY_US  5000 // SDN deassertion to chip ready time

//== API FUNCTIONS ==

/** @brief Activate PRO2
 * @return SL_STATUS_OK if successful
 */
extern void siPro2PowerUp(void);

/** @brief Shutdown PRO2
 * @return SL_STATUS_OK if successful
 */
extern void siPro2PowerDown(void);

/** @brief Initialize access to PRO2, activate it, and configure ISR callback
 * @return SL_STATUS_OK if successful
 */
extern sl_status_t siPro2Init(void (*isr)(void));

/** @brief Disable PRO2 interrupt
 */
extern void siPro2IntDisable(void);

/** @brief Enable PRO2 interrupt
 */
extern void siPro2IntEnable(void);

/** @brief Clear any pending PRO2 interrupt
 */
void siPro2IntClearPend(void);

/** @brief Get a single Fast Read Register (FRR)
 * @return The value of the FRR register
 */
extern uint8_t siPro2GetFRR(uint8_t reg);

/** @brief Get multiple Fast Read Registers (FRRs) in one operation
 *  @param startReg  The FRR register to start with
 *  @param numRegs   The number of FRR registers to read
 *  @param regValues Pointer to buffer to store numRegs values
 * @return SL_STATUS_OK on success
 */
extern sl_status_t siPro2GetFRRs(uint8_t startReg, uint8_t numRegs, uint8_t* regValues);

/** @brief Read data from the PRO2 RX FIFO
 *  @param rxLength Length in bytes of the data to retrieve
 *  @param rxData   Pointer to where rxLength bytes of retrieved data will
 *                  be written
 * @return SL_STATUS_OK on success
 */
extern sl_status_t siPro2ReadRxFifo(uint8_t rxLength, uint8_t* rxData);

/** @brief Write data to the PRO2 TX FIFO
 *  @param txLength Length in bytes of the data to put
 *  @param txData   Pointer to the data to put
 * @return SL_STATUS_OK on success
 */
extern sl_status_t siPro2WriteTxFifo(uint8_t txLength, uint8_t* txData);

/** @brief Wait for CTS (Ready)
 * @return SL_STATUS_OK on success, SL_ZIGBEE_ERR_FATAL on CTS wait timeout
 * @note Will use CTS GPIO if that's configured
 */
extern sl_status_t siPro2WaitForReady(void);

/** @brief Send a CTS-needing command and its parameters to the PRO2
 *  @param commandLength  Length in bytes of the command and its parameters
 *  @param commandData    Buffer containing command and its parameters
 * @return SL_STATUS_OK on success
 * @note Will wait for CTS prior to sending command, if command needs it
 */
extern sl_status_t siPro2SendCommand(uint8_t commandLength, uint8_t* commandData);
#define siPro2SendCommandNoResponse  siPro2SendCommand

/** @brief Get a command response from the PRO2
 *  @param responseLength Length in bytes of the response to get
 *  @param responseData   Pointer to where response bytes will be stored
 * @return SL_STATUS_OK on success
 * @note Will always wait for CTS prior to reading response
 */
extern sl_status_t siPro2GetResponse(uint8_t responseLength, uint8_t* responseData);

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
extern sl_status_t siPro2SendCommandGetResponse(uint8_t commandLength,
                                                uint8_t* commandData,
                                                uint8_t responseLength,
                                                uint8_t* responseData);

#endif//__PHY_SIPRO2_H__
