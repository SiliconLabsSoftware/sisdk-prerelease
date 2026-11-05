from pyradioconfig.parts.bobcat.calculators.calc_viterbi import Calc_Viterbi_Bobcat
from math import *


class Calc_Viterbi_Sol(Calc_Viterbi_Bobcat):

    def calc_trecs_optimize_cost_thd(self, model):
        model.vars.trecs_optimize_cost_thd.value = True

    def calc_swcoeffen_reg(self, model):

        afc1shot_en = model.vars.MODEM_AFC_AFCONESHOT.value
        fefilt_selected = model.vars.fefilt_selected.value

        reg = 1 if afc1shot_en else 0

        self._reg_write_by_name_concat(model, fefilt_selected, 'CHFCTRL_SWCOEFFEN', reg)

        self._reg_write(model.vars.MODEM_VTCORRCFG1_KSI3SWENABLE, reg)

    def calc_vtdemoden_reg(self, model):
        demod_sel = model.vars.demod_select.value
        modformat = model.vars.modulation_type.value

        if demod_sel == model.vars.demod_select.var_enum.TRECS_VITERBI or \
                demod_sel == model.vars.demod_select.var_enum.TRECS_SLICER or \
                (demod_sel == model.vars.demod_select.var_enum.SOFT_DEMOD and
                 modformat == model.vars.modulation_type.var_enum.FSK2):
            reg = 1
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_VITERBIDEMOD_VTDEMODEN, reg)

    def calc_freqtrackmode_reg(self, model):
        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        phscale_derate_factor = model.vars.phscale_derate_factor.value
        rtschmode = (model.vars.MODEM_REALTIMCFE_RTSCHMODE.value == 1)
        pmacquingwin = model.vars.MODEM_TRECPMDET_PMACQUINGWIN.value
        afc_oneshot_enabled = (model.vars.MODEM_AFC_AFCONESHOT.value == 1)

        if vtdemoden:
            if phscale_derate_factor <= 1:
                if rtschmode and (pmacquingwin < 7) and afc_oneshot_enabled:
                    freqtrackmode = 3
                else:
                    freqtrackmode = 1
            else:
                freqtrackmode = 3
        else:
            freqtrackmode = 0

        self._reg_write(model.vars.MODEM_VTTRACK_FREQTRACKMODE, freqtrackmode)

    def calc_realtimcfe_rtschmode_reg(self, model):

        dualsync = model.vars.syncword_dualsync.value
        demod_select = model.vars.demod_select.value
        mod_type = model.vars.modulation_type.value
        fast_detect_enable = (model.vars.fast_detect_enable.value == model.vars.fast_detect_enable.var_enum.ENABLED)
        pmdeten = model.vars.MODEM_PHDMODCTRL_PMDETEN.value

        if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or \
                demod_select == model.vars.demod_select.var_enum.TRECS_SLICER or \
                (demod_select == model.vars.demod_select.var_enum.SOFT_DEMOD and \
                 mod_type == model.vars.modulation_type.var_enum.FSK2):

            if dualsync:
                rtschmode = 1
            elif fast_detect_enable or pmdeten:
                rtschmode = 1
            else:
                rtschmode = 0
        else:
            rtschmode = 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_RTSCHMODE, rtschmode)

    def calc_pmoffset_reg(self, model):

        osr = model.vars.MODEM_TRECSCFG_TRECSOSR.value
        rtschmode = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value
        pmdetthd = model.vars.MODEM_PHDMODCTRL_PMDETTHD.value
        freq_offset_hz = model.vars.freq_offset_hz.value
        deviation = model.vars.deviation.value
        pmacquingwin = model.vars.MODEM_TRECPMDET_PMACQUINGWIN.value
        afc_oneshot_enabled = (model.vars.MODEM_AFC_AFCONESHOT.value == 1)

        if rtschmode == 1:
            if deviation !=0 and freq_offset_hz/deviation > 2:
                pmoffset = 2
            elif (pmacquingwin < 7) and afc_oneshot_enabled:
                pmoffset = 2
            else:
                pmoffset = osr * 2 + 2
        else:
            pmoffset = osr * pmdetthd + 2

        self._reg_write(model.vars.MODEM_TRECSCFG_PMOFFSET,  pmoffset)

    def calc_realtimcfe_trackingwin(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        freq_offset_hz = model.vars.freq_offset_hz.value
        deviation = model.vars.deviation.value
        baudrate_tol_ppm = model.vars.baudrate_tol_ppm.value

        if vtdemoden:
            if (deviation != 0 and freq_offset_hz/deviation > 2) or baudrate_tol_ppm >= 10000:
                trackingwin = 3 #Do not set below 3 as this causes floor in Apps + Design setups
            elif (deviation != 0 and freq_offset_hz/deviation > 1) or baudrate_tol_ppm >= 5000:
                trackingwin = 5
            else:
                trackingwin = 7
        else:
            trackingwin = 0
        self._reg_write(model.vars.MODEM_REALTIMCFE_TRACKINGWIN, trackingwin)

    def calc_pmendschen(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        rtschmode = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value
        fast_detect_enable = (model.vars.fast_detect_enable.value == model.vars.fast_detect_enable.var_enum.ENABLED)
        bcr_demod_en = model.vars.bcr_demod_en.value

        if vtdemoden:
            if rtschmode == 1 and fast_detect_enable:
                pmendschen = 1
            else:
                pmendschen = 0
        elif (bcr_demod_en == 1):
            pmendschen = 1
        else:
            pmendschen = 0

        self._reg_write(model.vars.MODEM_FRMSCHTIME_PMENDSCHEN, pmendschen)

    def calc_frc_spare_bugfix(self, model):

        demod_select = model.vars.demod_select.value
        var_length_shift = model.vars.var_length_shift.value #Bit position of dynamic length field
        var_length_numbits = model.vars.var_length_numbits.value #Number of bits in dynamic length field

        if (demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or
            demod_select == model.vars.demod_select.var_enum.TRECS_SLICER) and \
                ((var_length_numbits + var_length_shift) > 8): #Check for crossing of byte boundary
            frc_rxframe_end_ahead_disable = 1
        else:
            frc_rxframe_end_ahead_disable = 0

        self._reg_write(model.vars.FRC_SPARE_SPARE, frc_rxframe_end_ahead_disable) #No other useful bits in this field