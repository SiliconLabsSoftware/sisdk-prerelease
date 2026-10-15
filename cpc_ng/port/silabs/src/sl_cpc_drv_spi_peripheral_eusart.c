/***************************************************************************/ /**
 * @file
 * @brief CPC SPI peripheral driver — EUSART hardware init.
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

#include "sl_cpc_drv_spi_peripheral_eusart.h"
#include "sl_cpc_drv_instances.h"
#include "sl_cpc_drv_spi_peripheral.h"
#include "sli_cpc_drv_spi_peripheral.h"

#include <string.h>

#include "sl_clock_manager.h"
#include "sl_common.h"
#include "sl_component_catalog.h"
#include "sl_device_peripheral.h"
#include "sl_hal_gpio.h"
#include "sl_hal_prs.h"
#include "sl_status.h"

#include "../../../src/sli_cpc_assert.h"
#include "../../../src/sli_cpc_endianness.h"

static uint32_t prs_signal_txc(uint8_t peripheral_no)
{
  switch (peripheral_no) {
#if defined(SL_HAL_PRS_ASYNC_EUSART0L_TXC)
    case 0:
      return SL_HAL_PRS_ASYNC_EUSART0L_TXC;
#endif
#if defined(SL_HAL_PRS_ASYNC_EUSART1L_TXC)
    case 1:
      return SL_HAL_PRS_ASYNC_EUSART1L_TXC;
#endif
#if defined(SL_HAL_PRS_ASYNC_EUSART2L_TXC)
    case 2:
      return SL_HAL_PRS_ASYNC_EUSART2L_TXC;
#endif
#if defined(SL_HAL_PRS_ASYNC_EUSART3L_TXC)
    case 3:
      return SL_HAL_PRS_ASYNC_EUSART3L_TXC;
#endif
#if defined(SL_HAL_PRS_ASYNC_EUSART4L_TXC)
    case 4:
      return SL_HAL_PRS_ASYNC_EUSART4L_TXC;
#endif
    default:
      SLI_CPC_ASSERT(false);
      return 0;
  }
}

static uint32_t prs_signal_exti(uint8_t exti_no)
{
  switch (exti_no) {
#if defined(SL_HAL_PRS_ASYNC_GPIO_PIN0)
    case 0:
      return SL_HAL_PRS_ASYNC_GPIO_PIN0;
#endif
#if defined(SL_HAL_PRS_ASYNC_GPIO_PIN1)
    case 1:
      return SL_HAL_PRS_ASYNC_GPIO_PIN1;
#endif
#if defined(SL_HAL_PRS_ASYNC_GPIO_PIN2)
    case 2:
      return SL_HAL_PRS_ASYNC_GPIO_PIN2;
#endif
#if defined(SL_HAL_PRS_ASYNC_GPIO_PIN3)
    case 3:
      return SL_HAL_PRS_ASYNC_GPIO_PIN3;
#endif
#if defined(SL_HAL_PRS_ASYNC_GPIO_PIN4)
    case 4:
      return SL_HAL_PRS_ASYNC_GPIO_PIN4;
#endif
#if defined(SL_HAL_PRS_ASYNC_GPIO_PIN5)
    case 5:
      return SL_HAL_PRS_ASYNC_GPIO_PIN5;
#endif
#if defined(SL_HAL_PRS_ASYNC_GPIO_PIN6)
    case 6:
      return SL_HAL_PRS_ASYNC_GPIO_PIN6;
#endif
#if defined(SL_HAL_PRS_ASYNC_GPIO_PIN7)
    case 7:
      return SL_HAL_PRS_ASYNC_GPIO_PIN7;
#endif
    default:
      SLI_CPC_ASSERT(false);
      return 0;
  }
}

static sl_status_t init_caps(sl_cpc_drv_spi_peripheral_t *drv)
{
  // The max bitrate of the EUSART in secondary mode is 10MHz no matter what.
  sli_cpc_u32_to_le(10000000UL, drv->local_caps.max_speed_le);

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
 * Initialize clocks, GPIOs, and the EUSART peripheral.
 *
 * Also configures the CS EXTI route used as the CS PRS producer, and fills the
 * shared driver fields needed by the common SPI init (PRS producers, RXBLOCKEN).
 ******************************************************************************/
