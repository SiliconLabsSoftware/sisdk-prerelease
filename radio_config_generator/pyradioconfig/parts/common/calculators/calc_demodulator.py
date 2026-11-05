"""CALC_Demodulator

Calculator functions are pulled by using their names.
Calculator functions must start with "calc_", if they are to be consumed by the framework.
    Or they should be returned by overriding the function:
        def getCalculationList(self):
"""

from collections import OrderedDict
import math
from enum import Enum

from pyradioconfig.calculator_model_framework.Utils.CustomExceptions import CalculationException
from pyradioconfig.calculator_model_framework.interfaces.icalculator import ICalculator
from pyradioconfig.parts.common.calculators.calc_utilities import CALC_Utilities
from pyradioconfig.parts.common.calculators.calc_freq_offset_comp import CALC_Freq_Offset_Comp
from pycalcmodel.core.variable import ModelVariableFormat
from pycalcmodel.core.variable import ModelVariableFormat, CreateModelVariableEnum

from py_2_and_3_compatibility import *

class CALC_Demodulator(ICalculator):

    """
    Init internal variables
    """
    def __init__(self):
        self._major = 1
        self._minor = 0
        self._patch = 0

    def buildVariables(self, model):
        """Populates a list of needed variables for this calculator

        Args:
            model (ModelRoot) : Builds the variables specific to this calculator
        """
        self._addModelVariable(model,'cost_bandwidth', float, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model,'cost_osr',       float, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model,'cost_range',     float, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model,'cost_rate',      float, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model,'cost_src',       float, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model,'cost_fc',        float, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model,'cost_total',     float, ModelVariableFormat.DECIMAL)

        self._addModelVariable(model, 'sample_freq_actual', float, ModelVariableFormat.DECIMAL)
        self._addModelActual(model, 'adc_freq', int, ModelVariableFormat.DECIMAL)

        self._addModelVariable(model, 'src1_calcDenominator', int, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'src2_calcDenominator', int, ModelVariableFormat.DECIMAL)

        self._addModelVariable(model, 'fxo_or_fdec8', float, ModelVariableFormat.DECIMAL)

        self._addModelVariable(model, 'src1_bit_width', int, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'src2_bit_width', int, ModelVariableFormat.DECIMAL)

        self._addModelVariable(model, 'ch_filt_bw_available', float, ModelVariableFormat.DECIMAL, desc='Values of BWSEL available to use in SRC calculation.', is_array=True, units='unitless')

        self._addModelVariable(model, 'src1_range_available', int, ModelVariableFormat.DECIMAL, desc='Values of SRCRATIO1 available to use in SRC calculation.', is_array=True, units='unitless')

        self._addModelVariable(model, 'input_decimation_filter_allow_dec3', int, ModelVariableFormat.DECIMAL, desc='1=Allow input decimation filter decimate by 3', is_array=False, units='unitless')
        self._addModelVariable(model, 'input_decimation_filter_allow_dec8', int, ModelVariableFormat.DECIMAL, desc='1=Allow input decimation filter decimate by 8', is_array=False, units='unitless')
        self._addModelVariable(model, 'input_decimation_filter_allow_dec5', int, ModelVariableFormat.DECIMAL, desc='1=Allow input decimation filter decimate by 5', is_array=False, units='unitless')

        self._addModelVariable(model, 'rx_ch_hopping_order_num', int, ModelVariableFormat.DECIMAL)

        var = self._addModelVariable(model, 'rx_ch_hopping_mode', Enum, ModelVariableFormat.DECIMAL, 'For receive scanning PHYs: event to trigger a hop to next PHY')
        member_data = [
            ['RAIL_RX_CHANNEL_HOPPING_MODE_MANUAL', 0, 'Manual mode'],
            ['RAIL_RX_CHANNEL_HOPPING_MODE_TIMEOUT'  , 1, 'Fixed time out'],
            ['RAIL_RX_CHANNEL_HOPPING_MODE_TIMING_SENSE'   , 2, 'Timing sense'],
            ['RAIL_RX_CHANNEL_HOPPING_MODE_PREAMBLE_SENSE'   , 3, 'Preamble sense'],
        ]
        var.var_enum = CreateModelVariableEnum(
            'RAILRxChHoppingEnum',
            'List of supported RAIL RX channel hopping modes',
            member_data)
        self._addModelVariable(model, 'rx_ch_hopping_delay_usec', int, ModelVariableFormat.DECIMAL)

        self._addModelVariable(model, 'viterbi_demod_expect_patt', long, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'viterbi_demod_expect_patt_head_tail', int, ModelVariableFormat.DECIMAL)


        if model.part_family.lower() not in ["dumbo", "jumbo", "nerio", "nixi", "unit_test_part"]:
            self._addModelVariable(model, 'src1_range_available_minimum', int, ModelVariableFormat.DECIMAL, 'SRC range minimum')
            self._addModelVariable(model, 'viterbidemod_disdemodof_viterbi_demod_disable_overflow_detect', int, ModelVariableFormat.DECIMAL)



    def calc_fxo_or_fdec8(self, model):
        model.vars.fxo_or_fdec8.value = model.vars.xtal_frequency.value * 1.0

    def calc_src12_bit_widths(self, model):
        model.vars.src1_bit_width.value = 8
        model.vars.src2_bit_width.value = 11

    def calc_ch_filt_bw_available(self, model):
        model.vars.ch_filt_bw_available.value = [0.263, 0.196]

    def calc_src1_range_available(self, model):
        model.vars.src1_range_available = xrange(127, 156, 1)




    def calc_baudrate(self, model):
        """calculate baudrate based on modulation format and bitrate

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        modformat = model.vars.modulation_type.value
        spreading_factor = model.vars.dsss_spreading_factor.value * 1.0

        baudrate = model.vars.bitrate.value
        baudrate_offset = model.vars.rx_bitrate_offset_hz.value

        if modformat == model.vars.modulation_type.var_enum.FSK4:
            baudrate /= 2.0
            baudrate_offset /= 2.0

        if spreading_factor > 0:
            baudrate *= spreading_factor
            baudrate_offset *= spreading_factor

        model.vars.baudrate.value = int(round(baudrate))
        model.vars.rx_baudrate_offset_hz.value = int(round(baudrate_offset))

    def calc_d0_d1_d2_cfosr_rxbr_value(self, model):
        """calculates optimal settings for DEC0, DEC1, DEC2, CFOSR, RXBRNUM, RXBRDEN
        Although the manual provides step by step instruction to first calculate DEC0
        followed by DEC and CFOSR and then DEC2 and RXBRNUM/RXBRDEN we found that there
        is heavy dependency between these registers. For example an optimal setting
        for DEC0 might prevent us from choosing a RXBRNUM/RXBDEN pair that can give
        the exact baudrate we want. By choosing a less optimal DEC0 value we can
        get an optimal RXBRNUM/RXBRDEN pair that overall gives better performance.
        Therefore we search over all possible combinations of these register setting using
        a cost function that factors the following errors in (see return_cost() for details):
            1. bandwidth error
            2. oversampling rate fraction error
            3. oversampling rate range error
            4. baudrate error
            5. center (IF) frequency error
            6. not yet implemented but like to add freqyency deviation error
        We chose scaling factors for each of these error terms to provide reasonable
        PHY settings but we might need to tweak these scaling factors as we receiver more
        feedback from newly generated PHYs.

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        baudrate = model.vars.baudrate.value
        fxo = model.vars.fxo_or_fdec8.value
        bw_desired = model.vars.bandwidth_hz.value * 1.0
        brcalen = model.vars.brcalen.value
        tol = model.vars.baudrate_tol_ppm.value

        model.vars.if_frequency_hz.value = 0
        fc_desired = model.vars.if_frequency_hz.value * 1.0

        if brcalen == 1:
            baudrate -= baudrate * tol / 1000000.0

        if model.vars.bandwidth_hz.value_forced == None:
            forced = 0
        else:
            forced = 1

        best_cost = 99e99
        best_fc = 0

        for dec0 in [3, 4, 8]:

            for dec1 in xrange(11500, 0, -1):

                if fc_desired > 0:
                    cfosr_list = self.valid_cfosr(round(fxo / (dec0 * fc_desired)))
                else:
                    cfosr_list = [32, 16, 12, 8, 7]

                for cfosr in cfosr_list:
                    bw = (fxo * 0.263) / (dec0 * dec1)

                    if bw > 2e6:
                        carson_scale = 0.82
                    else:
                        carson_scale = 0.95


                    if bw < carson_scale * bw_desired and not (dec1 == 1 and (dec0 == 3 or dec0 == 4)):
                        continue

                    fc = fxo / (dec0 * cfosr)

                    if (fc < 250e3 and fc_desired == 0) or fc - bw / 2 < 107e3:
                        continue

                    dec0_bw = self.return_dec0_bw(dec0, fxo)

                    if (fc + bw / 2) > dec0_bw:
                        continue


                    if bw_desired > 84 * baudrate:
                        dec2_start = int(math.floor(fxo / (15.0 * dec0 * dec1 * baudrate)))
                        dec2_end = int(math.ceil(fxo / (5.0 * dec0 * dec1 * baudrate)))
                    else:
                        dec2_start = int(math.floor(fxo / (5.0 * dec0 * dec1 * baudrate)))
                        dec2_end = dec2_start + 1

                    if dec2_start == 0:
                        dec2_start = 1
                        dec2_end = 1

                    if dec2_end > 63:
                        dec2_end = 63

                    for dec2 in xrange(dec2_start, dec2_end + 1):

                        rxbrint, rxbrnum, rxbrden = self.return_rxbr(dec0, dec1, dec2, baudrate, fxo, brcalen)

                        baudrate_real = py2round(fxo / (dec0 * dec1 * dec2 * 2 * (rxbrint + float(rxbrnum) / rxbrden)))

                        osr = 2 * (rxbrint + float(rxbrnum) / rxbrden)

                        [cost, bw_error, osr_error, range_error, rate_error, fc_cost] = \
                            self.return_cost(model, osr, bw, bw_desired, baudrate_real, baudrate, fc, forced, brcalen)

                        if cost < best_cost or (cost == best_cost and dec0 > dec1 and fc == best_fc):
                            best_cost = cost
                            best_dec0 = dec0
                            best_dec1 = dec1
                            best_dec2 = dec2
                            best_cfosr = cfosr
                            best_rxbrnum = rxbrnum
                            best_rxbrden = rxbrden
                            best_rxbrint = rxbrint
                            best_fc = fc

                            model.vars.cost_bandwidth.value = bw_error
                            model.vars.cost_osr.value = osr_error
                            model.vars.cost_range.value = range_error
                            model.vars.cost_rate.value = rate_error
                            model.vars.cost_fc.value = fc_cost
                            model.vars.cost_total.value = cost

        model.vars.dec0.value = int(best_dec0)
        model.vars.dec1.value = int(best_dec1)
        model.vars.dec2.value = int(best_dec2)
        model.vars.cfosr.value = int(best_cfosr)
        model.vars.rxbrint.value = int(best_rxbrint)
        model.vars.rxbrnum.value = int(best_rxbrnum)
        model.vars.rxbrden.value = int(best_rxbrden)
        model.vars.if_frequency_hz.value = int(best_fc)


    def return_cost(self, model, osr, bw, bw_desired, baudrate_real, baudrate_hz, fc, forced, brcalen):
        """Cost function used to find optimal settigs for DEC0, DEC1, DEC2, CFOSR, RXBRNUM, RXBRDEN

        Args:
            model (ModelRoot) : Data model to read and write variables from
            osr (unknown) : unknwon
            bw (unknown) : unknwon
            bw_desired  (unknown) : unknwon
            baudrate_real (unknown) : unknwon
            baudrate_hz (unknown) : unknwon
            fc (unknown) : unknwon
            forced (unknown) : unknwon
            brcalen (unknown) : unknwon
        """
        if bw > bw_desired:
            bw_error = 100.0 * (bw - bw_desired) / bw_desired
        else:
            bw_error = 100.0 * (bw_desired - bw) / bw_desired

        osr_error = abs(osr - py2round(osr))

        if osr < 4:
            range_error = 200.0
        elif osr < 5:
            range_error = 1.0
        elif osr <= 7:
            range_error = 0.0
        else:
            range_error = 200.0

        rate_error = 100.0 * abs(baudrate_real - baudrate_hz) / baudrate_real

        if brcalen == 1:
            rate_error_limit = 1.0
        else:
            rate_error_limit = 0.1

        if rate_error > rate_error_limit:
            rate_error += 1.0e9
        elif rate_error > 0:
            rate_error += 100.0

        if fc < 250e3:
            fc_cost = 1.0e9
        else:
            fc_cost = 0.0

        if forced:
            cost = 100 * bw_error + osr_error + range_error + rate_error + fc_cost
        else:
            cost = bw_error + osr_error + range_error + rate_error + fc_cost

        return cost, bw_error, osr_error, range_error, rate_error, fc_cost


    def calc_dec0_reg(self, model):
        """Given desired decimation ratio for DEC0 and bandwidth returns best
        register setting for CF_DEC0. DEC0 can have decimation values of 3, 4 or 8
        There is only one filter for decimation by 3 at nominal bandwidth of 1.92 MHz.
        Decimation by 4 has two filter options: 2.65MHz and 1.42MHz while Decimation
        by 8 has 192KHz and 461KHz filter options. We like to choose the smallest
        bandwidth that is larger than the desired signal bandwidth.
        The filter bandwidths are a fraction of the fxo frequency the numbers above
        are for crystal frequency of 38.4 MHz.

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        val = model.vars.dec0.value
        fxo = model.vars.xtal_frequency.value
        bw = model.vars.bandwidth_hz.value * 1.0
        family = model.part_family.lower()

        if val == 3:
            reg = 0
        elif val == 4:
            if family in ["dumbo", "jumbo", "unit_test_part"]:
                if bw > fxo * 0.037:
                    reg = 1
                else:
                    reg = 2
            else:
                reg = 1
        elif val == 8:
            if bw > fxo * 0.005:
                reg = 3
            else:
                reg = 4

        self._reg_write(model.vars.MODEM_CF_DEC0,  reg)


    def return_dec0_from_reg(self, reg):
        """convert register value to decimation value

        Args:
            reg (int) : register value to decimation value
        """
        if reg == 0:
            dec0 = 3
        elif reg == 1 or reg == 2:
            dec0 = 4
        elif reg == 3 or reg == 4:
            dec0 = 8

        return dec0


    def calc_dec0_actual(self, model):
        """Read register value and return decimation rate for DEC0

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        reg = model.vars.MODEM_CF_DEC0.value

        model.vars.dec0_actual.value = self.return_dec0_from_reg(reg)


    def return_dec0_bw(self, val, fxo):
        """Returns maximum bandwidth for a given decimation value for DEC0

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        if val == 3:
            bw = 0.05 * fxo
        elif val == 4:
            bw = 0.069 * fxo
        else:
            bw = 0.012 * fxo

        return bw


    def calc_dec1_dec2_reg(self, model):
        """write DEC1 and DEC2 to register given calculated values

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        dec1 = model.vars.dec1.value
        dec2 = model.vars.dec2.value

        self._reg_write(model.vars.MODEM_CF_DEC1,  dec1 - 1)
        self._reg_write(model.vars.MODEM_CF_DEC2,  dec2 - 1)


    def calc_dec1_dec2_actual(self, model):
        """return actual decimation values from DEC1 and DEC2 registers

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        model.vars.dec1_actual.value = model.vars.MODEM_CF_DEC1.value + 1
        model.vars.dec2_actual.value = model.vars.MODEM_CF_DEC2.value + 1


    def return_cfosr_from_reg(self, reg):
        """convert CFOSR register value to decimation rate

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        if reg == 0:
            val = 7
        elif reg == 1:
            val = 8
        elif reg == 2:
            val = 12
        elif reg == 3:
            val = 16
        elif reg == 4:
            val = 32
        elif reg == 5:
            val = 0
        else:
            raise CalculationException("Invalid CFOSR value!")

        return val


    def calc_cfosr_actual(self, model):
        """convert CFOSR register value to decimation rate

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        reg = model.vars.MODEM_CF_CFOSR.value

        model.vars.cfosr_actual.value = self.return_cfosr_from_reg(reg)


    def calc_cfosr_reg(self, model):
        """convert CFOSR decimation value to register setting

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        val = model.vars.cfosr.value

        if val == 7:
            reg = 0
        elif val == 8:
            reg = 1
        elif val == 12:
            reg = 2
        elif val == 16:
            reg = 3
        elif val == 32:
            reg = 4
        else:
            reg = 99
            raise CalculationException("Undefined value for CFOSR!")

        self._reg_write(model.vars.MODEM_CF_CFOSR,  reg)

    def valid_cfosr(self, val):
        """

        Args:
            val (unknown) : val
        """
        cfosr_list = [32, 16, 12, 8, 7]

        for cfosr in cfosr_list:

            if val == cfosr:
                return [cfosr]

        return []

    def return_rxbr(self, dec0, dec1, dec2, baudrate_hz, fxo, brcalen):
        """calculate RXBR ratio needed to get desired baudrate.
        Using Equation (5.17) of EFR32 Reference Manual (internal.pdf)

        Args:
            dec0 (unknown): unknown)
            dec1 (unknown): unknown)
            dec2(unknown): unknown)
            baudrate_hz (unknown): unknown)
            fxo (unknown): unknown)
            brcalen (unknown): unknown)
        """

        rxbrfrac = fxo / (dec0 * dec1 * dec2 * baudrate_hz * 2.0)

        if rxbrfrac < 1/31.0:
            return 0, 1, 31

        rxbrint = math.floor(rxbrfrac)

        if rxbrint > 7:
            rxbrint = 7

        rxbrfrac -= rxbrint

        if brcalen == 1:
            rxbrden_range = [16]
        else:
            rxbrden_range = xrange(1, 32)

        best_error = 99
        best_num = 0
        best_den = 0

        for rxbrden in rxbrden_range:

            rxbrnum = py2round(rxbrden * rxbrfrac)

            if rxbrnum > 31:
                continue

            error = abs(rxbrnum*1.0/rxbrden - rxbrfrac)

            if brcalen == 1 and rxbrnum*1.0/rxbrden < rxbrfrac:
                rxbrnum += 1

            if error < best_error:
                best_error = error
                best_num = rxbrnum
                best_den = rxbrden*1.0

            if error == 0:
                break

        return rxbrint, best_num, best_den


    def calc_rxbr_reg(self, model):
        """write RXBR register values

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        baudrate_hz = model.vars.baudrate.value * 1.0
        baudrate_offset = model.vars.rx_baudrate_offset_hz.value
        fxo = model.vars.fxo_or_fdec8.value
        brcalen = model.vars.brcalen.value
        dec0 = model.vars.dec0.value
        dec1 = model.vars.dec1.value
        dec2 = model.vars.dec2.value

        if baudrate_offset == 0:
            rxbrint = model.vars.rxbrint.value
            rxbrnum = model.vars.rxbrnum.value
            rxbrden = model.vars.rxbrden.value
        else:
            offset_limit = 0.01 * baudrate_hz
            if baudrate_offset > offset_limit:
                baudrate_offset = offset_limit
            elif baudrate_offset < -offset_limit:
                baudrate_offset = - offset_limit

            new_baudrate = baudrate_hz + baudrate_offset

            (rxbrint, rxbrnum, rxbrden) = self.return_rxbr(dec0, dec1, dec2, new_baudrate, fxo, brcalen)

        self._reg_write(model.vars.MODEM_RXBR_RXBRINT, int(rxbrint))
        self._reg_write(model.vars.MODEM_RXBR_RXBRNUM, int(rxbrnum))
        self._reg_write(model.vars.MODEM_RXBR_RXBRDEN, int(rxbrden))


    def calc_rxbr_actual(self, model):
        """read back actual RXBR register settings and calculate the actual RXBR fraction

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        model.vars.rxbrint_actual.value = model.vars.MODEM_RXBR_RXBRINT.value
        model.vars.rxbrnum_actual.value = model.vars.MODEM_RXBR_RXBRNUM.value
        model.vars.rxbrden_actual.value = model.vars.MODEM_RXBR_RXBRDEN.value
        model.vars.rxbrfrac_actual.value = model.vars.MODEM_RXBR_RXBRINT.value + \
                                        model.vars.MODEM_RXBR_RXBRNUM.value * 1.0 / \
                                        model.vars.MODEM_RXBR_RXBRDEN.value


    def calc_rx_baud_rate_actual(self, model):
        """calculate actual RX baud rate from register settings
        Equation (5.17) of EFR32 Reference Manual (internal.pdf)

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """
        fxo = model.vars.xtal_frequency.value
        dec0 = model.vars.dec0_actual.value
        dec1 = model.vars.dec1_actual.value
        dec2 = model.vars.dec2_actual.value
        rxbrint = model.vars.rxbrint_actual.value
        rxbrden = model.vars.rxbrden_actual.value*1.0
        rxbrnum = model.vars.rxbrnum_actual.value

        rx_baud_rate = fxo / (dec0 * dec1 * dec2 * 2.0 * (rxbrint + rxbrnum / rxbrden))

        model.vars.rx_baud_rate_actual.value = rx_baud_rate


    def calc_bw_carson(self, model):
        """calculate carson's rule bandwidth: baudrate + 2 * max frequency deviation
        max frequency deviation can be due desired FSK deviation but also due to
        frequency offset in crystal frequencies.

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        baudrate_hz = model.vars.baudrate.value
        freq_dev_hz = model.vars.deviation.value * 1.0
        rx_ppm = model.vars.rx_xtal_error_ppm.value
        tx_ppm = model.vars.tx_xtal_error_ppm.value
        rf_freq_hz = model.vars.base_frequency.value
        mode = model.vars.frequency_comp_mode.value
        mod_format = model.vars.modulation_type.value
        mode_index = CALC_Freq_Offset_Comp().freq_comp_mode_index(model, mode)

        if mode_index >= 4:
            freq_tolerance = 0
        else:
            freq_tolerance = 2.0 * (rx_ppm + tx_ppm)/1.0e6 * rf_freq_hz

        if mod_format == model.vars.modulation_type.var_enum.OOK or \
           mod_format == model.vars.modulation_type.var_enum.ASK:
            bw_carson = 5.0 * baudrate_hz + freq_tolerance
        else:
            bw_carson = baudrate_hz + 2.0 * freq_dev_hz + freq_tolerance

        model.vars.bandwidth_carson_hz.value = int(py2round(bw_carson))

        if mod_format == model.vars.modulation_type.var_enum.OOK:
            model.vars.bandwidth_hz.value = int(py2round(baudrate_hz*75))
        else:
            model.vars.bandwidth_hz.value = int(py2round(bw_carson))


    def calc_bandwidth_actual(self, model):
        """calculate actual BW from register settings
        Equation (5.15) of EFR32 Reference Manual (internal.pdf)

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        fxo = model.vars.xtal_frequency.value
        dec0 = model.vars.dec0_actual.value
        dec1 = model.vars.dec1_actual.value

        bandwidth = py2round(fxo * 0.263 / (dec0 * dec1))

        model.vars.bandwidth_actual.value = int(bandwidth)


    def calc_if_frequency_actual(self, model):
        """calculate the actual IF frequency (IF frequency)
        Equation (5.14) of EFR32 Reference Manual (internal.pdf)

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        fxo = model.vars.xtal_frequency.value
        dec0 = model.vars.dec0_actual.value
        cfosr = model.vars.cfosr_actual.value

        if_frequency = fxo / (dec0 * cfosr)

        model.vars.if_frequency_hz_actual.value = int(if_frequency)

    def calc_freq_gain_value(self, model):
        """calculate desired frequency gain
        Using Equation (5.22) and (5.23) of EFR32 Reference Manual (internal.pdf)

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        fxo = model.vars.fxo_or_fdec8.value
        freq_offset = model.vars.freq_offset_hz.value * 1.0
        freq_dev_hz = model.vars.deviation.value
        dec0 = model.vars.dec0_actual.value
        dec1 = model.vars.dec1_actual.value
        dec2 = model.vars.dec2_actual.value
        mod_format = model.vars.modulation_type.value

        if freq_dev_hz != 0 and freq_offset >= freq_dev_hz:
            scale = freq_offset / freq_dev_hz
            freq_dev_hz = freq_offset
        else:
            scale = 1.0

        if freq_dev_hz > 0:
            if mod_format == model.vars.modulation_type.var_enum.FSK2:
                target_freq_gain = fxo / (4.0 * freq_dev_hz * dec0 * dec1 * dec2)
            elif mod_format == model.vars.modulation_type.var_enum.FSK4:
                target_freq_gain = fxo / (8.0 * freq_dev_hz * dec0 * dec1 * dec2)
            else:
                target_freq_gain = 0.0
        else:
            target_freq_gain = 0.0

        model.vars.freq_gain.value = target_freq_gain
        model.vars.freq_gain_scale.value = scale


    def calc_freq_gain_reg(self, model):
        """calculate register setting for frequency gain

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        target_freq_gain = model.vars.freq_gain.value

        if target_freq_gain > 0:
            m, e = CALC_Utilities().frac2exp(7, target_freq_gain)
            e = 2 - e
        else:
            m = e = 0




        if (int(m)) > 7:
            raise CalculationException("Calculated frequency signal scaling coefficient of %s exceeds limit of 7!" % int(m) )

        if (int(e)) > 7:
            raise CalculationException("Calculated frequency signal scaling exponent of %s exceeds limit of 7!" % int(e) )

        self._reg_write(model.vars.MODEM_MODINDEX_FREQGAINM,  int(m))
        self._reg_write(model.vars.MODEM_MODINDEX_FREQGAINE,  int(e))


    def calc_freq_gain_actual(self, model):
        """given register setting calculate actual frequency gain

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        m = model.vars.MODEM_MODINDEX_FREQGAINM.value
        e = model.vars.MODEM_MODINDEX_FREQGAINE.value

        freq_gain = 1.0 * m * 2**(2-e)

        model.vars.freq_gain_actual.value = freq_gain


    def calc_rx_freq_dev_actual(self, model):
        """given register settings calculate actual frequency deviation
        the PHY nominally expects in the receive path.

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        fxo = model.vars.fxo_or_fdec8.value
        dec0 = model.vars.dec0_actual.value
        dec1 = model.vars.dec1_actual.value
        dec2 = model.vars.dec2_actual.value
        gain = model.vars.freq_gain_actual.value
        mod_format = model.vars.modulation_type.value

        deviation = 0.0

        if gain > 0:
            if mod_format == model.vars.modulation_type.var_enum.FSK2:
                deviation = fxo / (4.0 * gain * dec0 * dec1 * dec2)
            elif mod_format == model.vars.modulation_type.var_enum.FSK4:
                deviation = fxo / (8.0 * gain * dec0 * dec1 * dec2)

        model.vars.rx_deviation_actual.value = deviation


    def calc_mod_type_actual(self, model):
        """given register setting return actual modulation method programmed

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        mod = model.vars.MODEM_CTRL0_MODFORMAT.value

        if mod == 0:
            modformat = '2-FSK'
        elif mod == 1:
            modformat = '4-FSK'
        elif mod == 2:
            modformat = 'BPSK'
        elif mod == 3:
            modformat = 'DBPSK'
        elif mod == 4:
            modformat = 'OQPSK'
        elif mod == 5:
            modformat = 'MSK'
        elif mod == 6:
            modformat = 'OOKASK'

        model.vars.mod_format_actual.value = modformat


    def calc_mod_type_reg(self, model):
        """reading input modulation type config parameter set the MODFORMAT register

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        modformat = model.vars.modulation_type.value

        if modformat == model.vars.modulation_type.var_enum.FSK2:
            mod = 0
        elif modformat == model.vars.modulation_type.var_enum.FSK4:
            mod = 1
        elif modformat == model.vars.modulation_type.var_enum.BPSK:
            mod = 2
        elif modformat == model.vars.modulation_type.var_enum.DBPSK:
            mod = 3
        elif modformat == model.vars.modulation_type.var_enum.OQPSK:
            mod = 4
        elif modformat == model.vars.modulation_type.var_enum.MSK:
            mod = 5
        elif modformat == model.vars.modulation_type.var_enum.OOK or \
             modformat == model.vars.modulation_type.var_enum.ASK:
            mod = 6
        else:
            raise CalculationException('ERROR: modulation method in input file not recognized')

        self._reg_write(model.vars.MODEM_CTRL0_MODFORMAT,  mod)



    def calc_interpolation_gain_actual(self, model):
        """calculate interpolation gain
        Using Equation (5.10) and (5.11) of EFR32 Reference Manual (internal.pdf)

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        txbrnum = model.vars.MODEM_TXBR_TXBRNUM.value
        modformat = model.vars.modulation_type.value

        if txbrnum < 256:
            interpolation_gain = txbrnum / 1.0
        elif txbrnum < 512:
            interpolation_gain = txbrnum / 2.0
        elif txbrnum < 1024:
            interpolation_gain = txbrnum / 4.0
        elif txbrnum < 2048:
            interpolation_gain = txbrnum / 8.0
        elif txbrnum < 4096:
            interpolation_gain = txbrnum / 16.0
        elif txbrnum < 8192:
            interpolation_gain = txbrnum / 32.0
        elif txbrnum < 16384:
            interpolation_gain = txbrnum / 64.0
        else:
            interpolation_gain = txbrnum / 128.0

        if modformat == model.vars.modulation_type.var_enum.MSK or \
           modformat == model.vars.modulation_type.var_enum.OQPSK:
            interpolation_gain = 2 ** (math.ceil(math.log(interpolation_gain, 2)))

        model.vars.interpolation_gain_actual.value = float(interpolation_gain)


    def calc_dec1gain_value(self, model):
        """calculate additional gain we want in the DEC1 decimator for very low bandwidth
        PHY settings.
        see register definition of DEC1GAIN in EFR32 Reference Manual (internal.pdf)

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        bw = model.vars.bandwidth_actual.value

        if bw < 500:
            dec1gain = 12
        elif bw < 2000:
            dec1gain = 6
        else:
            dec1gain = 0

        model.vars.dec1gain.value = dec1gain


    def calc_dec1gain_reg(self, model):
        """set DEC1GAIN register based on calculated value

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        val = model.vars.dec1gain.value

        if val == 12:
            reg = 2
        elif val == 6:
            reg = 1
        else:
            reg = 0

        self._reg_write(model.vars.MODEM_CF_DEC1GAIN,  reg)


    def calc_dec1gain_actual(self, model):
        """given register settings return actual DEC1GAIN used

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        reg = model.vars.MODEM_CF_DEC1GAIN.value

        if reg == 0:
            val = 0
        elif reg == 1:
            val = 6
        else:
            val = 12

        model.vars.dec1gain_actual.value = val





    def calc_datafilter_reg(self, model):
        """set register for datafilter size
        see DATAFILTER register entry in EFR32 Reference Manual (internal.pdf)

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        modformat = model.vars.modulation_type.value
        osr = model.vars.oversampling_rate_actual.value

        if modformat == model.vars.modulation_type.var_enum.FSK2 or \
           modformat == model.vars.modulation_type.var_enum.FSK4 or \
           modformat == model.vars.modulation_type.var_enum.BPSK:
            if osr >= 4.5:
                datafilter = 3
            elif osr >= 3.5:
                datafilter = 2
            elif osr >= 2.5:
                datafilter = 1
            else:
                datafilter = 0
        elif modformat == model.vars.modulation_type.var_enum.OOK or \
             modformat == model.vars.modulation_type.var_enum.ASK:
            if osr >= 2.5:
                datafilter = 1
            else:
                datafilter = 0
        else:
            datafilter = 0

        self._reg_write(model.vars.MODEM_CTRL2_DATAFILTER,  datafilter)


    def calc_devweightdis_reg(self, model):
        """determine if deviation weighting should be disabled
        see section 5.7.5 in EFR32 Reference Manual (internal.pdf)

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        scale = model.vars.freq_gain_scale.value

        if scale < 1:
            self._reg_write(model.vars.MODEM_CTRL2_DEVWEIGHTDIS,  1)
        else:
            self._reg_write(model.vars.MODEM_CTRL2_DEVWEIGHTDIS,  0)


    def calc_phasedemod_reg(self, model):
        """set register to define demodulation method for phase modulated signals

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        length = model.vars.dsss_len.value
        modulation = model.vars.modulation_type.value

        if modulation == model.vars.modulation_type.var_enum.OQPSK or \
           modulation == model.vars.modulation_type.var_enum.MSK:
            phasedemod = 1
        elif modulation == model.vars.modulation_type.var_enum.BPSK or \
            modulation == model.vars.modulation_type.var_enum.DBPSK:
            if length > 0:
                phasedemod = 2
            else:
                phasedemod = 1
        else:
            phasedemod = 0

        self._reg_write(model.vars.MODEM_CTRL1_PHASEDEMOD, phasedemod)


    max_accumulated_tolerance_ppm = 30000.0


    def calc_resyncper_brcal_val(self, model):
        """calculate resync period for baudrate offset compensation
        see section 5.7.4 in EFR32 Reference Manual (internal.pdf)

        Args:
            model (ModelRoot) : Data model to read and wrie variables from
        """

        timingwindow = model.vars.timing_window_actual.value
        tol = model.vars.baudrate_tol_ppm.value
        freq_offset = model.vars.freq_offset_hz.value * 1.0
        freq_dev_hz = model.vars.deviation.value
        length = model.vars.dsss_len.value
        mod_type = model.vars.modulation_type.value
        in_2fsk_opt_scope = model.vars.in_2fsk_opt_scope.value

        if mod_type == model.vars.modulation_type.var_enum.OOK or in_2fsk_opt_scope:
            resyncper = 1
        else:
            if tol > 0:
                resyncper = self.max_accumulated_tolerance_ppm / (tol * timingwindow)
            else:
                resyncper = 2.0

        if resyncper < 0.5 and length == 0 and mod_type != model.vars.modulation_type.var_enum.OOK \
                and not in_2fsk_opt_scope:
            resyncper = 1.0
            brcalen = 1

            if freq_offset > freq_dev_hz / 2:
                brcalavg = 1
            else:
                brcalavg = 2

        else:
            brcalavg = 0
            brcalen = 0

        if resyncper > 15.0:
            resyncper = 15.0

        model.vars.brcalavg.value = brcalavg
        model.vars.brcalen.value = brcalen
        model.vars.timing_resync_period.value = int(round(resyncper))


    def calc_rsyncper_reg(self, model):
        """write calculated resyncper to register after rounding

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        self._reg_write(model.vars.MODEM_CTRL1_RESYNCPER, model.vars.timing_resync_period.value)


    def calc_resyncper_actual(self, model):
        """return actual RESYNCPER given register setting

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        model.vars.resyncper_actual.value = float(model.vars.MODEM_CTRL1_RESYNCPER.value)


    def calc_brcal_reg(self, model):
        """write BRCALAVG AND BRCALEN register

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        self._reg_write(model.vars.MODEM_CTRL5_BRCALAVG, model.vars.brcalavg.value)
        self._reg_write(model.vars.MODEM_CTRL5_BRCALEN, model.vars.brcalen.value)


    def calc_baudrate_tol_ppm_actual(self, model):
        """calculate actual baudrate tolerance set by registers

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        timingwindow = model.vars.timing_window_actual.value
        resyncper = model.vars.resyncper_actual.value

        if resyncper > 0:
            tol = self.max_accumulated_tolerance_ppm / (resyncper * timingwindow)
        else:
            tol = 0

        model.vars.baudrate_tol_ppm_actual.value = int(py2round(tol))

    def calc_isicomp_reg(self, model):
        """calc_isicomp_reg

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        modulation = model.vars.modulation_type.value

        if modulation == model.vars.modulation_type.var_enum.FSK4:
            self._reg_write(model.vars.MODEM_CTRL4_ISICOMP, 8)
        else:
            self._reg_write(model.vars.MODEM_CTRL4_ISICOMP, 0)

    def calc_devoffcomp_reg(self, model):
        """calc_devoffcomp_reg

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        modulation = model.vars.modulation_type.value

        if modulation == model.vars.modulation_type.var_enum.FSK4:
            self._reg_write(model.vars.MODEM_CTRL4_DEVOFFCOMP, 0)
        else:
            self._reg_write(model.vars.MODEM_CTRL4_DEVOFFCOMP, 0)

    def calc_offsetphasemasking_reg(self, model):
        """calc_offsetphasemasking_reg

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        modulation = model.vars.modulation_type.value

        if modulation == model.vars.modulation_type.var_enum.BPSK or \
           modulation == model.vars.modulation_type.var_enum.DBPSK:
            self._reg_write(model.vars.MODEM_CTRL4_OFFSETPHASEMASKING, 1)
        else:
            self._reg_write(model.vars.MODEM_CTRL4_OFFSETPHASEMASKING, 0)

    def calc_sqithresh_val(self, model):
        """set signal quality threshold value to the default POR value.

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        model.vars.sqi_threshold.value = 0

    def calc_sqithresh_reg(self, model):
        """set sqithresh register - intermediate variable used to override as advanced input

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        self._reg_write(model.vars.MODEM_CTRL2_SQITHRESH, model.vars.sqi_threshold.value)

    def calc_rx_bitrate_offset_hz(self, model):
        """set to zero by default

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        model.vars.rx_bitrate_offset_hz.value = 0


    def calc_sample_freq_actual(self, model):

        xtal_frequency = model.vars.fxo_or_fdec8.value
        dec0 = model.vars.dec0.value
        dec1 = model.vars.dec1.value
        part_family = model.part_family.lower()

        if part_family not in ["dumbo","unit_test_part"]:
            src1_ratio_actual = model.vars.src1_ratio_actual.value
            src2_ratio_actual = model.vars.src2_ratio_actual.value
        else:
            src1_ratio_actual = 1.0
            src2_ratio_actual = 1.0

        sample_freq_actual =  int(xtal_frequency / dec0 / dec1) * src1_ratio_actual * src2_ratio_actual

        model.vars.sample_freq_actual.value = sample_freq_actual

    def calc_adc_freq_actual(self, model):
        xtal_frequency_hz = model.vars.xtal_frequency_hz.value

        adc_freq_actual = xtal_frequency_hz

        model.vars.adc_freq_actual.value = adc_freq_actual

    def calc_src1_denominator(self, model):
        dec0 = model.vars.dec0_actual.value
        dec1 = model.vars.dec1_actual.value
        bandwidth = model.vars.bandwidth_hz.value
        model.vars.src1_calcDenominator.value = bandwidth * dec0 * dec1

    def calc_src2_denominator(self, model):
        osr = model.vars.oversampling_rate_actual.value
        datarate = model.vars.baudrate.value
        dec0 = model.vars.dec0_actual.value
        dec1 = model.vars.dec1_actual.value
        dec2 = model.vars.dec2_actual.value

        src2_calcDenominator = int(datarate * dec0 * dec1 * dec2 * osr)

        model.vars.src2_calcDenominator.value = src2_calcDenominator

    def calc_dec0_values_available(self, model):
        model.vars.input_decimation_filter_allow_dec3.value = 1
        model.vars.input_decimation_filter_allow_dec8.value = 1
        model.vars.input_decimation_filter_allow_dec5.value = 0
