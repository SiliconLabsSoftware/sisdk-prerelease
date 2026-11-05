from pyradioconfig.calculator_model_framework.interfaces.itarget import ITarget
from os.path import join

class Target_FPGA_Sol(ITarget):

    _targetName = "FPGA"
    _description = "Supports the OTA FPGA"
    _store_config_output = False
    _cfg_location = join('target_fpga','sol')
    _tag = "FPGA"

    def target_calculate(self, model):

        model.vars.adc_clock_mode.value_forced = model.vars.adc_clock_mode.var_enum.HFXOMULT

        model.vars.if_frequency_hz.value_forced = 0

        model.vars.FEFILT0_DIGMIXCTRL_DIGIQSWAPEN.value_forced = 0
        model.vars.FEFILT1_DIGMIXCTRL_DIGIQSWAPEN.value_forced = 0

        model.vars.xtal_frequency_hz.value_forced = int(38.4e6)

        model.vars.fpll_divx.value_forced = 5
        model.vars.fpll_divy.value_forced = 16
        model.vars.fpll_divn.value_forced = 80

        model.vars.frequency_comp_mode.value_forced = model.vars.frequency_comp_mode.var_enum.INTERNAL_ALWAYS_ON
        model.vars.afc_run_mode.value_forced = model.vars.afc_run_mode.var_enum.CONTINUOUS
