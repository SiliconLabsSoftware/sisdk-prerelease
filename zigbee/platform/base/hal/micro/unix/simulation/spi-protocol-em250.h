/***************************************************************************//**
 * @file
 * @brief EM260 internal SPI Protocol implementation for use in simulation.
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
#ifndef __SPI_PROTOCOL_EM250_H__
#define __SPI_PROTOCOL_EM250_H__

/**
 * @description  A pointer to the length byte at the start of the Payload.
 * Upper layers will read the command from this location after
 * halHostSerialTick(false) returns true. The upper layer will write the
 * response to this location before calling halHostSerialTick(true). This
 * pointer is the upper layers' primary access into the command/response buffer.
 */
extern uint8_t *halHostFrame;

/**
 * @description  A flag that is set to true when the Host initiates the wake
 * handshake.
 */
extern bool spipFlagWakeFallingEdge;

/**
 * @description Returns true if SPIP is busy. Stubbed out in simulation.
 */
#define halHostSerialBusy()   (false)

/** @description
 *
 * The SPIP Init routine will also set a flag in the SPIP indicating a
 * wakeup handshake should be performed.  The handshake should only be
 * performed on a SerialTick.  Upon the next Tick call, the SPIP can assume
 * we are fully booted and operational and then take control peforming the
 * full handshake.
 */
void halHostSerialInit(void);

/**
 * @description Reinitializes the SPI Protocol when coming out of sleep
 * (powerdown).
 */
void halHostSerialPowerup(void);

/**
 * @description Shuts down the SPI Protocol when entering sleep (powerdown).
 */
void halHostSerialPowerdown(void);

/** @description
 *
 * halHostSerialTick is periodically called by the upper application just like
 * sl_zigbee_tick. The return bool from halHostSerialTick indicates to the upper
 * layer if there is data available to it in the RX buffer.The passed parameter
 * "responseReady" indicates to the SPIP if there is valid data in the TX
 * buffer.  If responseReady is true, the SPIP may immediately begin a transmit
 * DMA.  The SPIP is responsible for properly scheduling DMA transactions.
 * This Tick function maintains no acknowledgement so the two bool values
 * being passed in both directions are "single cycle" assertions.  This Tick
 * function is the primary method of passing CPU control to the SPIP.
 *
 * @param responseReady: If true, the SPIP may begin a transmit DMA.
 *
 * @return If true, the upper layer knows there is valid data in the command
 * buffer.
 */
bool halHostSerialTick(bool responseReady);

/** @description
 *
 * When the upper application has a callback it needs to deliver to the Host, it
 * calls halHostCallback() at will with haveData set to true. The HAL will
 * indicate to the Host through the nHOST_INT singal that there is a callback
 * pending. The EZSP application must make another call with haveData set to
 * false when there are no more callbacks pending.  The SPIP is responsible
 * for latching this call, timing actual nHOST_INT manipulation, and
 * multiplexing it in with SPIP generated assertions.
 *
 * @param haveData: true indicates there is a callback and the SPIP should
 * schedule nHOST_INT assertion.  false says the SPIP and deassert nHOST_INT.
 */
void halHostCallback(bool haveData);

/** @description Test function used by haltest. Nothing to see here...
 *
 * @param :
 *
 * @param :
 */
void spipTest(uint16_t test, uint16_t params);

#endif // __SPI_PROTOCOL_EM250_H__
