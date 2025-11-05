/***************************************************************************//**
 * @file
 * @brief Example AVR SPI Protocol implementation for interfacing with EM260.
 *
 * See @ref spi for documentation.
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
/** @addtogroup spi
 *
 * See also spi-protocol.h for source code.
 *
 * For complete documentation of the SPI Protocol, refer to the EM260 Datasheet.
 *
 *@{
 */

#ifndef __SPI_PROTOCOL_H__
#define __SPI_PROTOCOL_H__

#include "app/util/ezsp/ezsp-enum.h"

/**
 * @name SPI Protocol Interface
 */
//@{

/**
 * A pointer to the length byte at the start of the Payload. Upper
 * layers will write the command to this location before starting a transaction.
 * The upper layer will read the response from this location after a transaction
 * completes. This pointer is the upper layers' primary access into the
 * command/response buffer.
 */
extern uint8_t *halNcpFrame;

/**
 * This error byte is the third byte found in a special SPI
 * Protocol error case.  It provides more detail concerning the error.
 * Refer to the EM260 Datasheet for a more detailed description of this byte.
 * The application does not need to work with this byte, but it can be
 * useful information when developing.
 */
extern uint8_t halNcpSpipErrorByte;

/**
 * @brief Initializes the SPI Protocol.
 */
void halNcpSerialInit(void);

/**
 * @brief Reinitializes the SPI Protocol when coming out of sleep
 * (powerdown).
 */
void halNcpSerialPowerup(void);

/**
 * @brief Shuts down the SPI Protocol when entering sleep (powerdown).
 */
void halNcpSerialPowerdown(void);

/**
 * @brief Forcefully resets the EM260 by pulling on the nRESET line;
 * waits for the EM260 to boot; verifies that is has booted; verifies the
 * EM260 is active; verifies the SPI Protocol version.  When this function
 * returns, the EM260 is ready to accept all commands.
 *
 * This function is the same as halNcpHardResetReqBootload(), except that the
 * EM260 cannot be told to enter bootload mode through the nWAKE signal.
 *
 * @return A sl_zigbee_ezsp_status_t value indicating the success or failure of the
 * command.
 */
sl_zigbee_ezsp_status_t halNcpHardReset(void);

/**
 * @brief Forcefully resets the EM260 by pulling on the nRESET line;
 * sets the nWAKE signal based upon the state of the requestBootload bool;
 * waits for the EM260 to boot; verifies that is has booted; verifies the
 * EM260 is active; verifies the SPI Protocol version.  When this function
 * returns, the EM260 is ready to accept all commands.
 *
 * This function is the same as halNcpHardReset(), except that the ability to
 * request the EM260 enter bootload mode through the nWAKE signal is made
 * available.
 *
 * @return A sl_zigbee_ezsp_status_t value indicating the success or failure of the
 * command.
 */
sl_zigbee_ezsp_status_t halNcpHardResetReqBootload(bool requestBootload);

/**
 * @brief If the Host thinks that the EM260 is sleeping and wants to
 * wake it up, the EZSP calls halNcpWakeUp().
 *
 * Waking up can take some time
 * (milliseconds) so halNcpWakeUp() returns immediately and the SPI Protocol
 * calls halNcpIsAwakeIsr() once the wakeup handshaking is complete and the
 * EM260 is ready to accept commands.
 */
void halNcpWakeUp(void);

/**
 * @brief The EZSP writes a command into the command buffer and then
 * calls halNcpSendCommand().
 *
 * This function assumes the command being sent
 * is an EZSP frame and therefore sets the SPI Byte for an EZSP Frame.  If
 * sending a command other than EZSP, use halNcpSendRawCommand().  This
 * function returns immediately after transmission of the Command has
 * completed and the transaction has entered the Wait section. The EZSP must
 * now call halNcpPollForResponse() until the Response is received.
 */
void halNcpSendCommand(void);

/**
 * @brief The upper layer writes a command into the command buffer and
 * then calls halNcpSendRawCommand().
 *
 * This function makes no assumption about
 * the data in the SpipBuffer, it will just faithly try to perform the
 * transaction. This function returns immediately after transmission of the
 * Command has completed and the transaction has entered the Wait section.
 * The upper layer must now call halNcpPollForResponse() until the the
 * Response is received.
 */
void halNcpSendRawCommand(void);

/**
 * @brief After sending a Command with halNcpSendCommand(), the upper
 * layer repeatedly calls this function until the SPI Protocol has finished
 * reception of a Response.
 *
 * @return A sl_zigbee_ezsp_status_t value indicating the success or failure of the
 * command.
 */
sl_zigbee_ezsp_status_t halNcpPollForResponse(void);

/**
 * @brief The SPI Protocol calls halNcpIsAwakeIsr() once the wakeup
 * handshaking is complete and the EM260 is ready to accept a command.
 *
 * @param isAwake  true if the wake handshake completed and the EM260 is awake.
 * false is the wake handshake failed and the EM260 is unresponsive.
 */
void halNcpIsAwakeIsr(bool isAwake);

/** @brief If the Host wants to find out whether the EM260 has a
 * pending callback, the EZSP calls halNcpHasData(). If this function returns
 * true then the EZSP will send a callback command.
 */
bool halNcpHasData(void);

/**
 * @brief Transmits the SPI Protocol Version Command and checks the
 * response against a literal value to verify the SPI Protocol version.
 *
 * @return true if the SPI Protocol Version used in this function matches the
 * version returned by the EM260.  false is the versions do not match.
 */
bool halNcpVerifySpiProtocolVersion(void);

/**
 * @brief Transmits the SPI Status Command and checks the
 * response against a literal value to verify the SPI Protocol is active.
 *
 * @return true if the SPI Protocol is active. false if the SPI Protocol is
 * not active.
 */
bool halNcpVerifySpiProtocolActive(void);

/**
 * @brief Retrieves the file descriptor for the nHOST_INT line, which is what
 * the NCP pulls low when it has something to transmit to the host.
 *
 * @return A valid file descriptor, else -1.
 */
int halNcpGetIntFd(void);

//@}

/**
 * @name SPI Protocol Test Function
 */
//@{

/**
 * @brief A collection of code for use in testing pieces of the SPI
 * Protocol.
 *
 * The tests available here undocumented and unused by a formal
 * application.  They can be called from any test application or used as
 * inspiration or a template for further testing.  To invoke a test, simply
 * call spipTest() with the desired parameters. (For example, this function
 * can be accessed through a user driven interface to provide a convenient
 * method for invoking a test or set of tests.)
 *
 * @param test  The number provided here invokes a specific piece of the test
 * code.  For example, '1' loads the Command buffer with the SPI Protocol
 * Version Commmand and '0' initiates a transaction with whatever data is in
 * buffer.
 *
 * @param params  The parameters passed into a given test.  For example, test
 * 'A' simply write a byte on the SPI.  The byte written is provided by params.
 */
void spipTest(uint16_t test, uint16_t params);

//@}

#endif // __SPI_PROTOCOL_H__

/** @} END addtogroup */
