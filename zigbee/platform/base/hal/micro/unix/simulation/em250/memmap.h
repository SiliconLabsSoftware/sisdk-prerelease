/***************************************************************************//**
 * @file
 * @brief Definitions for PROG and DATA Space memory layout shared by BTL,
 * STK & APP.
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * !!! WARNING !!!      Keep in sync with memmap.xai
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
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
#ifndef __MEMMAP_H__
#define __MEMMAP_H__    1

/******************************************************************************
 * BTL+STK+APP (BootLoader + emberNet Stack + Customer Applications)
 *
 * Prog Space layout:
 *      Word addr   Size(bytes)  Win  Purpose
 *   ROM - Window 0: Initialization
 *    0x00000+1400   == 10240     0   Btl INFO CONST INITC CODE
 *    0x01400+0040   ==   128     0   Stk INFO
 *   ROM - Windows 0-6: Stk/App Code
 *    0x01440+CBC0   <=104320    0-6  Stk+App CODE  (<= 101.875 KB)
 *  [ROM - Window 6: Stk/App Big-Const; CODE reduced by 16 KB]
 *    0x0C000+2000   <= 16384     6   Stk+App BONST (<= 16 KB
 *    0x0DFC0                                            :
 *    0x0E000-0040   ==   128     0   App INFO       combined)
 *   ROM - Window 7: Stk/App Constants, InitC, Sim EEPROM (unusable for Code)
 *    0x0E000+1000   <=  8192     7   Stk+App CONST (<=  8 KB
 *    0x0E600                                            :
 *    0x0F000-0A00   <=  5120     7   Stk+App INITC  combined)
 *    0x0F000+1000   ==  8192     7   SIM EE
 *   ROM - Mapped RAM (for Code)
 *    0x0F000+0600   ==  3072     X   Data Space INV mapped to Prog Space
 *    0x0F600+0A00   ==  5120     X   Data Space RAM mapped to Prog Space
 *    0x10000  top   ==131072    (64 KW / 128 KB flash part)
 *
 *  NOTE: Prog Space is addressed only in words; _SIZEW definitions below
 *        are in units of words accordingly.
 *
 * Data Space layout:
 *      Byte addr   Size(bytes) Pages Purpose
 *   ROM - Window 0: Initialization
 *     0x0000+2400   ==  9216    288  Btl INFO CONST INITC CODE
 *     0x2400+0080   ==   128      4  Stk INFO
 *     0x2480+1B80   ==  7040    220  Stk+App CODE
 *   ROM - Windows 1-6: Stk/App Code
 *     0x0000+4000   == 16384    512  Stk+App CODE
 *  [ROM - Window 6: Stk/App Big-Const; CODE reduced by 16 KB]
 *     0x0000+4000   <= 16384    512  Stk+App BONST (<= 16 KB
 *     0x3F80                                            :
 *     0x4000-0080   ==   128      4  App INFO       combined)
 *   ROM - Window 7: Stk/App Constants, InitC, Sim EEPROM (unusable for Code)
 *     0x0000+2000   <=  8192    256  Stk+App CONST (<=  8 KB
 *     0x0C00                                            :
 *     0x2000-1400   <=  5120    160  Stk+App INITC  combined)
 *     0x2000+2000   ==  8192    256  SIM EE
 *   REGisters
 *     0x4000+1000   ==  4096    128   KREG (Krakatoa REGisters)
 *   MFG ROM
 *     0x5000+0400   ==  1024     32   MFG FLASH_INFO block
 *   INValid
 *     0x5400+0C00   ==  3072     96   Invalid (unmapped)
 *    [0x6000+0100   ==   256      8   EMU (Emulator Registers)]
 *     0x6000+0C00   ==  3072     96   Invalid (unmapped)
 *   FAKE Mappings (overlaid on MFG ROM / INValid)
 *    (0x5000+0080   ==   128      4   Stk INFO      fake mapping)
 *    (0x5080+0380   ==   896     28   Unused        fake mapping)
 *    (0x5400+0080   ==   128      4   App INFO      fake mapping)
 *    (0x5480+0380   ==   896     28   Unused        fake mapping)
 *    (0x5800+1400   <=  5120    160   Stk+App INITC fake mapping) (<=  5 KB
 *    (0x6C00-1400   <=  5120    160   Stk+App INITC fake mapping)  combined)
 *   RAM
 *    App Region
 *     0x6C00+0200   >=   512     16   App STACK (unprot, pagealign)
 *     0x____+0020   ==    32      1   App PCB   (  prot, pagealign)
 *     0x____+0020   ==    32      1   App PSR   (unprot, pagealign)
 *     0x____+____   ==  ____      _._ App VAR   (__prot, pagealign)
 * IFDEF  PACKET_BUFFERS_ALLOCATED_BY_APP
 *                                    (App-allocated packet buffers in App VAR)
 * ENDIF ;PACKET_BUFFERS_ALLOCATED_BY_APP
 *     0x____+____   ==  ____      _._ App INIT  (unprot)
 *    Stk Region
 * IFNDEF PACKET_BUFFERS_ALLOCATED_BY_APP
 *     0x____-____   ==  ____      _   Stk BUFS  (  prot, pagealign)
 *                                               (Sized to fill remaining RAM)
 * ENDIF ;PACKET_BUFFERS_ALLOCATED_BY_APP
 *     0x____-0200   >=   512     16   Stk STACK (  prot)
 *     0x____-____   ==  ____      _._ Stk VAR   (  prot)
 * IFDEF  DEBUG_CHANNEL
 *     0x7FB0-____   ==  ____      _._ Stk INIT  (  prot)
 *     0x7FB0+0010   ==    16      0.5 Stk SIF   (  prot, pagealign)
 * ENDIF
 *     0x7FC0+0020   ==    32      1   Stk PCB   (  prot, pagealign)
 *     0x7FE0+0020   ==    32      1   Stk PSR   (  prot, pagealign) (C_RESERVED2)
 *     0x8000  top   == 32768    (16 KW / 32 KB Data region in XAP2b)
 *   BYTE ACCESS REGION
 *     0x8000+8000   == 32768    BYTE ACCESS REGION:  these addresses
 *                               access the same memory defined above,
 *                               but in byte-sized chunks
 *
 *  NOTE: Data space in XAP2b mode can be addressed in words or bytes; only
 *        even word addresses are valid.  So the size of something in words
 *        really consumes twice that size in the 0000-7FFF word address space,
 *        which is how we'll lay out memory below.  _SIZE definitions below
 *        are in units of *bytes* so they can be used for address arithmetic
 *        in both word and byte regions.  These sizes MUST all be even.
 *****************************************************************************/

