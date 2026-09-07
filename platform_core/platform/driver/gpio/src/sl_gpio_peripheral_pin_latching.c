/***************************************************************************//**
 * @file
 * @brief GPIO Peripheral Pin Latching Implementation
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

#include "sl_gpio_peripheral_pin_latching.h"
#include "sl_hal_gpio.h"
#include "em_device.h"

// -----------------------------------------------------------------------------
// Route register field positions — consistent across all GPIO route types.

#define SLI_GPIO_ROUTE_PORT_SHIFT  0u
#define SLI_GPIO_ROUTE_PORT_MASK   0x3u
#define SLI_GPIO_ROUTE_PIN_SHIFT   16u
#define SLI_GPIO_ROUTE_PIN_MASK    0xFu

#define SLI_GPIO_ROUTEEN_OFFSET_INVALID  0xFFFFu
#define SLI_GPIO_ROUTE_NO_GAP           0xFFu

// -----------------------------------------------------------------------------
// Static helpers

/***************************************************************************//**
 * Extract ROUTEEN offset, mask, and route-register gap position from a
 * peripheral structure.
 *
 * All peripheral types that carry ROUTEEN information share the
 * sl_peripheral_val_t prefix; gpio_routeen_offset and gpio_routeen_mask are
 * read through sl_peripheral_t.
 *
 * Route-register gap:
 *  - Most peripherals have a 1:1 mapping between ROUTEEN enable bits and
 *    route registers (bit N → register at position N + 1 after ROUTEEN).
 *  - EUSART has CTSROUTE (input-only, no ROUTEEN enable bit) at position 2,
 *    shifting registers for bits ≥ 1 by one.
 *  - CMU has CLKIN0ROUTE (input-only) at position 1, shifting all output
 *    route registers by one.
 *
 * @param[in]  peripheral    Generic peripheral pointer.
 * @param[out] offset        ROUTEEN register offset from GPIO base (bytes).
 * @param[out] mask          32-bit bitmask of valid ROUTEEN enable bits.
 * @param[out] gap_position  Position of the input-only route register gap
 *                           (1-based from ROUTEEN).  0xFF means no gap.
 ******************************************************************************/
static void get_routeen_info(sl_peripheral_t peripheral,
                             uint16_t *offset,
                             uint32_t *mask,
                             uint8_t *gap_position)
{
  *offset = peripheral->gpio_routeen_offset;
  *mask = peripheral->gpio_routeen_mask;
  *gap_position = SLI_GPIO_ROUTE_NO_GAP;

#if defined(CMU_BASE)
  if (peripheral->base == CMU_BASE) {
    *gap_position = 1u;
  }
#endif

#if defined(EUSART_PRESENT)
  {
    bool is_eusart = false;
#if defined(EUSART0_BASE)
    is_eusart = is_eusart || (peripheral->base == EUSART0_BASE);
#endif
#if defined(EUSART1_BASE)
    is_eusart = is_eusart || (peripheral->base == EUSART1_BASE);
#endif
#if defined(EUSART2_BASE)
    is_eusart = is_eusart || (peripheral->base == EUSART2_BASE);
#endif
#if defined(EUSART3_BASE)
    is_eusart = is_eusart || (peripheral->base == EUSART3_BASE);
#endif
#if defined(EUSART4_BASE)
    is_eusart = is_eusart || (peripheral->base == EUSART4_BASE);
#endif
    if (is_eusart) {
      *gap_position = 2u;
    }
  }
#endif // EUSART_PRESENT
}

/***************************************************************************//**
 * Core implementation shared by latch and unlatch.
 *
 * Algorithm:
 *   1. Read the ROUTEEN register for the peripheral from the GPIO block.
 *   2. For each enabled route bit, read the corresponding route register to
 *      obtain port and pin.
 *   3. Latch or unlatch the pin via the HAL.
 *
 * Route registers follow ROUTEEN in memory at 4-byte intervals.  Bit N in
 * ROUTEEN normally maps to the register at position N + 1 from ROUTEEN.
 * When a gap_position is set, route registers at or past that position are
 * shifted by one to account for an input-only route register with no enable
 * bit.
 ******************************************************************************/
static sl_status_t latch_unlatch_peripheral_pins(sl_peripheral_t peripheral,
                                                 bool latch)
{
  uint16_t routeen_offset;
  uint32_t routeen_mask;
  uint8_t gap_position;

  get_routeen_info(peripheral, &routeen_offset, &routeen_mask, &gap_position);

  if (routeen_offset == SLI_GPIO_ROUTEEN_OFFSET_INVALID) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  volatile uint32_t *routeen_reg = (volatile uint32_t *)((uint8_t *)GPIO + routeen_offset);
  uint32_t routeen = *routeen_reg & routeen_mask;

  // Iterate over the enabled route bits
  for (uint32_t bit_pos = 0u; routeen != 0u; bit_pos++, routeen >>= 1u) {
    // Skip if the bit is not enabled
    if ((routeen & 1u) == 0u) {
      continue;
    }

    // Calculate the position of the route register
    uint32_t reg_pos = bit_pos + 1u;
    if (reg_pos >= (uint32_t)gap_position) {
      reg_pos++;
    }

    // Read the route register and extract the port and pin
    uint32_t route = *(routeen_reg + reg_pos);
    uint8_t port = (uint8_t)((route >> SLI_GPIO_ROUTE_PORT_SHIFT)
                             & SLI_GPIO_ROUTE_PORT_MASK);
    uint8_t pin  = (uint8_t)((route >> SLI_GPIO_ROUTE_PIN_SHIFT)
                             & SLI_GPIO_ROUTE_PIN_MASK);
    sl_gpio_t gpio = { .port = port, .pin = pin };

    // Latch or unlatch the pin
    if (latch) {
      sl_hal_gpio_latch_pin(&gpio);
    } else {
      sl_hal_gpio_unlatch_pin(&gpio);
    }
  }

  return SL_STATUS_OK;
}

// -----------------------------------------------------------------------------
// Public API

/***************************************************************************//**
 * Latch all GPIO pins routed to a peripheral.
 ******************************************************************************/
sl_status_t sl_gpio_latch_peripheral_pins(sl_peripheral_t peripheral)
{
  return latch_unlatch_peripheral_pins(peripheral, true);
}

/***************************************************************************//**
 * Unlatch all GPIO pins routed to a peripheral.
 ******************************************************************************/
sl_status_t sl_gpio_unlatch_peripheral_pins(sl_peripheral_t peripheral)
{
  return latch_unlatch_peripheral_pins(peripheral, false);
}
