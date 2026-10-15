/***************************************************************************//**
 * @file
 * @brief PORTAL (Power Request and Acknowledgement) peripheral internal API
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#ifndef SLI_HAL_PORTAL_H
#define SLI_HAL_PORTAL_H

#include "em_device.h"

#if defined(PORTAL_PRESENT) && defined(PORTAL)

#include <stdbool.h>
#include <stdint.h>
#include "sl_assert.h"
#include "sli_device_portal.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// DEFINES

/// Validation of portal power domain index.
#define SLI_PORTAL_VALIDATE_DOMAIN(domain)  SLI_PORTAL_DOMAIN_SUPPORTED(domain)

/***************************************************************************//**
 * @addtogroup portal PORTAL - Power Request and Acknowledgement
 * @brief PORTAL peripheral internal API for power domain management
 * @{
 ******************************************************************************/

/*******************************************************************************
 *****************************   INLINE FUNCTIONS   ****************************
 ******************************************************************************/

/***************************************************************************//**
 * @brief
 *   Request domain power up.
 *
 * @details
 *   Sets the power up request bit for the specified domain in the CTRL register.
 *
 * @param[in] domain
 *   Power domain to request power up for. Possible values are:
 *     - SLI_PORTAL_DOMAIN_HOSTBASE
 *     - SLI_PORTAL_DOMAIN_HOSTNPU
 *     - SLI_PORTAL_DOMAIN_LPW0
 *     - SLI_PORTAL_DOMAIN_WIFI0BASE
 *     - SLI_PORTAL_DOMAIN_WIFI0MODEM11B
 *     - SLI_PORTAL_DOMAIN_WIFI0OFDM
 ******************************************************************************/
__STATIC_INLINE void sli_hal_portal_request_domain_powerup(sli_portal_domain_t domain)
{
  EFM_ASSERT(SLI_PORTAL_VALIDATE_DOMAIN(domain));

  PORTAL->HOSTCTRL_SET = (1UL << domain);
}

/***************************************************************************//**
 * @brief
 *   Request domain power down.
 *
 * @details
 *   Clears the power up request bit for the specified domain in the CTRL register.
 *
 * @param[in] domain
 *   Power domain to power down. Possible values are:
 *     - SLI_PORTAL_DOMAIN_HOSTBASE
 *     - SLI_PORTAL_DOMAIN_HOSTNPU
 *     - SLI_PORTAL_DOMAIN_LPW0
 *     - SLI_PORTAL_DOMAIN_WIFI0BASE
 *     - SLI_PORTAL_DOMAIN_WIFI0MODEM11B
 *     - SLI_PORTAL_DOMAIN_WIFI0OFDM
 ******************************************************************************/
__STATIC_INLINE void sli_hal_portal_request_domain_powerdown(sli_portal_domain_t domain)
{
  EFM_ASSERT(SLI_PORTAL_VALIDATE_DOMAIN(domain));

  PORTAL->HOSTCTRL_CLR = (1UL << domain);
}

/***************************************************************************//**
 * @brief
 *   Check if domain is powered up.
 *
 * @details
 *   Checks if the power up acknowledgement bit is set in the STATUS register
 *   for the specified domain.
 *
 * @param[in] domain
 *   Power domain to check. Possible values are:
 *     - SLI_PORTAL_DOMAIN_HOSTBASE
 *     - SLI_PORTAL_DOMAIN_HOSTNPU
 *     - SLI_PORTAL_DOMAIN_LPW0
 *     - SLI_PORTAL_DOMAIN_WIFI0BASE
 *     - SLI_PORTAL_DOMAIN_WIFI0MODEM11B
 *     - SLI_PORTAL_DOMAIN_WIFI0OFDM
 *
 * @return
 *   true if domain is powered up, false otherwise.
 ******************************************************************************/
