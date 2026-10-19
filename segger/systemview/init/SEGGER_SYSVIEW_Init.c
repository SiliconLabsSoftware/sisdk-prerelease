/*********************************************************************
*                    SEGGER Microcontroller GmbH                     *
*                        The Embedded Experts                        *
**********************************************************************
*                                                                    *
*            (c) 1995 - 2019 SEGGER Microcontroller GmbH             *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       SEGGER SystemView * Real-time application analysis           *
*                                                                    *
**********************************************************************
*                                                                    *
* All rights reserved.                                               *
*                                                                    *
* SEGGER strongly recommends to not make any changes                 *
* to or modify the source code of this software in order to stay     *
* compatible with the SystemView and RTT protocol, and J-Link.       *
*                                                                    *
* Redistribution and use in source and binary forms, with or         *
* without modification, are permitted provided that the following    *
* condition is met:                                                  *
*                                                                    *
* o Redistributions of source code must retain the above copyright   *
*   notice, this condition and the following disclaimer.             *
*                                                                    *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND             *
* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,        *
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF           *
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE           *
* DISCLAIMED. IN NO EVENT SHALL SEGGER Microcontroller BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR           *
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT  *
* OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;    *
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF      *
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT          *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE  *
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH   *
* DAMAGE.                                                            *
*                                                                    *
**********************************************************************
*                                                                    *
*       SystemView version: 4.10                                     *
*                                                                    *
**********************************************************************
-------------------------- END-OF-HEADER -----------------------------

File    : SEGGER_SYSVIEW_Init.c
Purpose : Sample setup configuration of SystemView without an OS.
Revision: $Rev: 9599 $
*/

#include <stddef.h>
#include <stdbool.h>
#include "sl_sysview_custom_api.h"
#include "SEGGER_SYSVIEW.h"
#include "SEGGER_SYSVIEW_Conf.h"
#include "em_device.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#if (SEGGER_SYSVIEW_TIMESTAMP_SOURCE == SEGGER_SYSVIEW_TIMESTAMP_SOURCE_SLEEPTIMER)
#include "sl_sleeptimer.h"
#endif

#if defined(SL_CATALOG_SYSTEMVIEW_UART_RECORDER_PRESENT)
#include "sli_uart_recorder.h"
#endif

// System core clock frequency.
extern uint32_t SystemCoreClock;

static char _UpBufferCaptiveCore[SEGGER_SYSVIEW_RTT_BUFFER_SIZE];
static char _DownBufferCaptiveCore[8];
SEGGER_SYSVIEW_CORE_CONTEXT _ContextCaptiveCore;

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/
// The application name to be displayed in SystemViewer
#ifndef SYSVIEW_APP_NAME
#define SYSVIEW_APP_NAME        "Silicon Labs Simplicity SDK Application"
#endif

// The target device name
#ifndef SYSVIEW_DEVICE_NAME
#if (__CORTEX_M == 0U)
#define SYSVIEW_DEVICE_NAME     "Cortex-M0+"
#elif (__CORTEX_M == 3U)
#define SYSVIEW_DEVICE_NAME     "Cortex-M3"
#elif (__CORTEX_M == 4U)
#define SYSVIEW_DEVICE_NAME     "Cortex-M4"
#elif (__CORTEX_M == 33U)
#define SYSVIEW_DEVICE_NAME     "Cortex-M33"
#else
#define SYSVIEW_DEVICE_NAME     "Cortex-M"
#endif
#endif

// System Frequency. SystemcoreClock is used in most CMSIS compatible projects.
#define SYSVIEW_CPU_FREQ        (SystemCoreClock)

#if defined(SLI_SI917B0) && (SLI_SI917B0 == 1)
#define SYSVIEW_RAM_BASE        (0x400)
#else
// The lowest RAM address used for IDs (pointers)
#define SYSVIEW_RAM_BASE        (0x10000000)
#endif

#if defined(SL_CATALOG_MICRIUMOS_KERNEL_PRESENT)
#include "os.h"
#define SYSVIEW_KERNEL_MICRIUM_OS
extern SEGGER_SYSVIEW_OS_API SYSVIEW_X_OS_TraceAPI;
#elif defined(SL_CATALOG_FREERTOS_KERNEL_PRESENT)
#define SYSVIEW_KERNEL_FREERTOS
extern SEGGER_SYSVIEW_OS_API SYSVIEW_X_OS_TraceAPI;
#endif

static SEGGER_SYSVIEW_MODULE gecko_sdk_module;
static bool is_configured = false;

int SendSystemDescription_CaptiveCore(uint8_t core_id) {
  (void)core_id;
  U8 aPacket[SEGGER_SYSVIEW_INFO_SIZE + 1 + SEGGER_SYSVIEW_MAX_STRING_LEN];
  U8 EventId;
  U8* pPayload;
  U8* pPayloadStart;
  int Status;

  EventId = SYSVIEW_EVTID_SYSDESC;
  pPayloadStart = SEGGER_SYSVIEW_PREPARE_PACKET(aPacket);
  pPayload = pPayloadStart;

  // Add payload.
  pPayload = SEGGER_SYSVIEW_EncodeString(pPayload, "O=CaptiveCore", SEGGER_SYSVIEW_MAX_STRING_LEN);

  // Prepend event id.
  *--pPayloadStart = EventId;
  Status = SEGGER_SYSVIEW_SendPacket_Ex(&_ContextCaptiveCore, 0, pPayloadStart, pPayload);

  return Status;
}

