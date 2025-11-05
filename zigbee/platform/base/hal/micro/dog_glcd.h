/***************************************************************************//**
 * @file
 * @brief
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
#ifndef DOG_GLCD_H
#define DOG_GLCD_H

/*! Define this to get support for lcdPoint() and co. functions */
//#undef LCD_USE_DISPLAY_BUFFER
#define LCD_USE_DISPLAY_BUFFER

#define LCD_MIN_LINE                            (1u)
#define LCD_MAX_LINE                            (4u)
#define LCD_MIN_CHAR                            (1u)
#define LCD_MAX_CHAR                            (22u)
#define LCD_MIN_COLUMN                          (1u)
#define LCD_MAX_COLUMN                          (132u)
#define LCD_PIC_PAGE_NMBR                       (0u)
#define LCD_PIC_COLUMN_NMBR                     (1u)
#define LCD_PIC_ADDRESS_OFFSET                  (2u)

#define LCD_LINE_1                              (1u)
#define LCD_LINE_2                              (2u)
#define LCD_LINE_3                              (3u)
#define LCD_LINE_4                              (4u)

#ifdef LCD_USE_DISPLAY_BUFFER
#define LCD_MAX_X                               (132u)
#define LCD_MAX_Y                               (32u)
#endif

#define ASCII_5X7_MIN                           (0x20)
#define ASCII_5X7_MAX                           (0x7F)

/*****************************************************************************
*  Global Variable Declaration
*****************************************************************************/

extern uint8_t SEG_XDATA lcdData[LCD_MAX_CHAR + 1];

/*****************************************************************************
*  Global Function Definitions
*****************************************************************************/
void LcdInit(void);
void LcdOn(void);
void LcdOff(void);
void LcdClearDisplay(void);

void LcdSetPage(uint8_t data_in);
void LcdSetColumn(uint8_t data_in);
void LcdClearLine(uint8_t line);
void LcdPutChar(uint8_t ch, bool inv);
void LcdPutCharPos(uint8_t line, uint8_t ch_pos, uint8_t ch, bool inv);

void LcdWriteLine(uint8_t line, uint8_t * text);
void LcdDrawPicture(const uint8_t * picture);

uint8_t LcdSetCharCursor(uint8_t line, uint8_t ch_pos);
bit LcdSetPictureCursor(uint8_t page, uint8_t column);

#ifdef LCD_USE_DISPLAY_BUFFER
void lcdPoint(uint8_t x, uint8_t y);
void lcdClear(void);
void lcdRefresh(void);
void LcdIntro(const uint8_t * picture);

#endif

#endif
