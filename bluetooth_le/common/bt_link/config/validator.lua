-- Validation script for the cpc-ng message link configuration.
local config_link_count = slc.config("SL_BT_LINK_COUNT")
local config_queue_size = slc.config("SL_BT_LINK_RUNTIME_QUEUE_SIZE")
local link_count = config_link_count ~= nil
                   and autonumber_common.autonumber(config_link_count.value)
local queue_size = config_queue_size ~= nil
                  and autonumber_common.autonumber(config_queue_size.value)

-- Keep this multiplier aligned with SL_BT_LINK_RUNTIME_CONTROL_EVENTS.
local const_control_events_per_link = 3
local binding_rx_configs = {
  { name = "BGAPI", define = "SL_BT_BGAPI_LINK_RX_DEPTH" },
}

local enabled_binding_count = 0
local combined_rx_depth = 0
local queue_target_defines = {
  "SL_BT_LINK_RUNTIME_QUEUE_SIZE",
  "SL_BT_LINK_COUNT",
}
local binding_details = {}

for _, binding in ipairs(binding_rx_configs) do
  local config_rx_depth = slc.config(binding.define)
  local rx_depth = config_rx_depth ~= nil
                   and autonumber_common.autonumber(config_rx_depth.value)
  if rx_depth ~= nil then
    enabled_binding_count = enabled_binding_count + 1
    combined_rx_depth = combined_rx_depth + rx_depth
    table.insert(queue_target_defines, binding.define)
    table.insert(binding_details,
                 binding.name .. "=" .. tostring(rx_depth))
  end
end

if link_count ~= nil and link_count < enabled_binding_count then
  validation.error(
    "SL_BT_LINK_COUNT is " .. tostring(link_count) ..
      ", but " .. tostring(enabled_binding_count) ..
      " link bindings are enabled.",
    validation.target_for_defines({ "SL_BT_LINK_COUNT" }),
    "Set SL_BT_LINK_COUNT to at least " ..
      tostring(enabled_binding_count) .. ".",
    nil
  )
end

if queue_size ~= nil and link_count ~= nil then
  local control_event_count = const_control_events_per_link
                              * link_count
  local required_queue_size = control_event_count + combined_rx_depth

  if queue_size < required_queue_size then
    local detail = "no binding RX configuration is available"
    if #binding_details > 0 then
      detail = table.concat(binding_details, ", ")
    end

    validation.error(
      "SL_BT_LINK_RUNTIME_QUEUE_SIZE is " ..
        tostring(queue_size) .. ", but at least " ..
        tostring(required_queue_size) .. " events are required (" ..
        tostring(control_event_count) .. " control (" ..
        tostring(const_control_events_per_link) .. " per link * " ..
        tostring(link_count) .. " links) + " ..
        tostring(combined_rx_depth) .. " receive; " .. detail .. ").",
      validation.target_for_defines(queue_target_defines),
      "Set SL_BT_LINK_RUNTIME_QUEUE_SIZE to at least " ..
        tostring(required_queue_size) ..
        ", or reduce the combined binding receive depth to " ..
        tostring(math.max(0,
                          queue_size
                          - control_event_count)) .. ".",
      nil
    )
  end
end
