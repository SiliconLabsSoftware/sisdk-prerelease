#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "parser/gbl/btl_gbl_parser.h"
#include "btl_parse.h"

#define BTL_STORAGE_READ_BUFFER_SIZE 128

//This is just like calling storage_initParseSlot
int32_t initParser(BootloaderParserContext_t *context, size_t contextSize)
{
  //In btl core_initParser is called.
  if (sizeof(BootloaderParserContext_t) > contextSize) {
    // Context is not large enough
    return -1;
  }

  memset(context, 0, sizeof(BootloaderParserContext_t));

  context->imageProperties.instructions = 0x00U;

  return parser_init(&(context->parserContext),
                     &(context->decryptContext),
                     &(context->authContext),
                     0U);
}

int LLVMFuzzerTestOneInput(uint8_t *Data, size_t Size)
{
  if (Size < 128) {
    return -1;
  }
  static const BootloaderParserCallbacks_t callbacks = {
    .context = NULL,
    .applicationCallback = NULL,
    .metadataCallback = NULL,
    .bootloaderCallback = NULL
  };

  BootloaderParserContext_t context;
  size_t contextSize = sizeof(context);

  int32_t retval = 0;
  initParser(&context, contextSize);
  if (retval != BOOTLOADER_OK) {
    return -1;
  }

  context.imageProperties.instructions = 0x01;//BTL_IMAGE_INSTRUCTION_APPLICATION

  uint8_t readBuffer[BTL_STORAGE_READ_BUFFER_SIZE];
  memcpy(readBuffer, Data, sizeof(readBuffer));
  parser_parse(&(context.parserContext),
               &(context.imageProperties),
               readBuffer,
               BTL_STORAGE_READ_BUFFER_SIZE,
               &callbacks);

  return 1;
}
