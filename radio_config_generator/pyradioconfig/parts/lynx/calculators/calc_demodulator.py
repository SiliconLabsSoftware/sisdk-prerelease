from pyradioconfig.parts.panther.calculators.calc_demodulator import CALC_Demodulator_panther
from pyradioconfig.parts.lynx.calculators.calc_shaping import CALC_Shaping_lynx
from enum import Enum
import math
import numpy as np
import numpy.matlib
from scipy import signal as sp
import itertools
from collections import OrderedDict
from pycalcmodel.core.variable import ModelVariableFormat, CreateModelVariableEnum
from pyradioconfig.parts.common.calculators.ksi_cache_utils import freeze_for_cache
from pyradioconfig.parts.common.calculators.scipy_cache_utils import resample_poly_cached
from pyradioconfig.calculator_model_framework.Utils.cache_flags import is_advanced_cache_enabled

_ADVANCED_CACHE_EN = is_advanced_cache_enabled()


_KSI_CALC_CACHE_MAXSIZE = 1024
_ksi_calc_cache = OrderedDict()


def _ksi_cache_get(key):
    if not _ADVANCED_CACHE_EN:
        return None
    cached = _ksi_calc_cache.get(key)
    if cached is not None:
        _ksi_calc_cache.move_to_end(key)
    return cached


def _ksi_cache_put(key, value):
    if not _ADVANCED_CACHE_EN:
        return
    if key in _ksi_calc_cache:
        _ksi_calc_cache.move_to_end(key)
    _ksi_calc_cache[key] = value
    if len(_ksi_calc_cache) > _KSI_CALC_CACHE_MAXSIZE:
        _ksi_calc_cache.popitem(last=False)


