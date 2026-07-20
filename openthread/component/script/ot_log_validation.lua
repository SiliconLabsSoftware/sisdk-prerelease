--[[
    This script validates that apps which configure platform defined logging
    include a component that provides ot_platform_defined_logger so the
    platform logging API definitions and associated utility interfaces are
    available.
--]]
local log_output_cfg = slc.config("OPENTHREAD_CONFIG_LOG_OUTPUT")
local platform_logging_enabled = log_output_cfg and log_output_cfg.value == "OPENTHREAD_CONFIG_LOG_OUTPUT_PLATFORM_DEFINED"
local has_platform_logger = slc.is_provided("ot_platform_defined_logger")

if platform_logging_enabled and not has_platform_logger then
    validation.error("A component that provides ot_platform_defined_logger must be included when OPENTHREAD_CONFIG_LOG_OUTPUT is configured to PLATFORM_DEFINED.",
                      validation.target_for_project(),
                      "Include ot_sl_log, ot_rtt_log, ot_backchannel_log, or another component that provides ot_platform_defined_logger, or select a different log output configuration value.",
                      nil
    )
end
