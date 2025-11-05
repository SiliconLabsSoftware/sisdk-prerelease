/***************************************************************************//**
 * @file
 * @brief Header for flash manipulating functions.
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
#ifndef FLASH_H_
#define FLASH_H_

/**
 * Function to write a byte to the given flash address.
 *
 * @param uint16_t  Flash address.
 * @param int8_t   Byte value to write.
 */
void halFlashWrite(uint16_t address, int8_t value);

/**
 * Function to erase a flash page.
 *
 * @param uint16_t  Flash page address.
 */
void halFlashErase(uint16_t address);

/**
 * Check if the given flash page is empty or not.
 *
 * @param uint16_t  Address to check.
 * @return        true - Flash page empty / false - Flash page not empty.
 */
uint8_t halFlashBlankCheck(uint16_t address);

/**
 * Read a value from the given flash location.
 *
 * @param uint16_t  Flash address to read.
 *
 * @return        Byte value at the given location.
 */
uint8_t halFlashRead(uint16_t address);

#endif /* FLASH_H_ */
