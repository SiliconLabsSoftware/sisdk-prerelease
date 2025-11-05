/***************************************************************************//**
 * @file
 * @brief Micro-specifics used with either bootloader.
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
#ifndef __UNIX_BOOTLOADER_SIM_H__
#define __UNIX_BOOTLOADER_SIM_H__

// everything is little endian
#define halInternalReadFlashByte(addr)   *addr
#define halInternalReadFlashWord(addr)   HIGH_LOW_TO_INT(*(addr + 1), *(addr))

// Constants used by the bootloader
//#define FLASH_TAB_POINTERS                        0x000F
//#define FLASH_TAB_POINTERS_DEFAULT_TOKEN_TAB_PTR  0       //DEPRECATED
//#define FLASH_TAB_POINTERS_UPPER_TOKEN_TAB_PTR    2       //DEPRECATED
#define FLASH_TAB_POINTERS_BOOT_TRAP_TAB_PTR      4
#define FLASH_TAB_POINTERS_HELPERFUNC_TAB         6
#define BOOTLOADER_TRAPTAB_SIGNATURE              0xBEEF

#define NVM_SEGMENT_MAX         64
#define NVM_APPLICATION_SPACE    0
#define NVM_DOWNLOAD_SPACE       1

#define BOOTLOADER_PROGRAM_SPACE   0
#define BOOTLOADER_DOWNLOAD_SPACE  1

#define MICRO_NVM_PAGE_SIZE 256
#define MICRO_NVM_PAGE_BASE_ADDRESS_MASK \
  (0xFFFF & ~((uint16_t)(MICRO_NVM_PAGE_SIZE - 1)))

#endif //__UNIX_BOOTLOADER_SIM_H__
