/***************************************************************************/ /**
 * @file
 * @brief CPC SPI peripheral driver — USART hardware init.
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

#include "sl_cpc_drv_spi_peripheral_usart.h"
#include "sl_cpc_drv_instances.h"
#include "sl_cpc_drv_spi_peripheral.h"
#include "sli_cpc_drv_spi_peripheral.h"

#include <string.h>

#include "sl_clock_manager.h"
#include "sl_common.h"
#include "sl_component_catalog.h"
#include "sl_device_peripheral.h"
#include "sl_status.h"

#include "../../../src/sli_cpc_assert.h"
#include "../../../src/sli_cpc_endianness.h"

#include "sl_hal_gpio.h"
#include "sl_hal_prs.h"

static uint32_t prs_signal_txc(uint8_t peripheral_no)
{
  switch (peripheral_no) {
#if defined(SL_HAL_PRS_ASYNC_USART0_TXC)
    case 0:
      return SL_HAL_PRS_ASYNC_USART0_TXC;
#endif
#if defined(SL_HAL_PRS_ASYNC_USART1_TXC)
    case 1:
      return SL_HAL_PRS_ASYNC_USART1_TXC;
#endif
#if defined(SL_HAL_PRS_ASYNC_USART2_TXC)
    case 2:
      return SL_HAL_PRS_ASYNC_USART2_TXC;
#endif
    default:
      SLI_CPC_ASSERT(false);
      return 0;
  }
}

static uint32_t prs_signal_cs(uint8_t peripheral_no)
{
  switch (peripheral_no) {
#if defined(SL_HAL_PRS_ASYNC_USART0_CS)
    case 0:
      return SL_HAL_PRS_ASYNC_USART0_CS;
#endif
#if defined(SL_HAL_PRS_ASYNC_USART1_CS)
    case 1:
      return SL_HAL_PRS_ASYNC_USART1_CS;
#endif
#if defined(SL_HAL_PRS_ASYNC_USART2_CS)
    case 2:
      return SL_HAL_PRS_ASYNC_USART2_CS;
#endif
    default:
      SLI_CPC_ASSERT(false);
      return 0;
  }
}

static sl_status_t init_caps(sl_cpc_drv_spi_peripheral_t *drv)
{
  sl_clock_branch_t spi_clock_branch;
  sl_status_t status;
  uint32_t max_speed;

  spi_clock_branch = sl_device_peripheral_get_clock_branch(drv->peripheral);
  status = sl_clock_manager_get_clock_branch_frequency(spi_clock_branch, &max_speed);
  if (status != SL_STATUS_OK) {
    return status;
  }

  // The max speed of the USART in mode secondary is the peripheral clock feeding the USART / 10
  max_speed /= 10;

  sli_cpc_u32_to_le(max_speed, drv->local_caps.max_speed_le);

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Initialize peripheral clocks.
 ******************************************************************************/
static void init_clocks(const sl_cpc_drv_spi_peripheral_t *drv)
{
  sl_status_t status;

  status = sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_PRS);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);
  status = sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);
  sl_bus_clock_t spi_bus_clock = sl_device_peripheral_get_bus_clock(drv->peripheral);
  status = sl_clock_manager_enable_bus_clock(spi_bus_clock);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);
#if defined(SL_CATALOG_CPC_DRIVER_HW_CRC_PRESENT)
  status = sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPCRC0);
  SLI_CPC_ASSERT(status == SL_STATUS_OK);
#endif
}

/***************************************************************************/ /**
 * Initialize clocks, GPIOs, and the USART peripheral.
 *
 * Also fills the shared driver fields needed by the common SPI init
 * (PRS producers, RXBLOCKEN).
 ******************************************************************************/
