from pyradioconfig.parts.common.calculators.calc_ircal import CALC_IrCal
from pyradioconfig.calculator_model_framework.Utils.CustomExceptions import CalculationException
from pycalcmodel.core.variable import ModelVariableFormat
from py_2_and_3_compatibility import *

class CALC_IrCal_Ocelot(CALC_IrCal):

    def buildVariables(self, model):
        super().buildVariables(model)
        self._addModelVariable(model, 'ircal_murshf', int, ModelVariableFormat.DECIMAL)
        self._addModelVariable(model, 'ircal_muishf', int, ModelVariableFormat.DECIMAL)

    def calc_ircal_auxpll_dividers(self, model):
        auxdividers = {2419000000: (1, 63),  # For Lodiv = 1 Not currently used by the IRCal algorithm.
                       1306000000 : ( 2, 68),  # For Lodiv = 2 Untested
                       909000000: (3, 71),  # For Lodiv = 3
                       883000000: (3, 69),
                       643000000: (4, 67),  # For Lodiv = 4 Untested
                       486000000: (6, 76),  # For Lodiv = 5
                       435000000: (6, 68),  # For Lodiv = 6
                       365000000: (6, 58),  # For Lodiv = 7 # Added to support the 390MHz band.  Untested.
                       314000000: (6, 49),  # For Lodiv = 8
                       288000000: (8, 60),  # For Lodiv = 9 Untested
                       256000000: (9, 60),  # For Lodiv =10 Untested
                       214000000: (12, 67),  # For Lodiv =12 Untested
                       186000000: (12, 58),  # For Lodiv =14 Untested
                       170000000: (12, 53),  # For Lodiv =15
                       160000000: (12, 50),  # For Lodiv =16 Untested
                       143000000: (18, 67),  # For Lodiv =18 Untested
                       128000000: (18, 60),  # For Lodiv =20 Untested
                       124000000: (18, 58),  # For Lodiv =21 Untested
                       107000000: (18, 50),  # For Lodiv =24 Untested
                       102000000: (18, 48),  # For Lodiv =25 Untested
                       96000000 : (18, 45),  # For Lodiv =27 Untested
                       92000000 : (18, 43),  # For Lodiv =28 Untested
                       }

        rfFreq = model.vars.base_frequency_actual.value
        rfFreqMin = model.vars.tuning_limit_min.value
        rfFreqMax = model.vars.tuning_limit_max.value

        smallestDifference = sys.maxint  # large number
        closestBandFreq = None

        for bandFreq in auxdividers.keys():
            if (bandFreq < rfFreqMax) and (bandFreq > rfFreqMin):
                difference = abs(rfFreq - bandFreq)
                if difference < smallestDifference:
                    smallestDifference = difference
                    closestBandFreq = bandFreq

        if closestBandFreq is not None:
            (auxLODiv, auxNDiv) = auxdividers[closestBandFreq]
            model.vars.ircal_auxndiv.value = auxNDiv
            model.vars.ircal_auxlodiv.value = auxLODiv
        else:
            if rfFreq > 100000000:
                raise CalculationException(
                    "Unexpected frequency band found.  Unable to calculate IR calibration values.")

    def calc_ircal_murshf_muishf(self, model):

        model.vars.ircal_murshf.value = 24
        model.vars.ircal_muishf.value = 34