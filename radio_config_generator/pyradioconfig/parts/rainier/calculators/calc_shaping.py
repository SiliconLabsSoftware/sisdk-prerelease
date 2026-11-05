from pycalcmodel.core.variable import ModelVariableFormat
from pyradioconfig.parts.bobcat.calculators.calc_shaping import Calc_Shaping_Bobcat
from pyradioconfig.parts.common.utils.tinynumpy import tinynumpy
from pyradioconfig.calculator_model_framework.Utils.LogMgr import LogMgr
import numpy as np
from pyradioconfig.calculator_model_framework.Utils.CustomExceptions import CalculationException

class CalcShapingRainier(Calc_Shaping_Bobcat):

    def calc_shaping_filter_gain_actual(self, model):

        shaping_filter_mode = model.vars.MODEM_CTRL0_SHAPING.value
        sf = self.get_shaping_filter(model)

        if shaping_filter_mode == 0:
            shaping_filter_gain = 127.0 / 128.0

        elif shaping_filter_mode == 1:
            shaping_filter_gain = ((sf[0] + sf[8] + sf[0]) +
                                   (sf[1] + sf[7]) +
                                   (sf[2] + sf[6]) +
                                   (sf[3] + sf[5]) +
                                   (sf[4] + sf[4])) / 5.0 / 128.0

        elif shaping_filter_mode == 2:
            shaping_filter_gain = ((sf[0] + sf[7]) +
                                   (sf[1] + sf[6]) +
                                   (sf[2] + sf[5]) +
                                   (sf[3] + sf[4])) / 4.0 / 128.0

        else:
            shaping_filter_gain = (sf[0] + sf[8] + sf[16] + sf[24] + sf[32] + sf[40] + sf[48] + sf[56] +
                                   sf[1] + sf[9] + sf[17] + sf[25] + sf[33] + sf[41] + sf[49] + sf[57] +
                                   sf[2] + sf[10] + sf[18] + sf[26] + sf[34] + sf[42] + sf[50] + sf[58] +
                                   sf[3] + sf[11] + sf[19] + sf[27] + sf[35] + sf[43] + sf[51] + sf[59] +
                                   sf[4] + sf[12] + sf[20] + sf[28] + sf[36] + sf[44] + sf[52] + sf[60] +
                                   sf[5] + sf[13] + sf[21] + sf[29] + sf[37] + sf[45] + sf[53] + sf[61] +
                                   sf[6] + sf[14] + sf[22] + sf[30] + sf[38] + sf[46] + sf[54] + sf[62] +
                                   sf[7] + sf[15] + sf[23] + sf[31] + sf[39] + sf[47] + sf[55] + sf[63]) / 8.0 / 128.0

        model.vars.shaping_filter_gain_actual.value = float(shaping_filter_gain)


    def gaussian_shaping_filter(self, model, scaling_fac = 127):
        """

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        bt = model.vars.shaping_filter_param.value
        max_filter_taps = model.vars.max_filter_taps.value
        req_filter_taps = self.get_required_filter_taps(bt)
        modulator_select = model.vars.modulator_select.value
        implement_even_mode = False

        if req_filter_taps > max_filter_taps:
            minumum_bt_supported = 8 / max_filter_taps
            bt = minumum_bt_supported
            LogMgr.Error("ERROR: BT < {} not supported on this part, overriding the current BT to {}".format(minumum_bt_supported, minumum_bt_supported))

        if modulator_select == model.vars.modulator_select.var_enum.IQ_MOD:
            implement_even_mode = True
            if req_filter_taps > 16:
                bt = 8 / 16
                LogMgr.Error("ERROR: BT < 0.5 not supported, overriding the current BT to {}".format(bt))


        if implement_even_mode:
            shaping = 2 # sets shaping filter in Even mode
            std = 1.05 / bt

            w = self.gaussian(17, std)
            w = tinynumpy.divide(w, w.sum())
            f_hack = tinynumpy.convolve(w, tinynumpy.ones((1, 8)).flatten())
            c_hack = tinynumpy.round_((scaling_fac * f_hack))
            coeff = c_hack[4:12]

            return coeff, shaping

        else:
            shaping = 3 # sets shaping filter in Asymmetric mode
            std = 1.05 / bt

            w = self.gaussian(int(max_filter_taps + 1 - 8), std) # To get max_filter_taps points after convolution
            w = tinynumpy.divide(w, w.sum())
            f_hack = tinynumpy.convolve(w, tinynumpy.ones((1, 8)).flatten())
            c_hack = tinynumpy.round_((scaling_fac * f_hack))
            c_hack = np.array(c_hack)
            coeff = c_hack[c_hack != 0]
            return coeff, shaping

    def calc_shaping_reg(self, model):
        """
        given shaping filter input parameter set shaping filter coeffs and type

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        coeff, shaping = self.run_shaping_filter_calc(model)

        self.write_coeff_registers(model, coeff, shaping)

    def run_shaping_filter_calc(self, model):

        shaping_filter_option = model.vars.shaping_filter.value
        max_filter_taps = model.vars.max_filter_taps.value
        coeff = np.zeros(max_filter_taps)
        shaping = 0
        if shaping_filter_option.value == model.vars.shaping_filter.var_enum.NONE.value:
            shaping = 0

        elif shaping_filter_option.value == model.vars.shaping_filter.var_enum.Gaussian.value:
            scaling_fac = 127
            c, shaping = self.gaussian_shaping_filter(model, scaling_fac)
            coeff = self.update_coeffs(model, c, coeff)

        elif shaping_filter_option.value == model.vars.shaping_filter.var_enum.Custom_OQPSK.value:
            coeff[0] = coeff[1] = 1
            coeff[2] = 16
            coeff[3] = 48
            coeff[4] = 80
            coeff[5] = 112
            coeff[6] = coeff[7] = 127
            coeff[8] = 0
            shaping = 2

        elif shaping_filter_option.value == model.vars.shaping_filter.var_enum.Raised_Cosine.value:
            c = self.raised_cosine_filter(model)
            shaping = 1
            coeff = self.update_coeffs(model, c, coeff)

        elif shaping_filter_option.value == model.vars.shaping_filter.var_enum.Root_Raised_Cosine.value:
            c = self.root_raised_cosine_filter(model)
            shaping = 3
            coeff = self.update_coeffs(model, c, coeff)

        elif shaping_filter_option.value == model.vars.shaping_filter.var_enum.Custom_PSK.value:
            coeff[0] = 51
            coeff[1] = 117
            coeff[2] = 96
            coeff[3] = 53
            coeff[4] = 20
            coeff[5] = 2
            coeff[6] = 0
            coeff[7] = 0
            coeff[8] = 0  # not used
            shaping = 3
        else:
            raise CalculationException("ERROR: Unrecognized shaping filter option")

        return coeff, shaping
