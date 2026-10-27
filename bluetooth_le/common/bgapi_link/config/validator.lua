-- Validation script for the BGAPI over cpc-ng component configuration.
local primary_present = slc.is_provided("cpc_ng_primary")
local secondary_present = slc.is_provided("cpc_ng_secondary")
local config_message_size = slc.config("SL_BT_BGAPI_LINK_MESSAGE_SIZE")
local config_payload_size = slc.config("SL_BGAPI_MAX_PAYLOAD_SIZE")
local message_size = config_message_size ~= nil
                     and autonumber_common.autonumber(config_message_size.value)
local payload_size = config_payload_size ~= nil
                     and autonumber_common.autonumber(config_payload_size.value)

-- The BGAPI message header is four bytes. Keep this aligned with
-- SL_BGAPI_MSG_HEADER_LEN from sl_bgapi.h.
local const_bgapi_header_size = 4

if primary_present == secondary_present then
  validation.error(
    "Bluetooth BGAPI over cpc-ng requires exactly one CPC role.",
    validation.target_for_project(),
    "Select either cpc_ng_primary or cpc_ng_secondary.",
    nil
  )
end

if message_size ~= nil and payload_size ~= nil then
  local maximum_message_size = const_bgapi_header_size + payload_size

  if message_size > maximum_message_size then
    validation.error(
      "SL_BT_BGAPI_LINK_MESSAGE_SIZE is " ..
        tostring(message_size) ..
        " bytes, but the configured BGAPI protocol supports messages up to " ..
        tostring(maximum_message_size) .. " bytes.",
      validation.target_for_defines({
        "SL_BT_BGAPI_LINK_MESSAGE_SIZE",
        "SL_BGAPI_MAX_PAYLOAD_SIZE",
      }),
      "Reduce SL_BT_BGAPI_LINK_MESSAGE_SIZE to " ..
        tostring(maximum_message_size) ..
        " or less, or increase SL_BGAPI_MAX_PAYLOAD_SIZE.",
      nil
    )
  end
end