class CALC_Demodulator_lynx(CALC_Demodulator_panther):

    def buildVariables(self, model, add_demod_rate_var=True):
        super().buildVariables(model)

        if add_demod_rate_var:
            self._add_demod_rate_variable(model)

        self._addModelVariable(model, 'chflatency_actual', int, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'preamble_detection_length', int, ModelVariableFormat.DECIMAL,
                               desc='Number of preamble bits to use for timing detection')
        self._addModelVariable(model, 'phscale_derate_factor', int, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'trecs_enabled', bool, ModelVariableFormat.DECIMAL)

        self._addModelVariable(model, 'rx_deviation_scaled', float, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'freq_dev_max', int, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'freq_dev_min', int, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'modulation_index_for_ksi', float, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'datafilter_taps', int, ModelVariableFormat.DECIMAL)

        self._addModelVariable(model, 'rx_grp_delay_us', float, ModelVariableFormat.DECIMAL,
                               desc='RX group delay in us from adc to demod')

        self._addModelVariable(model, 'adc_rate_mode', Enum, ModelVariableFormat.DECIMAL)
        model.vars.adc_rate_mode.var_enum = CreateModelVariableEnum(
            enum_name='AdcRateModeEnum',
            enum_desc='ADC Clock Rate Mode',
            member_data=[
                ['FULLRATE', 0, 'Full rate mode'],
                ['HALFRATE', 1, 'Half rate mode'],
                ['EIGHTHRATE', 2, 'Eighth rate mode']
            ])

        self._addModelActual(model, 'adc_rate_mode', Enum, ModelVariableFormat.DECIMAL)
        model.vars.adc_rate_mode_actual.var_enum = model.vars.adc_rate_mode.var_enum

        self._addModelActual(model, 'timing_detection_threshold_gain', int, ModelVariableFormat.DECIMAL)
        self._addModelActual(model, 'digmix_res', float, ModelVariableFormat.DECIMAL)
        self._addModelActual(model, 'digmixfreq', int, ModelVariableFormat.DECIMAL)

    def calc_init_advanced(self, model):
        trecs_enabled = model.vars.trecs_enabled.value
        if trecs_enabled:
            model.vars.target_osr.value = 4
            model.vars.src_disable.value = model.vars.src_disable.var_enum.ENABLED
            model.vars.viterbi_enable.value = True
            model.vars.dsa_enable.value = False
        else: # :
            super().calc_init_advanced(model)

    def calc_bw_carson(self, model):
        """calculate carson's rule bandwidth: baudrate + 2 * max frequency deviation
        max frequency deviation can be due desired FSK deviation but also due to
        frequency offset in crystal frequencies.

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        baudrate_hz = model.vars.baudrate.value
        freq_dev_hz = model.vars.deviation.value * 1.0
        trecs_enabled = model.vars.trecs_enabled.value
        harddecision = model.vars.MODEM_VITERBIDEMOD_HARDDECISION.value
        freq_offset_hz = model.vars.freq_offset_hz.value
        modulation_index = model.vars.modulation_index.value
        shaping_filter = model.vars.shaping_filter.value
        shaping_filter_param = model.vars.shaping_filter_param.value

        if trecs_enabled:
            bw_carson  = baudrate_hz + 2*freq_dev_hz
            alpha = self.get_alpha(model, modulation_index, shaping_filter, shaping_filter_param)
            bw_acq = bw_carson + 2 * max(0.0, freq_offset_hz - alpha * bw_carson)
            if harddecision == 0:
                bw_demod = baudrate_hz * 1.1
            else:
                bw_demod = bw_carson

            bandwidth_hz = min(bw_demod, bw_acq)
            model.vars.bandwidth_hz.value = int(bandwidth_hz)
            model.vars.bandwidth_carson_hz.value = int(bw_carson)
        else:
            super().calc_bw_carson(model)

    def get_alpha(self, model, modulation_index, shaping_filter, shaping_filter_param):
        mi = modulation_index
        sf = shaping_filter

        if sf == model.vars.shaping_filter.var_enum.NONE.value:
            if mi < 0.75:
                alpha = 0.1
            elif mi < 0.85:
                alpha = 0
            elif mi < 1.5:
                alpha = -0.1
            else:
                alpha = -0.2
        elif sf == model.vars.shaping_filter.var_enum.Gaussian.value:

            bt = shaping_filter_param # BT might not be defined if not Gaussian shaping so read it here

            if bt < 0.75:
                if mi < 0.95:
                    alpha = 0.2
                elif mi < 1.5:
                    alpha = 0.1
                elif mi < 6.5:
                    alpha = 0
                else:
                    alpha = -0.1
            elif bt < 1.5:
                if mi < 0.85:
                    alpha = 0.1
                elif mi < 1.5:
                    alpha = 0
                else:
                    alpha = -0.1
            elif bt < 2.5:
                if mi < 0.75:
                    alpha = 0.1
                elif mi < 0.85:
                    alpha = 0
                else:
                    alpha = -0.1
        else:
            if (mi < 1.0):
                alpha = 0.2
            elif (mi == 1.0):
                alpha = 0.1
            else:
                alpha = 0

        return alpha

    def _add_demod_rate_variable(self, model):
        self._addModelActual(model, 'demod_rate', float, ModelVariableFormat.DECIMAL)

    def calc_fxo_or_fdec8(self, model):
        flag_using_half_rate_mode = (model.vars.RAC_IFADCTRIM0_IFADCENHALFMODE.value == 1)
        if flag_using_half_rate_mode:
            model.vars.fxo_or_fdec8.value = model.vars.rx_synth_freq_actual.value / 128.0
        else:
            model.vars.fxo_or_fdec8.value = model.vars.rx_synth_freq_actual.value / 64.0

    def calc_rssi_rf_adjust_db(self, model):
        model.vars.rssi_rf_adjust_db.value = -11.0

    def calc_trecs_enabled(self, model):
        demod_select = model.vars.demod_select.value
        concurrent_ble = model.vars.MODEM_COCURRMODE_CONCURRENT.value
        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:
            trecs_enabled = True
        elif concurrent_ble:
            trecs_enabled = True
        else:
            trecs_enabled = False

        model.vars.trecs_enabled.value = trecs_enabled

    def calc_datapath_delays(self, model):

        dec0 = model.vars.dec0_actual.value
        dec1 = model.vars.dec1_actual.value
        dec2 = model.vars.dec2_actual.value
        datafilter_taps = model.vars.MODEM_CTRL2_DATAFILTER.value + 2
        src1_actual = model.vars.src1_ratio_actual.value
        src2_actual = model.vars.src2_ratio_actual.value
        adc_freq_actual = model.vars.adc_freq_actual.value
        trecs_enabled = model.vars.trecs_enabled.value
        oversampling_rate = model.vars.oversampling_rate_actual.value

        dec8_filter_taps = del_dec8 = 22
        dec8_filter_rate = adc_freq_actual / 8
        dec8_grp_delay = dec8_filter_taps / 2
        dec8_grp_delay_us = dec8_grp_delay / adc_freq_actual * 1e6

        dec0_filter_taps = del_dec0 = 27.0 if dec0 in [3, 4, 5] else 40
        dec0_filter_rate = dec8_filter_rate / dec0
        dec0_grp_delay = dec0_filter_taps / 2
        dec0_grp_delay_us = dec0_grp_delay / dec8_filter_rate * 1e6

        dc_ircal_digmix_grp_delay = del_dc_ircal_digmix = 2
        dc_ircal_digmix_rate = dec0_filter_rate
        dc_ircal_digmix_grp_delay_us = dc_ircal_digmix_grp_delay / dc_ircal_digmix_rate * 1e6

        dec1_filter_taps = del_dec1 = (dec1 - 1) * 4.0 + 1
        dec1_filter_rate = dc_ircal_digmix_rate / dec1
        dec1_delay = dec1_filter_taps / 2
        dec1_grp_delay_us = dec1_delay / dc_ircal_digmix_rate * 1e6

        src1_grp_delay = del_src1 = 3
        src1_rate = dec1_filter_rate * src1_actual
        src1_delay_us = src1_grp_delay / src1_rate * 1e6

        chf_filter_taps = del_chflt = 2*len(self.return_coeffs(model))-1
        chf_filter_rate = dec1_filter_rate
        chf_grp_delay = (chf_filter_taps + 1) / 2 - 1 # : channel filter is odd (taps+1)/2.
        chf_grp_delay_us = chf_grp_delay / chf_filter_rate * 1e6

        src2_grp_delay = del_src2 = 2
        src2_rate = chf_filter_rate * src2_actual
        src2_delay_us = src2_grp_delay / chf_filter_rate * 1e6

        del_digigain = 0
        del_cordic = 0

        diff_grp_delay = del_diff = 1
        diff_rate = src2_rate
        diff_delay_us = diff_grp_delay / diff_rate * 1e6

        dec2_grp_delay = del_dec2 = dec2
        dec2_rate = src2_rate / dec2
        dec2_delay_us = dec2_grp_delay / src2_rate * 1e6

        del_data = datafilter_taps
        datafilter_grp_delay = datafilter_taps / 2
        datafilter_rate = dec2_rate
        datafilter_delay_us = datafilter_grp_delay / datafilter_rate * 1e6

        del_adc_to_src = ((((del_dec8 / 8 + del_dec0) / dec0 + del_dc_ircal_digmix + del_dec1) / dec1 + \
                           del_src1) / src1_actual + del_chflt + del_src2) / src2_actual

        del_adc_to_diff = del_adc_to_src + del_digigain + del_cordic + del_diff

        grpdel_mixer_to_diff = ((del_dec1 + 1) / 2 / dec1 + (del_chflt + 1) / 2 + del_src2) / src2_actual + del_digigain + del_cordic + del_diff

        grp_delay_to_src_us = dec8_grp_delay_us + dec0_grp_delay_us + dc_ircal_digmix_grp_delay_us + \
                              dec1_grp_delay_us + src1_delay_us + chf_grp_delay_us + src2_delay_us

        if trecs_enabled:
            grp_delay_us = grp_delay_to_src_us + diff_delay_us
            delay_adc_to_demod = del_adc_to_diff
            delay_adc_to_demod_symbols = delay_adc_to_demod / oversampling_rate
            grpdelay_to_demod = grpdel_mixer_to_diff
            delay_agc = del_adc_to_diff * src2_actual
        else:
            grp_delay_us = grp_delay_to_src_us + diff_delay_us + dec2_delay_us + datafilter_delay_us
            delay_adc_to_demod = (del_adc_to_diff + del_dec2) / dec2 + del_data
            delay_adc_to_demod_symbols = delay_adc_to_demod / oversampling_rate / dec2
            grpdelay_to_demod = (grpdel_mixer_to_diff + (del_dec2 + 1) / 2) / dec2 + (del_data + 1) / 2
            delay_agc = delay_adc_to_demod * dec2 * src2_actual

        model.vars.rx_grp_delay_us.value = grp_delay_us
        model.vars.grpdelay_to_demod.value = int(math.ceil(grpdelay_to_demod))
        model.vars.agc_settling_delay.value = int(math.ceil(delay_agc))
        model.vars.delay_adc_to_demod_symbols.value = int(math.ceil(delay_adc_to_demod_symbols))

    def calc_preamble_detection_length(self, model):

        preamble_length = model.vars.preamble_length.value

        model.vars.preamble_detection_length.value = preamble_length

    def calc_demod_rate_actual(self,model):

        adc_freq_actual = model.vars.adc_freq_actual.value
        dec0_actual = model.vars.dec0_actual.value
        dec1_actual = model.vars.dec1_actual.value
        dec2_actual = model.vars.dec2_actual.value
        src1_actual = model.vars.src1_ratio_actual.value
        src2_actual = model.vars.src2_ratio_actual.value

        demod_rate_actual = adc_freq_actual * src1_actual * src2_actual / (8 * dec0_actual * dec1_actual * dec2_actual)

        model.vars.demod_rate_actual.value = demod_rate_actual

    def calc_phscale_derate_factor(self, model):

        phscale_derate_factor = 1

        model.vars.phscale_derate_factor.value = phscale_derate_factor

    def calc_rx_deviation_scaled(self,model):

        deviation = model.vars.deviation.value
        freq_gain_actual = model.vars.freq_gain_actual.value
        demod_rate_actual = model.vars.demod_rate_actual.value

        rx_deviation_scaled = float(256*deviation*freq_gain_actual/demod_rate_actual)

        model.vars.rx_deviation_scaled.value = rx_deviation_scaled

    def calc_devweightdis_reg(self,model):

        trecs_enabled = model.vars.trecs_enabled.value
        mod_type = model.vars.modulation_type.value
        rx_deviation_scaled = model.vars.rx_deviation_scaled.value

        if(mod_type == model.vars.modulation_type.var_enum.FSK2) or \
          (mod_type == model.vars.modulation_type.var_enum.MSK):
            if(abs(rx_deviation_scaled-64) > 6):
                devweightdis = 1
            else:
                devweightdis = 0
        else:
            devweightdis = 0

        if trecs_enabled:
            self._reg_write(model.vars.MODEM_CTRL2_DEVWEIGHTDIS, devweightdis)
        else:
            super().calc_devweightdis_reg(model)

    def calc_phscale_reg(self, model):
        mi = model.vars.modulation_index.value
        demod_sel = model.vars.demod_select.value
        osr = model.vars.oversampling_rate_actual.value
        phscale_derate_factor = model.vars.phscale_derate_factor.value
        trecs_enabled = model.vars.trecs_enabled.value

        if mi > 0.0:
            phscale_reg = int(round(math.log(2 * mi, 2)))
        else:
            phscale_reg = 0

        phscale_reg += int(round(math.log2(phscale_derate_factor)))

        phscale_reg = max(min(phscale_reg, 3), 0)

        if not trecs_enabled:
            phscale_reg = 0

        self._reg_write(model.vars.MODEM_TRECPMDET_PHSCALE, phscale_reg)

    def calc_phscale_actual(self,model):
        phscale_reg = model.vars.MODEM_TRECPMDET_PHSCALE.value
        model.vars.phscale_actual.value = float(2 ** (phscale_reg))

    def calc_modulation_index_for_ksi(self, model):
        demod_sel = model.vars.demod_select.value
        modtype = model.vars.modulation_type.value
        freq_dev_max = model.vars.freq_dev_max.value
        freq_dev_min = model.vars.freq_dev_min.value
        baudrate = model.vars.baudrate.value

        mi_min = 2.0 * freq_dev_min / baudrate
        mi_max = 2.0 * freq_dev_max / baudrate

        mi_to_use = mi_min + (mi_max - mi_min) * 0.5

        model.vars.modulation_index_for_ksi.value = mi_to_use

    def return_ksi1_calc(self, model, phscale):
        demod_sel = model.vars.demod_select.value
        modtype = model.vars.modulation_type.value
        trecs_enabled = model.vars.trecs_enabled.value
        freq_gain_actual = model.vars.freq_gain_actual.value
        osr = model.vars.oversampling_rate_actual.value
        mi_to_use = model.vars.modulation_index_for_ksi.value

        gain = 1 / phscale

        if (trecs_enabled and
                (modtype == model.vars.modulation_type.var_enum.FSK2 or
                 modtype == model.vars.modulation_type.var_enum.FSK4 or
                 modtype == model.vars.modulation_type.var_enum.MSK)):
            saturation_value = 127
            ksi1 = int(round(saturation_value * mi_to_use * gain))
        else:
            ksi1 = 0

        return ksi1

    def calc_ksi1(self, model):
        trecs_enabled = model.vars.trecs_enabled.value

        phscale_actual = model.vars.phscale_actual.value

        if trecs_enabled:
            model.vars.ksi1.value = self.return_ksi1_calc(model, phscale_actual)
        else:
            model.vars.ksi1.value = 64

    def calc_ksi1_reg(self, model):

        ksi1 = model.vars.ksi1.value

        self._reg_sat_write(model.vars.MODEM_VITERBIDEMOD_VITERBIKSI1, ksi1)

    def gen_frequency_signal(self, x, sf, cf, sfosr, model):
        deviation = model.vars.deviation.value
        baudrate = model.vars.baudrate.value
        demodosr = round(model.vars.oversampling_rate_actual.value)
        src2 = model.vars.MODEM_SRCCHF_SRCRATIO2.value
        datafilter = model.vars.MODEM_CTRL2_DATAFILTER.value
        demod_select = model.vars.demod_select.value
        dec2 = model.vars.dec2_actual.value

        sf = sf / np.sum(sf) * sfosr

        y = sp.lfilter(sf, 1, x)

        z = y * deviation

        t = np.cumsum(z / (baudrate * sfosr))

        u = np.exp(1j * 2 * math.pi * t)

        osr = demodosr

        u2 = resample_poly_cached(u,osr*src2, sfosr*16384)

        v = sp.lfilter(cf, 1, u2)

        v2 = resample_poly_cached(v, 16384, src2)

        a = np.unwrap(np.angle(v2))

        f = a[osr:] - a[0:-osr]

        return f

    def _get_ksi_cache_additional_inputs(self, model):
        """Hook for inheritors to extend lynx KSI cache key inputs.

        If a child part changes gen_frequency_signal() behavior, add every
        newly-read model variable here to keep cache correctness.
        """
        return ()

    def _get_ksi_cache_gen_signal_inputs(self, model):
        """Inputs that affect gen_frequency_signal() output.

        Lynx currently has a simpler waveform path than Ocelot-family parts, so
        only the model values that actually affect the math are included here.

        New-part guide:
        - If you override gen_frequency_signal(), mirror every newly-read model
          variable here so cache invalidates correctly.
        - If return_ksi2_ksi3_calc() adds logic outside gen_frequency_signal(),
          add those fields in _get_ksi_cache_additional_inputs().
        """
        return (
            ("deviation", model.vars.deviation.value),
            ("baudrate", model.vars.baudrate.value),
            ("src2_ratio", model.vars.MODEM_SRCCHF_SRCRATIO2.value),
        )

    def _get_ksi_cache_impl_discriminator(self):
        """Return implementation identity used to prevent cache collisions.

        Lynx currently has a single known waveform-generation path, but adding
        the active implementation here makes the cache safer for future derived
        parts that override gen_frequency_signal().
        """
        gen_signal_fn = getattr(self.gen_frequency_signal, "__func__", self.gen_frequency_signal)
        return (
            getattr(gen_signal_fn, "__module__", type(self).__module__),
            getattr(gen_signal_fn, "__qualname__", gen_signal_fn.__class__.__name__),
            getattr(self, "SRC2DENUM", None),
        )

    def _build_ksi_cache_key(self, model, ksi1, osr, sf, cfh):
        cache_inputs = (
            ("ksi1", ksi1),
            ("osr", osr),
            ("sf", sf),
            ("cfh", cfh),
            ("impl", self._get_ksi_cache_impl_discriminator()),
            ("gen_signal", self._get_ksi_cache_gen_signal_inputs(model)),
        )
        cache_inputs += tuple(self._get_ksi_cache_additional_inputs(model))
        return ("lynx_ksi2_ksi3_v4", freeze_for_cache(cache_inputs))

    def return_ksi2_ksi3_calc(self, model, ksi1):
        osr = int(round(model.vars.oversampling_rate_actual.value))
        _ksi_key = None

        if (ksi1 == 0):
            best_ksi2 = 0
            best_ksi3 = 0
        else:
            sf = CALC_Shaping_lynx().get_shaping_filter(model)/1.0
            sfosr = 8 # shaping filter coeffs are sampled at 8x

            cfh = np.asarray(self.return_coeffs(model))
            cfwb = cf = np.block([cfh, cfh[-2::-1]])/1.0

            if _ADVANCED_CACHE_EN:
                try:
                    _ksi_key = self._build_ksi_cache_key(model, ksi1, osr, sf, cfh)
                    _cached = _ksi_cache_get(_ksi_key)
                    if _cached is not None:
                        return _cached
                except Exception:
                    _ksi_key = None

            a = np.array([ 1.0, 0, 0, 0, 0, 0, 0, 0])
            b = np.array([-1.0, 0, 0, 0, 0, 0, 0, 0])

            x1 = np.matlib.repmat(np.append(np.matlib.repmat(a, 1, 3),np.matlib.repmat(b, 1, 3)), 1, 4)
            f1 = self.gen_frequency_signal( x1[0], sf, cf, sfosr, model)

            x2 = np.matlib.repmat(np.append(np.matlib.repmat(a, 1, 2), np.matlib.repmat(b, 1, 2)), 1, 6)
            f2 = self.gen_frequency_signal( x2[0], sf, cf, sfosr, model)

            x3 = np.matlib.repmat(np.append(np.matlib.repmat(a, 1, 1), np.matlib.repmat(b, 1, 1)), 1, 12)
            f3 = self.gen_frequency_signal( x3[0], sf, cf, sfosr, model)

            f3wb = self.gen_frequency_signal( x3[0], sf, cfwb, sfosr, model)

            ind = osr - 1
            scaler = ksi1 / np.max(np.abs(f1[ind + 8 * osr - 1: - 2 * osr: osr]))
            f2 = scaler * f2
            f3 = scaler * f3
            f3wb = scaler * f3wb


            best_ksi3 = 0
            for ph in range(osr):
                ksi3 = np.max(np.round(np.abs(f3[ - 6 * osr + ph: - 2 * osr: osr])))
                if ksi3 > best_ksi3:
                    best_ksi3 = ksi3

            best_cost = 1e9
            for ph in range(osr):
                x = np.round(np.abs(f2[- 6 * osr + ph: - 2 * osr: osr]))
                cost = np.sum(np.abs(x - np.mean(x)))
                if cost < best_cost:
                    best_cost = cost
                    best_ksi2 = np.round(np.mean(x))

        best_ksi2 = ksi1 if best_ksi2 > ksi1 else best_ksi2
        best_ksi3 = best_ksi2 if best_ksi3 > best_ksi2 else best_ksi3
        _result = (best_ksi2, best_ksi3)
        if _ADVANCED_CACHE_EN and _ksi_key is not None:
            _ksi_cache_put(_ksi_key, _result)
        return _result

    def calc_ksi2_ksi3(self, model):
        trecs_enabled = model.vars.trecs_enabled.value

        ksi1 = model.vars.ksi1.value

        if trecs_enabled:
            ksi2, ksi3 = self.return_ksi2_ksi3_calc(model, ksi1)
            model.vars.ksi2.value = int(ksi2)
            model.vars.ksi3.value = int(ksi3)
        else:
            model.vars.ksi2.value = 48
            model.vars.ksi3.value = 32

    def calc_ksi2_ksi3_reg(self, model):

        ksi2 = model.vars.ksi2.value
        ksi3 = model.vars.ksi3.value

        self._reg_sat_write(model.vars.MODEM_VITERBIDEMOD_VITERBIKSI2, int(ksi2))
        self._reg_sat_write(model.vars.MODEM_VITERBIDEMOD_VITERBIKSI3, int(ksi3))

    def return_coeffs(self, model):
        bwsel = model.vars.MODEM_SRCCHF_BWSEL.value

        if bwsel == 0:
            coeffs = [46, 105, 94, -10, -155, -230, -124, 192, 629, 1009, 1160]
        elif bwsel == 1:
            coeffs = [44, 88, 80, 8, -100, -188, -176, -12, 288, 640, 928, 1036]
        else: # bwsel == 2 or bwsel == 3:
            coeffs = [20, 56, 80, 76, 32, -40, -120, -168, -148, -40, 148, 388, 624, 800, 864]

        return coeffs

    def calc_freq_dev_min(self, model):
        fdeverror = 0# : TODO add model.vars.deviation_tol_ppm.value
        deviation = model.vars.deviation.value
        freq_dev_min = int(deviation - (fdeverror * deviation) / 1000000)
        model.vars.freq_dev_min.value = freq_dev_min

    def calc_freq_dev_max(self, model):
        fdeverror = 0 # : TODO add model.vars.deviation_tol_ppm.value
        deviation = model.vars.deviation.value
        freq_dev_max = int(deviation + (fdeverror * deviation) / 1000000)
        model.vars.freq_dev_max.value = freq_dev_max

    def calc_viterbi_demod_expect_patt_value(self, model):
        syncword0 = model.vars.syncword_0.value
        syncword0_actual = model.vars.MODEM_SYNC0_SYNC0.value
        mapfsk = model.vars.MODEM_CTRL0_MAPFSK.value
        trecs_pre_bits_to_syncword = model.vars.trecs_pre_bits_to_syncword.value
        preamble_string = model.vars.preamble_string.value
        syncword_len = model.vars.syncword_length.value
        concurrent_phy = (model.vars.MODEM_COCURRMODE_CONCURRENT.value == 1)
        preamble_pattern = model.vars.preamble_pattern.value
        trecs_enabled = model.vars.trecs_enabled.value

        if concurrent_phy:
            viterbi_demod_expect_patt = (syncword0 >> 1) | ((preamble_pattern & 1) << 31)
            patt = viterbi_demod_expect_patt
        elif trecs_enabled:
            syncword_str_part = '{:032b}'.format(syncword0_actual)[-syncword_len:] #Read the rightmost characters

            if trecs_pre_bits_to_syncword > 0:
                preamble_str_part = preamble_string[-trecs_pre_bits_to_syncword:]  # Read the rightmost characters
            else:
                preamble_str_part = ""

            effective_syncword_str = preamble_str_part+syncword_str_part[::-1]+'0'*32 #reverse syncword part only

            viterbi_demod_expect_patt = int(effective_syncword_str[0:32],2)
            patt = viterbi_demod_expect_patt

            if mapfsk:
                patt ^= 0xFFFFFFFF
        else:
            patt = 0x123556B7

        model.vars.viterbi_demod_expect_patt.value = patt

    def calc_viterbi_demod_expect_patt_head_tail_value(self, model):

        demod_select = model.vars.demod_select.value
        preamble_pattern = model.vars.preamble_pattern.value
        mapfsk = model.vars.MODEM_CTRL0_MAPFSK.value

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or \
            demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:

            viterbi_demod_expect_patt_head_tail = int((preamble_pattern << 2) & 0xF)

            if mapfsk:
                viterbi_demod_expect_patt_head_tail ^= 0xF

            model.vars.viterbi_demod_expect_patt_head_tail.value = viterbi_demod_expect_patt_head_tail
        else:
            model.vars.viterbi_demod_expect_patt_head_tail.value = 5

    def calc_viterbi_demod_expect_patt_head_tail_reg(self, model):

        viterbi_demod_expect_patt_head_tail = model.vars.viterbi_demod_expect_patt_head_tail.value
        self._reg_write(model.vars.MODEM_VTCORRCFG1_EXPECTHT, viterbi_demod_expect_patt_head_tail)

    def calc_syncacqwin_actual(self, model):
        """ set syc word acquisition window for TRECS basd on register value

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        model.vars.syncacqwin_actual.value = 4 * (model.vars.MODEM_REALTIMCFE_SYNCACQWIN.value + 1)

    def calc_datafilter(self,model):

        osr = model.vars.oversampling_rate_actual.value
        demod_sel = model.vars.demod_select.value
        modformat = model.vars.modulation_type.value
        trecs_enabled = model.vars.trecs_enabled.value

        if demod_sel==model.vars.demod_select.var_enum.COHERENT and \
                modformat == model.vars.modulation_type.var_enum.OQPSK:
            datafilter_taps = 9
        if modformat == model.vars.modulation_type.var_enum.OQPSK or trecs_enabled:
            datafilter_taps = 2 # 2 here translates to datafilter_reg = 0 meaning disabled datafilter
        else:
            if osr >= 8.5:
                datafilter_taps = 9
            elif osr >= 7.5:
                datafilter_taps = 8
            elif osr >= 6.5:
                datafilter_taps = 7
            elif osr >= 5.5:
                datafilter_taps = 6
            elif osr >= 4.5:
                datafilter_taps = 5
            elif osr >= 3.5:
                datafilter_taps = 4
            elif osr >= 2.5:
                datafilter_taps = 3
            else:
                datafilter_taps = 2

        model.vars.datafilter_taps.value = datafilter_taps

    def calc_datafilter_reg(self,model):

        datafilter_taps = model.vars.datafilter_taps.value

        datafilter_reg = datafilter_taps - 2

        if datafilter_reg < 0:
            datafilter_reg = 0

        self._reg_write(model.vars.MODEM_CTRL2_DATAFILTER, datafilter_reg)

    def calc_digmix_res_actual(self,model):

        adc_freq_actual = model.vars.adc_freq_actual.value
        dec0_actual = model.vars.dec0_actual.value
        digmix_res = adc_freq_actual/((2**20) * 8.0 * dec0_actual)
        model.vars.digmix_res_actual.value = digmix_res

    def calc_digmixfreq_actual(self,model):

        digmixfreq_reg = model.vars.MODEM_DIGMIXCTRL_DIGMIXFREQ.value

        if model.vars.lo_injection_side.value == model.vars.lo_injection_side.var_enum.HIGH_SIDE:
            digmixfreq_actual = int(digmixfreq_reg * model.vars.digmix_res_actual.value)
        else:
            model_var = model.vars.MODEM_DIGMIXCTRL_DIGMIXFREQ
            digmixfreq_regsize = model_var.rm.bitWidth
            digmixfreq_pos = (2 ** digmixfreq_regsize) - digmixfreq_reg
            digmixfreq_actual = -1 * int(digmixfreq_pos * model.vars.digmix_res_actual.value)

        model.vars.digmixfreq_actual.value = digmixfreq_actual
