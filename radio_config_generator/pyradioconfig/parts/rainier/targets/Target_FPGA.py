from pyradioconfig.calculator_model_framework.interfaces.itarget import ITarget
from os.path import join

class TargetFpgaRainier(ITarget):

    _targetName = "FPGA"
    _description = "Supports the OTA FPGA"
    _store_config_output = False
    _cfg_location = join('target_fpga','rainier')
    _tag = "FPGA"

    def target_calculate(self, model):

        model.vars.adc_rate_mode.value_forced = model.vars.adc_rate_mode.var_enum.FULLRATE
        model.vars.adc_clock_mode.value_forced = model.vars.adc_clock_mode.var_enum.HFXOMULT

        model.vars.xtal_frequency_hz.value_forced = int(38.4e6)