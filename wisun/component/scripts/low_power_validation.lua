-- Low power needs a slow, low-frequency-clocked VCOM UART so the chip can sleep
-- and still receive CLI commands. Component installs after project creation do
-- not re-evaluate .slcp conditions, so check the resolved config here instead.

-- Boolean options may be written as 0/1 or false/true, so compare normalized.
local function normalize(value)
  if value == "false" then return "0" end
  if value == "true" then return "1" end
  return value
end

local function expect(name, expected)
  local config = slc.config(name)
  if config == nil or normalize(config.value) == expected then
    return
  end

  validation.warning(
    "wisun_low_power: " .. name .. " should be " .. expected,
    validation.target_for_defines({name}),
    "Current value: " .. config.value .. ", expected value: " .. expected ..
      ". See " .. name .. " in the generated config headers (config/*.h).",
    validation.quickfix({expected}))
end

if slc.is_provided("wisun_low_power") and slc.is_provided("iostream_eusart") then
  expect("SL_IOSTREAM_EUSART_VCOM_BAUDRATE", "9600")
  expect("SL_IOSTREAM_EUSART_VCOM_ENABLE_HIGH_FREQUENCY", "0")
  expect("SL_CLOCK_MANAGER_EUSART0CLK_SOURCE", "CMU_EUSART0CLKCTRL_CLKSEL_LFXO")
  expect("SL_IOSTREAM_EUSART_VCOM_RESTRICT_ENERGY_MODE_TO_ALLOW_RECEPTION", "0")
  expect("SL_IOSTREAM_EUSART_INSTANCE_RESTRICT_ENERGY_MODE_TO_ALLOW_RECEPTION", "0")
end

-- A USART (as opposed to EUSART) VCOM cannot receive in EM2, so the CLI would
-- either block sleep or miss input while the chip sleeps.
if slc.is_provided("wisun_low_power") and slc.is_provided("iostream_usart")
   and not slc.is_provided("iostream_eusart") then
  validation.warning(
    "wisun_low_power: USART VCOM cannot receive in EM2",
    validation.target_for_project(),
    "Use an EUSART-based iostream (iostream_eusart) clocked from LFXO so the CLI keeps working while the chip sleeps.")
end

-- Micrium OS scheduler/statistics options that are not wanted with low power.
if slc.is_provided("wisun_low_power") and slc.is_provided("micriumos_kernel") then
  expect("OS_CFG_SCHED_ROUND_ROBIN_EN", "0")
  expect("OS_CFG_STAT_TASK_EN", "0")
  expect("OS_CFG_TASK_PROFILE_EN", "0")
end
