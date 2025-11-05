/***************************************************************************//**
 * @file
 * @brief Segment LCD LDMA examples functions
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "sl_segmentlcd.h"
#include "em_ldma.h"

#define LDMA_CHANNEL      0
#define LDMA_CH_MASK      (1 << LDMA_CHANNEL)
#define NUM_NUMBER        10
#define NUM_STATE         10
#define NUM_DIGIT         4
#define NUM_SEG           8

uint32_t display[NUM_STATE][NUM_SEG];
LDMA_Descriptor_t descriptors[3];

/**************************************************************************//**
 * Working instance of LCD display
 *****************************************************************************/
extern const sl_segment_lcd_mcu_display_t efm_display;
extern const uint16_t segment_numbers[];

/**************************************************************************//**
 * Defines the value for each LCD_SEGn register based on the DIGIT place and
 * NUMBER to display.
 *****************************************************************************/
uint32_t segments[SL_SEGMENT_LCD_NUM_DIGITS][10][8];

/**************************************************************************//**
 * Initialize the segments[][][] buffer.
 *****************************************************************************/
void segment_lcd_ldma_buffer_init(void)
{
  uint16_t bitpattern;
  uint8_t dig, num, seg, i, bit;

  for (dig = 0; dig < SL_SEGMENT_LCD_NUM_DIGITS; dig++) {
    for (num = 0; num < 10; num++) {
      for (seg = 0; seg < 8; seg++) {
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
 * Function to determine total COM line count for the LCD.
 ******************************************************************************/
uint8_t get_count(void)
{
  uint8_t count = LCD_COM_NUM;

  // Check if the LCD supports additional COM as SEG lines.
  if (LCD_OCTAPLEX) {
    count += LCD_SEGASCOM_NUM;
  }

  return count;
}

/***************************************************************************//**
 * LDMA IRQ handler.
 ******************************************************************************/
void LDMA_IRQHandler(void)
{
  uint32_t pending;

  // Read interrupt source
  pending = LDMA_IntGet();

  // Clear interrupts
  LDMA_IntClear(pending);

  // Check for LDMA error
  if (pending & LDMA_IF_ERROR) {
    // Loop here to enable the debugger to see what has happened
    while (1) {
    }
  }
}

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void segment_lcd_app_init(void)
{
  int8_t num, seg, dig;

  // Initialize the LCD
  sl_segment_lcd_init(true);

  // Example only used upper numeric segments; disable unused segments
  SL_LCD_SEGMENTS_ALPHA_DIS();

  // Fill the segments[][][] buffer
  segment_lcd_ldma_buffer_init();

  // Fill the display[][] buffer to output:
  for (num = 0; num < 10; num++) {
    for (seg = 0; seg < NUM_SEG; seg++) {
      display[num][seg] = 0;
      for (dig = 0; dig < NUM_DIGIT; dig++) {
        display[num][seg] |= segments[dig][num][seg];
      }
    }
  }

  // Initialize the LDMA
  LDMA_Init_t init = LDMA_INIT_DEFAULT;
  LDMA_Init(&init);

  // Configure the LDMA to trigger on an LCD DMA request
  LDMA_TransferCfg_t transferCfg = LDMA_TRANSFER_CFG_PERIPHERAL_LOOP(
    ldmaPeripheralSignal_LCD,
    NUM_STATE - 1);

  // 1st descriptor sets the base SRC address of the LDMA channel
  descriptors[0] = (LDMA_Descriptor_t)LDMA_DESCRIPTOR_LINKREL_WRITE(
    (uint32_t)&(display[0][0]),
    &(LDMA->CH[LDMA_CHANNEL].SRC),
    1);

  // 2nd descriptor writes values from display[][] buffer to the LCD_SEGn
  // registers
  uint32_t count = get_count();
  descriptors[1] = (LDMA_Descriptor_t)LDMA_DESCRIPTOR_LINKREL_M2M_WORD(
    0,
    &(LCD->SEGD0),
    count,
    0);
  descriptors[1].xfer.srcAddrMode = ldmaCtrlSrcAddrModeRel;
  descriptors[1].xfer.srcInc = ldmaCtrlSrcIncOne;
  descriptors[1].xfer.dstInc = ldmaCtrlDstIncTwo;
  descriptors[1].xfer.structReq = false;
  descriptors[1].xfer.decLoopCnt = 1;

  // 3rd descriptor resets the LOOP counter
  descriptors[2] = (LDMA_Descriptor_t)LDMA_DESCRIPTOR_LINKREL_WRITE(
    NUM_STATE - 1,
    &(LDMA->CH[LDMA_CHANNEL].LOOP),
    -2);

  // Start LDMA transfers
  LDMA_StartTransfer(0, (void *)&transferCfg, (void *)&descriptors[0]);
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void segment_lcd_app_process_action(void)
{
  return;
}
