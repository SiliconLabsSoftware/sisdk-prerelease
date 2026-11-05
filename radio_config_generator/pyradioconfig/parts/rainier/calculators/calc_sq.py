from pyradioconfig.parts.bobcat.calculators.calc_sq import Calc_SQ_Bobcat
from math import ceil


class CalcSqRainier(Calc_SQ_Bobcat):

    def calc_sq_timeout1_timeout2(self, model):

        demod_select = model.vars.demod_select.value
        sq_enabled = (model.vars.MODEM_SQ_SQEN.value == 1)
        bits_per_symbol = model.vars.bits_per_symbol_actual.value
        delay_adc_to_demod_symbols = model.vars.delay_adc_to_demod_symbols.value
        delay_adc_to_demod_bits = delay_adc_to_demod_symbols * bits_per_symbol
        pmcostvalthd = model.vars.MODEM_TRECPMDET_PMCOSTVALTHD.value
        baudrate = model.vars.baudrate.value


        if sq_enabled:
            if demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI or demod_select == model.vars.demod_select.var_enum.TRECS_SLICER:
                trecs_pmacqingwin_actual = model.vars.pmacquingwin_actual.value
                trecs_pmdet_enabled = model.vars.MODEM_PHDMODCTRL_PMDETEN.value

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
                sqtimout = 0
                sqstg2timout = 0

            else:
                sqtimout = 0
                sqstg2timout = 0
        else:
            sqtimout = 0
            sqstg2timout = 0

        self._reg_write(model.vars.MODEM_SQ_SQTIMOUT, sqtimout, check_saturation=True)
        self._reg_write(model.vars.MODEM_SQEXT_SQSTG2TIMOUT, sqstg2timout, check_saturation=True)

    def calc_sqen_reg(self, model):

        fast_detect_enable = (model.vars.fast_detect_enable.value == model.vars.fast_detect_enable.var_enum.ENABLED)
        duty_cycled = model.vars.rxdc_power_save_mode.value != model.vars.rxdc_power_save_mode.var_enum.DISABLED
        symbol_encoding = model.vars.symbol_encoding.value
        demod_select = model.vars.demod_select.value
        hop_enabled = model.vars.hop_enable.value

        supported_demod = (demod_select == model.vars.demod_select.var_enum.TRECS_VITERBI) or \
                          (demod_select == model.vars.demod_select.var_enum.TRECS_SLICER) or \
                          (demod_select == model.vars.demod_select.var_enum.ENHANCED_DSSS)
        if duty_cycled and not hop_enabled:
            sqen_reg = 0
        elif hop_enabled:
            sqen_reg = 1
        elif fast_detect_enable:
            if supported_demod:
                sqen_reg = 1
            else:
                LogMgr.Warning("Fast detect is currently only supported with 2FSK PHYs in the Base Profile and DSSS PHYs in LongRange Profile")
                sqen_reg = 0
        else:
            sqen_reg = 0

        self._reg_write(model.vars.MODEM_SQ_SQEN, sqen_reg)
