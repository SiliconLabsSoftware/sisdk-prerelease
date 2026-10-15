local changeset = {}

if slc.is_provided("power_manager") then
  local pin_retention = slc.config("SL_POWER_MANAGER_INIT_EMU_EM4_PIN_RETENTION_MODE")

  if pin_retention ~= nil then
    table.insert(changeset, {
      ["option"] = "SL_POWER_MANAGER_INIT_EMU_EM4_PIN_RETENTION_MODE",
      ["value"] = pin_retention.value,
      ["description"] = "Preserve EM4 pin retention mode in sl_power_manager_em4_config.h"
    })
  end
end

return changeset
