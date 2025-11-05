/***************************************************************************//**
 * @file
 * @brief Defines the PCB "structure" and shortcuts for accessing the context
 * area
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * !!! WARNING !!!      Keep in sync with pcb.xai
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
#ifndef __PCB_H__
#define __PCB_H__       1

#include "regs.h"

// PCB structure

typedef struct pcb_s {
  uint16_t pcbSavedFL;                            // Saved FLAGS | SYS_MODE
# define        pcbSavedSM      pcbSavedFL      //       SysMode overlaid
  uint16_t pcbSavedPC;                            // Saved PC (IX)
# define        pcbSavedIX      pcbSavedPC      //       IX synonym
  uint16_t pcbSavedAL;                            // Saved AL
  uint16_t pcbSavedAH;                            // Saved AH
# define        pcbFake2nd      pcbSavedAH      //       Fake int 2nd level mask
  uint16_t pcbSavedUX;                            // Saved UX
  uint16_t pcbSavedUY;                            // Saved UY (user-mode SP +16)
  uint16_t pcbSavedIY;                            // Saved IY (for scheduler)
  uint16_t pcbSavedIM;                            // Saved INT_CFG
  uint16_t pcbSavedSS;                            // Saved SchedStatus
  uint16_t pcbNewSS;                              // New SchedStatus on RTI
  uint16_t pcbOtherUY;                            // Other UY (user-mode SP +16)
  uint16_t pcbIsrLevel;                           // ISR level (mask) in process
  uint16_t pcbFakeTop;                            // Fake interrupt top level mask
  uint16_t pcbScratch0;                           // Scratch 0
  uint16_t pcbSavedSP;                            // Saved SP when UY not the SP
  uint16_t pcbPushedIY;                           // Orig IY in pushed Int Frame
} pcb_t;

#define PCB_SIZE                (sizeof(pcb_t))

#ifndef PSR_SIZE
#define PSR_SIZE                32      // bytes
#endif//PSR_SIZE

#define ContextToPcb(addr)      ((pcb_t *) (((uint16_t) addr) - PCB_SIZE - PSR_SIZE))

// crashInfo structure - gets overlaid on the pcb at crash time to record
//                       useful diagnostic particulars related to a crash

typedef struct crashInfo_s {
  uint16_t crashFL;               // pcbSavedFL   // FLAGS | SYS_MODE
  uint16_t crashPC;               // pcbSavedPC   // PC @ watchdog, memfault, etc.
  uint16_t crashProtFltPC;        // pcbSavedAL   // PROT_FLT_PC
  uint16_t crashAH;               // pcbSavedAH   // AH
  uint16_t crashUX;               // pcbSavedUX   // UX
  uint16_t crashUY;               // pcbSavedUY   // UY (stk/appContextSP)
  uint16_t crashIY;               // pcbSavedIY   // IY
  uint16_t crashIM;               // pcbSavedIM   // INT_CFG | INT_EN
  uint16_t crashSS;               // pcbSavedSS   // schedStatus
  uint16_t crashProtFltOP;        // pcbNewSS     // PROT_FLT_OP
  uint16_t crashOtherUY;          // pcbOtherUY   // app/stkContextSP
  uint16_t crashIS;               // pcbIsrLevel  // INT_FLAG
  uint16_t crashTrapNo;           // pcbFakeTop   // INT_SWCTRL
  uint16_t crashEvent;            // pcbScratch0  // merge w/RESET_EVENT see below
  uint16_t crashIntNest;          // pcbSavedSP   // nestedIntDepth<<8|nestedIntMax
  // also BootLoadMagicHi
  uint16_t crashValid;            // pcbPushedIY  // 0xA11D marks CrashInfo Valid
  // also BootLoadMagicLo
# define        crashDebug      crashValid      // also em250_load CAFE hack
} crashInfo_t;

#define CRASHINFO_SIZE          (sizeof(crashInfo_t))

// Sentinal Values

#define CRASH_DEBUG_ADDR        0x7FDE          // crashDebug fixed RAM location
#define CRASH_DEBUG_VALUE       0xCAFE          // Force sif-loop after reset
#define CRASH_VALID_VALUE       0xA11D          // CrashInfo is Valid
#define FORCE_SW_RESET_VALUE    0x3210          // From EM250 spec file SW_RESET

// Crash Events
// (really flavors of SW_RESET, OR'd with RESET_EVENT register)

#define CE_EVENT_MASK           0xFF00          // Mask for below events
#define CE_CLASS_MASK           0xF000          // Mask for the event classes
#define CE_RESET_MASK           0x00FF          // Mask for RESET_EVENT

#define CE_PCZERO_CLASS         0x1000          // PCZero class
#define CE_PCZERO_SYS           0x1100          // Branch-to-0 in Sys mode
#define CE_PCZERO_APP           0x1200          // Branch-to-0 in App mode
#define CE_PCZERO_OTHER         0x1F00          // Other/generic

#define CE_REBOOT_CLASS         0x2000          // Reboot class
#define CE_REBOOT_SOFTWARE      0x2100          // halReboot() Software reset
#define CE_REBOOT_ASSERT        0x2200          // assert() reset
#define CE_REBOOT_BOOTLOAD      0x2300          // halLaunchStandaloneBootloader() reset
#define CE_REBOOT_EXIT          0x2400          // exit() invoked
#define CE_REBOOT_F_VERIFY      0x2500          // flash verify failed reset
#define CE_REBOOT_F_INHIBIT     0x2600          // flash write inhibit reset
#define CE_REBOOT_BL_IMG_BAD    0x2700          // reset to app. Bootload image is bad
#define CE_REBOOT_OTHER         0x2F00          // Other/generic

#define CE_PROTFLT_CLASS        0x3000          // Protection fault class
#define CE_PROTFLT_NULL_DEREF   0x3100          // NULL pointer dereference
#define CE_PROTFLT_5EAD_DEREF   0x3200          // 5EAD at NULL pointer deref
#define CE_PROTFLT_DROM_DEREF   0x3500          // DROM (Flash) pointer deref
#define CE_PROTFLT_STACK_UFLOW  0x3300          // Stack underflow (PCB guard)
#define CE_PROTFLT_STACK_OFLOW  0x3400          // Stack overflow (INV region)
#define CE_PROTFLT_OTHER        0x3F00          // Other/generic

#define CE_WATCHDOG_CLASS       0x4000          // Watchdog class
#define CE_WATCHDOG_INT         0x4100          // Watchdog low-watermark int
#define CE_WATCHDOG_OTHER       0x4F00          // Other/generic

#define CE_CRASH_CLASS          0x5000          // Crash class
#define CE_CRASH_STACK_OFLOW    0x5100          // Stack bad/overflow detected
#define CE_CRASH_ASSERT         0x5200          // assert() alternative
#define CE_CRASH_SYSTRAP_FAIL   0x5300          // SysTrap failure
#define CE_CRASH_OTHER          0x5F00          // Other/generic

extern crashInfo_t nvCrashInfo;                 // Fixed to overlay STK PCB

extern void halInternalSysReset(uint16_t crashEvent);     // In cstartup

// bootParam structure - gets overlaid on crashInfo at bootloader launch time
//                       to pass relevant information into the bootloader.
//                       The bootloader may maintain a separate copy as well.
//                       When modifying structure membership,
//                       bootReserved must be updated to maintain size of
//                       struct (currently 32 bytes total).

typedef struct bootParam_s {
  uint16_t bootPanId;               // crashFL      // PanId that the bootloader will use
  uint8_t bootRadioChannel;         // crashP-FltPC // sli_zigbee_stack_get_radio_channel() - 11
  uint8_t bootRadioPower;           // crashP-FltPC // sli_zigbee_stack_get_radio_power()
  uint8_t bootRadioCfCal;           // crashAH      // Channel Filter calibration
  uint8_t bootRadioLnaCal;          // crashAH      // Low Noise Amplifier calibration
  uint8_t bootReserved[19];         // crashUX,...  // (reserved for future use)
  uint8_t bootMode;                 // crashTrapNo  // halLaunchStandaloneBootloader() mode arg
  uint16_t bootEvent;               // crashEvent   // CE_REBOOT_BOOTLOAD
  uint16_t bootMagicHi;             // crashIntNest // BootLoadMagicHi
  uint16_t bootValid;               // crashValid   // 0xA11D marks BootParam Valid
  // also BootLoadMagicLo
} bootParam_t;

#define BOOTPARAM_SIZE          (sizeof(bootParam_t))

extern bootParam_t nvStkBootParam;                // Fixed to overlay STK PCB

#endif//__PCB_H__
