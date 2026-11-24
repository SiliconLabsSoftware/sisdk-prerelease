-- Software AES tables land in RAM unless mbedtls_aes_rom_tables is installed.
-- On a 96 KB part that pushes ot-ncp-ftd past the end of SRAM at link time.

local changeset = {}

if slc.is_provided("ot_ncp")
   and slc.is_provided("ot_stack_ftd")
   and slc.is_provided("mbedtls_aes")
   and not slc.is_provided("mbedtls_aes_rom_tables") then

  table.insert(changeset, {
    ['component'] = 'mbedtls_aes_rom_tables',
    ['action'] = 'add'
  })
end

return changeset