static void _cbStart(void) {
  _ContextCaptiveCore.SysFreq = SYSVIEW_TIMESTAMP_FREQ;
  _ContextCaptiveCore.CPUFreq = SYSVIEW_CPU_FREQ;
  _ContextCaptiveCore.RAMBaseAddress = SEGGER_SYSVIEW_ID_BASE;

  SEGGER_SYSVIEW_Start_Ex(&_ContextCaptiveCore, 0);
  SendSystemDescription_CaptiveCore(1);
}

static void _cbStop(void) {
  SEGGER_SYSVIEW_Stop_Ex(&_ContextCaptiveCore);
}

/*********************************************************************
*
*       _cbSendSystemDesc()
*
*  Function description
*    Sends SystemView description strings.
*/
static void _cbSendSystemDesc(void) {
  SEGGER_SYSVIEW_SendSysDesc("N="SYSVIEW_APP_NAME",D="SYSVIEW_DEVICE_NAME);
  SEGGER_SYSVIEW_SendSysDesc("I#15=SysTick");

#if defined(SYSVIEW_KERNEL_MICRIUM_OS)
  SEGGER_SYSVIEW_SendSysDesc("O=Micrium OS Kernel");
#elif defined(SYSVIEW_KERNEL_FREERTOS)
  SEGGER_SYSVIEW_SendSysDesc("O=FreeRTOS");
#endif
}

/*********************************************************************
*
*       Global functions
*
**********************************************************************
*/

void SEGGER_SYSVIEW_Conf(void) {
  SEGGER_SYSVIEW_OS_API *os_api = NULL;

  // SEGGER_SYSVIEW_Conf() can run from C++ pre-init, sl_main_init() and the
  // RTOS start hooks. Re-running it would reset the SystemView RTT buffer
  // (dropping events already recorded) and allocate another captive core RTT
  // buffer.
  if (is_configured) {
    return;
  }
  is_configured = true;

  // Sleeptimer is not initialized here: SEGGER_SYSVIEW_Conf() may run before
  // clocks are set up. Timestamps read 0 until sl_sleeptimer_init() and the
  // frequency is updated with sli_systemview_update_timestamp_freq().

#if (SEGGER_SYSVIEW_TIMESTAMP_SOURCE == SEGGER_SYSVIEW_TIMESTAMP_SOURCE_DWT) && !defined(SL_COMPONENT_CATALOG_KERNEL_PRESENT)
  // Enable DWT and ensure it is functionnal even when no debugger attached.
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  ITM->LAR          = 0xc5acce55;
  DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;
#endif

  // Register Simplicity SDK module
  gecko_sdk_module.EventOffset = 0;
  gecko_sdk_module.NumEvents = 28671;
  gecko_sdk_module.pfSendModuleDesc = 0;
  gecko_sdk_module.pNext = 0;
  gecko_sdk_module.sModule = "M=GeckoSDK";

#if defined(SYSVIEW_KERNEL_MICRIUM_OS) || defined(SYSVIEW_KERNEL_FREERTOS)
  os_api = &SYSVIEW_X_OS_TraceAPI;
#endif

  SEGGER_SYSVIEW_Init_Ex(SYSVIEW_TIMESTAMP_FREQ, SYSVIEW_CPU_FREQ,
                         os_api, _cbSendSystemDesc, _cbStart, _cbStop);
  SEGGER_SYSVIEW_InitAdditionalBuffer(&_ContextCaptiveCore, &_UpBufferCaptiveCore[0], sizeof(_UpBufferCaptiveCore), &_DownBufferCaptiveCore[0], sizeof(_DownBufferCaptiveCore));
  SEGGER_SYSVIEW_SetRAMBase(SYSVIEW_RAM_BASE);
}

int sl_systemview_GetGSDKEventOffset(void) {
  return gecko_sdk_module.EventOffset;
}

bool sli_systemview_is_configured(void) {
  return is_configured;
}

void sli_systemview_start(void) {
  SEGGER_SYSVIEW_Conf();
  if (SEGGER_SYSVIEW_GetMainContext()->EnableState == 0u) {
    SEGGER_SYSVIEW_Start();
  }
}

void sli_systemview_update_timestamp_freq(void) {
  SEGGER_SYSVIEW_CORE_CONTEXT *context = SEGGER_SYSVIEW_GetMainContext();
  U32 timestamp_freq = SYSVIEW_TIMESTAMP_FREQ;
  U32 cpu_freq = SYSVIEW_CPU_FREQ;

  context->SysFreq = timestamp_freq;
  context->CPUFreq = cpu_freq;
  // Captive-core context was configured during early Conf with a placeholder
  // frequency; keep it in sync with the main context once the source is ready.
  _ContextCaptiveCore.SysFreq = timestamp_freq;
  _ContextCaptiveCore.CPUFreq = cpu_freq;
}

/*************************** End of file ****************************/
