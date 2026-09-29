local changeset = {}

if slc.is_provided("sl_main") and slc.is_provided("device_series_3") then
  table.insert(changeset, {
    ["option"] = "SL_MAIN_START_TASK_ALLOCATE_SHORT_TERM",
    ["value"] = "0"
  })
end

return changeset
