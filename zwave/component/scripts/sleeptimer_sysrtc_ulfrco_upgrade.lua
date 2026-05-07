-- Upgrade helper: align sleeptimer with Z-Wave 8.1+ expectations (SYSRTC + ULFRCO).
-- See SWPROT-10070 / migration guide § Sleeptimer peripheral configuration.

local changeset = {}

local device_series_2 = slc.is_provided("device_series_2")

if slc.is_selected("zw_core") and device_series_2 then
  local sleeptimer_peripheral = slc.config("SL_SLEEPTIMER_PERIPHERAL")
  if sleeptimer_peripheral ~= nil and sleeptimer_peripheral.value ~= "SL_SLEEPTIMER_PERIPHERAL_DEFAULT" then
    table.insert(changeset, {
      ["option"] = "SL_SLEEPTIMER_PERIPHERAL",
      ["value"] = "SL_SLEEPTIMER_PERIPHERAL_DEFAULT",
      ["description"] =
        "Use SYSRTC for the platform sleeptimer (BURTC is reserved for Z-Wave power management).",
      ["status"] = "automatic",
    })
  end

  local sysrtc_clk = slc.config("SL_CLOCK_MANAGER_SYSRTCCLK_SOURCE")
  if sysrtc_clk ~= nil and sysrtc_clk.value ~= "CMU_SYSRTC0CLKCTRL_CLKSEL_ULFRCO" then
    table.insert(changeset, {
      ["option"] = "SL_CLOCK_MANAGER_SYSRTCCLK_SOURCE",
      ["value"] = "CMU_SYSRTC0CLKCTRL_CLKSEL_ULFRCO",
      ["description"] =
        "Clock SYSRTC from ULFRCO (~1 kHz), matching the prior BURTC-based sleeptimer timing; leave LFXO enabled for other clock branches.",
      ["status"] = "automatic",
    })
  end
end

return changeset
