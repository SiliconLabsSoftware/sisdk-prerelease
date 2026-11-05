from pyradioconfig.parts.panther.calculators.calc_freq_offset_comp import CALC_Freq_Offset_Comp_panther
import math


class CALC_Freq_Offset_Comp_lynx(CALC_Freq_Offset_Comp_panther):

    def calc_freq_comp_mode(self, model):
        """
        determine best frequency compensation mode based on emprical data

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        demod_select = model.vars.demod_select.value

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:
            freq_mode = model.vars.frequency_comp_mode.var_enum.INTERNAL_ALWAYS_ON
            afc_mode = model.vars.afc_run_mode.var_enum.CONTINUOUS

            model.vars.frequency_comp_mode.value = freq_mode
            model.vars.afc_run_mode.value = afc_mode
        else:
            super().calc_freq_comp_mode(model)

    def calc_afcdel_reg(self, model):
        """
        calculate AFC Delay based on over sampling rate (osr) if AFC is enabled

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        osr = model.vars.oversampling_rate_actual.value
        demod_select = model.vars.demod_select.value
        afconeshot = model.vars.MODEM_AFC_AFCONESHOT.value
        del_digmix_to_demod = model.vars.grpdelay_to_demod.value

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:
            if afconeshot:
                afcdel = math.ceil(del_digmix_to_demod / osr)
            else:
                afcdel = 0

            if afcdel > 31:
                afcdel = 31

            self._reg_write(model.vars.MODEM_AFC_AFCDEL, int(afcdel))
        else:
            super().calc_afcdel_reg(model)
            
    def calc_afcadjrx_res(self, model):
        digmixfb = model.vars.MODEM_DIGMIXCTRL_DIGMIXFB.value
        vtafcframe = model.vars.MODEM_REALTIMCFE_VTAFCFRAME.value
        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value

        if digmixfb:
            model.vars.afcadjrx_res.value = model.vars.digmix_res_actual.value
        else:
            model.vars.afcadjrx_res.value = model.vars.synth_res_actual.value
        
        if not vtafcframe and vtdemoden:
            model.vars.afcadjrx_res.value = 0.0

    def calc_freq_offset_scale_value(self, model):
        """
        Series 2 scaling fixes of freq_offset_scale
        
        Fixes and improvements:
        - Original calculation did not count with SRC1 and SRC2; experimental results
        show that using demod_rate_actual is correct way to go.
        - Coherent demod is MODEM_CTRL1_PHASEDEMOD == 2
        - BLE LR PHYs use 2x divider
        
        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        demod_rate = model.vars.demod_rate_actual.value
        osr = model.vars.oversampling_rate_actual.value # This is (2*RXBRFRAC)
        mod_format = model.vars.modulation_type.value
        chip_rate = model.vars.baudrate.value # In the calculator chiprate = baudrate
        freq_gain = model.vars.freq_gain_actual.value
        
        try:
            if model.vars.MODEM_CTRL1_PHASEDEMOD.value == 2: # (COH)
                freq_offset_scale = (chip_rate//(2**13))*1.0 # force a float #
            else:
                if mod_format == model.vars.modulation_type.var_enum.FSK2 or \
                    mod_format == model.vars.modulation_type.var_enum.MSK or \
                    mod_format == model.vars.modulation_type.var_enum.FSK4:
                    freq_offset_scale = demod_rate / (freq_gain * 256.0)
                elif mod_format == model.vars.modulation_type.var_enum.OQPSK or \
                    mod_format == model.vars.modulation_type.var_enum.BPSK or \
                    mod_format == model.vars.modulation_type.var_enum.DBPSK:
                    freq_offset_scale = demod_rate / (osr * 256.0)
                else:
                    freq_offset_scale = 0.0
        except ZeroDivisionError:
            freq_offset_scale = 0.0
            
        if model.vars.MODEM_LONGRANGE_LRBLE.value == 1:
            freq_offset_scale /= 2.0
        
        model.vars.freq_offset_scale.value = freq_offset_scale