from pyradioconfig.parts.ocelot.calculators.calc_sq import Calc_SQ_Ocelot
from pyradioconfig.calculator_model_framework.Utils.LogMgr import LogMgr
from math import ceil

class Calc_SQ_Sol(Calc_SQ_Ocelot):
    def calc_sq_timeout1_timeout2(self, model):

        demod_select = model.vars.demod_select.value
        sq_enabled = (model.vars.MODEM_SQ_SQEN.value == 1)
        bits_per_symbol = model.vars.bits_per_symbol_actual.value
        delay_adc_to_demod_symbols = model.vars.delay_adc_to_demod_symbols.value
        delay_adc_to_demod_bits = delay_adc_to_demod_symbols / bits_per_symbol
        pmcostvalthd = model.vars.MODEM_TRECPMDET_PMCOSTVALTHD.value
        baudrate = model.vars.baudrate.value

        if sq_enabled:
            if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:
                trecs_pmacqingwin_actual = model.vars.pmacquingwin_actual.value
                phase_dsa_enabled = model.vars.MODEM_DSACTRL_DSAMODE.value
                trecs_pmdet_enabled = model.vars.MODEM_PHDMODCTRL_PMDETEN.value

                if phase_dsa_enabled:
                    schprd = model.vars.MODEM_DSACTRL_SCHPRD.value
                    arrthd = model.vars.MODEM_DSACTRL_ARRTHD.value

                    symbols_in_schprd = 2*(schprd+1)
                    min_preamble_bits = symbols_in_schprd * arrthd

                    sqtimout = int(ceil(delay_adc_to_demod_bits + min_preamble_bits)) # Set timeout1 from the DSA window
                    sqstg2timout = trecs_pmacqingwin_actual # Set timeout2 according to the CFE size

                else:
                    min_preamble_bits = trecs_pmacqingwin_actual

                    if pmcostvalthd > 1:
                        detection_delay = 2 * pmcostvalthd
                    else:  # : use at least 4 delay symbols based on sweep of MI and datarates
                        detection_delay = 4

                    sqtimout_buffer = 5
                    sqtimout = int(ceil(sqtimout_buffer + detection_delay + delay_adc_to_demod_bits + min_preamble_bits))

                    if trecs_pmdet_enabled:
                        pmdetthd = model.vars.MODEM_PHDMODCTRL_PMDETTHD.value
                        sqstg2timout = max(2,pmdetthd-min_preamble_bits)
                    else:
                        sqstg2timout = 0 #No separate preamble detect step


                    min_rx_duty_cucle_ontime_us = 20
                    sqtimeoutmin = ceil(int(min_rx_duty_cucle_ontime_us * 1e-6 * baudrate))

                    sqtimout = max(sqtimeoutmin, sqtimout)

            elif demod_select == model.vars.demod_select.var_enum.LEGACY:
                phase_dsa_enabled = model.vars.MODEM_DSACTRL_DSAMODE.value
                symbols_in_timing_window = model.vars.symbols_in_timing_window.value
                number_of_timing_windows = model.vars.number_of_timing_windows.value

                if phase_dsa_enabled:
                    schprd = model.vars.MODEM_DSACTRL_SCHPRD.value
                    arrthd = model.vars.MODEM_DSACTRL_ARRTHD.value

                    symbols_in_schprd = 2 * (schprd + 1)
                    min_preamble_bits = symbols_in_schprd * arrthd

                    sqtimout = int(ceil(delay_adc_to_demod_bits + min_preamble_bits))  # Set timeout1 from the DSA window
                    sqstg2timout = symbols_in_timing_window * number_of_timing_windows

                else:
                    sqtimout = 0
                    sqstg2timout = 0

            else:
                sqtimout = 0
                sqstg2timout = 0
        else:
            sqtimout = 0
            sqstg2timout = 0

        self._reg_write(model.vars.MODEM_SQ_SQTIMOUT, sqtimout)
        self._reg_write(model.vars.MODEM_SQEXT_SQSTG2TIMOUT, sqstg2timout)


    def calc_sq_timeout3(self, model):

        pmendschen = model.vars.MODEM_FRMSCHTIME_PMENDSCHEN.value
        preamble_length = model.vars.preamble_length.value #This is the TX preamble length
        syncword_length = model.vars.syncword_length.value
        sq_enabled = (model.vars.MODEM_SQ_SQEN.value == 1)

        if sq_enabled:
            sqstg3timout = int(preamble_length + syncword_length * 1.5)
        else:
            sqstg3timout = 0

        self._reg_write(model.vars.MODEM_SQEXT_SQSTG3TIMOUT, sqstg3timout)

    def calc_psm_max_sleep_time(self, model):

        preamble_length = model.vars.preamble_length.value #This is the TX preamble length
        sqtimout = model.vars.MODEM_SQ_SQTIMOUT.value
        sqstg2timout = model.vars.MODEM_SQEXT_SQSTG2TIMOUT.value
        baudrate = model.vars.baudrate.value
        sq_enabled = (model.vars.MODEM_SQ_SQEN.value == 1)

        if sq_enabled:
            psm_max_sleep_us = int((preamble_length - 2*sqtimout - sqstg2timout)/baudrate*1000000) #sleep time is in us
        else:
            psm_max_sleep_us = 0

        model.vars.psm_max_sleep_us.value = max(0,psm_max_sleep_us) #Lower bound of 0

    def calc_sqen_reg(self, model):

        fast_detect_enable = (model.vars.fast_detect_enable.value == model.vars.fast_detect_enable.var_enum.ENABLED)
        demod_select = model.vars.demod_select.value

        supported_demod = (demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI) or \
                          (demod_select == model.vars.demod_select.var_enum.TRECS_SLICER)

        if fast_detect_enable:
            if supported_demod:
                sqen_reg = 1
            else:
                LogMgr.Warning("Fast detect is currently only supported with 2FSK PHYs in the Base Profile")
                sqen_reg = 0
        else:
            sqen_reg = 0

        self._reg_write(model.vars.MODEM_SQ_SQEN, sqen_reg)