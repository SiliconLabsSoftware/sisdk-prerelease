from pyradioconfig.parts.lynx.calculators.calc_freq_offset_comp import CALC_Freq_Offset_Comp_lynx
from pycalcmodel.core.variable import ModelVariableFormat
from py_2_and_3_compatibility import *


class CALC_Freq_Offset_Comp_ocelot(CALC_Freq_Offset_Comp_lynx):

    def buildVariables(self, model):
        super().buildVariables(model)

        self._addModelVariable(model, 'afc_tx_adjust_enable', bool, ModelVariableFormat.DECIMAL,
                               desc='Enable TX frequency adjustment based on AFC during RX')

    def calc_offsub_reg(self, model):


        rxbrfrac = 2*model.vars.rxbrfrac_actual.value
        timing_window = model.vars.timing_window_actual.value
        demod_sel = model.vars.demod_select.value

        if(demod_sel==model.vars.demod_select.var_enum.LEGACY):

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

    def calc_freqoffestper_val(self, model):

        mod_type = model.vars.modulation_type.value

        if (mod_type == model.vars.modulation_type.var_enum.BPSK):
            freqoffestper = 2
        elif (mod_type == model.vars.modulation_type.var_enum.OQPSK):
            freqoffestper = 1
        elif (mod_type == model.vars.modulation_type.var_enum.FSK2) or \
                (mod_type == model.vars.modulation_type.var_enum.FSK4) or \
                (mod_type == model.vars.modulation_type.var_enum.MSK):
            freqoffestper = 2
        else:
            freqoffestper = 0

        model.vars.frequency_offset_period.value = freqoffestper

    def calc_freq_offset(self, model):

        rx_ppm = model.vars.rx_xtal_error_ppm.value
        tx_ppm = model.vars.tx_xtal_error_ppm.value
        rf_freq_hz = model.vars.base_frequency_hz.value * 1.0

        freq_offset = (rx_ppm + tx_ppm) * rf_freq_hz / 1e6

        model.vars.freq_offset_hz.value = int(round(freq_offset,0))

    def calc_freq_offset_scale_value(self, model):
        """
        Series 2 scaling fixes of freq_offset_scale
        
        Fixes and improvements:
        - Original calculation did not count with SRC1 and SRC2; experimental results
        show that using demod_rate_actual is correct way to go.
        - Coherent demod is MODEM_CTRL1_PHASEDEMOD == 2
        - BCR EN2TBEST doubles the resolution
        - TRECS scales depends on REMODEN
        
        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        demod_rate = model.vars.demod_rate_actual.value
        osr = model.vars.oversampling_rate_actual.value # This is (2*RXBRFRAC)
        mod_format = model.vars.modulation_type.value
        chip_rate = model.vars.baudrate.value # In the calculator chiprate = baudrate
        freq_gain = model.vars.freq_gain_actual.value
        is_trecs = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        remoden = model.vars.MODEM_PHDMODCTRL_REMODEN.value
        phscale = model.vars.MODEM_TRECPMDET_PHSCALE.value
        demod_sel = model.vars.demod_select.value
        
        is_bcr = (demod_sel == model.vars.demod_select.var_enum.BCR)
        
        if is_bcr:
            rawndec = 2 ** model.vars.MODEM_BCRDEMODOOK_RAWNDEC.value
        else:
            rawndec = 1
        
        if hasattr(model.vars, 'MODEM_BCRDEMODAFC0_EN2TBEST'):
            en2tbest = model.vars.MODEM_BCRDEMODAFC0_EN2TBEST.value
        else:
            en2tbest = False

        try:
            if model.vars.MODEM_CTRL1_PHASEDEMOD.value == 2: # (COH)
                freq_offset_scale = (chip_rate//(2**13))*1.0 # force a float #
            else:
                if mod_format == model.vars.modulation_type.var_enum.FSK2 or \
                    mod_format == model.vars.modulation_type.var_enum.MSK or \
                    mod_format == model.vars.modulation_type.var_enum.FSK4:
                    if is_trecs and not remoden:
                        freq_offset_scale = demod_rate * (2**phscale) / (osr * 256.0)
                    elif is_bcr:
                        freq_offset_scale = demod_rate / (rawndec * osr * 256.0)
                    else:
                        freq_offset_scale = demod_rate / (freq_gain * 256.0)
                elif mod_format == model.vars.modulation_type.var_enum.OQPSK or \
                    mod_format == model.vars.modulation_type.var_enum.BPSK or \
                    mod_format == model.vars.modulation_type.var_enum.DBPSK:
                    freq_offset_scale = demod_rate / (osr * 256.0)
                else:
                    freq_offset_scale = 0.0
        except ZeroDivisionError:
            freq_offset_scale = 0.0
            
        if is_bcr and not en2tbest:
            freq_offset_scale = freq_offset_scale * 2.0

        model.vars.freq_offset_scale.value = freq_offset_scale

    def calc_afc_scale_value(self, model):

        freqgain = model.vars.freq_gain_actual.value
        mod_format = model.vars.modulation_type.value
        mode = model.vars.frequency_comp_mode.value
        scale = model.vars.afc_step_scale.value
        remoden = model.vars.MODEM_PHDMODCTRL_REMODEN.value
        remodoutsel = model.vars.MODEM_PHDMODCTRL_REMODOUTSEL.value
        digmix_res = model.vars.digmix_res_actual.value
        synth_res = model.vars.synth_res_actual.value
        phscale = 2 ** model.vars.MODEM_TRECPMDET_PHSCALE.value
        mode_index = self.freq_comp_mode_index(model, mode)
        demod_sel = model.vars.demod_select.value
        digmixfb = model.vars.MODEM_DIGMIXCTRL_DIGMIXFB.value
        baudrate = model.vars.rx_baud_rate_actual.value
        osr = model.vars.oversampling_rate_actual.value
        vtafcframe = model.vars.MODEM_REALTIMCFE_VTAFCFRAME.value
        afc_tx_adjust_enable = model.vars.afc_tx_adjust_enable.value
        afc_oneshot = model.vars.MODEM_AFC_AFCONESHOT.value
        bcr_det_en = model.vars.MODEM_PHDMODCTRL_BCRDETECTOR.value
        freq_offset_hz = model.vars.freq_offset_hz.value
        modulation_index = model.vars.modulation_index.value
        fast_detect_enable = (model.vars.fast_detect_enable.value == model.vars.fast_detect_enable.var_enum.ENABLED)

        if digmixfb:
            res = digmix_res
        else:
            res = synth_res

        if(demod_sel==model.vars.demod_select.var_enum.LEGACY):
            if mode_index >= 4 and freqgain > 0:
                if mod_format == model.vars.modulation_type.var_enum.FSK2 or \
                    mod_format == model.vars.modulation_type.var_enum.MSK or \
                    mod_format == model.vars.modulation_type.var_enum.FSK4:
                    afcscale = baudrate * osr / ( 256 * freqgain * res)
                    afcscale_tx = baudrate * osr / ( 256 * freqgain * synth_res)
                else:
                    afcscale = baudrate * osr / ( 256 * res)
                    afcscale_tx = baudrate * osr / ( 256 * synth_res)
            else:
                afcscale = 0.0
                afcscale_tx = 0.0

        elif((demod_sel==model.vars.demod_select.var_enum.TRECS_VITERBI or
              demod_sel==model.vars.demod_select.var_enum.TRECS_SLICER) and
             model.vars.MODEM_VITERBIDEMOD_VITERBIKSI1.value != 0) or \
                demod_sel==model.vars.demod_select.var_enum.LONGRANGE:
            if remoden and remodoutsel == 1:
                afcscale = baudrate * osr * phscale / (256 * freqgain * res)
                afcscale_tx = baudrate * osr * phscale / (256 * freqgain * synth_res)
            else:
                if vtafcframe == 0:
                    afcscale = 1.0 * baudrate * phscale / (256 * res)
                elif (freq_offset_hz / baudrate) > 0.57 and modulation_index <= 0.5:
                    afcscale = 1.1 * baudrate * phscale / (256 * res)
                else:
                    afcscale = 0.8 * baudrate * phscale / (256 * res) #Less correction jitter if we use gain < 1
                afcscale_tx = baudrate * phscale / (256 * synth_res)

                if afc_oneshot == 0 and bcr_det_en == 1:
                    afcscale = afcscale / 8.0
        elif (demod_sel == model.vars.demod_select.var_enum.BCR):
            afcscale =  model.vars.pro2_afc_gain.value /  res
            afcscale_tx = model.vars.pro2_afc_gain.value / synth_res
        else:
            afcscale = 0.0
            afcscale_tx = 0.0

        afcscale = afcscale * scale

        if (afc_tx_adjust_enable == False) and afc_oneshot:
            afcscale_tx = 0.0

        if fast_detect_enable:
            afcscale_tx = 0.0

        model.vars.afc_scale.value = afcscale
        model.vars.afc_scale_tx.value = afcscale_tx

    def calc_afc_tx_adjust_enable(self, model):

        afc_tx_adjust_enable = False

        model.vars.afc_tx_adjust_enable.value = afc_tx_adjust_enable

    def calc_afctxmode_reg(self, model):

        afc_tx_adjust_enable = model.vars.afc_tx_adjust_enable.value

        if afc_tx_adjust_enable:
            afctxmode = 1
        else:
            afctxmode = 0

        self._reg_write(model.vars.MODEM_AFC_AFCTXMODE, afctxmode)

    def calc_afc_scale_tx_reg(self, model):
        """
        convert AFC scale TX value to mantissa and exponent register values

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        afc_scale = model.vars.afc_scale_tx.value

        if afc_scale == 0:
            best_m = 0
            best_e = 0
        else:
            best_diff = 99e9
            for m in range(1,32):
                for e in range(-8,8):
                    diff = afc_scale - m * 2**e

                    if diff > 0 and diff <= best_diff:
                        best_diff = diff
                        best_e = e
                        best_m = m

            if best_e < 0:
                best_e += 16

        self._reg_write(model.vars.MODEM_AFCADJTX_AFCSCALEE, int(best_e))
        self._reg_write(model.vars.MODEM_AFCADJTX_AFCSCALEM, int(best_m))

    def calc_afc_scale_tx_actual(self, model):
        """

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        e = float(model.vars.MODEM_AFCADJTX_AFCSCALEE.value)
        m = float(model.vars.MODEM_AFCADJTX_AFCSCALEM.value)

        if e > 7:
            e -= 16

        model.vars.afc_scale_tx_actual.value = m * 2**e

    def calc_fdm0thresh_val(self, model):
        """
        in FDM0 mode set FDM0THRESH register

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        timingbases = model.vars.timingbases_actual.value

        if timingbases > 0:
            model.vars.fdm0_thresh.value = 0
            return

        fdm0_thresh = 32 #* scale

        if fdm0_thresh < 8:
            fdm0_thresh = 8
        elif fdm0_thresh > 71:      # Limiting so the register won't overflow
            fdm0_thresh = 71        # See calc_fdm0thresh_reg for details

        model.vars.fdm0_thresh.value = int(fdm0_thresh)

    def calc_afc_scale_reg(self, model):
        """
        convert AFC scale value to mantissa and exponent register values

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        afc_scale = model.vars.afc_scale.value

        if afc_scale == 0:
            best_m = 0
            best_e = 0
        else:
            best_diff = 99e9
            best_e = 0
            best_m = 0
            for m in xrange(1, 31, 1):
                e = math.floor(math.log(afc_scale / m, 2))
                diff = abs(afc_scale - m * 2**e)

                if (diff < best_diff) and e >= -8 and e <= 7:
                    best_diff = diff
                    best_e = e
                    best_m = m

            if best_e < 0:
                best_e += 16

            if best_m > 31:
                best_m = 31

        self._reg_write(model.vars.MODEM_AFCADJRX_AFCSCALEE, int(best_e))
        self._reg_write(model.vars.MODEM_AFCADJRX_AFCSCALEM, int(best_m))

    def calc_afc_scale_actual(self, model):
        """
        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        e = float(model.vars.MODEM_AFCADJRX_AFCSCALEE.value)
        m = float(model.vars.MODEM_AFCADJRX_AFCSCALEM.value)

        if e > 7:
            e -= 16

        model.vars.afc_scale_actual.value = m * 2**e

    def calc_freq_comp_mode(self, model):
        """
        determine best frequency compensation mode based on emprical data

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        preamble_detection_length = model.vars.preamble_detection_length.value
        demod_select = model.vars.demod_select.value
        preamsch = model.vars.MODEM_TRECPMDET_PREAMSCH.value
        modtype = model.vars.modulation_type.value
        tol = model.vars.baudrate_tol_ppm.value
        freq_offset_hz = model.vars.freq_offset_hz.value
        baudrate = model.vars.baudrate.value
        rtschmode = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value
        antdivmode = model.vars.antdivmode.value
        fast_detect_enable = (model.vars.fast_detect_enable.value == model.vars.fast_detect_enable.var_enum.ENABLED)

        freq_mode = model.vars.frequency_comp_mode.var_enum.INTERNAL_ALWAYS_ON
        afc_mode = model.vars.afc_run_mode.var_enum.INTERNAL

        if modtype == model.vars.modulation_type.var_enum.OOK or modtype == model.vars.modulation_type.var_enum.ASK:
            afc_mode = model.vars.afc_run_mode.var_enum.INTERNAL
        elif demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:
            if preamsch and not fast_detect_enable and ((rtschmode == 0) or (freq_offset_hz/baudrate <= 2)):
                if antdivmode == model.vars.antdivmode.var_enum.DISABLE or rtschmode == 0:
                    afc_mode = model.vars.afc_run_mode.var_enum.ONE_SHOT
        elif demod_select == model.vars.demod_select.var_enum.BCR:
            if preamble_detection_length >= 16 and tol < 50000:
                afc_mode = model.vars.afc_run_mode.var_enum.ONE_SHOT
        elif demod_select == model.vars.demod_select.var_enum.COHERENT:
            freq_mode = model.vars.frequency_comp_mode.var_enum.DISABLED

        model.vars.frequency_comp_mode.value = freq_mode
        model.vars.afc_run_mode.value = afc_mode

    def calc_afconeshoft_reg(self, model):
        modtype = model.vars.modulation_type.value
        run_mode = model.vars.afc_run_mode.value
        comp_mode = model.vars.frequency_comp_mode.value

        comp_mode_index = self.freq_comp_mode_index(model, comp_mode)

        if run_mode == model.vars.afc_run_mode.var_enum.ONE_SHOT:
            oneshot = 1
        else:
            oneshot = 0

        if comp_mode_index > 3:
            limreset = 1
        else:
            limreset = 0

        self._reg_write(model.vars.MODEM_AFC_AFCONESHOT, oneshot)
        self._reg_write(model.vars.MODEM_AFC_AFCDELDET, 0)
        self._reg_write(model.vars.MODEM_AFC_AFCDSAFREQOFFEST, 0)
        self._reg_write(model.vars.MODEM_AFC_AFCENINTCOMP, 0)
        self._reg_write(model.vars.MODEM_AFC_AFCLIMRESET, limreset)
        self._reg_write(model.vars.MODEM_AFC_AFCGEAR, 3)

    def calc_afcdel_reg(self, model):
        """
        calculate AFC Delay based on over sampling rate (osr) if AFC is enabled

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        mode = model.vars.frequency_comp_mode.value
        osr = model.vars.oversampling_rate_actual.value
        demod_select = model.vars.demod_select.value
        afconeshot = model.vars.MODEM_AFC_AFCONESHOT.value
        del_digmix_to_demod = model.vars.grpdelay_to_demod.value
        remoddwn = model.vars.MODEM_PHDMODCTRL_REMODDWN.value + 1

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:
            if afconeshot:
                afcdel = math.ceil(del_digmix_to_demod / osr * remoddwn)
            else:
                afcdel = 0
        else:
            mode_index = self.freq_comp_mode_index(model, mode)

            if mode_index >= 4:
                afcdel = model.vars.grpdelay_to_demod.value
            else:
                afcdel = 0

        if afcdel > 31:
            afcdel = 31

        self._reg_write(model.vars.MODEM_AFC_AFCDEL, int(afcdel))


    def afc_adj_limit(self, model):

        freq_limit = model.vars.freq_offset_hz.value
        synth_res = model.vars.synth_res_actual.value
        afclimreset = model.vars.afc_lim_reset_actual.value
        digmix_res = model.vars.digmix_res_actual.value
        digmixfb = model.vars.MODEM_DIGMIXCTRL_DIGMIXFB.value

        if digmixfb:
            res = digmix_res
        else:
            res = synth_res

        afcadjlim = freq_limit / res

        if afclimreset:
            afcadjlim *= 1.2

        return int(round(afcadjlim))

    def calc_afc_adjlim_actual(self, model):

        afcadjlim = model.vars.MODEM_AFCADJLIM_AFCADJLIM.value
        synth_res = model.vars.synth_res_actual.value
        digmix_res = model.vars.digmix_res_actual.value
        digmixfb = model.vars.MODEM_DIGMIXCTRL_DIGMIXFB.value

        if digmixfb:
            res = digmix_res
        else:
            res = synth_res

        model.vars.afc_limit_hz_actual.value = afcadjlim * res

    def calc_intafc_preaverage(self, model):
        dsss_sf = model.vars.dsss_spreading_factor.value
        demod_select = model.vars.demod_select.value

        if demod_select == model.vars.demod_select.var_enum.COHERENT:
            FOEPREAVG_mode = 7 # : enable dynamic pre averaging

            stage_0_avg = 1
            stage_1_avg = int(math.log2(dsss_sf))
            stage_2_avg = stage_1_avg + 2
            stage_3_avg = stage_2_avg # : TODO silicon bug prevents continuous increase

            model.vars.MODEM_CTRL5_POEPER.value_forced = 1

            self._reg_write(model.vars.MODEM_CTRL5_FOEPREAVG, FOEPREAVG_mode)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG0, stage_0_avg)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG1, stage_1_avg)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG2, stage_2_avg)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG3, stage_3_avg)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG4, 0)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG5, 0)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG6, 0)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG7, 0)
        else:
            self._reg_write(model.vars.MODEM_CTRL5_FOEPREAVG, 0)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG0, 0)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG1, 0)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG2, 0)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG3, 0)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG4, 0)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG5, 0)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG6, 0)
            self._reg_write(model.vars.MODEM_INTAFC_FOEPREAVG7, 0)
    
    def calc_afcadjrx_res(self, model):
        digmixfb = model.vars.MODEM_DIGMIXCTRL_DIGMIXFB.value
        vtafcframe = model.vars.MODEM_REALTIMCFE_VTAFCFRAME.value
        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        bcrafconeshot = model.vars.MODEM_BCRDEMODAFC1_ONESHOTAFCEN.value

        if digmixfb:
            model.vars.afcadjrx_res.value = model.vars.digmix_res_actual.value
        else:
            model.vars.afcadjrx_res.value = model.vars.synth_res_actual.value
        
        if not vtafcframe and vtdemoden:
            model.vars.afcadjrx_res.value = 0.0
        
        if bcrafconeshot:
            model.vars.afcadjrx_res.value = 0.0