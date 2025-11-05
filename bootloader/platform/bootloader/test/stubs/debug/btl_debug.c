#include "btl_debug.h"
#include <stdbool.h>

#if defined(SL_DEBUG_ASSERT_VERBOSE)
SL_NORETURN void btl_assert(const char* file, int line)
{
  // Reset here
#if defined(SL_DEBUG_PRINT) && (SL_DEBUG_PRINT == 1)
  btl_debugWriteString("Assert: (");
  btl_debugWriteString(file);
  btl_debugWriteString(": ");
  btl_debugWriteInt(line);
  btl_debugWriteChar(')');
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
#else
  (void)file;
  (void)line;
#endif
  while (true) {
  }
}

#else
void btl_assert(void)
{
  // Reset here
#if defined(SL_DEBUG_PRINT) && (SL_DEBUG_PRINT == 1)
  btl_debugWriteLine("Assert!");
#endif
  while (true) {
  }
}

#endif

#if defined(SL_DEBUG_PRINT) && (SL_DEBUG_PRINT == 1)

#include <string.h>

static void btl_debugWriteNibbleHex(uint8_t nibble)
{
  nibble = nibble > 9u ? nibble + 0x37u : nibble + 0x30u;
  BTL_DEBUG_PRINTC((char)nibble);
}

void btl_debugWriteCharHex(uint8_t number)
{
  btl_debugWriteNibbleHex((number & 0xF0u) >> 4);
  btl_debugWriteNibbleHex((number & 0x0Fu) >> 0);
}

void btl_debugWriteShortHex(uint16_t number)
{
  btl_debugWriteCharHex((uint8_t)((number & 0xFF00u) >> 8));
  btl_debugWriteCharHex((uint8_t)(number & 0x00FFu));
}

void btl_debugWriteWordHex(uint32_t number)
{
  btl_debugWriteShortHex((uint16_t)((number & 0xFFFF0000UL) >> 16));
  btl_debugWriteShortHex((uint16_t)(number & 0x0000FFFFUL));
}

void btl_debugWriteInt(int number)
{
  char buffer[10] = { 0 };
  size_t position = 0;
  uint32_t digit;
  uint32_t unsignedNumber;
  bool negative = false;

  if (number < 0) {
    negative = true;
    number = 0L - number;
  }
  unsignedNumber = (uint32_t)number;

  do {
    digit = unsignedNumber;
    unsignedNumber /= 10u;
    digit -= unsignedNumber * 10u;
    buffer[position] = (char)digit;
    position++;
  } while (unsignedNumber != 0u);

  if (negative) {
    btl_debugWriteChar('-');
  }

  do {
    position--;
    btl_debugWriteChar((buffer[position] + 48u));
  } while (position != 0u);
}

void btl_debugWriteString(const char * s)
{
  size_t position = 0;
  while (s[position] != (char)0u) {
    btl_debugWriteChar(s[position]);
    position++;
  }
}

void btl_debugWriteLine(const char * s)
{
  btl_debugWriteString(s);
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
}

void btl_debugWriteNewline(void)
{
  btl_debugWriteString(BTL_DEBUG_NEWLINE);
}

#else
static void btl_debugWriteNibbleHex(uint8_t nibble)
{
  (void) nibble;
}

void btl_debugWriteCharHex(uint8_t number)
{
  (void) number;
}

void btl_debugWriteShortHex(uint16_t number)
{
  (void) number;
}

void btl_debugWriteWordHex(uint32_t number)
{
  (void) number;
}

void btl_debugWriteInt(int number)
{
  (void) number;
}

void btl_debugWriteString(const char * s)
{
  (void) s;
}

void btl_debugWriteLine(const char * s)
{
  (void) s;
}

void btl_debugWriteNewline(void)
{
  (void) 0;
}

#endif // BTL_PLUGIN_DEBUG_PRINT