__STATIC_INLINE bool sli_hal_portal_is_domain_powered_up(sli_portal_domain_t domain)
{
  EFM_ASSERT(SLI_PORTAL_VALIDATE_DOMAIN(domain));

  return (PORTAL->HOSTSTATUS & (1UL << domain));
}

/***************************************************************************//**
 * @brief
 *   Enable interrupt for a specific power domain.
 *
 * @param[in] domain
 *   Power domain to enable interrupt for. Possible values are:
 *     - SLI_PORTAL_DOMAIN_HOSTBASE
 *     - SLI_PORTAL_DOMAIN_HOSTNPU
 *     - SLI_PORTAL_DOMAIN_LPW0
 *     - SLI_PORTAL_DOMAIN_WIFI0BASE
 *     - SLI_PORTAL_DOMAIN_WIFI0MODEM11B
 *     - SLI_PORTAL_DOMAIN_WIFI0OFDM
 ******************************************************************************/
__STATIC_INLINE void sli_hal_portal_enable_domain_interrupt(sli_portal_domain_t domain)
{
  EFM_ASSERT(SLI_PORTAL_VALIDATE_DOMAIN(domain));

  PORTAL->HOSTIEN_SET = (1UL << domain);
}

/***************************************************************************//**
 * @brief
 *   Check if interrupt is enabled for a specific power domain.
 *
 * @param[in] domain
 *   Power domain to check. Possible values are:
 *     - SLI_PORTAL_DOMAIN_HOSTBASE
 *     - SLI_PORTAL_DOMAIN_HOSTNPU
 *     - SLI_PORTAL_DOMAIN_LPW0
 *     - SLI_PORTAL_DOMAIN_WIFI0BASE
 *     - SLI_PORTAL_DOMAIN_WIFI0MODEM11B
 *     - SLI_PORTAL_DOMAIN_WIFI0OFDM
 *
 * @return
 *   true if interrupt is enabled, false otherwise.
 ******************************************************************************/
__STATIC_INLINE bool sli_hal_portal_is_domain_interrupt_enabled(sli_portal_domain_t domain)
{
  EFM_ASSERT(SLI_PORTAL_VALIDATE_DOMAIN(domain));

  return (PORTAL->HOSTIEN & (1UL << domain));
}

/***************************************************************************//**
 * @brief
 *   Check if interrupt flag is set for a specific power domain.
 *
 * @details
 *   The interrupt flag in the IF register is set by hardware on the rising
 *   edge of the acknowledgement signal.
 *
 * @param[in] domain
 *   Power domain to check. Possible values are:
 *     - SLI_PORTAL_DOMAIN_HOSTBASE
 *     - SLI_PORTAL_DOMAIN_HOSTNPU
 *     - SLI_PORTAL_DOMAIN_LPW0
 *     - SLI_PORTAL_DOMAIN_WIFI0BASE
 *     - SLI_PORTAL_DOMAIN_WIFI0MODEM11B
 *     - SLI_PORTAL_DOMAIN_WIFI0OFDM
 *
 * @return
 *   true if interrupt flag is set, false otherwise.
 ******************************************************************************/
__STATIC_INLINE bool sli_hal_portal_is_domain_interrupt_flag_set(sli_portal_domain_t domain)
{
  EFM_ASSERT(SLI_PORTAL_VALIDATE_DOMAIN(domain));

  return (PORTAL->HOSTIF & (1UL << domain));
}

/***************************************************************************//**
 * @brief
 *   Clear interrupt flags for a set of power domains.
 *
 * @param[in] mask
 *   Bit mask of power domains whose interrupt flags shall be cleared.
 ******************************************************************************/
__STATIC_INLINE void sli_hal_portal_clear_domain_interrupt_flags(uint32_t mask)
{
  PORTAL->HOSTIF_CLR = (mask & _PORTAL_HOSTIF_MASK);
}

/***************************************************************************//**
 * @brief
 *   Disable interrupts for a set of power domains.
 *
 * @param[in] mask
 *   Bit mask of power domains to disable interrupts for.
 ******************************************************************************/
