from pyradioconfig.parts.ocelot.calculators.calc_freq_offset_comp import CALC_Freq_Offset_Comp_ocelot
from py_2_and_3_compatibility import *

class Calc_Freq_Offset_Comp_Bobcat(CALC_Freq_Offset_Comp_ocelot):

    def calc_offsub_reg(self, model):


        rxbrfrac = 2*model.vars.rxbrfrac_actual.value
        timing_window = model.vars.timing_window_actual.value
        demod_sel = model.vars.demod_select.value

        enable_parallel_correlation = model.vars.antdiv_enable_parallel_correlation.value
        oversampling_rate_actual = model.vars.oversampling_rate_actual.value

        if enable_parallel_correlation and demod_sel != model.vars.demod_select.var_enum.COHERENT:
            adpcwndsizechip = model.vars.antdiv_adpcwndsize.value
            exp = adpcwndsizechip * oversampling_rate_actual / 128.0
            diff_min = 9999.0
            num = 1
            num_max = 15
            best_num = 1
            best_den = 1
            while num <= num_max:
                den = 1
                while den < num:
                    res = 1.0 * num / den
                    if res <= exp:
                        diff = exp - res
                        if diff < diff_min:
                            diff_min = diff
                            best_num = num
                            best_den = den
                    den += 1
                num += 1

        elif(demod_sel==model.vars.demod_select.var_enum.LEGACY):
            timing_samples = timing_window * rxbrfrac
            closest_power_of_two = 2.0 ** (math.floor(math.log(timing_samples, 2)))

            if timing_samples == closest_power_of_two:
                best_den = 0
                best_num = 0
            else:
                error_min = 1e9

                for den in xrange(1, 16):
                    for num in xrange(1, 16):

                        error = abs(closest_power_of_two * num / den - timing_samples)

                        if error < error_min:
                            error_min = error
                            best_den = den
                            best_num = num
        else:
            best_den = 0
            best_num = 0

        self._reg_write(model.vars.MODEM_TIMING_OFFSUBDEN, best_den)
        self._reg_write(model.vars.MODEM_TIMING_OFFSUBNUM, best_num)

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