#include "regs.h"               // For FLASH and RAM block parameters

#ifndef waddrinc
#define waddrinc                2
#endif//waddrinc

// Flash Constants

#define FLASH_TOTAL_SIZEW       0x010000u       // 64 KW / 128 KB flash

#define FLASH_WINDOW_SIZEW      (DATA_FLASH_SIZE / waddrinc)
#define FLASH_WINDOW_SIZE       DATA_FLASH_SIZE
#define FLASH_WINDOW_MASK       FLASH_WINDOW_FLASH_WINDOW_MASK

#define FLASH_PAGE_SIZEW        (1u << FLASH_PAGE_FLASH_PAGE_BIT)
#define FLASH_PAGE_SIZE         (FLASH_PAGE_SIZEW * waddrinc)
#define FLASH_PAGE_MASK         FLASH_PAGE_FLASH_PAGE_MASK
#define FLASH_PAGES             (1u << (16 - FLASH_PAGE_FLASH_PAGE_BIT))

#define FLASH_ROW_SIZEW         0x40u   // 64 words (columns) in a Flash Row
#define FLASH_COL_MASK          (FLASH_ROW_SIZEW - 1)
#define FLASH_ROW_MASK          (~FLASH_COL_MASK)

// RAM Constants

#define RAM_TOTAL_SIZE          DATA_RAM_SIZE
#define RAM_PAGE_SIZE           0x20u           // RAM made up of 32-byte pages

// PROG Space :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

#define PROG_SPACE_BASEW        0x000000u
#define PROG_SPACE_SIZEW        FLASH_TOTAL_SIZEW

#define PROG_BTL_BASEW          PROG_SPACE_BASEW