__STATIC_INLINE void sli_hal_portal_disable_domain_interrupts(uint32_t mask)
{
  PORTAL->HOSTIEN_CLR = (mask & _PORTAL_HOSTIF_MASK);
}

/***************************************************************************//**
 * @brief
 *   Get mask of active domain interrupts.
 *
 * @details
 *   Returns the set of domains whose interrupts are currently active, i.e.:
 *   - the interrupt flag is set in the IF register, and
 *   - the interrupt is enabled in the IEN register
 *
 * @return
 *   Bit mask of active domain interrupts.
 ******************************************************************************/
__STATIC_INLINE uint32_t sli_hal_portal_get_active_domain_interrupt_mask(void)
{
  uint32_t hostif = PORTAL->HOSTIF;
  uint32_t hostien = PORTAL->HOSTIEN;

  return (hostif & hostien) & _PORTAL_HOSTIF_MASK;
}

/** @} (end addtogroup portal) */

#ifdef __cplusplus
}
#endif

/* *INDENT-OFF* */
/***************************************************************************//**
 * @addtogroup portal PORTAL - Power Request and Acknowledgement
 * @{
 *
 * @li @ref portal_intro
 * @li @ref portal_example
 *
 *@n @section portal_intro Introduction
 *  The PORTAL module provides a mechanism for the Host CPU to ensure that a
 *  target subsystem domain is powered up. This API supports the Host APB
 *  interface using HOSTCTRL/HOSTSTATUS/HOSTIF/HOSTIEN registers.
 *
 *  Software should ensure that the target domain is powered up before accessing
 *  memory or peripherals in that domain by polling the domain's power status
 *  bit in the Portal before accessing the target domain's modules.
 *
 *  ## Typical Power Domain Request Sequence
 *
 *  The following shows an example of how the Host can power up the PD1LPW0
 *  domain to access various peripherals (such as SEQRAM) without waking up
 *  the LPW0 CPU(s):
 *
 *  1. Set the power up request: PORTAL->HOSTCTRL |= PORTAL_HOSTCTRL_LPW0PWRUPREQ
 *     - The EMU powers up the PD1LPW0 power domain
 *  2. Poll the acknowledgement: Wait until (PORTAL->HOSTSTATUS & PORTAL_HOSTSTATUS_LPW0PWRUPACK)
 *  3. Access peripherals: The Host can now safely access peripherals in PD1LPW0
 *  4. Clear request (optional): PORTAL->HOSTCTRL &= ~PORTAL_HOSTCTRL_LPW0PWRUPREQ when done
 *
 *  @note The Host could also use an IRQ instead of polling, controlled via
 *        PORTAL->HOSTIF and PORTAL->HOSTIEN registers.
 *
 *  The set of power domains and their HOSTCTRL bit positions is device-specific.
 *  See sli_device_portal.h for the enumerators on the selected part, and use
 *  SLI_PORTAL_DOMAIN_SUPPORTED() to check whether a domain is supported.
 *
 *@n @section portal_example Example
 *  @code{.c}
 *  // Request power up for LPW0 domain
 *  sli_hal_portal_request_domain_powerup(SLI_PORTAL_DOMAIN_LPW0);
 *
 *  // Poll until power domain is ready
 *  while (!sli_hal_portal_is_domain_powered_up(SLI_PORTAL_DOMAIN_LPW0)) {
 *  }
 *
 *  // PD1LPW0 is now powered up, safe to access SEQRAM and other peripherals
 *  // ... access peripherals ...
 *
 *  // Clear the power request when done (optional)
 *  sli_hal_portal_request_domain_powerdown(SLI_PORTAL_DOMAIN_LPW0);
 *  @endcode
 *
 * @} (end addtogroup portal)
 ******************************************************************************/
/* *INDENT-ON* */

#endif /* defined(PORTAL_PRESENT) && defined(PORTAL) */
#endif /* SLI_HAL_PORTAL_H */
