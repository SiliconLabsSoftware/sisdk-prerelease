#include "api/btl_interface.h"
#include "em_device.h"

// Place BBT (vector table) and App properties using a struct, to ensure
// app properties come _after_ the vector table in flash
const struct {
  BareBootTable_t bbt;
  ApplicationProperties_t appProps;
} boot_valid = {
  .bbt = {
    .signature = (void *)&boot_valid.appProps
  },
  .appProps = {
    .magic = APPLICATION_PROPERTIES_MAGIC,
    .app = {
      .version = 0x13,
      .productId = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2, 3, 4, 5, 6, 7 }
    }
  }
};

const struct {
  BareBootTable_t bbt;
  ApplicationProperties_t appProps;
} boot_invalid = {
  .bbt = {
    .signature = (void *)0xFFFFFFFFUL
  },
  .appProps = {
    .magic = APPLICATION_PROPERTIES_MAGIC,
    .app = {
      .version = 0x13
    }
  }
};

const struct {
  BareBootTable_t bbt;
  ApplicationProperties_t appProps;
} boot_invalid_magic = {
  .bbt = {
    .signature = (void *)&boot_invalid_magic.appProps
  },
  .appProps = {
    .magic = {
      0x13, 0xb7, 0x79, 0xfa,
      0xc9, 0x25, 0xdd, 0xb7,
      0xad, 0xf3, 0xcf, 0xe0,
      0xf1, 0xb6, 0x11, 0xb1
    },
    .app = {
      .version = 0x13,
      .productId = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2, 3, 4, 5, 6, 7 }
    }
  }
};

MainBootloaderTable_t mbt_valid = {
  .startOfAppSpace = (void *)&boot_valid.bbt,
  .endOfAppSpace = (uint32_t *)(FLASH_BASE + 0x40000)
};

MainBootloaderTable_t mbt_invalid = {
  .startOfAppSpace = (void *)&boot_invalid.bbt,
  .endOfAppSpace = (uint32_t *)(FLASH_BASE + 0x40000)
};

MainBootloaderTable_t mbt_invalid_magic = {
  .startOfAppSpace = (void *)&boot_invalid_magic.bbt,
  .endOfAppSpace = (uint32_t *)(FLASH_BASE + 0x40000)
};