#ifdef  PROG_BTL_SIZEW       // Allow for PROG_BTL_SIZEW override
#if     (PROG_BTL_SIZEW & (FLASH_PAGE_SIZEW - 1))
#error  PROG_BTL_SIZEW must be multiple of FLASH_PAGE_SIZEW
#endif//(PROG_BTL_SIZEW & (FLASH_PAGE_SIZEW - 1))
#else//!PROG_BTL_SIZEW       // Allow for PROG_BTL_SIZEW override
#ifdef APP_BTL
#define PROG_BTL_SIZEW          0x000A00u  // build for application bootloader
#else//!APP_BTL
#ifdef SERIAL_UART_BTL
#define PROG_BTL_SIZEW          0x000A00u  // build for serial uart bootloader
#else//!SERIAL_UART_BTL
#define PROG_BTL_SIZEW          0x001400u  // default to standalone bootloader
#endif//!SERIAL_UART_BTL
#endif//!APP_BTL
#endif//PROG_BTL_SIZEW       // Allow for PROG_BTL_SIZEW override

#define PROG_BTL_INFO_BASEW     PROG_BTL_BASEW
#define PROG_BTL_INFO_SIZEW     0x000014u
#define PROG_STK_INFO_BASEW     (PROG_BTL_BASEW + PROG_BTL_SIZEW)
#define PROG_STK_INFO_SIZEW     0x000040u
#define PROG_APP_INFO_SIZEW     0x000040u
#define PROG_APP_INFO_BASEW     (0x00E000u - PROG_APP_INFO_SIZEW)

#define PROG_CODE_BASEW         (PROG_STK_INFO_BASEW + PROG_STK_INFO_SIZEW)
#define PROG_CODE_SIZEW         (0x00E000u - PROG_CODE_BASEW) // Assume no BONST or APP_INFO
#define PROG_STK_CODE_BASEW     PROG_CODE_BASEW
#define PROG_STK_CODE_SIZEW     PROG_CODE_SIZEW
#define PROG_APP_CODE_BASEW     PROG_STK_CODE_BASEW     // Shared
#define PROG_APP_CODE_SIZEW     PROG_STK_CODE_SIZEW     // Shared

#define PROG_BONST_BASEW        0x00C000u               // Big-CONST option
#define PROG_BONST_SIZEW        0x002000u               // Big-CONST option assume no APP_INFO
#define PROG_STK_BONST_BASEW    PROG_BONST_BASEW        // Big-CONST option
#define PROG_STK_BONST_SIZEW    PROG_BONST_SIZEW        // Big-CONST option
#define PROG_APP_BONST_BASEW    PROG_STK_BONST_BASEW    // Shared
#define PROG_APP_BONST_SIZEW    (PROG_STK_BONST_SIZEW - PROG_APP_INFO_SIZEW) // Shared

#define PROG_CONST_BASEW        0x00E000u
#define PROG_CONST_SIZEW        0x001000u
#define PROG_STK_CONST_BASEW    PROG_CONST_BASEW
#define PROG_STK_CONST_SIZEW    PROG_CONST_SIZEW
#define PROG_APP_CONST_BASEW    PROG_STK_CONST_BASEW    // Shared
#define PROG_APP_CONST_SIZEW    PROG_STK_CONST_SIZEW    // Shared

#define PROG_INITC_TOPW         (PROG_CONST_BASEW + PROG_CONST_SIZEW)
#define PROG_INITC_SIZEW        (DATA_RAM_SIZE / waddrinc)
#define PROG_STK_INITC_TOPW     PROG_INITC_TOPW
#define PROG_STK_INITC_SIZEW    PROG_INITC_SIZEW
#define PROG_APP_INITC_TOPW     PROG_INITC_TOPW         // Shared, from top
#define PROG_APP_INITC_SIZEW    PROG_INITC_SIZEW        // Shared, from top

#define PROG_SIMEE_BASEW        0x00F000u
#define PROG_SIMEE_SIZEW        0x001000u
#if     ((PROG_SIMEE_BASEW % FLASH_PAGE_SIZEW) != 0)
# error PROG_SIMEE_BASEW must be aligned on a FLASH_PAGE_SIZEW boundary
#endif
#if     ((PROG_SIMEE_SIZEW % (FLASH_PAGE_SIZEW * 2)) != 0)
# error PROG_SIMEE_SIZEW must be an even multiple of FLASH_PAGE_SIZEW
#endif
#if     ((PROG_CODE_BASEW  + PROG_CODE_SIZEW) > PROG_SIMEE_BASEW)
# error PROG_CODE encroaches into PROG_SIMEE region; check PROG_CODE_SIZEW
#endif
#if     ((PROG_CONST_BASEW + PROG_CONST_SIZEW) > PROG_SIMEE_BASEW)
# error PROG_CONST encroaches into PROG_SIMEE region; check PROG_CONST_SIZEW
#endif

