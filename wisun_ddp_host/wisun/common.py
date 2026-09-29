from typing import Optional

#!/usr/bin/env python3
# vim: set sw=2 expandtab:

def get_default_nvm3_parameters(flash_size: int, flashpage_size: int, nvm3_start_addr: Optional[int] = None, nvm3_size: Optional[int] = None) -> (int, int):
  """Return NVM3 base address and size based on flash size.

  :param flash_size: Total flash size in bytes.
  :param flashpage_size: Flash page size in bytes.
  :param nvm3_start_addr: NVM3 base address, or ``None`` to use the default layout.
  :param nvm3_size: NVM3 region size in bytes, or ``None`` for default size.
  :return: ``(nvm3_start_addr, nvm3_size)``.
  """
  if not nvm3_size:
    # Default NVM3 size is 5 flash pages
    nvm3_size = flashpage_size * 5
  if not nvm3_start_addr:
    # Default NVM3 start address is (NVM3 size + 1 flash page) from the end of the flash
    nvm3_start_addr = 0x08000000 + flash_size - nvm3_size - flashpage_size
  return (nvm3_start_addr, nvm3_size)