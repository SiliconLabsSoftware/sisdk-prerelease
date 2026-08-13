/***************************************************************************//**
 * @file
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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
 *******************************************************************************
 * # Experimental Quality
 * This code has not been formally tested and is provided as-is. It is not
 * suitable for production environments. In addition, this code will not be
 * maintained and there may be no bug maintenance planned for these resources.
 * Silicon Labs may update projects from time to time.
 ******************************************************************************/

#include "sl_segmentlcd.h"
#include "sl_hal_ldma.h"
#include "sl_dma_manager.h"
#include "sl_clock_manager.h"

#define LDMA_CHANNEL      0
#define LDMA_CH_MASK      (1 << LDMA_CHANNEL)
#define NUM_NUMBER        10
#define NUM_STATE         10

#if defined(SL_SEGMENT_LCD_MODULE_CE322_1002)
#define NUM_DIGIT         5
#define NUM_SEG           4
#elif defined(SL_SEGMENT_LCD_MODULE_CL010_1087)
#define NUM_DIGIT         4
#define NUM_SEG           8
#endif

uint32_t display[NUM_STATE][NUM_SEG];
sl_hal_ldma_descriptor_t descriptors[3];

/**************************************************************************//**
 * Working instance of LCD display
 *****************************************************************************/
extern const sl_segment_lcd_mcu_display_t efm_display;
extern const uint16_t segment_numbers[];

/**************************************************************************//**
 * Defines the value for each LCD_SEGn register based on the DIGIT place and
 * NUMBER to display
 * E.g.:
 * segments[2][5][0] contains the value to write to the LCD_SEG0 register to
 * display the number 5 in the 2nd digit place.
 *****************************************************************************/
#if defined(SL_SEGMENT_LCD_MODULE_CL010_1087)
uint32_t segments[SL_SEGMENT_LCD_NUM_DIGITS][10][8];
#elif defined(SL_SEGMENT_LCD_MODULE_CE322_1002)
uint32_t segments[SL_SEGMENT_LCD_NUM_DIGITS][10][4];
#endif

/**************************************************************************//**
 * Initialize the segments[][][] buffer
 *****************************************************************************/
void segment_lcd_ldma_buffer_init(void)
{
  uint16_t bitpattern;
  uint8_t dig, num, seg, i, bit;

  for (dig = 0; dig < SL_SEGMENT_LCD_NUM_DIGITS; dig++) {
    for (num = 0; num < 10; num++) {
#if defined(SL_SEGMENT_LCD_MODULE_CL010_1087)
      for (seg = 0; seg < 8; seg++) {
#elif defined(SL_SEGMENT_LCD_MODULE_CE322_1002)
      for (seg = 0; seg < 4; seg++) {
#endif
        segments[dig][num][seg] = 0;
      }
    }
  }

  for (dig = 0; dig < SL_SEGMENT_LCD_NUM_DIGITS; dig++) {
    for (num = 0; num < 10; num++) {
      bitpattern = segment_numbers[num];
      for (i = 0; i < 7; i++) {
        bit = efm_display.number[dig].bit[i];
        seg = efm_display.number[dig].com[i];
        if (bitpattern & (1 << i)) {
          segments[dig][num][seg] |= (1 << bit);
        }
      }
    }
  }
}

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init(void)
{
  int8_t num, seg, dig;

  // Initialize the LCD
  sl_segment_lcd_init(true);
#if defined(SL_SEGMENT_LCD_MODULE_CL010_1087)
  // Example only used upper numeric segments; disable unused segments
  SL_LCD_SEGMENTS_ALPHA_DIS();
#endif
  // Fill the segments[][][] buffer
  segment_lcd_ldma_buffer_init();

  // Fill the display[][] buffer to output:
  // 00000 -> 11111 -> 22222 -> ... -> 99999
  for (num = 0; num < 10; num++) {
    for (seg = 0; seg < NUM_SEG; seg++) {
      display[num][seg] = 0;
      for (dig = 0; dig < NUM_DIGIT; dig++) {
        display[num][seg] |= segments[dig][num][seg];
      }
    }
  }

  // DMA Manager is auto-initialized via SL Main (LDMA clock, LDMA init, NVIC setup).
  // LDMAXBAR has a separate bus clock that the DMA Manager does not enable. Without it,
  // sl_hal_ldma_init_transfer faults when writing to LDMAXBAR->CH[n].REQSEL. Enable it
  // manually whenever sl_hal_ldma_* is used directly instead of the dma_channel driver.
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_LDMAXBAR0);

  // Reserve the fixed channel so the DMA Manager does not allocate it to others.
  sl_dma_manager_reserve_channel(NULL, LDMA_CHANNEL);

  // Configure the LDMA to trigger on an LCD DMA request
  sl_hal_ldma_transfer_init_t transfer_config = SL_HAL_LDMA_TRANSFER_CFG_PERIPHERAL_LOOP(
    SL_HAL_LDMA_PERIPHERAL_SIGNAL_LCD,
    NUM_STATE - 1);

  // 1st descriptor sets the base SRC address of the LDMA channel
  descriptors[0] = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKREL_WRITE(
    (uint32_t)&(display[0][0]),
    &(LDMA->CH[LDMA_CHANNEL].SRC),
    1);

  // 2nd descriptor writes values from display[][] buffer to the LCD_SEGn registers
#if defined(_SILICON_LABS_32B_SERIES_2_CONFIG_3)
  descriptors[1] = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKREL_M2M(
    SL_HAL_LDMA_CTRL_SIZE_WORD,
    0,
    &(LCD->SEGD0),
    4,
    0);
#elif defined(_SILICON_LABS_32B_SERIES_2_CONFIG_8)
  descriptors[1] = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKREL_M2M(
    SL_HAL_LDMA_CTRL_SIZE_WORD,
    0,
    &(LCD->SEGD0),
    8,
    0);
#endif
  descriptors[1].xfer.src_addr_mode = SL_HAL_LDMA_CTRL_SRC_ADDR_MODE_REL;
  descriptors[1].xfer.src_inc = SL_HAL_LDMA_CTRL_SRC_INC_ONE;
  descriptors[1].xfer.dst_inc = SL_HAL_LDMA_CTRL_DST_INC_TWO;
  descriptors[1].xfer.struct_req = false;
  descriptors[1].xfer.dec_loop_count = 1;

  // 3rd descriptor resets the LOOP counter
  descriptors[2] = (sl_hal_ldma_descriptor_t)SL_HAL_LDMA_DESCRIPTOR_LINKREL_WRITE(
    NUM_STATE - 1,
    &(LDMA->CH[LDMA_CHANNEL].LOOP),
    -2);

  // Start LDMA transfers
  sl_hal_ldma_init_transfer(LDMA0, 0, &transfer_config, &descriptors[0]);
  sl_hal_ldma_start_transfer(LDMA0, 0);
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
}
