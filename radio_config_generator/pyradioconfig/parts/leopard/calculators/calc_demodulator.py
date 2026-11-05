from pyradioconfig.parts.lynx.calculators.calc_demodulator import CALC_Demodulator_lynx
from enum import Enum
from pycalcmodel.core.variable import ModelVariableFormat, CreateModelVariableEnum
from math import log2, floor
from py_2_and_3_compatibility import *

class calc_demodulator_leopard(CALC_Demodulator_lynx):

    def buildVariables(self, model):

        super().buildVariables(model)

        self._addModelVariable(model, 'adc_clock_mode', Enum, ModelVariableFormat.DECIMAL)
        model.vars.adc_clock_mode.var_enum = CreateModelVariableEnum(
            enum_name='AdcClockModeEnum',
            enum_desc='Defines how the ADC clock is derived',
            member_data=[
                ['HFXOMULT', 0, 'Multiply HFXO for ADC Clock'],
                ['VCODIV', 1, 'Divide VCO for ADC Clock'],
            ])

        self._addModelActual(model, 'adc_clock_mode', Enum, ModelVariableFormat.DECIMAL)
        model.vars.adc_clock_mode_actual.var_enum = model.vars.adc_clock_mode.var_enum
        self._addModelVariable(model, 'adc_xo_mult', int, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'enable_high_mod_trecs', int, ModelVariableFormat.DECIMAL)
        self._addModelActual(model, 'adc_xo_mult', int, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'lo_target_freq', long, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'adc_target_freq', int, ModelVariableFormat.DECIMAL)
        self._addModelActual(model, 'adc_vco_div', int, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'adc_vco_div', int, ModelVariableFormat.DECIMAL)
        self._addModelActual(model, 'a_divider', int, ModelVariableFormat.DECIMAL)
        self._addModelActual(model, 'b_divider', int, ModelVariableFormat.DECIMAL)
        self._addModelActual(model, 'c_divider', int, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'adc_freq_error', float, ModelVariableFormat.DECIMAL)

    def calc_src2_denominator(self, model):
        osr = model.vars.oversampling_rate_actual.value
        datarate = model.vars.baudrate.value
        dec0 = model.vars.dec0_actual.value
        dec1 = model.vars.dec1_actual.value
        dec2 = model.vars.dec2_actual.value
        adc_clock_mode = model.vars.adc_clock_mode.value

        if (model.vars.adc_clock_mode.var_enum.HFXOMULT == adc_clock_mode):
            src2_calcDenominator = 0
        else:
            src2_calcDenominator = int(datarate * dec0 * dec1 * dec2 * osr)

        model.vars.src2_calcDenominator.value = src2_calcDenominator

    def calc_fxo_or_fdec8(self, model):

        flag_using_half_rate_mode = (model.vars.RAC_IFADCTRIM0_IFADCENHALFMODE.value == 1)
        adc_clock_mode = model.vars.adc_clock_mode.value
        xtal_frequency = model.vars.xtal_frequency.value

        if adc_clock_mode == model.vars.adc_clock_mode.var_enum.VCODIV:
            if flag_using_half_rate_mode:
                fxo_or_fdec8 = model.vars.rx_synth_freq_actual.value / 128.0
            else:
                fxo_or_fdec8 = model.vars.rx_synth_freq_actual.value / 64.0
        else:
            fxo_or_fdec8 = xtal_frequency * 1.0

        model.vars.fxo_or_fdec8.value = fxo_or_fdec8

    def calc_log2x4_reg(self, model):
        dec1 = model.vars.dec1_actual.value
        log2df = floor(4.0*log2(dec1))
        log2df = min(63,log2df) #Limit to 6 bits

        reg = (log2df << 1) | 1

        self._reg_write(model.vars.MODEM_BLEIQDSAEXT2_BBSSDEBOUNCETIM, reg)

    def calc_rssi_rf_adjust_db(self, model):

        model.vars.rssi_rf_adjust_db.value = -12.35

    def calc_adc_freq_actual(self, model):
        pass