static sl_status_t init_hw(sl_cpc_drv_spi_peripheral_t *drv, const sl_cpc_drv_spi_peripheral_eusart_config_t *cfg)
{
  EUSART_TypeDef *eusart = sl_device_peripheral_eusart_get_base_addr(cfg->peripheral);
  uint8_t peripheral_no = EUSART_NUM(eusart);

  SLI_CPC_ASSERT(EUSART_NUM(eusart) != -1);

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
#if defined(_GPIO_EUSART_ROUTEEN_MASK)
    // Route MISO to the EUSART
    GPIO->EUSARTROUTE[peripheral_no].TXROUTE = ((uint32_t)cfg->copi.port << _GPIO_EUSART_TXROUTE_PORT_SHIFT)
                                               | ((uint32_t)cfg->copi.pin << _GPIO_EUSART_TXROUTE_PIN_SHIFT);

    // Route MOSI to the EUSART
    GPIO->EUSARTROUTE[peripheral_no].RXROUTE = ((uint32_t)cfg->cipo.port << _GPIO_EUSART_RXROUTE_PORT_SHIFT)
                                               | ((uint32_t)cfg->cipo.pin << _GPIO_EUSART_RXROUTE_PIN_SHIFT);

    // Route SCLK to the EUSART
    GPIO->EUSARTROUTE[peripheral_no].SCLKROUTE = ((uint32_t)cfg->clk.port << _GPIO_EUSART_SCLKROUTE_PORT_SHIFT)
                                                 | ((uint32_t)cfg->clk.pin << _GPIO_EUSART_SCLKROUTE_PIN_SHIFT);

    // Route CS to the EUSART
    GPIO->EUSARTROUTE[peripheral_no].CSROUTE = ((uint32_t)cfg->cs.port << _GPIO_EUSART_CSROUTE_PORT_SHIFT)
                                               | ((uint32_t)cfg->cs.pin << _GPIO_EUSART_CSROUTE_PIN_SHIFT);

    // Activate the routes to the EUSART
    GPIO->EUSARTROUTE[peripheral_no].ROUTEEN
      = GPIO_EUSART_ROUTEEN_TXPEN | GPIO_EUSART_ROUTEEN_RXPEN | GPIO_EUSART_ROUTEEN_SCLKPEN | GPIO_EUSART_ROUTEEN_CSPEN;
#endif
  }

  // Init of the EUSART
  {
    // MSB first, EUSART in SPI mode
    eusart->CFG0 = EUSART_CFG0_MSBF | EUSART_CFG0_SYNC;

    // TX and RX FIFO level interrupt of 1.
    eusart->CFG1 = EUSART_CFG1_RXFIW_ONEFRAME | EUSART_CFG1_TXFIW_ONEFRAME;

    // Slave mode 0 (cpol 0, cpha 0)
    eusart->CFG2 = EUSART_CFG2_FORCELOAD | EUSART_CFG2_CLKPHA_SAMPLELEADING | EUSART_CFG2_CLKPOL_IDLELOW
                   | _EUSART_CFG2_MASTER_SLAVE;

    // 8 bits per byte
    eusart->FRAMECFG = EUSART_FRAMECFG_DATABITS_EIGHT;

    // 0s sent when nothing to send
    eusart->DTXDATCFG = EUSART_DTXDATCFG_DTXDAT_DEFAULT;

    // Default setup window of 5 is recommended when bitrate < 5Mhz
    eusart->TIMINGCFG = EUSART_TIMINGCFG_SETUPWINDOW_DEFAULT;

    // Enable EUSART
    eusart->EN = EUSART_EN_EN;

    // Enable RX and TX
    eusart->CMD = EUSART_CMD_RXEN | EUSART_CMD_TXEN;

    // Activate Load Error and TX overflow interrupts for monitoring
    eusart->IEN = EUSART_IF_LOADERR | EUSART_IF_TXOF;

    while (eusart->SYNCBUSY & _EUSART_SYNCBUSY_MASK) {
      // Wait until all operations are synch'ed
    }
  }

  // EUSART cannot route CS directly to PRS; go through EXTI.
  // Pin / EXTI number constraints:
  //   pins 0-3 -> EXTI 0-3
  //   pins 4-7 -> EXTI 4-7
  // Only EXTI0..7 can feed a PRS channel.
  if (cfg->cs_exti_no <= 3) {
    SLI_CPC_ASSERT(cfg->cs.pin <= 3);
  } else if (cfg->cs_exti_no <= 7) {
    SLI_CPC_ASSERT(cfg->cs.pin >= 4 && cfg->cs.pin <= 7);
  } else {
    SLI_CPC_ASSERT(false);
  }

  sl_hal_gpio_configure_external_interrupt(&cfg->cs, cfg->cs_exti_no, SL_GPIO_INTERRUPT_NO_EDGE);

  drv->irq = cfg->irq;
  drv->tx_availability_synctrig_ch = cfg->tx_availability_synctrig_ch;

  drv->prs_producers.txc.channel = cfg->txc_synctrig_prs_ch;
  drv->prs_producers.txc.signal = prs_signal_txc(peripheral_no);
  drv->prs_producers.cs.channel = cfg->cs_synctrig_prs_ch;
  drv->prs_producers.cs.signal = prs_signal_exti(cfg->cs_exti_no);
  drv->rx_blocken_cmd = EUSART_CMD_RXBLOCKEN;
  drv->rx_blockdis_cmd = EUSART_CMD_RXBLOCKDIS;
  drv->reg.rxdata = &eusart->RXDATA;
  drv->reg.txdata = &eusart->TXDATA;
  drv->reg.cmd = &eusart->CMD;

  return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Initialize an EUSART-backed SPI peripheral CPC driver and its embedded bus.
 ******************************************************************************/
sl_status_t sl_cpc_drv_spi_peripheral_eusart_init(sl_cpc_drv_spi_peripheral_eusart_t *drv,
                                                  const sl_cpc_drv_spi_peripheral_eusart_config_t *cfg,
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

sl_cpc_drv_spi_peripheral_eusart_t
  sl_cpc_drv_spi_peripheral_eusart_instances[SL_CPC_DRV_SPI_PERIPHERAL_EUSART_INSTANCES_COUNT];