#define PROG_RAMMAP_SIZEW       (DATA_RAM_SIZE / waddrinc)
#define PROG_RAMMAP_BASEW       (FLASH_TOTAL_SIZEW - PROG_RAMMAP_SIZEW)

#define PROG_SPACE_TOPW         (PROG_SPACE_BASEW + PROG_SPACE_SIZEW)

// DATA Space :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

#define DATA_SPACE_BASE         0x0000u
#define DATA_SPACE_SIZE         0x8000u

#define DATA_ROM_BASE           DATA_FLASH_BASE
#define DATA_ROM_SIZE           DATA_FLASH_SIZE

#define DATA_REG_BASE           DATA_CHIP_BASE
#define DATA_REG_SIZE           (DATA_FLASH_INFO_BASE - DATA_REG_BASE)

#define DATA_MFG_BASE           DATA_FLASH_INFO_BASE
#define DATA_MFG_SIZE           DATA_FLASH_INFO_SIZE

#define DATA_EMU_BASE           DATA_EMU_REGS_BASE
#define DATA_EMU_SIZE           DATA_EMU_REGS_SIZE

#define DATA_INV_BASE           (DATA_MFG_BASE + DATA_MFG_SIZE)
#define DATA_INV_SIZE           (DATA_RAM_BASE - DATA_INV_BASE)

//      DATA_RAM_BASE           // defined in regs.h
//      DATA_RAM_SIZE           // defined in regs.h

#define DATA_FAKE_BASE          DATA_MFG_BASE
#define DATA_FAKE_SIZE          (DATA_RAM_BASE - DATA_FAKE_BASE)

#define DATA_SPACE_TOP          (DATA_SPACE_BASE + DATA_SPACE_SIZE)

// DATA Space - ROM Window 0 ::::::::::::::::::::::::::::::::::::::::::::::::::

#define DROM_BTL_BASE                   ((PROG_BTL_BASEW       * waddrinc) & (FLASH_WINDOW_SIZE - 1))
#define DROM_BTL_SIZE                   (PROG_BTL_SIZEW       * waddrinc)
#define DROM_STK_INFO_BASE              ((PROG_STK_INFO_BASEW  * waddrinc) & (FLASH_WINDOW_SIZE - 1))
#define DROM_STK_INFO_SIZE              (PROG_STK_INFO_SIZEW  * waddrinc)
#define DROM_APP_INFO_BASE              ((PROG_APP_INFO_BASEW  * waddrinc) & (FLASH_WINDOW_SIZE - 1))
#define DROM_APP_INFO_SIZE              (PROG_APP_INFO_SIZEW  * waddrinc)

// DATA Space - ROM Window 6 ::::::::::::::::::::::::::::::::::::::::::::::::::

#define DROM_BONST_BASE                 ((PROG_BONST_BASEW     * waddrinc) & (FLASH_WINDOW_SIZE - 1))
#define DROM_BONST_SIZE                 (PROG_BONST_SIZEW     * waddrinc)
#define DROM_STK_BONST_BASE             ((PROG_STK_BONST_BASEW * waddrinc) & (FLASH_WINDOW_SIZE - 1))
#define DROM_STK_BONST_SIZE             (PROG_STK_BONST_SIZEW * waddrinc)
#define DROM_APP_BONST_BASE             ((PROG_APP_BONST_BASEW * waddrinc) & (FLASH_WINDOW_SIZE - 1))
#define DROM_APP_BONST_SIZE             (PROG_APP_BONST_SIZEW * waddrinc)

// DATA Space - ROM Window 7 ::::::::::::::::::::::::::::::::::::::::::::::::::

#define DROM_CONST_BASE                 ((PROG_CONST_BASEW     * waddrinc) & (FLASH_WINDOW_SIZE - 1))
#define DROM_CONST_SIZE                 (PROG_CONST_SIZEW     * waddrinc)
#define DROM_STK_CONST_BASE             ((PROG_STK_CONST_BASEW * waddrinc) & (FLASH_WINDOW_SIZE - 1))
#define DROM_STK_CONST_SIZE             (PROG_STK_CONST_SIZEW * waddrinc)
#define DROM_APP_CONST_BASE             ((PROG_APP_CONST_BASEW * waddrinc) & (FLASH_WINDOW_SIZE - 1))
#define DROM_APP_CONST_SIZE             (PROG_APP_CONST_SIZEW * waddrinc)

