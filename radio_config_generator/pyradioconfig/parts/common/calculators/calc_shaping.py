"""Core AGC Calculator Package

Calculator functions are pulled by using their names.
Calculator functions must start with "calc_", if they are to be consumed by the framework.
    Or they should be returned by overriding the function:
        def getCalculationList(self):
"""

from collections import OrderedDict
import math

from enum import Enum

from pycalcmodel.py2_and_3_compatibility import py2round
from pyradioconfig.calculator_model_framework.Utils.CustomExceptions import CalculationException
from pycalcmodel.core.variable import ModelVariableFormat, CreateModelVariableEnum
from pyradioconfig.calculator_model_framework.interfaces.icalculator import ICalculator
from pycalcmodel.core.variable import ModelVariableFormat
from pyradioconfig.parts.common.utils.tinynumpy import tinynumpy

from pyradioconfig.calculator_model_framework.Utils.LogMgr import LogMgr

class CALC_Shaping(ICalculator):

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


        var = self._addModelVariable(model, 'shaping_filter', Enum, ModelVariableFormat.DECIMAL, desc='Defines the shaping filter to be used in the TX side.')
        member_data = [
            ['NONE',  0, 'No shaping filter is applied'],
            ['Gaussian',  1, 'Gaussian shaping filter - BT is defined at entry field Shaping Filter Parameter'],
            ['Raised_Cosine',  2, 'Raised Cosine shaping filter - R is defined at entry field Shaping Filter Parameter'],
            ['Root_Raised_Cosine',  3, 'Filter for 802.15.4 250 kbps DSSS OQPSK PHY'],
            ['Custom_OQPSK',  4, 'Filter for 802.15.4 250 kbps DSSS OQPSK PHY'],
            ['Custom_PSK', 5, 'Legacy 3rd party MSK filter']
        ]
        var.var_enum = CreateModelVariableEnum(
            'ShapingFilterEnum',
            'Defines the shaping filter to be used in the TX side.',
            member_data)

        self._addModelVariable(model, 'max_filter_taps', int, ModelVariableFormat.DECIMAL, 'Maximum Filter Taps available in Shaping filter')



    def gaussian_shaping_filter(self, model):
        """

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        bt = model.vars.shaping_filter_param.value

        max_filter_taps = model.vars.max_filter_taps.value
        req_filter_taps = self.get_required_filter_taps(bt)

        if req_filter_taps > max_filter_taps:
            minumum_bt_supported = 8 / max_filter_taps
            LogMgr.Error("ERROR: BT < {} not supported on this part".format(minumum_bt_supported))

        std = 1.05 / bt

        w = self.gaussian(17, std)
        w = tinynumpy.divide(w, w.sum())
        f_hack = tinynumpy.convolve(w, tinynumpy.ones((1, 8)).flatten())
        c_hack = tinynumpy.round_(85 * f_hack)
        coeff = []
        for i in range(4,14):
            coeff.append(c_hack[i])

        coeff[8] = 0
        return coeff

    @staticmethod
    def gaussian(M, std, sym=True):
        r"""Return a Gaussian window.

        Parameters
        ----------
        M : int
            Number of points in the output window. If zero or less, an empty
            array is returned.
        std : float
            The standard deviation, sigma.
        sym : bool, optional
            When True (default), generates a symmetric window, for use in filter
            design.
            When False, generates a periodic window, for use in spectral analysis.

        Returns
        -------
        w : ndarray
            The window, with the maximum value normalized to 1 (though the value 1
            does not appear if `M` is even and `sym` is True).

        Notes
        -----
        The Gaussian window is defined as

        .. math::  w(n) = e^{ -\frac{1}{2}\left(\frac{n}{\sigma}\right)^2 }
        """
        if M < 1:
            return tinynumpy.array([])
        if M == 1:
            return tinynumpy.ones(1, 'd')
        odd = M % 2
        if not sym and not odd:
            M = M + 1
        n = tinynumpy.arange(0, M) - (M - 1.0) / 2.0
        sig2 = 2 * std * std
        w = tinynumpy.exp(-n ** int(2) * (1/sig2))
        if not sym and not odd:
            w = w[:-1]
        return w

    def raised_cosine_filter(self, model):
        """

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        bt = model.vars.shaping_filter_param.value

        if bt > 1.0 or bt < 0.0:
            LogMgr.Error("shaping_filter_param for Raised Cosine filter is treated as Roll-off factor. "
                         "Please ensure that 0 <= shaping_filter_param <= 1. "
                         "Overriding the current shaping_filter_param value to 1.")
            bt = 1.0
        h = [0.0] * 17
        for x in tinynumpy.arange(0, 17):
            x = int(x)
            t = (x - 8) / 8.0
            if t == 0.0:
                h[x] = 127
            elif bt != 0 and t == 1/(2*bt):
                h[x] = py2round(127*(math.pi/4)*(math.sin(math.pi*t)/(math.pi*t)))
            elif bt != 0 and t == -1/(2*bt):
                h[x] = py2round(127*(math.pi/4)*(math.sin(math.pi*t)/(math.pi*t)))
            else:
                h[x] = py2round(127*(math.sin(math.pi*t)/(math.pi*t))*(math.cos(math.pi*bt*t)/(1-(((2*bt*t))*((2*bt*t))))))
        coeff = h[0:9]

        return coeff

    def root_raised_cosine_filter(self, model):
        """

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        bt = model.vars.shaping_filter_param.value

        if bt > 1.0 or bt < 0.0:
            LogMgr.Error("shaping_filter_param for Root Raised Cosine filter is treated as Roll-off factor. "
                         "Please ensure that 0 <= shaping_filter_param <= 1. "
                         "Overriding the current shaping_filter_param value to 0.")
            bt = 0.0

        pi = math.pi
        h = [0.0] * 17
        for x in tinynumpy.arange(0, 17):
            x = int(x)
            t = (x - 8) / 8.0
            if t == 0.0:
                h[x] = (1-bt)+4*bt/pi
            elif bt != 0 and t == 1/(4*bt):
                h[x] = bt/math.sqrt(2) * ((1+2/pi)*math.sin(pi/(4*bt))+(1-2/pi)*math.cos(pi/(4*bt)))
            elif bt != 0 and t == -1/(4*bt):
                h[x] = bt/math.sqrt(2) * ((1+2/pi)*math.sin(pi/(4*bt))+(1-2/pi)*math.cos(pi/(4*bt)))
            else:
                h[x] = (math.sin(pi*t*(1-bt)) + 4*bt*t*math.cos(pi*t*(1+bt)))/( pi*t*(1-(4*bt*t)*(4*bt*t)) )

        peak = max(h)
        for x in tinynumpy.arange(0, 17):
            x = int(x)
            h[x] = py2round(h[x] / peak * 127 - 0.5)
            if h[x] < 0:
                h[x] = self.floor_filter_coefficients(h[x])

        coeff = h[0:9]

        return coeff

    def calc_shaping_reg(self, model):
        """
        given shaping filter input parameter set shaping filter coeffs and type

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        shaping_filter_option = model.vars.shaping_filter.value
        c = tinynumpy.array([0, 0, 0, 0, 0, 0, 0, 0, 0])

        if shaping_filter_option.value == model.vars.shaping_filter.var_enum.NONE.value:
            shaping = 0
    
        elif shaping_filter_option.value == model.vars.shaping_filter.var_enum.Gaussian.value:
            c = self.gaussian_shaping_filter(model)
            shaping = 2
    
        elif shaping_filter_option.value == model.vars.shaping_filter.var_enum.Custom_OQPSK.value:
            c[0] = c[1] = 1
            c[2] = 16
            c[3] = 48
            c[4] = 80
            c[5] = 112
            c[6] = c[7] = 127
            c[8] = 0
            shaping = 2

        elif shaping_filter_option.value == model.vars.shaping_filter.var_enum.Raised_Cosine.value:
            c = self.raised_cosine_filter(model)
            shaping = 1

        elif shaping_filter_option.value == model.vars.shaping_filter.var_enum.Root_Raised_Cosine.value:
            c = self.root_raised_cosine_filter(model)
            shaping = 1

        elif shaping_filter_option.value == model.vars.shaping_filter.var_enum.Custom_PSK.value:
            c[0] = 51
            c[1] = 117
            c[2] = 96
            c[3] = 53
            c[4] = 20
            c[5] = 2
            c[6] = 0
            c[7] = 0
            c[8] = 0    # not used
            shaping = 3
        else:
            raise CalculationException("ERROR: Unrecognized shaping filter option")

        self._reg_write(model.vars.MODEM_SHAPING0_COEFF0,  int(c[0]))
        self._reg_write(model.vars.MODEM_SHAPING0_COEFF1,  int(c[1]))
        self._reg_write(model.vars.MODEM_SHAPING0_COEFF2,  int(c[2]))
        self._reg_write(model.vars.MODEM_SHAPING0_COEFF3,  int(c[3]))
        self._reg_write(model.vars.MODEM_SHAPING1_COEFF4,  int(c[4]))
        self._reg_write(model.vars.MODEM_SHAPING1_COEFF5,  int(c[5]))
        self._reg_write(model.vars.MODEM_SHAPING1_COEFF6,  int(c[6]))
        self._reg_write(model.vars.MODEM_SHAPING1_COEFF7,  int(c[7]))
        self._reg_write(model.vars.MODEM_SHAPING2_COEFF8,  int(c[8]))
        self._reg_write(model.vars.MODEM_CTRL0_SHAPING,  shaping)


    def calc_shaping_filter_gain_actual(self, model):
        """
        given shaping filter coefficients and mode calculate shaping filter gain
        Equations from Table 5.26 in EFR32 Reference Manual (internal.pdf)

        Args:
            model (ModelRoot) : Data model to read and write variables from
        """

        shaping_filter_mode = model.vars.MODEM_CTRL0_SHAPING.value
        c0 = model.vars.MODEM_SHAPING0_COEFF0.value
        c1 = model.vars.MODEM_SHAPING0_COEFF1.value
        c2 = model.vars.MODEM_SHAPING0_COEFF2.value
        c3 = model.vars.MODEM_SHAPING0_COEFF3.value
        c4 = model.vars.MODEM_SHAPING1_COEFF4.value
        c5 = model.vars.MODEM_SHAPING1_COEFF5.value
        c6 = model.vars.MODEM_SHAPING1_COEFF6.value
        c7 = model.vars.MODEM_SHAPING1_COEFF7.value
        c8 = model.vars.MODEM_SHAPING2_COEFF8.value
    
        if shaping_filter_mode == 0:
            shaping_filter_gain = 127
    
        elif shaping_filter_mode == 1:
            shaping_filter_gain = max(c0+c8+c0, c1+c7, c2+c6, c3+c5, c4+c4)
    
        elif shaping_filter_mode == 2:
            shaping_filter_gain = max(c0+c7, c1+c6, c2+c5, c3+c4)
    
        else:
            shaping_filter_gain = max(c0, c1, c2, c3, c4, c5, c6, c7)
    
        model.vars.shaping_filter_gain_actual.value = int(shaping_filter_gain)

    def calc_max_available_filter_taps(self, model):
        model.vars.max_filter_taps.value = 17

    def get_required_filter_taps(self, bt):
        """
        Calculate required filter taps for gaussian shaping filter with BT
        Pulse shaping will spread over (1/bt) symbols. Since shaping filter is implemented at 8*baudrate, the required
        taps for a bt will be 8/bt
        :param bt:Bandwidth time product
        :return: req_filter_taps
        """
        req_filter_taps = math.ceil(8/bt)
        return req_filter_taps

    def floor_filter_coefficients(self, h):
        return 0