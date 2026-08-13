if slc.is_provided("device_cortexm")
   and not slc.is_provided("device_series_2")
   and not slc.is_provided("device_generic_family_sixx301") then
  validation.error(
    "sl_system is supported only on Series 2 and Series 3 sixx301. Use sl_main on other Series 3 devices.",
    validation.target_for_project(),
    "Replace sl_system with sl_main in your project.",
    nil)
end