#define DROM_INITC_TOP                  ((PROG_INITC_TOPW      * waddrinc) & (FLASH_WINDOW_SIZE - 1))
#define DROM_INITC_SIZE                 (PROG_INITC_SIZEW     * waddrinc)
#define DROM_STK_INITC_TOP              ((PROG_STK_INITC_TOPW  * waddrinc) & (FLASH_WINDOW_SIZE - 1))
#define DROM_STK_INITC_SIZE             (PROG_STK_INITC_SIZEW * waddrinc)
#define DROM_APP_INITC_TOP              ((PROG_APP_INITC_TOPW  * waddrinc) & (FLASH_WINDOW_SIZE - 1))
#define DROM_APP_INITC_SIZE             (PROG_APP_INITC_SIZEW * waddrinc)

#define DROM_SIMEE_BASE                 ((PROG_SIMEE_BASEW * waddrinc) & (FLASH_WINDOW_SIZE - 1))
#define DROM_SIMEE_SIZE                 (PROG_SIMEE_SIZEW * waddrinc)

// DATA Space - FAKE mappings :::::::::::::::::::::::::::::::::::::::::::::::::

#define FAKE_STK_INFO_BASE              DATA_FAKE_BASE
#define FAKE_STK_INFO_SIZE              DROM_STK_INFO_SIZE

#define FAKE_APP_INFO_BASE              0x5400u
#define FAKE_APP_INFO_SIZE              DROM_APP_INFO_SIZE

#define FAKE_INITC_TOP                  (DATA_FAKE_BASE + DATA_FAKE_SIZE)
#define FAKE_INITC_SIZE                 DROM_INITC_SIZE
#define FAKE_STK_INITC_TOP              FAKE_INITC_TOP
#define FAKE_STK_INITC_SIZE             DROM_STK_INITC_SIZE
#define FAKE_APP_INITC_TOP              FAKE_INITC_TOP
#define FAKE_APP_INITC_SIZE             DROM_APP_INITC_SIZE

// Memory allocation & initialisation macros

#define STK_BONST_NULL_PAT              0x5BC0  // BONST sentinal at PROG_CONST_BASEW
#define STK_CONST_NULL_PAT              0x5EAD  // NULL pointer deref sentinal

#define BTL_STACK_INIT_PAT              0x6767  // Bootloader stack init pattern
#define STK_STACK_INIT_PAT              0x5555  // Stk app stack init pattern
#define APP_STACK_INIT_PAT              0x6666  // App app stack init pattern

// PROG<-->DROM space address translation

#define PROG_DRAM_MAP_REGION    0xF000
#define PROG_TO_DROM_ADDR(progAddr) \
  ((uint16_t *) ((((uint16_t) (progAddr)) & (FLASH_WINDOW_SIZEW - 1)) * waddrinc))
#define PROG_ADDR_TO_PAGE(progAddr) \
  (((uint16_t) (progAddr)) >> FLASH_PAGE_FLASH_PAGE_BIT)
#define PAGE_TO_PROG_ADDR(page) (((uint16_t) (page)) << FLASH_PAGE_FLASH_PAGE_BIT)
#define DRAM_TO_PROG_ADDR(dramAddr) \
  ((((uint16_t) (dramAddr)) / waddrinc) | PROG_DRAM_MAP_REGION)
#define DROM_TO_PROG_ADDR(dromAddr) \
  ((((uint16_t) (dramAddr)) / waddrinc))
#define DATA_TO_PROG(dataX)     ((dataX) / waddrinc)
#define PROG_TO_DATA(dataX)     ((dataX) * waddrinc)
#define SIMEE_ADDR_TO_PAGE(progAddr) \
  (((uint16_t) (progAddr)) >> FLASH_PAGE_FLASH_PAGE_BIT)
#define PAGE_TO_SIMEE_ADDR(page) (((uint16_t) (page)) << FLASH_PAGE_FLASH_PAGE_BIT)

#endif//__MEMMAP_H__