static sl_status_t init_hw(sl_cpc_drv_spi_peripheral_t *drv, const sl_cpc_drv_spi_peripheral_usart_config_t *cfg)
{
  USART_TypeDef *usart = sl_device_peripheral_usart_get_base_addr(cfg->peripheral);
  uint8_t peripheral_no = USART_NUM(usart);

  SLI_CPC_ASSERT(USART_NUM(usart) != -1);

  init_clocks(drv);

  // Set pin modes and drive characteristics for the SPI mode 0
  sl_hal_gpio_set_pin_mode(&cfg->copi, SL_GPIO_MODE_INPUT, 0);
  sl_hal_gpio_set_pin_mode(&cfg->cipo, SL_GPIO_MODE_PUSH_PULL, 0);
  sl_hal_gpio_set_pin_mode(&cfg->clk, SL_GPIO_MODE_INPUT, 0);

  // Pull up to give an idle high state to the input Chip Select signal
  sl_hal_gpio_set_pin_mode(&cfg->cs, SL_GPIO_MODE_INPUT_PULL, 1);

  // Initial value of IRQ signal is HIGH (no frame to send)
  sl_hal_gpio_set_pin_mode(&cfg->irq, SL_GPIO_MODE_PUSH_PULL, 1);

  // Configure the GPIO routing to the SPI peripheral
  {
#if defined(_GPIO_USART_ROUTEEN_MASK)
    // Route MISO to the USART
    GPIO->USARTROUTE[peripheral_no].TXROUTE = ((uint32_t)cfg->copi.port << _GPIO_USART_TXROUTE_PORT_SHIFT)
                                              | ((uint32_t)cfg->copi.pin << _GPIO_USART_TXROUTE_PIN_SHIFT);

    // Route MOSI to the USART
    GPIO->USARTROUTE[peripheral_no].RXROUTE = ((uint32_t)cfg->cipo.port << _GPIO_USART_RXROUTE_PORT_SHIFT)
                                              | ((uint32_t)cfg->cipo.pin << _GPIO_USART_RXROUTE_PIN_SHIFT);

    // Route SCLK to the USART
    GPIO->USARTROUTE[peripheral_no].CLKROUTE = ((uint32_t)cfg->clk.port << _GPIO_USART_CLKROUTE_PORT_SHIFT)
                                               | ((uint32_t)cfg->clk.pin << _GPIO_USART_CLKROUTE_PIN_SHIFT);

    // Route CS to the USART
    GPIO->USARTROUTE[peripheral_no].CSROUTE = ((uint32_t)cfg->cs.port << _GPIO_USART_CSROUTE_PORT_SHIFT)
                                              | ((uint32_t)cfg->cs.pin << _GPIO_USART_CSROUTE_PIN_SHIFT);

    // Activate the routes to the USART
    GPIO->USARTROUTE[peripheral_no].ROUTEEN
      = GPIO_USART_ROUTEEN_TXPEN | GPIO_USART_ROUTEEN_RXPEN | GPIO_USART_ROUTEEN_CLKPEN | GPIO_USART_ROUTEEN_CSPEN;
#endif
  }

  // Init of the USART
  {
#if defined(USART_EN_EN)
    usart->EN_SET = USART_EN_EN;
#endif

    usart->CTRL = USART_CTRL_SYNC | USART_CTRL_CLKPOL_IDLELOW | USART_CTRL_CLKPHA_SAMPLELEADING | USART_CTRL_MSBF;

    usart->FRAME = USART_FRAME_DATABITS_EIGHT;

    usart->CMD = USART_CMD_RXEN | USART_CMD_TXEN;
  }

  // USART can produce CS and TXC directly on PRS — no EXTI detour needed.
  drv->irq = cfg->irq;
  drv->tx_availability_synctrig_ch = cfg->tx_availability_synctrig_ch;

  drv->prs_producers.txc.channel = cfg->txc_synctrig_prs_ch;
  drv->prs_producers.txc.signal = prs_signal_txc(peripheral_no);
  drv->prs_producers.cs.channel = cfg->cs_synctrig_prs_ch;
  drv->prs_producers.cs.signal = prs_signal_cs(peripheral_no);
  drv->rx_blocken_cmd = USART_CMD_RXBLOCKEN;
  drv->rx_blockdis_cmd = USART_CMD_RXBLOCKDIS;
  drv->reg.rxdata = &usart->RXDATA;
  drv->reg.txdata = &usart->TXDATA;
  drv->reg.cmd = &usart->CMD;

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Initialize a USART-backed SPI peripheral CPC driver and its embedded bus.
 ******************************************************************************/
sl_status_t sl_cpc_drv_spi_peripheral_usart_init(sl_cpc_drv_spi_peripheral_usart_t *drv,
                                                 const sl_cpc_drv_spi_peripheral_usart_config_t *cfg,
                                                 const sl_cpc_bus_config_t *bus_cfg)
{
  sl_status_t status;
  SLI_CPC_ASSERT(drv != NULL);

  if (cfg == NULL || bus_cfg == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  memset(drv, 0, sizeof(*drv));
  drv->peripheral = cfg->peripheral;

  status = init_hw(drv, cfg);
  if (status != SL_STATUS_OK) {
    return status;
  }

  status = init_caps(drv);
  if (status != SL_STATUS_OK) {
    return status;
  }

  return sli_cpc_drv_spi_peripheral_init(drv, bus_cfg);
}

sl_cpc_drv_spi_peripheral_usart_t
  sl_cpc_drv_spi_peripheral_usart_instances[SL_CPC_DRV_SPI_PERIPHERAL_USART_INSTANCES_COUNT];
