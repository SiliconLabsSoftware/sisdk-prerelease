from pyradioconfig.parts.panther.calculators.calc_viterbi import CALC_Viterbi_panther
from math import *
from py_2_and_3_compatibility import *
from pycalcmodel.core.variable import ModelVariableFormat

class CALC_Viterbi_lynx(CALC_Viterbi_panther):

    acqwin_unit = 4

    def buildVariables(self, model):

        super().buildVariables(model)

        self._addModelVariable(model,'trecs_pre_bits_to_syncword',int,ModelVariableFormat.DECIMAL,desc='Number of preamble bits to move to syncword with TRECS'	)
        self._addModelVariable(model, 'trecs_effective_preamble_len', int, ModelVariableFormat.DECIMAL,desc='TRECS preamble length minus bits shifted to syncword')
        self._addModelVariable(model, 'trecs_effective_syncword_len', int, ModelVariableFormat.DECIMAL,desc='TRECS syncword length plus bits shifted from preamble')
        self._addModelVariable(model, 'trecs_syncword_timeout_us', float, ModelVariableFormat.DECIMAL, desc='TRECS syncword timeout in us')
        self._addModelVariable(model, 'trecs_optimize_cost_thd', bool, ModelVariableFormat.BINARY, desc='Enable conservative threshold calculation')

    MIN_COST_THD_FULL = 600 # threshold for min cost function when using full 32 bit search (sync or preamble)

    def calc_trecs_demod_enable(self, model):
        demod_select_forced = model.vars.demod_select._value_forced

        enable_trecs = 0
        enable_harddecision = 0
        if demod_select_forced == model.vars.demod_select.var_enum.TRECS_SLICER:
            enable_trecs = 1
        elif demod_select_forced == model.vars.demod_select.var_enum.TRECS_VITERBI:
            enable_trecs = 1

        self._reg_write(model.vars.MODEM_VITERBIDEMOD_VTDEMODEN, enable_trecs)
        self._reg_write(model.vars.MODEM_VITERBIDEMOD_HARDDECISION, enable_harddecision)

    def calc_shift_trecs_pre_bits_to_syncword(self, model):

        demod_select = model.vars.demod_select.value
        preamble_detection_length = model.vars.preamble_detection_length.value
        delay_samp = model.vars.grpdelay_to_demod.value
        osr = model.vars.oversampling_rate_actual.value
        rtschmode_actual = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value
        ber_force_sync = model.vars.ber_force_sync.value
        is_trecs = demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or \
                   demod_select == model.vars.demod_select.var_enum.TRECS_SLICER

        if ber_force_sync:
            syncword_len = 32
        else:
            syncword_len = model.vars.syncword_length.value

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or \
            demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:

            target_cfe_len = 32 #The goal is to end up with a 32 bit window, length must end up as a mult of 4
            delay_symbols = int(ceil(delay_samp / osr)) # Calculate the group delay in symbols
            min_pre_len = 26 + delay_symbols #Need to leave at least 26 preamble bits after delay so that we can enable preamsch and AFC
            min_pre_no_preamsch_len = 8 #Need to leave 8 bits of preamble if preamsch is disabled

            if (syncword_len < target_cfe_len) and (rtschmode_actual == 0) and not ber_force_sync:
                cfe_len_shortage = target_cfe_len - syncword_len

                if  preamble_detection_length >= cfe_len_shortage + min_pre_len:
                    trecs_pre_bits_to_syncword = cfe_len_shortage #Resulting CFE len is equal to target

                elif preamble_detection_length >= min_pre_len:
                    trecs_pre_bits_to_syncword = preamble_detection_length - min_pre_len

                elif preamble_detection_length > min_pre_no_preamsch_len:
                    trecs_pre_bits_to_syncword = min(preamble_detection_length - min_pre_no_preamsch_len,cfe_len_shortage)

                else:
                    trecs_pre_bits_to_syncword = 0

            else:
                trecs_pre_bits_to_syncword = 0

        else:
            trecs_pre_bits_to_syncword = 0

        if trecs_pre_bits_to_syncword > 0:

            cfe_len_expected = syncword_len + trecs_pre_bits_to_syncword
            correction_factor = cfe_len_expected % 4
            trecs_pre_bits_to_syncword = trecs_pre_bits_to_syncword - correction_factor

        model.vars.trecs_pre_bits_to_syncword.value = trecs_pre_bits_to_syncword
        model.vars.trecs_effective_preamble_len.value = preamble_detection_length - trecs_pre_bits_to_syncword
        model.vars.trecs_effective_syncword_len.value = syncword_len + trecs_pre_bits_to_syncword

        model.vars.trecs_pre_bits_to_syncword.in_public_log = is_trecs
        model.vars.trecs_effective_preamble_len.in_public_log = is_trecs
        model.vars.trecs_effective_syncword_len.in_public_log = is_trecs

    def calc_expsynclen_reg(self, model):
        """ set register EXPSYNCLEN that determines RAM pointer rollback from the last write
        after TRECS recovers timing from sync word. This needs to include 2 preamble bits. It may need
        additional bits depending on delay between last write and timing acquisition
        This worked for FSK2 for wmbus.
        """
        trecs_enabled = model.vars.trecs_enabled.value
        syncacqwinsize = model.vars.syncacqwin_actual.value
        osr_actual = model.vars.oversampling_rate_actual.value
        osr = int(round(osr_actual))

        if trecs_enabled:
            expsynclen = osr * (syncacqwinsize + 4) + 2
            if expsynclen > 511:
                expsynclen = 511
        else:
            expsynclen = 8

        self._reg_write(model.vars.MODEM_VTCORRCFG1_EXPSYNCLEN, int(expsynclen))

    def calc_syncacqwin_reg(self, model):

        trecs_enabled = model.vars.trecs_enabled.value
        trecs_effective_syncword_len = model.vars.trecs_effective_syncword_len.value

        if trecs_enabled:
            syncacqwin = trecs_effective_syncword_len // self.acqwin_unit - 1
        else:
            syncacqwin = 7

        self._reg_sat_write(model.vars.MODEM_REALTIMCFE_SYNCACQWIN, syncacqwin)

    def calc_preamsch_len(self, model):
        preamsch_len = 0 # : disable preamsch mode in Lynx, until mode is verified working on silicon
        model.vars.preamsch_len.value = preamsch_len

    def calc_preamsch_reg(self, model):

        preamsch_len = model.vars.preamsch_len.value

        if preamsch_len == 0:
            reg = 0
        else:
            reg = 1

        self._reg_write(model.vars.MODEM_TRECPMDET_PREAMSCH, reg)

    def calc_pmacqwin_reg(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        preamsch_len = model.vars.preamsch_len.value

        if preamsch_len >= 4 and vtdemoden:
            pmacqwin = preamsch_len // self.acqwin_unit - 1
        else:
            pmacqwin = 7

        self._reg_write(model.vars.MODEM_TRECPMDET_PMACQUINGWIN, pmacqwin)

    def calc_pmacquingwin_actual(self, model):
        pmacquingwin = model.vars.MODEM_TRECPMDET_PMACQUINGWIN.value
        model.vars.pmacquingwin_actual.value = self.acqwin_unit * (pmacquingwin + 1)

    def calc_pmcostthd_reg(self, model):
        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        preamsch_len = model.vars.preamsch_len.value
        pmacquingwin = model.vars.MODEM_TRECPMDET_PMACQUINGWIN.value
        freq_dev_min = model.vars.freq_dev_min.value
        freq_dev_max = model.vars.freq_dev_max.value
        deviation = model.vars.deviation.value
        antdivmode = model.vars.antdivmode.value
        modulation_index = model.vars.modulation_index.value
        phscale_derate_factor = model.vars.phscale_derate_factor.value

        large_freq_dev = (freq_dev_min < 0.8*deviation) or (freq_dev_max > 1.2*deviation)

        ksi3 = model.vars.MODEM_VITERBIDEMOD_VITERBIKSI3.value
        trecs_optimize_cost_thd = model.vars.trecs_optimize_cost_thd.value

        if preamsch_len > 0 and vtdemoden:
            if large_freq_dev and pmacquingwin >= (28 // self.acqwin_unit - 1):
                reg = 750 - (32 // self.acqwin_unit - 1 - pmacquingwin) * (15 * self.acqwin_unit)
            else:
                reg = 500 - (32 // self.acqwin_unit - 1 - pmacquingwin) * (15 * self.acqwin_unit)

            reg = int(reg/phscale_derate_factor)

            if ksi3 and trecs_optimize_cost_thd:  # Never set PMMINCOSTTHD = 0
                pmmincostthd_ksi3wb_optimization = self.acqwin_unit * (pmacquingwin + 1) * ceil(0.4 * ksi3)
                reg = min(reg, pmmincostthd_ksi3wb_optimization)
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_TRECPMDET_PMMINCOSTTHD, reg)

    def calc_trecs_optimize_cost_thd(self, model):
        model.vars.trecs_optimize_cost_thd.value = True

    def calc_mincostthd_reg(self, model):
        syncacqwin_reg = model.vars.MODEM_REALTIMCFE_SYNCACQWIN.value
        phscale_derate_factor = model.vars.phscale_derate_factor.value
        trecs_enabled = model.vars.trecs_enabled.value

        if trecs_enabled:
            reg = self.MIN_COST_THD_FULL - (32 // self.acqwin_unit - 1 - syncacqwin_reg) * (15 * self.acqwin_unit)
            reg = int(reg / phscale_derate_factor)  #Derate for PHSCALE if not set to nominal 64
        else:
            reg = 500

        self._reg_write(model.vars.MODEM_REALTIMCFE_MINCOSTTHD, reg)

    def calc_rtcfeen_reg(self, model):

        trecs_enabled = model.vars.trecs_enabled.value

        reg = 1 if trecs_enabled else 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_RTCFEEN, reg)

    def calc_vtbletimingsel_reg(self, model):

        self._reg_write(model.vars.MODEM_VTBLETIMING_VTBLETIMINGSEL, 0)

    def calc_viterbi_misc_reg(self, model):

        trecs_enabled = model.vars.trecs_enabled.value

        self._reg_write(model.vars.MODEM_REALTIMCFE_SINEWEN, 0)
        self._reg_write(model.vars.MODEM_TRECPMDET_COSTHYST, 0)
        self._reg_write(model.vars.MODEM_VITERBIDEMOD_CORRCYCLE, 0)
        self._reg_write(model.vars.MODEM_VTBLETIMING_FLENOFF, 0)
        self._reg_write(model.vars.MODEM_VTTRACK_FREQBIAS, 0)
        self._reg_write(model.vars.MODEM_VTTRACK_TIMGEAR, 0)


        if trecs_enabled:
            self._reg_write(model.vars.MODEM_VITERBIDEMOD_CORRSTPSIZE, 4)
            self._reg_write(model.vars.MODEM_VTBLETIMING_TIMINGDELAY, 60)
            self._reg_write(model.vars.MODEM_VTTRACK_HIPWRTHD, 1)
            self._reg_write(model.vars.MODEM_VTTRACK_TIMTRACKTHD, 2)
            self._reg_write_default(model.vars.MODEM_VTTRACK_TIMEACQUTHD)
        else:
            self._reg_write(model.vars.MODEM_VITERBIDEMOD_CORRSTPSIZE, 0)
            self._reg_write(model.vars.MODEM_VTBLETIMING_TIMINGDELAY, 0)
            self._reg_write(model.vars.MODEM_VTTRACK_HIPWRTHD, 1)
            self._reg_write(model.vars.MODEM_VTTRACK_TIMTRACKTHD, 2)
            self._reg_write(model.vars.MODEM_VTTRACK_TIMEACQUTHD, 238)

    def calc_freqtrackmode_reg(self, model):
        trecs_enabled = model.vars.trecs_enabled.value
        rtschmode = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value == 1
        pmacquingwin = model.vars.MODEM_TRECPMDET_PMACQUINGWIN.value
        afc_oneshot_enabled = (model.vars.MODEM_AFC_AFCONESHOT.value == 1)

        if trecs_enabled:
            if rtschmode and (pmacquingwin < 7) and afc_oneshot_enabled:
                freqtrackmode = 3
            else:
                freqtrackmode = 1
        else:
            freqtrackmode = 0

        self._reg_write(model.vars.MODEM_VTTRACK_FREQTRACKMODE, freqtrackmode)

    def calc_realtimcfe_vtafcframe_reg(self, model):

        trecs_enabled = model.vars.trecs_enabled.value

        if trecs_enabled:
            vtafcframe = 1
        else:
            vtafcframe = 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_VTAFCFRAME, vtafcframe)

    def calc_realtimcfe_trackingwin(self, model):

        baudrate_tol_ppm = model.vars.baudrate_tol_ppm.value
        trecs_enabled = model.vars.trecs_enabled.value
        freq_offset_hz = model.vars.freq_offset_hz.value
        modulation_index = model.vars.modulation_index.value
        baudrate = model.vars.baudrate.value

        if trecs_enabled:
            if baudrate_tol_ppm >= 5000:
                trackingwin = 5
            elif (freq_offset_hz / baudrate) > 0.57 and modulation_index <= 0.5:
                trackingwin = 2
            else:
                trackingwin = 7
        else:
            trackingwin = 7

        self._reg_write(model.vars.MODEM_REALTIMCFE_TRACKINGWIN, trackingwin)

    def calc_rtschwin_reg(self, model):

        trecs_enabled = model.vars.trecs_enabled.value
        preamsch_len = model.vars.preamsch_len.value
        preamble_pattern_len = model.vars.preamble_pattern_len.value

        if trecs_enabled:
            if (preamsch_len <= 24) and (preamble_pattern_len == 2):
                reg = 4
            else:
                reg = 5
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_RTSCHWIN, reg)

    def calc_pmcostvalthd_reg(self, model):

        preamsch_len = model.vars.preamsch_len.value

        modulation_index = model.vars.modulation_index.value

        if preamsch_len <= 16:
            reg = 2
        elif preamsch_len <= 24:
            reg = 3
        else:
            reg = 4

        self._reg_write(model.vars.MODEM_TRECPMDET_PMCOSTVALTHD, reg)

    def calc_pmtimeoutsel_reg(self, model):

        trecs_enabled = model.vars.trecs_enabled.value
        preamble_length = model.vars.preamble_length.value #This is the TX preamble length
        sync_len = model.vars.MODEM_CTRL1_SYNCBITS.value + 1

        total_len = int(preamble_length + sync_len * 1.5)

        if not trecs_enabled:
            reg = 0
        elif total_len <= 16 or total_len == 0:
            reg = 0
        elif total_len <= 24:
            reg = 1
        elif total_len <= 32:
            reg = 2
        else:
            reg = 3

        if total_len > 65535:
            total_len = 65535

        self._reg_write(model.vars.MODEM_TRECPMDET_PMTIMEOUTSEL, reg)

    def calc_vtpmdetsel_reg(self, model):
        preamsch_len = model.vars.preamsch_len.value

        if preamsch_len > 0:
            reg = (preamsch_len - 8) // 4
            if reg > 3:
                reg = 3
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_TRECPMDET_VTPMDETSEL, reg)


    def calc_pmexpectpatt_reg(self, model):

        pre_str = model.vars.preamble_string.value
        mapfsk = model.vars.MODEM_CTRL0_MAPFSK.value
        demod_sel = model.vars.demod_select.value
        trecs_effective_preamble_len = model.vars.trecs_effective_preamble_len.value

        if demod_sel == model.vars.demod_select.var_enum.TRECS_VITERBI or demod_sel == model.vars.demod_select.var_enum.TRECS_SLICER:

            effective_pre_str = pre_str[:trecs_effective_preamble_len] #This is the preamble once some bits are shifted to the syncword
            zero_filler_str = '0'*32 #Add 32 zeroes to the end to make sure we have a long enough string
            combined_str = effective_pre_str + zero_filler_str

            reg = int(combined_str[0:32],2)
            if mapfsk:
                reg ^= 0xFFFFFFFF
        else:
            reg = 1431655765

        self._reg_write(model.vars.MODEM_TRECPMPATT_PMEXPECTPATT, reg)

    def calc_synthafc_reg(self, model):
        afc1shot_en = model.vars.MODEM_AFC_AFCONESHOT.value
        ber_force_freq_comp_off = model.vars.ber_force_freq_comp_off.value

        if afc1shot_en or ber_force_freq_comp_off:
            reg = 1
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_VITERBIDEMOD_SYNTHAFC, reg)

    def calc_vtfreqlim_reg(self, model):
        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        freq_offset_hz = model.vars.freq_offset_hz.value
        rf_freq_hz = model.vars.base_frequency_hz.value * 1.0
        baudrate = model.vars.baudrate.value

        freq_offset_hz_min = (2) * rf_freq_hz / 1e6
        freq_offset_hz = max(freq_offset_hz_min, freq_offset_hz)

        val = (20+(1.2*2 * 256 * freq_offset_hz / baudrate) )if vtdemoden else 192

        reg = int(511 if val > 511 else val)

        self._reg_write(model.vars.MODEM_VTCORRCFG1_VTFRQLIM, reg)

    def calc_realtimcfe_rtschmode_reg(self, model):

        dualsync = model.vars.syncword_dualsync.value
        demod_select = model.vars.demod_select.value
        ber_force_fdm0 = model.vars.ber_force_fdm0.value

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or \
                demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:
            if dualsync and not ber_force_fdm0:
                rtschmode = 1
            else:
                rtschmode = 0 # 0 means detect timing again using syncword
        else:
            rtschmode = 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_RTSCHMODE, rtschmode)
