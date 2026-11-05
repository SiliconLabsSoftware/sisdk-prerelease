from pyradioconfig.parts.ocelot.calculators.calc_utilities import CALC_Utilities_Ocelot
from pycalcmodel.core.variable import ModelVariableFormat
from enum import Enum

class Calc_Utilities_Sol(CALC_Utilities_Ocelot):

    def buildVariables(self, model):
        super().buildVariables(model)

        self._addModelVariable(model, 'fefilt_selected', str, ModelVariableFormat.ASCII)

    def calc_fefilt_selected(self, model):

        demod_select = model.vars.demod_select.value

        if demod_select == model.vars.demod_select.var_enum.SOFT_DEMOD:
            fefilt_selected = 'FEFILT1'
        else:
            fefilt_selected = 'FEFILT0'

        model.vars.fefilt_selected.value = fefilt_selected

    def get_fefilt_actual(self, model, reg_name_str):

        fefilt_selected = model.vars.fefilt_selected.value

        reg_name_complete = fefilt_selected+'_'+reg_name_str
        reg = getattr(model.vars, reg_name_complete)

        return reg.value

    def get_fefiltoutputsel_actual(self, model, reg_name_str):

        fefiltoutputsel = model.vars.RAC_RX_FEFILTOUTPUTSEL.value

        if fefiltoutputsel == 0:
            fefiltoutputsel_selected = 'FEFILT0'
        else:
            fefiltoutputsel_selected = 'FEFILT1'

        reg_name_complete = fefiltoutputsel_selected+'_'+reg_name_str
        reg = getattr(model.vars, reg_name_complete)

        return reg.value

    def get_fefilt_value_forced(self, model, reg_name_str):

        fefilt_selected = model.vars.fefilt_selected.value

        reg_name_complete = fefilt_selected+'_'+reg_name_str
        reg = getattr(model.vars, reg_name_complete)

        return reg.value_forced

    def write_fefilt_reg(self, model, reg_name_str, value):

        fefilt_selected = model.vars.fefilt_selected.value

        self._reg_write_by_name_concat(model, fefilt_selected, reg_name_str, value)

    def calc_preamble_syncword_duration(self, model):
        OFDM_LTF_SYMBOLS = 2
        chip_rate = model.vars.sun_oqpsk_chiprate.value
        chip_enum = model.vars.sun_oqpsk_chiprate.var_enum
        softmodem_mod = model.vars.softmodem_modulation_type.value
        baudrate = model.vars.baudrate.value
        encoding = model.vars.symbol_encoding.value
        spreading_factor = model.vars.dsss_spreading_factor.value

        if encoding == model.vars.symbol_encoding.var_enum.DSSS:
            preamble_duration_us = (model.vars.preamble_length.value * spreading_factor) / baudrate * 1e6
            syncword_duration_us = (model.vars.syncword_length.value * spreading_factor) / baudrate * 1e6
        else:
            preamble_duration_us = model.vars.preamble_length.value / baudrate * 1e6
            syncword_duration_us = model.vars.syncword_length.value / baudrate * 1e6

        if softmodem_mod == model.vars.softmodem_modulation_type.var_enum.SUN_OFDM:
            t_sym_us = 1e6 / model.vars.ofdm_symbol_rate.value
            preamble_duration_us = (model.vars.ofdm_stf_length.value + OFDM_LTF_SYMBOLS) * t_sym_us
            syncword_duration_us = 0  # No syncword in this case
        elif softmodem_mod == model.vars.softmodem_modulation_type.var_enum.CW:
            preamble_duration_us = 0   # No preamble in this case
            syncword_duration_us = 0  # No syncword in this case

        elif softmodem_mod == model.vars.softmodem_modulation_type.var_enum.SUN_OQPSK:
            band_frequency = int(model.profile.inputs.base_frequency_hz.var_value / 1e6)

            spreading_16_bands = {780, 915, 917}

            if chip_rate == chip_enum._1000_KCPS and band_frequency in spreading_16_bands:
                spreading = 16
            elif chip_rate == chip_enum._2000_KCPS and band_frequency == 2450:
                spreading = 128
            else:
                spreading = 32

            preamble_duration_us = (model.vars.preamble_length.value * spreading) / baudrate * 1e6
            syncword_duration_us = (model.vars.syncword_length.value * spreading) / baudrate * 1e6

        model.vars.preamble_duration_us.value = float(preamble_duration_us)
        model.vars.syncword_duration_us.value = float(syncword_duration_us)