local time_trace_en = slc.config("NVM3_TIME_TRACE_ENABLE")

if time_trace_en ~= nil and tonumber(time_trace_en.value) ~= 0 then
  if not slc.is_provided("nvm3_time_trace") then
    validation.error(
      "NVM3_TIME_TRACE_ENABLE is set but the nvm3_time_trace component is not included.",
      validation.target_for_defines({ "NVM3_TIME_TRACE_ENABLE" }),
      "Add the nvm3_time_trace component to pull in cycle_counter.",
      nil)
  end
end
