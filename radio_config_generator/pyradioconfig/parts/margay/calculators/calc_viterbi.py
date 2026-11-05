from pyradioconfig.parts.ocelot.calculators.calc_viterbi import CALC_Viterbi_ocelot


class CALC_Viterbi_Margay(CALC_Viterbi_ocelot):

    def calc_freqtrackmode_reg(self, model):

        vtdemoden = model.vars.MODEM_VITERBIDEMOD_VTDEMODEN.value
        remoden = (model.vars.MODEM_PHDMODCTRL_REMODEN.value == 1)
        rtschmode = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value
        freq_offset_hz = model.vars.freq_offset_hz.value
        deviation = model.vars.deviation.value

        if deviation != 0:
            large_freq_offset = (freq_offset_hz / deviation) > 2
        else:
            large_freq_offset = False

        if vtdemoden:
            if (rtschmode == 1) and not remoden:
                freqtrackmode = 3
            else:
                freqtrackmode = 1
        else:
            freqtrackmode = 0

        self._reg_write(model.vars.MODEM_VTTRACK_FREQTRACKMODE, freqtrackmode)

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
                trackingwin = 3 #Do not set below 3 as this causes floor in Apps + Design setups
            else:
                trackingwin = 7
        else:
            trackingwin = 0

        self._reg_write(model.vars.MODEM_REALTIMCFE_TRACKINGWIN, trackingwin)

    def calc_pmoffset_reg(self, model):

        afc_oneshot_enabled = (model.vars.MODEM_AFC_AFCONESHOT.value == 1)
        rtschmode = model.vars.MODEM_REALTIMCFE_RTSCHMODE.value
        osr = model.vars.MODEM_TRECSCFG_TRECSOSR.value
        pmwinsize = model.vars.pmacquingwin_actual.value

        if (rtschmode==1) and afc_oneshot_enabled:
            pmoffset = 2
        else:
            pmoffset =  osr * pmwinsize + 2

        self._reg_write(model.vars.MODEM_TRECSCFG_PMOFFSET,  pmoffset)

