#ifndef BTL_DEBUG_H
#define BTL_DEBUG_H

#include <stdint.h>
#if defined(SL_DEBUG_ASSERT) && (SL_DEBUG_ASSERT == 1)
#if defined(SL_DEBUG_ASSERT_VERBOSE)
void btl_assert(const char* file, int line);

#define BTL_ASSERT(exp)  ((exp) ? ((void)0) : btl_assert(__FILE__, __LINE__))
#else
void btl_assert(void);

#define BTL_ASSERT(exp)  ((exp) ? ((void)0) : btl_assert())
#endif
#else
/// Assertion in bootloader
#define BTL_ASSERT(exp)  ((void)(exp))
#endif

// Print debug information throughout the bootloader
#if defined(SL_DEBUG_PRINT) && (SL_DEBUG_PRINT == 1)

#ifndef BTL_DEBUG_NEWLINE
#define BTL_DEBUG_NEWLINE "\n"
#endif

void btl_debugInit(void);
void btl_debugWriteChar(char c);
void btl_debugWriteString(const char * s);
void btl_debugWriteLine(const char * s);

void btl_debugWriteCharHex(uint8_t number);
void btl_debugWriteShortHex(uint16_t number);
void btl_debugWriteWordHex(uint32_t number);
void btl_debugWriteInt(int number);
void btl_debugWriteNewline(void);

#define BTL_DEBUG_INIT()                  (btl_debugInit())
#define BTL_DEBUG_PRINT(str)              (btl_debugWriteString(str))
#define BTL_DEBUG_PRINTLN(str)            (btl_debugWriteLine(str))
#define BTL_DEBUG_PRINTC(chr)             (btl_debugWriteChar(chr))

#define BTL_DEBUG_PRINT_CHAR_HEX(number)  (btl_debugWriteCharHex(number))
#define BTL_DEBUG_PRINT_SHORT_HEX(number) (btl_debugWriteShortHex(number))
#define BTL_DEBUG_PRINT_WORD_HEX(number)  (btl_debugWriteWordHex(number))
#define BTL_DEBUG_PRINT_LF()              (btl_debugWriteNewline())

#else // No debug prints

/// Initialize debug output
#define BTL_DEBUG_INIT()                  do {} while (0)
/// Print a string to debug out
#define BTL_DEBUG_PRINT(str)              do {} while (0)
/// Print a string followed by a newline to debug out
#define BTL_DEBUG_PRINTLN(str)            do {} while (0)
/// Print a character to debut out
#define BTL_DEBUG_PRINTC(chr)             do {} while (0)
/// Print a single hex byte
#define BTL_DEBUG_PRINT_CHAR_HEX(number)  do {} while (0)
/// Print two hex bytes
#define BTL_DEBUG_PRINT_SHORT_HEX(number) do {} while (0)
/// Print a hex word
#define BTL_DEBUG_PRINT_WORD_HEX(number)  do {} while (0)
/// Print a newline
#define BTL_DEBUG_PRINT_LF()              do {} while (0)

#endif

/**
 * @} (end addtogroup Debug)
 * @} (end addtogroup Components)
 */

#endif // BTL_DEBUG_H
