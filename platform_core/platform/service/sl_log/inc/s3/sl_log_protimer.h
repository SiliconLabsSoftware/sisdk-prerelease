/***************************************************************************//**
 * @file
 * @brief Minimal PROTIMER register surface for the logger (Series 3).
 *
 *   platform_core public device headers omit radio peripheral register
 *   definitions (including PROTIMER). Rather than bundle a full device header,
 *   the logger declares here only the registers it reads, at their fixed
 *   hardware offsets, plus the instance base address. The offsets and base
 *   address are identical on every Series 3 part that exposes a PROTIMER
 *   (verified across the SIMG/SIBG/SIWG/SIWN/SIWX/SIXG device families - later
 *   parts only append registers after the ones used here), so this view is
 *   generic across Series 3 and sl_log does not depend on em_device.h.
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 ******************************************************************************/

#ifndef SL_LOG_PROTIMER_H
#define SL_LOG_PROTIMER_H

#include <stdint.h>

/* When the full device header provides a PROTIMER definition (e.g. internal
 * SIWG353 builds that ship the radio register map), use it directly rather
 * than the minimal fallback below - defining our own would clash with the
 * device header's PROTIMER type and macro. Pull in em_device.h so that the
 * detection works regardless of the include order in the translation unit; on
 * platform_core public parts em_device.h exists but omits PROTIMER, so the
 * minimal view is declared instead. */
#include "em_device.h"

#ifndef PROTIMER

/* Minimal PROTIMER register block. Registers the logger does not touch are
 * still named so each field lands at its documented byte offset (shown in the
 * comments). Reading LPRECNT latches (L)BASECNT and (L)WRAPCNT so the 64-bit
 * time can be read coherently. */
typedef struct {
  volatile uint32_t IPVERSION;    /**< 0x000 IP version                        */
  volatile uint32_t EN;           /**< 0x004 Module enable                     */
  volatile uint32_t CTRL;         /**< 0x008 Control                           */
  volatile uint32_t CMD;          /**< 0x00C Command                           */
  volatile uint32_t PRSCTRL;      /**< 0x010 PRS control                       */
  volatile uint32_t STATUS;       /**< 0x014 Status                            */
  volatile uint32_t PRECNT;       /**< 0x018 Pre-counter                       */
  volatile uint32_t BASECNT;      /**< 0x01C Base counter                      */
  volatile uint32_t WRAPCNT;      /**< 0x020 Wrap counter                      */
  volatile uint32_t LPRECNT;      /**< 0x024 Latched pre-counter (latch read)  */
  volatile uint32_t LBASECNT;     /**< 0x028 Latched base counter (timestamp)  */
  volatile uint32_t LWRAPCNT;     /**< 0x02C Latched wrap counter (epoch)      */
  volatile uint32_t PRECNTTOPADJ; /**< 0x030 Pre-counter top adjust            */
  volatile uint32_t PRECNTTOP;    /**< 0x034 Pre-counter top                   */
  volatile uint32_t BASECNTTOP;   /**< 0x038 Base counter top                  */
} PROTIMER_TypeDef;

/* PROTIMER instance base addresses (identical on all Series 3 parts that
 * expose a PROTIMER). */
#define SL_LOG_PROTIMER_S_BASE    (0xB021C000UL)
#define SL_LOG_PROTIMER_NS_BASE   (0xA021C000UL)

/* Match the TrustZone selection in the internal part headers. */
#if ((defined(SL_TRUSTZONE_SECURE) && !defined(SL_TRUSTZONE_PERIPHERAL_PROTIMER_S)) \
  || (defined(SL_TRUSTZONE_PERIPHERAL_PROTIMER_S) \
      && (SL_TRUSTZONE_PERIPHERAL_PROTIMER_S != 0)))
#define SL_LOG_PROTIMER_BASE      (SL_LOG_PROTIMER_S_BASE)
#else
#define SL_LOG_PROTIMER_BASE      (SL_LOG_PROTIMER_NS_BASE)
#endif

#define PROTIMER                  ((PROTIMER_TypeDef *) SL_LOG_PROTIMER_BASE)

#endif // PROTIMER

/* PROTIMER_PRECNTTOP.PRECNTTOP field. Guarded individually so the full device
 * header's identical definitions take precedence when present. */
#ifndef _PROTIMER_PRECNTTOP_PRECNTTOP_SHIFT
#define _PROTIMER_PRECNTTOP_PRECNTTOP_SHIFT   16
#endif
#ifndef _PROTIMER_PRECNTTOP_PRECNTTOP_MASK
#define _PROTIMER_PRECNTTOP_PRECNTTOP_MASK    0xFFFF0000UL
#endif

#endif // SL_LOG_PROTIMER_H
