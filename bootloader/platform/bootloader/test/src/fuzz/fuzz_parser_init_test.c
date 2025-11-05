#include <stdlib.h>
#include <string.h>
#include "btl_parse.h"

int LLVMFuzzerTestOneInput(uint8_t *Data, size_t Size)
{
  if (Size < sizeof(BootloaderParserContext_t) + sizeof(uint8_t)) {
    return -1;
  }

  BootloaderParserContext_t context;
  memcpy(&context, Data, sizeof(BootloaderParserContext_t));

  uint8_t flags = Data[sizeof(BootloaderParserContext_t)];

  parser_init(&(context.parserContext),
              &(context.decryptContext),
              &(context.authContext),
              *flags);
  return 0;
}
