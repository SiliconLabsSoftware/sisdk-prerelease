/***************************************************************************//**
 * @file
 * @brief Flash manipulation routines.
 *
 * See @ref flash for documentation.
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
/** @addtogroup flash
 *
 * @note
 * During an erase or a write the flash is not available,
 * which means code will not be executable from flash.  Therefore, these
 * routines copy their core functionality into RAM buffers for RAM execution.
 * <b>Additonally, this also means all interrupts will be disabled.</b>
 *
 * <b>Hardware documentation indicates 40us for a write and 21ms for an erase.</b>
 *
 *  Bit map of the Window and Page register access into flash memory:
 * @code
 * -----------------------------------------------------------------------
 * | 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
 * -----------------------------------------------------------------------
 * |    FLASH_PAGE  (pageSelect)     | (in_page - smallest erasable unit)|
 * -----------------------------------------------------------------------
 * | FLASH_WINDOW |           (in_window - CODE mapped to DATA)          |
 * -----------------------------------------------------------------------
 * @endcode
 *
 * See flash.h for source code.
 * All functions in flash.h return an ::EmberStatus value. See
 * error-def.h for definitions of all ::EmberStatus return values.
 *
 *@{
 */

#ifndef __EM250_FLASH_H__
#define __EM250_FLASH_H__

/** @brief Initializes the flash code.
 *
 * Page Erase and Block Write most
 * both be executed from RAM.  For this to occur, the code has to be copied
 * into RAM - halInternalFlashInit() triggers the copy.  Additionally, there is a simple
 * active flag that will supress an erase or write if flash has not been
 * initialized yet, preventing a potential crash.
 */
void halInternalFlashInit(void);

/** @brief Verify flash routines in RAM remain uncorrupted
 *
 * A paranoid application can use this to check if something has corrupted
 * the flash routines copied into RAM.  Returns SL_ZIGBEE_SUCCESS if all is well,
 * SL_ZIGBEE_ERR_FLASH_VERIFY_FAILED if corruption is detected.  An application
 * can then decide whether to crash and diagnose such corruption, or call
 * halInternalFlashInit() again to eradicate the corruption and recover.
 */
EmberStatus halInternalFlashVerifyInit(void);

/** @brief Erases a hardware page.
 *
 * A hardware page is defined to be
 * 0x0200 rows and each row is a word.  The flash is addressed from 0x0000
 * to 0xFFFF meaning there 128 hardware pages of size 0x0200 each.
 *
 * @param pageSelect  The page number to be erased.  The top 7 bits of the
 * address correspond to the hardware page.  This function expects pageSelect
 * to be those top 7 bits shifted right so they occupy the bottom 7 bits
 * of the 8 bit pageSelect.
 *
 * @return An ::EmberStatus value indicating the success or
 * failure of the command.
 */
EmberStatus halInternalFlashPageErase(uint8_t pageSelect);

/** @brief Reads a block of words from flash.
 *
 * @param addr    The absolute address to begin reading from.  The address is
 * NOT a pointer.  Due to the CPU's handling of pointers, words, and bytes,
 * the address must be passed around and worked with as a simple 16bit value.
 * At the critical moments the flash routines will cast the address to a
 * pointer for the actual hardware access.
 *
 * @param data    A pointer to where the data being read should be placed.
 *
 * @param length  The number of words to read.
 *
 * @return An ::EmberStatus value indicating the success or
 * failure of the command.
 */
EmberStatus halInternalFlashBlockRead(uint16_t addr, uint16_t *data, uint16_t length);

/** @brief Writes a block of words from flash.
 *
 * A page is erased
 * to 0xFFFF at every address.  A write can only flip a bit from 1 to 0.
 * Only two writes can be performed at the same address between page
 * erasures covering that address.
 *
 * @param addr    The absolute address to begin writing at.  The address is
 * NOT a pointer.  Due to the CPU's handling of pointers, words, and bytes,
 * the address must be passed around and worked with as a simple 16bit value.
 * At the critical moments the flash routines will cast the address to a
 * pointer for the actual hardware access.
 *
 * @param data    A pointer to the data that should be written.
 *
 * @param length  The number of words to write.
 *
 * @return An ::EmberStatus value indicating the success or
 * failure of the command.
 */
EmberStatus halInternalFlashBlockWrite(uint16_t addr, uint16_t *data, int16_t length);

#endif //__EM250_FLASH_H__

/** @} END addtogroup */
