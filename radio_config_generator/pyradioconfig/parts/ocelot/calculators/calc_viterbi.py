from pyradioconfig.parts.lynx.calculators.calc_viterbi import CALC_Viterbi_lynx
from math import *
from py_2_and_3_compatibility import *
from pycalcmodel.core.variable import ModelVariableFormat

class CALC_Viterbi_ocelot(CALC_Viterbi_lynx):

    acqwin_unit = 4

    def buildVariables(self, model):

        super().buildVariables(model)
        self._addModelVariable(model, 'trecs_weak_syncword_optimization', bool, ModelVariableFormat.BINARY, desc='Optimize threshold for packet reception using weak syncword')

    MIN_COST_THD_FULL = 600 # threshold for min cost function when using full 32 bit search (sync or preamble)

    def calc_shift_trecs_pre_bits_to_syncword(self, model):

        demod_select = model.vars.demod_select.value
        preamble_detection_length = model.vars.preamble_detection_length.value
        delay_samp = model.vars.grpdelay_to_demod.value
        osr = model.vars.oversampling_rate_actual.value
        rtschmode_actual = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value
        ber_force_sync = model.vars.ber_force_sync.value
        antdivmode = model.vars.antdivmode.value
        fast_detect_enable = (model.vars.fast_detect_enable.value == model.vars.fast_detect_enable.var_enum.ENABLED)
        is_trecs = demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or \
                   demod_select == model.vars.demod_select.var_enum.TRECS_SLICER

        if ber_force_sync:
            syncword_len = 32
        else:
            syncword_len = model.vars.syncword_length.value

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or \
            demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:

            if antdivmode == model.vars.antdivmode.var_enum.PHDEMODANTDIV or antdivmode == model.vars.antdivmode.var_enum.ANTENNA1:
                target_cfe_len = 16 #20 # If antenna diversity enabled, reduce window so smaller preamble can be used
            else:
                target_cfe_len = 32 #The goal is to end up with a 32 bit window, length must end up as a mult of 4
            delay_symbols = int(ceil(delay_samp / osr)) # Calculate the group delay in symbols
            min_pre_len = 26 + delay_symbols #Need to leave at least 26 preamble bits after delay so that we can enable preamsch and AFC
            min_pre_no_preamsch_len = 8 #Need to leave 8 bits of preamble if preamsch is disabled

            if (syncword_len < target_cfe_len) and (rtschmode_actual == 0) and not fast_detect_enable and not ber_force_sync:
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

    def calc_demod_expect_patt_value(self, model):

        demod_select = model.vars.demod_select.value
        syncword0 = model.vars.MODEM_SYNC0_SYNC0.value
        mapfsk = model.vars.MODEM_CTRL0_MAPFSK.value
        trecs_pre_bits_to_syncword = model.vars.trecs_pre_bits_to_syncword.value
        preamble_string = model.vars.preamble_string.value
        syncword_len = model.vars.syncword_length.value
        ber_force_sync = model.vars.ber_force_sync.value

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or \
            demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:

            if ber_force_sync:
                patt = 0x052bcbb8

            else:
                syncword_str_part = '{:032b}'.format(syncword0)[-syncword_len:] #Read the rightmost characters

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
            patt = long(0x123556B7)

        model.vars.viterbi_demod_expect_patt.value = patt

    def calc_demod_expect_patt_reg(self, model):

        viterbi_demod_expect_patt = model.vars.viterbi_demod_expect_patt.value

        self._reg_write(model.vars.MODEM_VTCORRCFG0_EXPECTPATT, viterbi_demod_expect_patt)

    def calc_demod_expect_patt_head_tail_value(self, model):

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

    def calc_demod_expect_patt_head_tail_reg(self, model):

        viterbi_demod_expect_patt_head_tail = model.vars.viterbi_demod_expect_patt_head_tail.value
        self._reg_write(model.vars.MODEM_VTCORRCFG1_EXPECTHT, viterbi_demod_expect_patt_head_tail)

    def calc_expsynclen_reg(self, model):
        """ set register EXPSYNCLEN that determines RAM pointer rollback from the last write
        after TRECS recovers timing from sync word. This needs to include 2 preamble bits. It may need
        additional bits depending on delay between last write and timing acquisition
        This worked for FSK2 for wmbus.
        """
        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        syncacqwinsize = model.vars.syncacqwin_actual.value
        osr = model.vars.MODEM_TRECSCFG_TRECSOSR.value

        if vtdemoden:
            expsynclen = osr * (syncacqwinsize + 4) + 2
            if expsynclen > 511:
                expsynclen = 511
        else:
            expsynclen = 0

        self._reg_write(model.vars.MODEM_VTCORRCFG1_EXPSYNCLEN, int(expsynclen))

    def calc_syncacqwin_reg(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        trecs_effective_syncword_len = model.vars.trecs_effective_syncword_len.value

        if vtdemoden:
            syncacqwin = trecs_effective_syncword_len // self.acqwin_unit - 1
        else:
            syncacqwin = 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_SYNCACQWIN, syncacqwin)

    def calc_preamsch_len(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        ber_force_fdm0 = model.vars.ber_force_fdm0.value
        symbols_in_timing_window = model.vars.symbols_in_timing_window.value

        if vtdemoden and not ber_force_fdm0:
            preamsch_len = symbols_in_timing_window
        else:
            preamsch_len = 0

        model.vars.preamsch_len.value = preamsch_len

    def calc_pmacqwin_reg(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        preamsch_len = model.vars.preamsch_len.value
        antdivmode = int(model.vars.antdivmode.value)

        if preamsch_len >= 4 and vtdemoden:
            if antdivmode == 5 or antdivmode == 1:
                pmacqwin = 12 // self.acqwin_unit - 1 #12 bits
            else:
                pmacqwin = preamsch_len // self.acqwin_unit - 1
        else:
            pmacqwin = 0

        self._reg_write(model.vars.MODEM_TRECPMDET_PMACQUINGWIN, pmacqwin)

    def calc_pmacquingwin_actual(self, model):

        pmacquingwin = model.vars.MODEM_TRECPMDET_PMACQUINGWIN.value

        model.vars.pmacquingwin_actual.value = self.acqwin_unit * (pmacquingwin + 1)

    def calc_pmdetthd_reg(self, model):

        self._reg_write(model.vars.MODEM_PHDMODCTRL_PMDETTHD, 8)

    def calc_pmoffset_reg(self, model):

        osr = model.vars.MODEM_TRECSCFG_TRECSOSR.value
        pmwinsize = model.vars.pmacquingwin_actual.value
        baudrate = model.vars.baudrate.value
        rtschmode = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value == 1
        modulation_index = model.vars.modulation_index.value
        freq_offset_hz = model.vars.freq_offset_hz.value
        pmacquingwin = model.vars.MODEM_TRECPMDET_PMACQUINGWIN.value
        afc_oneshot_enabled = (model.vars.MODEM_AFC_AFCONESHOT.value == 1)

        relative_freq_offs = freq_offset_hz / baudrate

        if rtschmode:
            if (pmacquingwin < 7) and afc_oneshot_enabled:
                pmoffset = 2
            elif (modulation_index <= 0.5 and relative_freq_offs  > 0.57) or baudrate > 90000:
                pmoffset = osr * 2 + 2
            else:
                pmoffset = osr * pmwinsize + 2
        else:
            pmoffset =  osr * pmwinsize + 2

        self._reg_write(model.vars.MODEM_TRECSCFG_PMOFFSET,  pmoffset)

    def calc_pmcostthd_reg(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        preamsch_len = model.vars.preamsch_len.value
        pmacquingwin = model.vars.MODEM_TRECPMDET_PMACQUINGWIN.value
        freq_dev_min = model.vars.freq_dev_min.value
        freq_dev_max = model.vars.freq_dev_max.value
        deviation = model.vars.deviation.value
        antdivmode = model.vars.antdivmode.value
        fast_detect_enable = (model.vars.fast_detect_enable.value == model.vars.fast_detect_enable.var_enum.ENABLED)
        modulation_index = model.vars.modulation_index.value
        phscale_derate_factor = model.vars.phscale_derate_factor.value

        large_freq_dev = (freq_dev_min < 0.8*deviation) or (freq_dev_max > 1.2*deviation)

        ksi3wb = model.vars.MODEM_VTCORRCFG1_VITERBIKSI3WB.value
        trecs_optimize_cost_thd = model.vars.trecs_optimize_cost_thd.value

        if preamsch_len > 0 and vtdemoden:
            if antdivmode == model.vars.antdivmode.var_enum.PHDEMODANTDIV or antdivmode == model.vars.antdivmode.var_enum.ANTENNA1:
                reg = 200 if large_freq_dev else 150
            elif large_freq_dev and pmacquingwin >= (28 // self.acqwin_unit - 1):
                reg = 750 - (32 // self.acqwin_unit - 1 - pmacquingwin) * (15 * self.acqwin_unit)
            else:
                if fast_detect_enable:
                    if modulation_index <= 0.7:
                        reg = 100
                    else:
                        reg = 80
                else:
                    reg = 500 - (32 // self.acqwin_unit - 1 - pmacquingwin) * (15 * self.acqwin_unit)

            reg = int(reg/phscale_derate_factor)

            if ksi3wb and trecs_optimize_cost_thd:  # Never set PMMINCOSTTHD = 0
                pmmincostthd_ksi3wb_optimization = self.acqwin_unit * (pmacquingwin + 1) * ceil(0.4 * ksi3wb)
                reg = min(reg, pmmincostthd_ksi3wb_optimization)
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_TRECPMDET_PMMINCOSTTHD, reg)

    def calc_trecs_optimize_cost_thd(self, model):
        model.vars.trecs_optimize_cost_thd.value = True

    def calc_trecs_weak_syncword_optimization(self, model):
        """A syncword is considered as weak if one of the following case is verified:
            * Too many 0s in a row (eg. 0101_0101_0000_0000)
            * Too many 1s in a row (eg. 1010_1010_1111_1111)
            * Too similar to a preamble (eg. 1010_0010_1010_1010)
        In such a case, there is a risk the TRECs uses the preamble instead of the syncword (or EXPECTPATT) to adjust
        frequency or timing, resulting in CRC error. It is then strongly advised to change the syncword.
        Another solution is to set trecs_weak_syncword_optimization to True, to be more strict on MINCOSTTHD
        calculation."""
        model.vars.trecs_weak_syncword_optimization.value = False

    def calc_mincostthd_reg(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        syncacqwin_reg = model.vars.MODEM_REALTIMCFE_SYNCACQWIN.value
        phscale_derate_factor = model.vars.phscale_derate_factor.value
        trecs_weak_syncword_optimization = model.vars.trecs_weak_syncword_optimization.value

        if vtdemoden:
            if trecs_weak_syncword_optimization:
                thd_limit = int(self.return_trecs_min_cost_limit(model))
            else:
                thd_limit = 1023
            reg = self.MIN_COST_THD_FULL - (32 // self.acqwin_unit - 1 - syncacqwin_reg) * (15 * self.acqwin_unit)
            reg = int(reg / phscale_derate_factor)  # Derate for PHSCALE if not set to nominal 64
            reg = min(thd_limit, reg)
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_MINCOSTTHD, reg)

    def return_trecs_min_cost_limit(self, model):
        """After the preamble detection and before the syncword detection, if the expected pattern
        (viterbi_demod_expect_patt) is similar to a preamble, there is a chance the CFE uses the preamble
        instead of the expected pattern to adjust timing and frequency. If the Tx frequency is offset, there
        is a risk the adjustment is impaired and the packet wrong, resulting in a CRC error. This method
        returns the minimum cost that can be measured when the CFE slide in a preamble pattern,
        shifted in frequency and timing. The returned value is used to calculate MINCOSTTHD."""

        ksi1 = model.vars.ksi1.value
        ksi2 = model.vars.ksi2.value
        ksi3 = model.vars.ksi3.value

        cost_thd_min_offset = -20

        if not ksi1:  # Viterbi is not used
            return 0

        expectpatt = model.vars.viterbi_demod_expect_patt.value
        syncacqwin_reg = model.vars.MODEM_REALTIMCFE_SYNCACQWIN.value
        head_and_tail = model.vars.viterbi_demod_expect_patt_head_tail.value

        pre_tail = "{:04b}".format(head_and_tail)  # The bit before and after the expected pattern, necessary to calculate first and last ksis values
        expectpatt_len = self.acqwin_unit * (1 + syncacqwin_reg)
        expected_pattern = pre_tail[1] + "{:032b}".format(expectpatt)[:expectpatt_len] + pre_tail[-1]

        ksis = [-ksi1, -ksi2, ksi3, ksi2, -ksi2, -ksi3, ksi2, ksi1]
        expected_signal = [ksis[int(expected_pattern[i-1:i+2], 2)] for i in range(1, len(expected_pattern)-1)]

        cost_frequency = []
        cost_timing = []

        for i in range(-2 * ksi1, 2 * ksi1):
            unwanted_signal_frequency = [-ksi3 + i, ksi3 + i] * int(expectpatt_len / 2)
            cost_frequency.append(
                sum([min(abs(unwanted_signal_frequency[j] - expected_signal[j]), 32)
                     for j in range(len(unwanted_signal_frequency))]))

        for i in range(-ksi3, ksi3):
            unwanted_signal_timing = [i, -i] * int(expectpatt_len / 2)
            cost_timing.append(
                sum([min(abs(unwanted_signal_timing[j] - expected_signal[j]), 32)
                     for j in range(len(unwanted_signal_timing))]))

        return max(0, min(cost_frequency + cost_timing) + cost_thd_min_offset)

    def calc_rtcfeen_reg(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value

        reg = 1 if vtdemoden else 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_RTCFEEN, reg)

    def calc_harddecision_reg(self, model):

        demod_select = model.vars.demod_select.value
        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        mi = model.vars.modulation_index.value

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI:
            reg = 0
        elif demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:
            reg = 1
        elif vtdemoden and mi >= 1.0:
            reg = 1
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_VITERBIDEMOD_HARDDECISION, reg)

    def calc_vtdemoden_reg(self, model):
        demod_sel = model.vars.demod_select.value

        if demod_sel == model.vars.demod_select.var_enum.TRECS_VITERBI or demod_sel == model.vars.demod_select.var_enum.TRECS_SLICER:
            reg = 1
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_VITERBIDEMOD_VTDEMODEN, reg)

    def calc_vtbletimingsel_reg(self, model):

        self._reg_write(model.vars.MODEM_VTBLETIMING_VTBLETIMINGSEL, 0)

    def calc_bcr_detector_reg(self, model):
        rtschmode = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value
        antdivmode = model.vars.antdivmode.value

        bcrdetector_en = 0

        if antdivmode == model.vars.antdivmode.var_enum.PHDEMODANTDIV \
            or antdivmode == model.vars.antdivmode.var_enum.ANTENNA1:
            if rtschmode == 1:
                bcrdetector_en = 1

        self._reg_write(model.vars.MODEM_PHDMODCTRL_BCRDETECTOR, bcrdetector_en)

    def calc_viterbi_misc_reg(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        ber_force_freq_comp_off = model.vars.ber_force_freq_comp_off.value

        self._reg_write(model.vars.MODEM_REALTIMCFE_SINEWEN, 0)
        self._reg_write(model.vars.MODEM_TRECPMDET_COSTHYST, 0)
        self._reg_write(model.vars.MODEM_VITERBIDEMOD_CORRCYCLE, 0)
        self._reg_write(model.vars.MODEM_VTBLETIMING_FLENOFF, 0)
        self._reg_write(model.vars.MODEM_VTTRACK_FREQBIAS, 0)
        self._reg_write(model.vars.MODEM_VTTRACK_TIMGEAR, 0)

        self._reg_write(model.vars.MODEM_PHDMODCTRL_BCRTRECSCONC, 0)
        self._reg_write(model.vars.MODEM_PHDMODCTRL_BCRLEGACYCONC, 0)


        if vtdemoden:
            self._reg_write(model.vars.MODEM_VITERBIDEMOD_CORRSTPSIZE, 4)
            self._reg_write(model.vars.MODEM_VTBLETIMING_DISDEMODOF, 1)
            self._reg_write(model.vars.MODEM_VTBLETIMING_TIMINGDELAY, 60)
            self._reg_write(model.vars.MODEM_VTTRACK_HIPWRTHD, 1)
            self._reg_write(model.vars.MODEM_VTTRACK_TIMTRACKTHD, 2)
            self._reg_write_default(model.vars.MODEM_VTTRACK_TIMEACQUTHD)
        else:
            self._reg_write(model.vars.MODEM_VITERBIDEMOD_CORRSTPSIZE, 0)
            self._reg_write(model.vars.MODEM_VTBLETIMING_DISDEMODOF, 0)
            self._reg_write(model.vars.MODEM_VTBLETIMING_TIMINGDELAY, 0)
            self._reg_write(model.vars.MODEM_VTTRACK_HIPWRTHD, 0)
            self._reg_write(model.vars.MODEM_VTTRACK_TIMTRACKTHD, 0)
            self._reg_do_not_care(model.vars.MODEM_VTTRACK_TIMEACQUTHD)

    def calc_freqtrackmode_reg(self, model):
        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        rtschmode = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value == 1
        pmacquingwin = model.vars.MODEM_TRECPMDET_PMACQUINGWIN.value
        afc_oneshot_enabled = (model.vars.MODEM_AFC_AFCONESHOT.value == 1)

        if vtdemoden:
            if rtschmode and (pmacquingwin < 7) and afc_oneshot_enabled:
                freqtrackmode = 3
            else:
                freqtrackmode = 1
        else:
            freqtrackmode = 0

        self._reg_write(model.vars.MODEM_VTTRACK_FREQTRACKMODE, freqtrackmode)

    def calc_realtimcfe_vtafcframe_reg(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value

        if vtdemoden:
            vtafcframe = 1
        else:
            vtafcframe = 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_VTAFCFRAME, vtafcframe)

    def calc_realtimcfe_trackingwin(self, model):

        baudrate_tol_ppm = model.vars.baudrate_tol_ppm.value
        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        freq_offset_hz = model.vars.freq_offset_hz.value
        modulation_index = model.vars.modulation_index.value
        baudrate = model.vars.baudrate.value

        if vtdemoden:
            if baudrate_tol_ppm >= 5000:
                trackingwin = 5
            elif (freq_offset_hz / baudrate) > 0.57 and modulation_index <= 0.5:
                trackingwin = 2
            else:
                trackingwin = 7
        else:
            trackingwin = 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_TRACKINGWIN, trackingwin)

    def calc_rtschwin_reg(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        preamsch_len = model.vars.preamsch_len.value
        preamble_pattern_len = model.vars.preamble_pattern_len.value

        if vtdemoden:
            if (preamsch_len <= 24) and (preamble_pattern_len == 2):
                reg = 4
            else:
                reg = 5
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_RTSCHWIN, reg)

    def calc_pmcostvalthd_reg(self, model):

        preamsch_len = model.vars.preamsch_len.value

        fast_detect_enable = (model.vars.fast_detect_enable.value == model.vars.fast_detect_enable.var_enum.ENABLED)
        modulation_index = model.vars.modulation_index.value

        if fast_detect_enable:
            reg = 2
        elif preamsch_len <= 16:
            reg = 2
        elif preamsch_len <= 24:
            reg = 3
        else:
            reg = 4

        self._reg_write(model.vars.MODEM_TRECPMDET_PMCOSTVALTHD, reg)


    def calc_pmtimeoutsel_reg(self, model):

        preamble_length = model.vars.preamble_length.value #This is the TX preamble length
        sync_len = model.vars.syncbits_actual.value

        total_len = int(preamble_length + sync_len * 1.5)

        if total_len <= 16 or total_len == 0:
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
        self._reg_write(model.vars.MODEM_FRMSCHTIME_FRMSCHTIME, total_len)

    def calc_vtpmdetsel_reg(self, model):
        pass

    def calc_pmexpectpatt_reg(self, model):

        pre_str = model.vars.preamble_string.value
        mapfsk = model.vars.MODEM_CTRL0_MAPFSK.value
        demod_sel = model.vars.demod_select.value
        trecs_effective_preamble_len = model.vars.trecs_effective_preamble_len.value

        if demod_sel == model.vars.demod_select.var_enum.TRECS_VITERBI or demod_sel == model.vars.demod_select.var_enum.TRECS_SLICER or demod_sel == model.vars.demod_select.var_enum.BCR:

            effective_pre_str = pre_str[:trecs_effective_preamble_len] #This is the preamble once some bits are shifted to the syncword
            zero_filler_str = '0'*32 #Add 32 zeroes to the end to make sure we have a long enough string
            combined_str = effective_pre_str + zero_filler_str

            reg = int(combined_str[0:32],2)
            if mapfsk:
                reg ^= 0xFFFFFFFF
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_TRECPMPATT_PMEXPECTPATT, reg)

    def calc_vtfreqlim_reg(self, model):
        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        freq_offset_hz = model.vars.freq_offset_hz.value
        rf_freq_hz = model.vars.base_frequency_hz.value * 1.0
        baudrate = model.vars.baudrate.value

        freq_offset_hz_min = (2) * rf_freq_hz / 1e6
        freq_offset_hz = max(freq_offset_hz_min, freq_offset_hz)

        val = (20+(1.2*2 * 256 * freq_offset_hz / baudrate) )if vtdemoden else 0

        reg = int(511 if val > 511 else val)

        self._reg_write(model.vars.MODEM_VTCORRCFG1_VTFRQLIM, reg)

    def calc_swcoeffen_reg(self, model):

        afc1shot_en = model.vars.MODEM_AFC_AFCONESHOT.value

        reg = 1 if afc1shot_en else 0

        self._reg_write(model.vars.MODEM_CHFCTRL_SWCOEFFEN, reg)
        self._reg_write(model.vars.MODEM_VTCORRCFG1_KSI3SWENABLE, reg)

    def calc_pmdeten_reg(self, model):

        pmdeten = 0

        self._reg_write(model.vars.MODEM_PHDMODCTRL_PMDETEN, pmdeten)

    def calc_pmtimlosen_reg(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        pmdeten = model.vars.MODEM_PHDMODCTRL_PMDETEN.value
        fast_detect_enable = (model.vars.fast_detect_enable.value == model.vars.fast_detect_enable.var_enum.ENABLED)

        if vtdemoden and fast_detect_enable:
            pmtimlosen = 1
        else:
            pmtimlosen = 0

        self._reg_write(model.vars.MODEM_PHDMODCTRL_PMTIMLOSEN, pmtimlosen)

    def calc_pmtimlosthd_reg(self, model):

        pmtimlosen = model.vars.MODEM_PHDMODCTRL_PMTIMLOSEN.value
        pmmincostthd = model.vars.MODEM_TRECPMDET_PMMINCOSTTHD.value

        if pmtimlosen:
            pmtimlosthd = int(1.5*pmmincostthd)
        else:
            pmtimlosthd = 0

        self._reg_write(model.vars.MODEM_PHDMODCTRL_PMTIMLOSTHD, pmtimlosthd, check_saturation=True)

    def calc_pmendschen(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        rtschmode = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value
        fast_detect_enable = (model.vars.fast_detect_enable.value == model.vars.fast_detect_enable.var_enum.ENABLED)

        if vtdemoden:
            if rtschmode == 1 and fast_detect_enable:
                pmendschen = 1
            else:
                pmendschen = 0
        else:
            pmendschen = 0

        self._reg_write(model.vars.MODEM_FRMSCHTIME_PMENDSCHEN, pmendschen)

    def calc_remodoutsel_reg(self, model):

        self._reg_write(model.vars.MODEM_PHDMODCTRL_REMODOUTSEL, 1)

    def calc_realtimcfe_rtschmode_reg(self, model):

        dualsync = model.vars.syncword_dualsync.value
        demod_select = model.vars.demod_select.value
        ber_force_fdm0 = model.vars.ber_force_fdm0.value
        pmdeten = model.vars.MODEM_PHDMODCTRL_PMDETEN.value
        fast_detect_enable = (model.vars.fast_detect_enable.value == model.vars.fast_detect_enable.var_enum.ENABLED)

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or \
                demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:
            if dualsync and not ber_force_fdm0:
                rtschmode = 1
            elif fast_detect_enable or pmdeten:
                rtschmode = 1
            else:
                rtschmode = 0 # 0 means detect timing again using syncword
        else:
            rtschmode = 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_RTSCHMODE, rtschmode)

    def calc_realtimcfe_extenschbyp_reg(self, model):
        self._reg_write(model.vars.MODEM_REALTIMCFE_EXTENSCHBYP, 1) # Enable the extended search bypass as workaround for PGOCELOT-5342

    def calc_trecs_syncword_timeout_us(self, model):
        frmschtime = model.vars.MODEM_FRMSCHTIME_FRMSCHTIME.value
        baudrate = model.vars.baudrate.value
        demod_select = model.vars.demod_select.value
        is_trecs = demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or \
                   demod_select == model.vars.demod_select.var_enum.TRECS_SLICER

        model.vars.trecs_syncword_timeout_us.value = frmschtime / baudrate * 1e6

        model.vars.trecs_syncword_timeout_us.in_public_log = is_trecs

    def calc_trecs_demod_enable(self, model):
        pass